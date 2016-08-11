
/*
 * Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2009 Damien Sandras <dsandras@seconix.com>

 * This program is free software; you can  redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version. This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Ekiga is licensed under the GPL license and as a special exception, you
 * have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination, without
 * applying the requirements of the GNU GPL to the OPAL, OpenH323 and PWLIB
 * programs, as long as you do follow the requirements of the GNU GPL for all
 * the rest of the software thus combined.
 */


/*
 *                         audiooutput-manager-ptlib.cpp  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : declaration of the interface of a audiooutput core.
 *                          A audioinput core manages AudioOutputManagers.
 *
 */

#include "audiooutput-manager-ptlib.h"
#include "ptlib.h"
#include "utils.h"

#define DEVICE_TYPE "PTLIB"

GMAudioOutputManager_ptlib::GMAudioOutputManager_ptlib (Ekiga::ServiceCore & _core)
: core (_core)
{
  current_state[Ekiga::primary].opened = false;
  current_state[Ekiga::secondary].opened = false;
  output_device[Ekiga::primary] = NULL;
  output_device[Ekiga::secondary] = NULL;
}

GMAudioOutputManager_ptlib::~GMAudioOutputManager_ptlib ()
{
}

void GMAudioOutputManager_ptlib::get_devices(std::vector <Ekiga::AudioOutputDevice> & devices)
{
  PStringArray audio_sources;
  PStringArray audio_devices;
  char **sources_array;
  char **devices_array;

  Ekiga::AudioOutputDevice device;
  device.type   = DEVICE_TYPE;

  audio_sources = PSoundChannel::GetDriverNames ();
  sources_array = audio_sources.ToCharArray ();
  for (PINDEX i = 0; sources_array[i] != NULL; i++) {

    device.source = sources_array[i];

    if ((device.source != "EKIGA") &&
        (device.source != "WAVFile") &&
        (device.source != "NullAudio")) {
      audio_devices = PSoundChannel::GetDeviceNames (device.source, PSoundChannel::Player);
      devices_array = audio_devices.ToCharArray ();

      for (PINDEX j = 0; devices_array[j] != NULL; j++) {

#ifdef WIN32
        /* Windows uses codepage encoding for device name, while ekiga uses utf-8 */
        device.name = codepage2utf (devices_array[j]);
#else
        device.name = devices_array[j];
#endif
        devices.push_back(device);
      }
      free (devices_array);
    }
  }
  free (sources_array);
}

bool GMAudioOutputManager_ptlib::set_device (Ekiga::AudioOutputPS ps, const Ekiga::AudioOutputDevice & device)
{
  if ( device.type == DEVICE_TYPE ) {

    PTRACE(4, "GMAudioOutputManager_ptlib\tSetting Device[" << ps << "] " << device);
    current_state[ps].device = device;
    return true;
  }

  return false;
}

bool GMAudioOutputManager_ptlib::open (Ekiga::AudioOutputPS ps, unsigned channels, unsigned samplerate, unsigned bits_per_sample)
{
  PTRACE(4, "GMAudioOutputManager_ptlib\tOpening Device " << current_state[ps].device);
  PTRACE(4, "GMAudioOutputManager_ptlib\tOpening Device with " << channels << "-" << samplerate << "/" << bits_per_sample);

  current_state[ps].channels        = channels;
  current_state[ps].samplerate      = samplerate;
  current_state[ps].bits_per_sample = bits_per_sample;

  output_device[ps] = PSoundChannel::CreateOpenedChannel (current_state[ps].device.source,
#ifdef WIN32
                                                          utf2codepage (current_state[ps].device.name),  // reencode back to codepage
#else
                                                          current_state[ps].device.name,
#endif
                                                          PSoundChannel::Player,
                                                          channels,
                                                          samplerate,
                                                          bits_per_sample);

  Ekiga::AudioOutputErrorCodes error_code = Ekiga::AO_ERROR_NONE;
  if (!output_device[ps])
    error_code = Ekiga::AO_ERROR_DEVICE;

  if (error_code != Ekiga::AO_ERROR_NONE) {
    PTRACE(1, "GMAudioOutputManager_ptlib\tEncountered error " << error_code << " while opening device[" << ps << "]");
    Ekiga::Runtime::run_in_main (boost::bind (&GMAudioOutputManager_ptlib::device_error_in_main, this, ps, current_state[ps].device, error_code));
    return false;
  }

  unsigned volume;
  output_device[ps]->GetVolume (volume);
  current_state[ps].opened = true;

  Ekiga::AudioOutputSettings settings;
  settings.volume = volume;
  settings.modifyable = true;
  Ekiga::Runtime::run_in_main (boost::bind (&GMAudioOutputManager_ptlib::device_opened_in_main, this, ps, current_state[ps].device, settings));

  return true;
}

void GMAudioOutputManager_ptlib::close(Ekiga::AudioOutputPS ps)
{
  PTRACE(4, "GMAudioOutputManager_ptlib\tClosing device[" << ps << "] " << current_state[ps].device);
  if (output_device[ps]) {
     delete output_device[ps];
     output_device[ps] = NULL;
  }
  current_state[ps].opened = false;
  Ekiga::Runtime::run_in_main (boost::bind (&GMAudioOutputManager_ptlib::device_closed_in_main, this, ps, current_state[ps].device));
}

void GMAudioOutputManager_ptlib::set_buffer_size (Ekiga::AudioOutputPS ps, unsigned buffer_size, unsigned num_buffers)
{
  PTRACE(4, "GMAudioOutputManager_ptlib\tSetting buffer size of device[" << ps << "] " << buffer_size << "/" <<  num_buffers);

  if (output_device[ps])
    output_device[ps]->SetBuffers (buffer_size, num_buffers);
}


bool GMAudioOutputManager_ptlib::set_frame_data (Ekiga::AudioOutputPS ps,
                                                 const char *data,
                                                 unsigned size,
                                                 unsigned & bytes_written)
{
  bool ret = false;
  bytes_written = 0;

  if (!current_state[ps].opened) {
    PTRACE(1, "GMAudioOutputManager_ptlib\tTrying to get frame from closed device[" << ps << "]");
    return false;
  }

  if (output_device[ps]) {
    if (size != 0)
      ret = output_device[ps]->Write ((void*)data, size);
    if (ret) {
      bytes_written = output_device[ps]->GetLastWriteCount();
    }
    if (bytes_written != size) {
      PTRACE(1, "GMAudioOutputManager_ptlib\tEncountered error while trying to write data");
      Ekiga::Runtime::run_in_main (boost::bind (&GMAudioOutputManager_ptlib::device_error_in_main, this, ps, current_state[ps].device, Ekiga::AO_ERROR_WRITE));
    }
  }

  return (ret || bytes_written == size);
}

void GMAudioOutputManager_ptlib::set_volume (Ekiga::AudioOutputPS ps, unsigned volume )
{
  PTRACE(4, "GMAudioOutputManager_ptlib\tSetting volume of device [" << ps << "] to " << volume);
  if (output_device[ps])
    output_device[ps]->SetVolume(volume);
}

bool GMAudioOutputManager_ptlib::has_device(const std::string & sink, const std::string & device_name, Ekiga::AudioOutputDevice & device)
{
  if (sink == "alsa") {
    device.type = DEVICE_TYPE;
    device.source = "ALSA";
    device.name = device_name;
    return true;
  }
/*  if (source == "oss") {
    device.type = DEVICE_TYPE;
    device.source = "OSS";
    device.device = device_name;
    return true;
  }*/
  return false;
}

void
GMAudioOutputManager_ptlib::device_opened_in_main (Ekiga::AudioOutputPS ps,
						   Ekiga::AudioOutputDevice device,
						   Ekiga::AudioOutputSettings settings)
{
  device_opened (ps, device, settings);
}

void
GMAudioOutputManager_ptlib::device_closed_in_main (Ekiga::AudioOutputPS ps,
						   Ekiga::AudioOutputDevice device)
{
  device_closed (ps, device);
}

void
GMAudioOutputManager_ptlib::device_error_in_main (Ekiga::AudioOutputPS ps,
						  Ekiga::AudioOutputDevice device,
						  Ekiga::AudioOutputErrorCodes code)
{
  device_error (ps, device, code);
}
