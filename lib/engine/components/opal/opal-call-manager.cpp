
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2009 Damien Sandras <dsandras@seconix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Ekiga is licensed under the GPL license and as a special exception,
 * you have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination,
 * without applying the requirements of the GNU GPL to the OPAL, OpenH323
 * and PWLIB programs, as long as you do follow the requirements of the
 * GNU GPL for all the rest of the software thus combined.
 */


/*
 *                         opal-call-manager.cpp  -  description
 *                         ---------------------------
 *   begin                : Sat Dec 23 2000
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : This file contains the Endpoint class.
 *
 */

#include <algorithm>
#include <glib/gi18n.h>

#include "opal-call-manager.h"

#include "pcss-endpoint.h"

#include "call-core.h"
#include "opal-codec-description.h"
#include "videoinput-info.h"

#include "call-manager.h"
#include "form-request-simple.h"

#include <opal/transcoders.h>

#include <stdlib.h>

// opal manages its endpoints itself, so we must be wary
struct null_deleter
{
 void operator()(void const *) const
 { }
};

static  bool same_codec_desc (Ekiga::CodecDescription a, Ekiga::CodecDescription b)
{
  return (a.name == b.name && a.rate == b.rate);
}


class StunDetector : public PThread
{
  PCLASSINFO(StunDetector, PThread);

public:

  StunDetector (const std::string & _server,
		Opal::CallManager& _manager,
                GAsyncQueue* _queue)
    : PThread (1000, AutoDeleteThread),
      server (_server),
      manager (_manager),
      queue (_queue)
  {
    PTRACE (3, "Ekiga\tStarted STUN detector");
    g_async_queue_ref (queue);
    this->Resume ();
  };

  ~StunDetector ()
  {
    g_async_queue_unref (queue);
    PTRACE (3, "Ekiga\tStopped STUN detector");
  }

  void Main ()
  {
    PSTUNClient::NatTypes result = manager.SetSTUNServer (server);

    g_async_queue_push (queue, GUINT_TO_POINTER ((guint)result + 1));
  };

private:
  const std::string server;
  Opal::CallManager & manager;
  GAsyncQueue* queue;
};


using namespace Opal;


/* The class */
CallManager::CallManager (Ekiga::ServiceCore & _core)
  : core (_core)
{
  stun_thread = 0;

  /* Initialise the endpoint parameters */
#if P_HAS_IPV6
  char * ekiga_ipv6 = getenv("EKIGA_IPV6");
  // use IPv6 instead of IPv4 if EKIGA_IPV6 env var is set
  if (ekiga_ipv6 && PIPSocket::IsIpAddressFamilyV6Supported())
    PIPSocket::SetDefaultIpAddressFamilyV6();
  else
    PIPSocket::SetDefaultIpAddressFamilyV4();
#else
  PIPSocket::SetDefaultIpAddressFamilyV4();
#endif
  SetAutoStartTransmitVideo (true);
  SetAutoStartReceiveVideo (true);
  SetUDPPorts (5000, 5100);
  SetTCPPorts (30000, 30100);
  SetRtpIpPorts (5000, 5100);

  pcssEP = NULL;

  forward_on_no_answer = false;
  forward_on_busy = false;
  unconditional_forward = false;
  stun_enabled = false;
  auto_answer = false;

  // Create video devices
  PVideoDevice::OpenArgs video = GetVideoOutputDevice();
  video.deviceName = "EKIGAOUT";
  SetVideoOutputDevice (video);

  video = GetVideoOutputDevice();
  video.deviceName = "EKIGAIN";
  SetVideoPreviewDevice (video);

  video = GetVideoInputDevice();
  video.deviceName = "EKIGA";
  SetVideoInputDevice (video);

  // Create endpoints
  pcssEP = new GMPCSSEndpoint (*this, core);
  pcssEP->SetSoundChannelPlayDevice("EKIGA");
  pcssEP->SetSoundChannelRecordDevice("EKIGA");

  // Media formats
  SetMediaFormatOrder (PStringArray ());
  SetMediaFormatMask (PStringArray ());

  // used to communicate with the StunDetector
  queue = g_async_queue_new ();

  PInterfaceMonitor::GetInstance().SetRefreshInterval (15000);
}


CallManager::~CallManager ()
{
  if (stun_thread)
    stun_thread->WaitForTermination ();
  ClearAllCalls (OpalConnection::EndedByLocalUser, true);
  ShutDownEndpoints ();

  g_async_queue_unref (queue);
}


void CallManager::set_display_name (const std::string & name)
{
  display_name = name;

  SetDefaultDisplayName (display_name);
}


const std::string & CallManager::get_display_name () const
{
  return display_name;
}


void CallManager::set_echo_cancellation (bool enabled)
{
  OpalEchoCanceler::Params ec;

  // General settings
  ec = GetEchoCancelParams ();
  ec.m_enabled = enabled;
  SetEchoCancelParams (ec);

  // Adjust setting for all connections of all calls
  for (PSafePtr<OpalCall> call = activeCalls;
       call != NULL;
       ++call) {

    for (int i = 0;
         i < 2;
         i++) {

      PSafePtr<OpalConnection> connection = call->GetConnection (i);
      if (connection) {

        OpalEchoCanceler *echo_canceler = connection->GetEchoCanceler ();

        if (echo_canceler)
          echo_canceler->SetParameters (ec);
      }
    }
  }
}


bool CallManager::get_echo_cancellation () const
{
  OpalEchoCanceler::Params ec = GetEchoCancelParams ();

  return ec.m_enabled;
}


void CallManager::set_maximum_jitter (unsigned max_val)
{
  // Adjust general settings
  unsigned val = std::min (std::max (max_val, (unsigned) 20), (unsigned) 1000);

  SetAudioJitterDelay (20, val);

#if 0
  // Adjust setting for all sessions of all connections of all calls
  for (PSafePtr<OpalCall> call = activeCalls;
       call != NULL;
       ++call) {

    for (int i = 0;
         i < 2;
         i++) {

      PSafePtr<OpalRTPConnection> connection = PSafePtrCast<OpalConnection, OpalRTPConnection> (call->GetConnection (i));
      if (connection) {

        OpalMediaStreamPtr stream = connection->GetMediaStream (OpalMediaType::Audio (), false);
        if (stream != NULL) {

          RTP_Session *session = connection->GetSession (stream->GetSessionID ());
          if (session != NULL) {

            unsigned units = session->GetJitterTimeUnits ();
            session->SetJitterBufferSize (20 * units, max_val * units, units);
          }
        }
      }
    }
  }
#endif
}


unsigned CallManager::get_maximum_jitter () const
{
  return GetMaxAudioJitterDelay ();
}


void CallManager::set_silence_detection (bool enabled)
{
  OpalSilenceDetector::Params sd;

  // General settings
  sd = GetSilenceDetectParams ();
  if (enabled)
    sd.m_mode = OpalSilenceDetector::AdaptiveSilenceDetection;
  else
    sd.m_mode = OpalSilenceDetector::NoSilenceDetection;
  SetSilenceDetectParams (sd);

  // Adjust setting for all connections of all calls
  for (PSafePtr<OpalCall> call = activeCalls;
       call != NULL;
       ++call) {

    for (int i = 0;
         i < 2;
         i++) {

      PSafePtr<OpalConnection> connection = call->GetConnection (i);
      if (connection) {

        OpalSilenceDetector *silence_detector = connection->GetSilenceDetector ();

        if (silence_detector)
          silence_detector->SetParameters (sd);
      }
    }
  }
}


bool CallManager::get_silence_detection () const
{
  OpalSilenceDetector::Params sd;

  sd = GetSilenceDetectParams ();

  return (sd.m_mode != OpalSilenceDetector::NoSilenceDetection);
}


void CallManager::set_reject_delay (unsigned delay)
{
  reject_delay = std::max ((unsigned) 5, delay);
}


unsigned CallManager::get_reject_delay () const
{
  return reject_delay;
}


void CallManager::set_auto_answer (bool enabled)
{
  auto_answer = enabled;
}


bool CallManager::get_auto_answer (void) const
{
  return auto_answer;
}


const Ekiga::CodecList & CallManager::get_codecs () const
{
  return codecs;
}


void CallManager::set_codecs (Ekiga::CodecList & _codecs)
{
  PStringArray initial_order;
  PStringArray initial_mask;

  OpalMediaFormatList all_media_formats;
  OpalMediaFormatList media_formats;

  PStringArray order;
  PStringArray mask;

  // What do we support
  GetAllowedFormats (all_media_formats);
  Ekiga::CodecList all_codecs = Opal::CodecList (all_media_formats);

  //
  // Clean the CodecList given as paramenter : remove unsupported codecs and
  // add missing codecs at the end of the list
  //

  // Build the Ekiga::CodecList taken into account by the CallManager
  // It contains codecs given as argument to set_codecs, and other codecs
  // supported by the manager
  for (Ekiga::CodecList::iterator it = all_codecs.begin ();
       it != all_codecs.end ();
       it++) {

    Ekiga::CodecList::iterator i  =
      search_n (_codecs.begin (), _codecs.end (), 1, *it, same_codec_desc);
    if (i == _codecs.end ()) {
      _codecs.append (*it);
    }
  }

  // Remove unsupported codecs
  for (Ekiga::CodecList::iterator it = _codecs.begin ();
       it != _codecs.end ();
       it++) {

    Ekiga::CodecList::iterator i  =
      search_n (all_codecs.begin (), all_codecs.end (), 1, *it, same_codec_desc);
    if (i == all_codecs.end ()) {
      _codecs.remove (it);
      it = _codecs.begin ();
    }
  }
  codecs = _codecs;


  //
  // Update OPAL
  //
  Ekiga::CodecList::iterator codecs_it;
  for (codecs_it = codecs.begin () ;
       codecs_it != codecs.end () ;
       codecs_it++) {

    bool active = (*codecs_it).active;
    std::string name = (*codecs_it).name;
    unsigned rate = (*codecs_it).rate;
    int j = 0;

    // Find the OpalMediaFormat corresponding to the Ekiga::CodecDescription
    if (active) {
      for (j = 0 ;
           j < all_media_formats.GetSize () ;
           j++) {

        if (name == (const char *) all_media_formats [j].GetEncodingName ()
            && (rate == all_media_formats [j].GetClockRate () || name == "G722")) {

          // Found something
          order = order + all_media_formats [j];
        }
      }
    }
  }
  

  // Add the PCSS codecs
  all_media_formats = pcssEP->GetMediaFormats ();
  for (int j = 0 ;
       j < all_media_formats.GetSize () ;
       j++)
    order = order + all_media_formats [j];


  // Build the mask
  all_media_formats = OpalTranscoder::GetPossibleFormats (pcssEP->GetMediaFormats ());
  all_media_formats.Remove (order);

  for (int i = 0 ;
       i < all_media_formats.GetSize () ;
       i++)
    mask = mask + all_media_formats [i];

  // Blacklist IM protocols for now
  mask += "T.140";
  mask += "MSRP";
  mask += "SIP-IM";

  // Blacklist NSE, since it is unused in ekiga and might create
  // problems with some registrars (such as Eutelia)
  mask += "NamedSignalEvent";

  // Update the OpalManager
  SetMediaFormatMask (mask);
  SetMediaFormatOrder (order);
}

void CallManager::set_forward_on_no_answer (bool enabled)
{
  forward_on_no_answer = enabled;
}

bool CallManager::get_forward_on_no_answer ()
{
  return forward_on_no_answer;
}

void CallManager::set_forward_on_busy (bool enabled)
{
  forward_on_busy = enabled;
}

bool CallManager::get_forward_on_busy ()
{
  return forward_on_busy;
}

void CallManager::set_unconditional_forward (bool enabled)
{
  unconditional_forward = enabled;
}

bool CallManager::get_unconditional_forward ()
{
  return unconditional_forward;
}

void CallManager::set_udp_ports (unsigned min_port,
                                 unsigned max_port)
{
  if (min_port < max_port) {

    SetUDPPorts (min_port, max_port);
    SetRtpIpPorts (min_port, max_port);
  }
}


void CallManager::get_udp_ports (unsigned & min_port,
                                 unsigned & max_port) const
{
  min_port = GetUDPPortBase ();
  max_port = GetUDPPortMax ();
}

void CallManager::set_tcp_ports (unsigned min_port,
                                 unsigned max_port)
{
  if (min_port < max_port)
    SetTCPPorts (min_port, max_port);
}


void CallManager::get_tcp_ports (unsigned & min_port,
                                 unsigned & max_port) const
{
  min_port = GetTCPPortBase ();
  max_port = GetTCPPortMax ();
}

void
CallManager::get_rtp_tos (unsigned &tos) const
{
  tos = GetMediaTypeOfService ();
}

void
CallManager::set_rtp_tos (unsigned tos)
{
  SetMediaTypeOfService (tos);
}

void CallManager::set_stun_server (const std::string & server)
{
  stun_server = server;
}


void CallManager::set_stun_enabled (bool enabled)
{
  stun_enabled = enabled;
  if (stun_enabled && !stun_thread) {

    // Ready
    stun_thread = new StunDetector (stun_server, *this, queue);
    patience = 20;
    Ekiga::Runtime::run_in_main (boost::bind (&CallManager::HandleSTUNResult, this), 1);
  } else
    ready ();
}


bool CallManager::dial (const std::string & uri)
{
  for (CallManager::iterator iter = begin ();
       iter != end ();
       iter++)
    if ((*iter)->dial (uri))
      return true;

  return false;
}


void CallManager::set_video_options (const CallManager::VideoOptions & options)
{
  OpalMediaFormatList media_formats_list;
  OpalMediaFormat::GetAllRegisteredMediaFormats (media_formats_list);

  // Configure all mediaOptions of all Video MediaFormats
  for (int i = 0 ; i < media_formats_list.GetSize () ; i++) {

    OpalMediaFormat media_format = media_formats_list [i];
    if (media_format.GetMediaType() == OpalMediaType::Video ()) {

      media_format.SetOptionInteger (OpalVideoFormat::FrameWidthOption (),
                                     Ekiga::VideoSizes [options.size].width);
      media_format.SetOptionInteger (OpalVideoFormat::FrameHeightOption (),
                                     Ekiga::VideoSizes [options.size].height);
      media_format.SetOptionInteger (OpalVideoFormat::FrameTimeOption (),
                                     (int) (90000 / (options.maximum_frame_rate > 0 ? options.maximum_frame_rate : 30)));
      media_format.SetOptionInteger (OpalVideoFormat::MaxBitRateOption (),
                                     (options.maximum_received_bitrate > 0 ? options.maximum_received_bitrate : 4096) * 1000);
      media_format.SetOptionInteger (OpalVideoFormat::TargetBitRateOption (),
                                     (options.maximum_transmitted_bitrate > 0 ? options.maximum_transmitted_bitrate : 48) * 1000);
      media_format.SetOptionInteger (OpalVideoFormat::MinRxFrameWidthOption(),
                                     160);
      media_format.SetOptionInteger (OpalVideoFormat::MinRxFrameHeightOption(),
                                     120);
      media_format.SetOptionInteger (OpalVideoFormat::MaxRxFrameWidthOption(),
                                     1920);
      media_format.SetOptionInteger (OpalVideoFormat::MaxRxFrameHeightOption(),
                                     1088);
      media_format.AddOption(new OpalMediaOptionUnsigned (OpalVideoFormat::TemporalSpatialTradeOffOption (),
                                                          true, OpalMediaOption::NoMerge,
                                                          options.temporal_spatial_tradeoff));
      media_format.SetOptionInteger (OpalVideoFormat::TemporalSpatialTradeOffOption(),
                                     (options.temporal_spatial_tradeoff > 0 ? options.temporal_spatial_tradeoff : 31));
      media_format.AddOption(new OpalMediaOptionUnsigned (OpalVideoFormat::MaxFrameSizeOption (),
                                                          true, OpalMediaOption::NoMerge, 1400));
      media_format.SetOptionInteger (OpalVideoFormat::MaxFrameSizeOption (),
                                     1400);

      if ( media_format.GetName() != "YUV420P" &&
           media_format.GetName() != "RGB32" &&
           media_format.GetName() != "RGB24") {

        media_format.SetOptionInteger (OpalVideoFormat::RateControlPeriodOption(),
                                       300);
      }

      switch (options.extended_video_roles) {
      case 0 :
        media_format.SetOptionInteger(OpalVideoFormat::ContentRoleMaskOption(), 0);
        break;

      case 2 : // Force Presentation (slides)
        media_format.SetOptionInteger(OpalVideoFormat::ContentRoleMaskOption(),
                                      OpalVideoFormat::ContentRoleBit(OpalVideoFormat::ePresentation));
        break;

      case 3 : // Force Live (main)
        media_format.SetOptionInteger(OpalVideoFormat::ContentRoleMaskOption(),
                                      OpalVideoFormat::ContentRoleBit(OpalVideoFormat::eMainRole));
        break;

        default :
          break;
      }

      OpalMediaFormat::SetRegisteredMediaFormat(media_format);
    }
  }

  // Adjust setting for all sessions of all connections of all calls
  for (PSafePtr<OpalCall> call = activeCalls;
       call != NULL;
       ++call) {

    for (int i = 0;
         i < 2;
         i++) {

      PSafePtr<OpalRTPConnection> connection = PSafePtrCast<OpalConnection, OpalRTPConnection> (call->GetConnection (i));
      if (connection) {

        OpalMediaStreamPtr stream = connection->GetMediaStream (OpalMediaType::Video (), false);
        if (stream != NULL) {

          OpalMediaFormat mediaFormat = stream->GetMediaFormat ();
          mediaFormat.SetOptionInteger (OpalVideoFormat::TemporalSpatialTradeOffOption(),
                                        (options.temporal_spatial_tradeoff > 0 ? options.temporal_spatial_tradeoff : 31));
          mediaFormat.SetOptionInteger (OpalVideoFormat::TargetBitRateOption (),
                                        (options.maximum_transmitted_bitrate > 0 ? options.maximum_transmitted_bitrate : 48) * 1000);
          mediaFormat.ToNormalisedOptions();
          stream->UpdateMediaFormat (mediaFormat);
        }
      }
    }
  }
}


void CallManager::get_video_options (CallManager::VideoOptions & options) const
{
  OpalMediaFormatList media_formats_list;
  OpalMediaFormat::GetAllRegisteredMediaFormats (media_formats_list);

  for (int i = 0 ; i < media_formats_list.GetSize () ; i++) {

    OpalMediaFormat media_format = media_formats_list [i];
    if (media_format.GetMediaType () == OpalMediaType::Video ()) {

      int j;
      for (j = 0; j < NB_VIDEO_SIZES; j++) {

        if (Ekiga::VideoSizes [j].width == media_format.GetOptionInteger (OpalVideoFormat::FrameWidthOption ())
            && Ekiga::VideoSizes [j].height == media_format.GetOptionInteger (OpalVideoFormat::FrameHeightOption ()))
          break;
      }
      if (j >= NB_VIDEO_SIZES)
        g_error ("Cannot find video size");
      options.size = j;

      options.maximum_frame_rate =
        (int) (90000 / media_format.GetOptionInteger (OpalVideoFormat::FrameTimeOption ()));
      options.maximum_received_bitrate =
        (int) (media_format.GetOptionInteger (OpalVideoFormat::MaxBitRateOption ()) / 1000);
      options.maximum_transmitted_bitrate =
        (int) (media_format.GetOptionInteger (OpalVideoFormat::TargetBitRateOption ()) / 1000);
      options.temporal_spatial_tradeoff =
        media_format.GetOptionInteger (OpalVideoFormat::TemporalSpatialTradeOffOption ());

      int evr = media_format.GetOptionInteger (OpalVideoFormat::OpalVideoFormat::ContentRoleMaskOption ());
      switch (evr) {
      case 0: // eNoRole
        options.extended_video_roles = 0;
        break;
      case 1: // ePresentation
        options.extended_video_roles = 2;
        break;
      case 2: // eMainRole
        options.extended_video_roles = 3;
        break;
      default:
        options.extended_video_roles = 1;
        break;
      }

      break;
    }
  }
}

void
CallManager::create_call_in_main (Opal::Call* call)
{
  boost::shared_ptr<Ekiga::CallCore> call_core = core.get<Ekiga::CallCore> ("call-core");

  call_core->add_call (boost::shared_ptr<Opal::Call>(call, null_deleter()),
		       boost::dynamic_pointer_cast<CallManager>(shared_from_this ()));
}

OpalCall *CallManager::CreateCall (void *uri)
{
  Opal::Call* call = 0;

  if (uri != 0)
    call = new Opal::Call (*this, core, (const char *) uri);
  else
    call = new Opal::Call (*this, core, "");

  Ekiga::Runtime::run_in_main (boost::bind (&CallManager::create_call_in_main, this, call));

  return call;
}

void
CallManager::emit_removed_in_main (Ekiga::Call* call)
{
  call->removed ();
}

void
CallManager::DestroyCall (OpalCall *_call)
{
  Ekiga::Call *call = dynamic_cast<Ekiga::Call *> (_call);

  Ekiga::Runtime::run_in_main(boost::bind (&CallManager::emit_removed_in_main, this, call));
}


void
CallManager::OnClosedMediaStream (const OpalMediaStream & stream)
{
  OpalMediaFormatList list = pcssEP->GetMediaFormats ();
  OpalManager::OnClosedMediaStream (stream);

  if (list.FindFormat(stream.GetMediaFormat()) != list.end ())
    dynamic_cast <Opal::Call &> (stream.GetConnection ().GetCall ()).OnClosedMediaStream ((OpalMediaStream &) stream);
}


bool
CallManager::OnOpenMediaStream (OpalConnection & connection,
				OpalMediaStream & stream)
{
  OpalMediaFormatList list = pcssEP->GetMediaFormats ();
  if (!OpalManager::OnOpenMediaStream (connection, stream))
    return FALSE;

  if (list.FindFormat(stream.GetMediaFormat()) == list.end ())
    dynamic_cast <Opal::Call &> (connection.GetCall ()).OnOpenMediaStream (stream);

  return TRUE;
}


void CallManager::GetAllowedFormats (OpalMediaFormatList & full_list)
{
  OpalMediaFormatList list = OpalTranscoder::GetPossibleFormats (pcssEP->GetMediaFormats ());
  std::list<std::string> black_list;

  black_list.push_back ("GSM-AMR");
  black_list.push_back ("Linear-16-Stereo-48kHz");
  black_list.push_back ("LPC-10");
  black_list.push_back ("SpeexIETFNarrow-11k");
  black_list.push_back ("SpeexIETFNarrow-15k");
  black_list.push_back ("SpeexIETFNarrow-18.2k");
  black_list.push_back ("SpeexIETFNarrow-24.6k");
  black_list.push_back ("SpeexIETFNarrow-5.95k");
  black_list.push_back ("iLBC-13k3");
  black_list.push_back ("iLBC-15k2");
  black_list.push_back ("RFC4175_YCbCr-4:2:0");
  black_list.push_back ("RFC4175_RGB");

  // Purge blacklisted codecs
  for (PINDEX i = 0 ; i < list.GetSize () ; i++) {

    std::list<std::string>::iterator it = find (black_list.begin (), black_list.end (), (const char *) list [i]);
    if (it == black_list.end ()) {
      if (list [i].GetMediaType () == OpalMediaType::Audio () || list [i].GetMediaType () == OpalMediaType::Video ())
        full_list += list [i];
    }
  }
}

void
CallManager::HandleSTUNResult ()
{
  gboolean error = false;
  gboolean got_answer = false;

  if (g_async_queue_length (queue) > 0) {

    PSTUNClient::NatTypes result
      = (PSTUNClient::NatTypes)(GPOINTER_TO_UINT (g_async_queue_pop (queue))-1);
    got_answer = true;
    stun_thread = 0;

    if (result == PSTUNClient::SymmetricNat
	|| result == PSTUNClient::BlockedNat
	|| result == PSTUNClient::PartiallyBlocked) {

      error = true;
    } else {

      for (Ekiga::CallManager::iterator iter = begin ();
	   iter != end ();
	   ++iter)
	(*iter)->set_listen_port ((*iter)->get_listen_interface ().port);
      ready ();
    }
  } else if (patience == 0) {

    error = true;
  }

  if (error) {

    ReportSTUNError (_("Ekiga did not manage to configure your network settings automatically. You can"
		       " still use it, but you need to configure your network settings manually.\n\n"
		       "Please see http://wiki.ekiga.org/index.php/Enable_port_forwarding_manually for"
		       " instructions"));
    ready ();
  } else if (!got_answer) {

    patience--;
    Ekiga::Runtime::run_in_main (boost::bind (&CallManager::HandleSTUNResult, this), 1);

  }
}

void
CallManager::ReportSTUNError (const std::string error)
{
  boost::shared_ptr<Ekiga::CallCore> call_core = core.get<Ekiga::CallCore> ("call-core");

  // notice we're in for an infinite loop if nobody ever reports to the user!
  if ( !call_core->errors (error)) {

    Ekiga::Runtime::run_in_main (boost::bind (&CallManager::ReportSTUNError, this, error),
				 10);
  }
}

PBoolean
CallManager::CreateVideoOutputDevice(const OpalConnection & connection,
                                     const OpalMediaFormat & media_fmt,
                                     PBoolean preview,
                                     PVideoOutputDevice * & device,
                                     PBoolean & auto_delete)
{
  PVideoDevice::OpenArgs videoArgs;
  PString title;

  videoArgs = preview ?
    GetVideoPreviewDevice() : GetVideoOutputDevice();

  if (!preview) {
    unsigned openChannelCount = 0;
    OpalMediaStreamPtr mediaStream;

    while ((mediaStream = connection.GetMediaStream(OpalMediaType::Video(),
                                                    preview, mediaStream)) != NULL)
      ++openChannelCount;

    videoArgs.deviceName += psprintf(" ID=%u", openChannelCount);
  }

  media_fmt.AdjustVideoArgs(videoArgs);

  auto_delete = true;
  device = PVideoOutputDevice::CreateOpenedDevice(videoArgs, false);
  return device != NULL;
}
