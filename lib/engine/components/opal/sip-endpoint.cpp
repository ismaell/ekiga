
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
 *                         sipendpoint.cpp  -  description
 *                         --------------------------------
 *   begin                : Wed 8 Dec 2004
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : This file contains the SIP Endpoint class.
 *
 */


#include <algorithm>
#include <sstream>

#include <glib/gi18n.h>

#include "config.h"

#include "sip-endpoint.h"

#include "opal-bank.h"
#include "opal-call.h"

#include "account-core.h"
#include "chat-core.h"
#include "personal-details.h"
#include "opal-account.h"

namespace Opal {

  namespace Sip {

    class subscriber : public PThread
    {
      PCLASSINFO(subscriber, PThread);

    public:
      subscriber (std::string _username,
		  std::string _host,
		  std::string _authentication_username,
		  std::string _password,
		  bool _is_enabled,
		  SIPRegister::CompatibilityModes _compat_mode,
		  unsigned _timeout,
		  std::string _aor,
                  Opal::Sip::EndPoint & _manager,
		  bool _registering,
                  const PSafePtr<OpalPresentity> & _presentity)
        : PThread (1000, AutoDeleteThread),
	  username(_username),
	  host(_host),
	  authentication_username(_authentication_username),
	  password(_password),
	  is_enabled(_is_enabled),
	  compat_mode(_compat_mode),
	  timeout(_timeout),
	  aor(_aor),
	  manager (_manager),
	  registering (_registering),
	  presentity (_presentity)
      {
        this->Resume ();
      };

      void Main ()
      {
	if (registering) {

          if (presentity && !presentity->IsOpen ())
            presentity->Open ();
	  manager.Register (username, host, authentication_username, password, is_enabled, compat_mode, timeout);
	}
        else {
	  manager.Unregister (aor);

          if (presentity && presentity->IsOpen ())
            presentity->Close ();
	}
      };

    private:
      std::string username;
      std::string host;
      std::string authentication_username;
      std::string password;
      bool is_enabled;
      SIPRegister::CompatibilityModes compat_mode;
      unsigned timeout;
      std::string aor;
      Opal::Sip::EndPoint & manager;
      bool registering;
      const PSafePtr<OpalPresentity> & presentity;
    };
  };
};



/* The class */
Opal::Sip::EndPoint::EndPoint (Opal::CallManager & _manager,
                               Ekiga::ServiceCore & _core,
                               unsigned _listen_port)
    :   SIPEndPoint (_manager),
	manager (_manager),
	core (_core)
{
  boost::shared_ptr<Ekiga::ChatCore> chat_core = core.get<Ekiga::ChatCore> ("chat-core");

  protocol_name = "sip";
  uri_prefix = "sip:";
  listen_port = (_listen_port > 0 ? _listen_port : 5060);

  dialect = boost::shared_ptr<SIP::Dialect>(new SIP::Dialect (core, boost::bind (&Opal::Sip::EndPoint::send_message, this, _1, _2)));
  chat_core->add_dialect (dialect);

  /* Timeouts */
  SetAckTimeout (PTimeInterval (0, 32));
  SetPduCleanUpTimeout (PTimeInterval (0, 1));
  SetInviteTimeout (PTimeInterval (0, 60));
  SetNonInviteTimeout (PTimeInterval (0, 6));
  SetRetryTimeouts (500, 4000);
  SetMaxRetries (8);

  /* Start listener */
  set_listen_port (listen_port);

  /* Update the User Agent */
  SetUserAgent ("Ekiga/" PACKAGE_VERSION);

  /* Ready to take calls */
  manager.AddRouteEntry("sip:.* = pc:*");
  manager.AddRouteEntry("pc:.* = sip:<da>");

  /* NAT Binding */
  SetNATBindingRefreshMethod (KeepAliveByOPTION);
}


Opal::Sip::EndPoint::~EndPoint ()
{
}


bool
Opal::Sip::EndPoint::populate_menu (Ekiga::ContactPtr contact,
				    const std::string uri,
				    Ekiga::MenuBuilder &builder)
{
  return menu_builder_add_actions (contact->get_name (), uri, builder);
}


bool
Opal::Sip::EndPoint::populate_menu (Ekiga::PresentityPtr presentity,
				    const std::string uri,
				    Ekiga::MenuBuilder& builder)
{
  return menu_builder_add_actions (presentity->get_name (), uri, builder);
}


bool
Opal::Sip::EndPoint::menu_builder_add_actions (const std::string& fullname,
					       const std::string& uri,
					       Ekiga::MenuBuilder & builder)
{
  bool populated = false;
  boost::shared_ptr<Opal::Bank> bk = bank.lock ();

  if (!bk)
    return false;

  std::list<std::string> uris;
  std::list<std::string> accounts_list;

  if (!(uri.find ("sip:") == 0 || uri.find (":") == string::npos))
    return false;

  if (uri.find ("@") == string::npos) {

    for (Opal::Bank::iterator it = bk->begin ();
	 it != bk->end ();
	 it++) {

      if ((*it)->get_protocol_name () == "SIP" && (*it)->is_enabled ()) {

	std::stringstream uristr;
	std::string str = uri;

	for (unsigned i = 0 ; i < str.length() ; i++) {

	  if (str [i] == ' ' || str [i] == '-') {
	    str.erase (i,1);
	    i--;
	  }
	}

	if (str.find ("sip:") == string::npos)
	  uristr << "sip:" << str;
	else
	  uristr << str;

	uristr << "@" << (*it)->get_host ();

	uris.push_back (uristr.str ());
	accounts_list.push_back ((*it)->get_name ());
      }
    }
  } else {
    uris.push_back (uri);
    accounts_list.push_back ("");
  }

  std::list<std::string>::iterator ita = accounts_list.begin ();
  for (std::list<std::string>::iterator it = uris.begin ();
       it != uris.end ();
       it++) {

    std::stringstream call_action;
    std::stringstream transfer_action;
    if (!(*ita).empty ()) {
      call_action << _("Call") << " [" << (*ita) << "]";
      transfer_action << _("Transfer") << " [" << (*ita) << "]";
    }
    else {
      call_action << _("Call");
      transfer_action << _("Transfer");
    }

    if (0 == GetConnectionCount ())
      builder.add_action ("phone-pick-up", call_action.str (),
                          boost::bind (&Opal::Sip::EndPoint::on_dial, this, (*it)));
    else
      builder.add_action ("mail-forward", transfer_action.str (),
                          boost::bind (&Opal::Sip::EndPoint::on_transfer, this, (*it)));

    ita++;
  }

  ita = accounts_list.begin ();
  for (std::list<std::string>::iterator it = uris.begin ();
       it != uris.end ();
       it++) {

    std::stringstream msg_action;
    if (!(*ita).empty ())
      msg_action << _("Message") << " [" << (*ita) << "]";
    else
      msg_action << _("Message");

    builder.add_action ("im-message-new", msg_action.str (),
                        boost::bind (&Opal::Sip::EndPoint::on_message, this, (*it), fullname));

    ita++;
  }

  populated = true;

  return populated;
}


bool
Opal::Sip::EndPoint::send_message (const std::string & _uri,
				   const std::string & _message)
{
  if (!_uri.empty () && (_uri.find ("sip:") == 0 || _uri.find (':') == string::npos) && !_message.empty ()) {
    OpalIM im;
    im.m_to = PURL (_uri);
    im.m_bodies.SetAt (PMIMEInfo::TextPlain(), _message);
    Message (im);
    return true;
  }

  return false;
}


bool
Opal::Sip::EndPoint::dial (const std::string & uri)
{
  std::stringstream ustr;

  if (uri.find ("sip:") == 0 || uri.find (":") == string::npos) {

    if (uri.find (":") == string::npos)
      ustr << "sip:" << uri;
    else
      ustr << uri;

    PString token;
    manager.SetUpCall("pc:*", ustr.str(), token, (void*) ustr.str().c_str());

    return true;
  }

  return false;
}


const std::string&
Opal::Sip::EndPoint::get_protocol_name () const
{
  return protocol_name;
}


void
Opal::Sip::EndPoint::set_dtmf_mode (unsigned mode)
{
  switch (mode) {

  case 0:  // RFC2833
    SetSendUserInputMode (OpalConnection::SendUserInputAsInlineRFC2833);
    break;
  case 1:  // SIP Info
    SetSendUserInputMode (OpalConnection::SendUserInputAsTone);
    break;
  default:
    g_return_if_reached ();
    break;
  }
}


unsigned
Opal::Sip::EndPoint::get_dtmf_mode () const
{
  // RFC2833
  if (GetSendUserInputMode () == OpalConnection::SendUserInputAsInlineRFC2833)
    return 0;

  // SIP Info
  if (GetSendUserInputMode () == OpalConnection::SendUserInputAsTone)
    return 1;

  g_return_val_if_reached (1);
  return 1;
}


bool
Opal::Sip::EndPoint::set_listen_port (unsigned port)
{
  unsigned udp_min, udp_max;

  listen_iface.protocol = "udp";
  listen_iface.voip_protocol = "sip";
  listen_iface.id = "*";

  manager.get_udp_ports (udp_min, udp_max);

  if (port > 0) {

    std::stringstream str;
    RemoveListener (NULL);

    str << "udp$*:" << port;
    if (!StartListeners (PStringArray (str.str ()))) {

      port = udp_min;
      str << "udp$*:" << port;
      while (port <= udp_max) {

        if (StartListeners (PStringArray (str.str ()))) {

          listen_iface.port = port;
          return true;
        }

        port++;
      }
    }
    else {
      listen_iface.port = port;
      return true;
    }
  }

  return false;
}


const Ekiga::CallProtocolManager::Interface&
Opal::Sip::EndPoint::get_listen_interface () const
{
  return listen_iface;
}



void
Opal::Sip::EndPoint::set_forward_uri (const std::string & uri)
{
  forward_uri = uri;
}


const std::string&
Opal::Sip::EndPoint::get_forward_uri () const
{
  return forward_uri;
}


void
Opal::Sip::EndPoint::set_outbound_proxy (const std::string & uri)
{
  outbound_proxy = uri;
  SetProxy (SIPURL (outbound_proxy));
}


const std::string&
Opal::Sip::EndPoint::get_outbound_proxy () const
{
  return outbound_proxy;
}


void
Opal::Sip::EndPoint::set_nat_binding_delay (unsigned delay)
{
  PTRACE (3, "Ekiga\tNat binding delay set to " << delay);
  if (delay > 0)
    SetNATBindingTimeout (PTimeInterval (0, delay));
}


unsigned
Opal::Sip::EndPoint::get_nat_binding_delay ()
{
  return GetNATBindingTimeout ().GetSeconds ();
}


std::string
Opal::Sip::EndPoint::get_aor_domain (const std::string & aor)
{
  std::string domain;
  std::string::size_type loc = aor.find ("@", 0);

  if (loc != string::npos)
    domain = aor.substr (loc+1);

  return domain;
}


bool
Opal::Sip::EndPoint::subscribe (const Opal::Account & account,
                                const PSafePtr<OpalPresentity> & presentity)
{
  if (account.get_protocol_name () != "SIP")
    return false;

  new subscriber (account.get_username (),
		  account.get_host (),
		  account.get_authentication_username (),
		  account.get_password (),
		  account.is_enabled (),
		  account.get_compat_mode (),
		  account.get_timeout (),
		  account.get_aor (),
		  *this,
                  true,
                  presentity);
  return true;
}


bool
Opal::Sip::EndPoint::unsubscribe (const Opal::Account & account,
                                  const PSafePtr<OpalPresentity> & presentity)
{
  if (account.get_protocol_name () != "SIP")
    return false;

  new subscriber (account.get_username (),
		  account.get_host (),
		  account.get_authentication_username (),
		  account.get_password (),
		  account.is_enabled (),
		  account.get_compat_mode (),
		  account.get_timeout (),
		  account.get_aor (),
		  *this,
                  false,
                  presentity);
  return true;
}


void
Opal::Sip::EndPoint::Register (const std::string username,
			       const std::string host_,
			       const std::string auth_username,
			       const std::string password,
			       bool is_enabled,
			       SIPRegister::CompatibilityModes compat_mode,
			       unsigned timeout)
{
  PString _aor;
  std::string host(host_);
  std::string::size_type loc = host.find (":", 0);
  if (loc != std::string::npos)
    host = host.substr (0, loc);

  SIPRegister::Params params;
  params.m_addressOfRecord = PString (username);
  params.m_registrarAddress = PString (host_);
  params.m_compatibility = compat_mode;
  params.m_authID = auth_username;
  params.m_password = password;
  params.m_expire = is_enabled ? timeout : 0;
  params.m_minRetryTime = PMaxTimeInterval;  // use default value
  params.m_maxRetryTime = PMaxTimeInterval;  // use default value

  // Register the given aor to the give registrar
  if (!SIPEndPoint::Register (params, _aor)) {
    SIPEndPoint::RegistrationStatus status;
    status.m_wasRegistering = true;
    status.m_reRegistering = false;
    status.m_userData = NULL;
    status.m_reason = SIP_PDU::Local_TransportError;
    status.m_addressofRecord = PString (username);

    OnRegistrationStatus (status);
  }
}

void
Opal::Sip::EndPoint::OnRegistrationStatus (const RegistrationStatus & status)
{
  std::string aor = (const char *) status.m_addressofRecord;
  std::string info;
  std::stringstream strm;

  if (status.m_reason == SIP_PDU::Information_Trying)
    return;

  if (aor.find (uri_prefix) == std::string::npos)
    strm << uri_prefix << aor;
  else
    strm << aor;

  SIPEndPoint::OnRegistrationStatus (status);

  /* Successful registration or unregistration */
  if (status.m_reason == SIP_PDU::Successful_OK) {

    Ekiga::Runtime::run_in_main (boost::bind (&Opal::Sip::EndPoint::registration_event_in_main, this, strm.str (), status.m_wasRegistering ? Account::Registered : Account::Unregistered, std::string ()));
  }
  /* Registration or unregistration failure */
  else {

    /* all these codes are defined in opal, file include/sip/sippdu.h */
    switch (status.m_reason) {
    case SIP_PDU::IllegalStatusCode:
      info = _("Illegal status code");
      break;

    case SIP_PDU::Local_TransportError:
      info = _("Transport error");
      break;

    case SIP_PDU::Local_BadTransportAddress:
      info = _("Invalid address");
      break;

    case SIP_PDU::Local_Timeout:
      /* Translators: Host of the remote party is offline, this should
       * appear when the remote host does not reply in an acceptable time */
      info = _("Remote party host is offline");
      break;

    case SIP_PDU::Information_Trying:
    case SIP_PDU::Information_Ringing:
    case SIP_PDU::Information_CallForwarded:
    case SIP_PDU::Information_Queued:
    case SIP_PDU::Information_Session_Progress:
    case SIP_PDU::Successful_OK:
    case SIP_PDU::Successful_Accepted:
      break;

    case SIP_PDU::Redirection_MultipleChoices:
      /* Translators: the following strings are answers from the SIP server
       * when the packet it receives has an error, see
       * http://www.ietf.org/rfc/rfc3261.txt, chapter 21 for more information */
      info = _("Multiple choices");
      break;

    case SIP_PDU::Redirection_MovedPermanently:
      info = _("Moved permanently");
      break;

    case SIP_PDU::Redirection_MovedTemporarily:
      info = _("Moved temporarily");
      break;

    case SIP_PDU::Redirection_UseProxy:
      info = _("Use proxy");
      break;

    case SIP_PDU::Redirection_AlternativeService:
      info = _("Alternative service");
      break;

    case SIP_PDU::Failure_BadRequest:
      info = _("Bad request");
      break;

    case SIP_PDU::Failure_UnAuthorised:
      info = _("Unauthorized");
      break;

    case SIP_PDU::Failure_PaymentRequired:
      info = _("Payment required");
      break;

    case SIP_PDU::Failure_Forbidden:
      info = _("Forbidden, please check that username and password are correct");
      break;

    case SIP_PDU::Failure_NotFound:
      info = _("Not found");
      break;

    case SIP_PDU::Failure_MethodNotAllowed:
      info = _("Method not allowed");
      break;

    case SIP_PDU::Failure_NotAcceptable:
      info = _("Not acceptable");
      break;

    case SIP_PDU::Failure_ProxyAuthenticationRequired:
      info = _("Proxy authentication required");
      break;

    case SIP_PDU::Failure_RequestTimeout:
      info = _("Timeout");
      break;

    case SIP_PDU::Failure_Conflict:
      info = _("Conflict");
      break;

    case SIP_PDU::Failure_LengthRequired:
      info = _("Length required");
      break;

    case SIP_PDU::Failure_RequestEntityTooLarge:
      info = _("Request entity too big");
      break;

    case SIP_PDU::Failure_RequestURITooLong:
      info = _("Request URI too long");
      break;

    case SIP_PDU::Failure_UnsupportedMediaType:
      info = _("Unsupported media type");
      break;

    case SIP_PDU::Failure_UnsupportedURIScheme:
      info = _("Unsupported URI scheme");
      break;

    case SIP_PDU::Failure_BadExtension:
      /* Translators:  The extension we are trying to register does not exist.
       * Here extension is a specific "phone number", see
       * http://en.wikipedia.org/wiki/Extension_(telephone)
       * for more information */
      info = _("Bad extension");
      break;

    case SIP_PDU::Failure_ExtensionRequired:
      info = _("Extension required");
      break;

    case SIP_PDU::Failure_IntervalTooBrief:
      info = _("Interval too brief");
      break;

    case SIP_PDU::Failure_TemporarilyUnavailable:
      info = _("Temporarily unavailable");
      break;

    case SIP_PDU::Failure_LoopDetected:
      info = _("Loop detected");
      break;

    case SIP_PDU::Failure_TooManyHops:
      info = _("Too many hops");
      break;

    case SIP_PDU::Failure_AddressIncomplete:
      info = _("Address incomplete");
      break;

    case SIP_PDU::Failure_Ambiguous:
      info = _("Ambiguous");
      break;

    case SIP_PDU::Failure_BusyHere:
      info = _("Busy Here");
      break;

    case SIP_PDU::Failure_RequestTerminated:
      info = _("Request terminated");
      break;

    case SIP_PDU::Failure_NotAcceptableHere:
      info = _("Not acceptable here");
      break;

    case SIP_PDU::Failure_BadEvent:
      info = _("Bad event");
      break;

    case SIP_PDU::Failure_RequestPending:
      info = _("Request pending");
      break;

    case SIP_PDU::Failure_Undecipherable:
      info = _("Undecipherable");
      break;

    case SIP_PDU::Failure_InternalServerError:
      info = _("Internal server error");
      break;

    case SIP_PDU::Failure_NotImplemented:
      info = _("Not implemented");
      break;

    case SIP_PDU::Failure_BadGateway:
      info = _("Bad gateway");
      break;

    case SIP_PDU::Failure_ServiceUnavailable:
      info = _("Service unavailable");
      break;

    case SIP_PDU::Failure_ServerTimeout:
      info = _("Server timeout");
      break;

    case SIP_PDU::Failure_SIPVersionNotSupported:
      info = _("SIP version not supported");
      break;

    case SIP_PDU::Failure_MessageTooLarge:
      info = _("Message too large");
      break;

    case SIP_PDU::GlobalFailure_BusyEverywhere:
      info = _("Busy everywhere");
      break;

    case SIP_PDU::GlobalFailure_Decline:
      info = _("Decline");
      break;

    case SIP_PDU::GlobalFailure_DoesNotExistAnywhere:
      info = _("Does not exist anymore");
      break;

    case SIP_PDU::GlobalFailure_NotAcceptable:
      info = _("Globally not acceptable");
      break;

    case SIP_PDU::Failure_TransactionDoesNotExist:
    case SIP_PDU::Failure_Gone:
    case SIP_PDU::MaxStatusCode:
    default:
      info = _("Failed");
    }

    /* Opal adds a RequestTerminated, and this should not be shown to user,
     * as a sip code has already been scheduled to be shown
     */
    if (status.m_reason != SIP_PDU::Failure_RequestTerminated) {
      Ekiga::Runtime::run_in_main (boost::bind (&Opal::Sip::EndPoint::registration_event_in_main, this, strm.str (), status.m_wasRegistering ? Account::RegistrationFailed : Account::UnregistrationFailed, info));
    }
  }
}


void
Opal::Sip::EndPoint::OnMWIReceived (const PString & party,
				    OpalManager::MessageWaitingType /*type*/,
				    const PString & info)
{
  std::string mwi = info;
  std::transform (mwi.begin(), mwi.end(), mwi.begin(), ::tolower);
  if (mwi == "no")
    mwi = "0/0";

  /* Signal */
  Ekiga::Runtime::run_in_main (boost::bind (&Opal::Sip::EndPoint::mwi_received_in_main, this, party, mwi));
}


bool
Opal::Sip::EndPoint::OnIncomingConnection (OpalConnection &connection,
					   unsigned options,
					   OpalConnection::StringOptions * stroptions)
{
  bool busy = false;
  PTRACE (3, "Opal::Sip::EndPoint\tIncoming connection");

  if (!SIPEndPoint::OnIncomingConnection (connection, options, stroptions))
    return false;

  for (PSafePtr<OpalConnection> conn(connectionsActive, PSafeReference); conn != NULL; ++conn) {
    if (conn->GetCall().GetToken() != connection.GetCall().GetToken() && !conn->IsReleased ())
      busy = true;
  }

  if (!forward_uri.empty () && manager.get_unconditional_forward ())
    connection.ForwardCall (forward_uri);
  else if (busy) {

    if (!forward_uri.empty () && manager.get_forward_on_busy ())
      connection.ForwardCall (forward_uri);
    else {
      connection.ClearCall (OpalConnection::EndedByLocalBusy);
    }
  }
  else {

    Opal::Call *call = dynamic_cast<Opal::Call *> (&connection.GetCall ());
    if (call) {

      if (!forward_uri.empty () && manager.get_forward_on_no_answer ())
        call->set_no_answer_forward (manager.get_reject_delay (), forward_uri);
      else // Pending
        call->set_reject_delay (manager.get_reject_delay ());
    }
  }

  return true;
}


bool
Opal::Sip::EndPoint::OnReceivedMESSAGE (SIP_PDU & pdu)
{
  if (pdu.GetMIME().GetContentType(false) != "text/plain")
    return false; // Ignore what we do not handle.

  PString from = pdu.GetMIME().GetFrom();
  PINDEX j = from.Find (';');
  if (j != P_MAX_INDEX)
    from = from.Left(j); // Remove all parameters
  j = from.Find ('<');
  if (j != P_MAX_INDEX && from.Find ('>') == P_MAX_INDEX)
    from += '>';

  SIPURL uri = from;
  uri.Sanitise (SIPURL::RequestURI);
  std::string display_name = (const char *) uri.GetDisplayName ();
  std::string message_uri = (const char *) uri.AsString ();
  std::string _message = (const char *) pdu.GetEntityBody ();

  Ekiga::Runtime::run_in_main (boost::bind (&Opal::Sip::EndPoint::push_message_in_main, this, message_uri, display_name, _message));

  return SIPEndPoint::OnReceivedMESSAGE (pdu);
}


void
Opal::Sip::EndPoint::OnMESSAGECompleted (const SIPMessage::Params & params,
                                         SIP_PDU::StatusCodes reason)
{
  PTRACE (4, "IM sending completed, reason: " << reason);

  // after TemporarilyUnavailable, RequestTimeout appears too, hence do not process it too
  if (reason == SIP_PDU::Successful_OK || reason == SIP_PDU::Failure_RequestTimeout)
    return;

  SIPURL to = params.m_remoteAddress;
  to.Sanitise (SIPURL::ToURI);
  std::string uri = (const char*) to.AsString ();
  std::string display_name = (const char*) to.GetDisplayName ();

  std::string reason_shown = _("Could not send message: ");
  if (reason == SIP_PDU::Failure_TemporarilyUnavailable)
    reason_shown += _("user offline");
  else
    reason_shown += SIP_PDU::GetStatusCodeDescription (reason).operator std::string ();  // too many to translate them with _()...

  Ekiga::Runtime::run_in_main (boost::bind (&Opal::Sip::EndPoint::push_notice_in_main, this, uri, display_name, reason_shown));
}


SIPURL
Opal::Sip::EndPoint::GetRegisteredPartyName (const SIPURL & aor,
					     const OpalTransport & transport)
{
  PWaitAndSignal m(aorMutex);
  std::string local_aor = accounts[(const char*) aor.GetHostName ()];

  if (!local_aor.empty ())
    return local_aor.c_str ();

  // as a last resort, use the local address
  return GetDefaultLocalURL (transport);
}


void
Opal::Sip::EndPoint::OnDialogInfoReceived (const SIPDialogNotification & info)
{
  gchar* _status = NULL;
  std::string status;
  std::string presence;
  std::string uri = (const char *) info.m_entity;
  PString remote_uri = info.m_remote.m_identity;
  PString remote_display_name = info.m_remote.m_display.IsEmpty () ? remote_uri : info.m_remote.m_display;
  if (uri.find ("sip:") == string::npos)
    uri = "sip:" + uri;

  switch (info.m_state) {
  case SIPDialogNotification::Proceeding:
  case SIPDialogNotification::Early:
    if (!remote_display_name.IsEmpty ())
      _status = g_strdup_printf (_("Incoming call from %s"), (const char *) remote_display_name);
    else
      _status = g_strdup_printf (_("Incoming call"));
    status = _status;
    presence = "ringing";
    break;
  case SIPDialogNotification::Confirmed:
    if (!remote_display_name.IsEmpty ())
      _status = g_strdup_printf (_("In a call with %s"), (const char *) remote_display_name);
    else
      _status = g_strdup_printf (_("In a call"));
    presence = "inacall";
    status = _status;
    break;
  default:
  case SIPDialogNotification::Trying:
  case SIPDialogNotification::Terminated:
    break;
  }
}


void Opal::Sip::EndPoint::on_dial (std::string uri)
{
  manager.dial (uri);
}


void Opal::Sip::EndPoint::on_message (std::string uri,
                                      std::string name)
{
  dialect->start_chat_with (uri, name);
}


void Opal::Sip::EndPoint::on_transfer (std::string uri)
{
  /* FIXME : we don't handle several calls here */
  for (PSafePtr<OpalConnection> connection(connectionsActive, PSafeReference); connection != NULL; ++connection)
    if (!PIsDescendant(&(*connection), OpalPCSSConnection))
      connection->TransferConnection (uri);
}


void
Opal::Sip::EndPoint::registration_event_in_main (const std::string aor,
						 Opal::Account::RegistrationState state,
						 const std::string msg)
{
  if (boost::shared_ptr<Opal::Bank> bk = bank.lock ()) {

    AccountPtr account = bk->find_account (aor);

    if (account)
      account->handle_registration_event (state, msg);
  }
}

void
Opal::Sip::EndPoint::push_message_in_main (const std::string uri,
					   const std::string name,
					   const std::string msg)
{
  dialect->push_message (uri, name, msg);
}

void
Opal::Sip::EndPoint::push_notice_in_main (const std::string uri,
					  const std::string name,
					  const std::string msg)
{
  dialect->push_notice (uri, name, msg);
}

void
Opal::Sip::EndPoint::mwi_received_in_main (const std::string aor,
					   const std::string info)
{
  if (boost::shared_ptr<Opal::Bank> bk = bank.lock ()) {

    AccountPtr account = bk->find_account (aor);

    if (account) {

      account->handle_message_waiting_information (info);
    }
  }
}

void
Opal::Sip::EndPoint::update_bank ()
{
  bank = core.get<Opal::Bank> ("opal-account-store");
  if (boost::shared_ptr<Opal::Bank> bk = bank.lock ()) { // should always happen, but still

    bk->account_added.connect (boost::bind (&Opal::Sip::EndPoint::account_added, this, _1));
    bk->account_updated.connect (boost::bind (&Opal::Sip::EndPoint::account_updated_or_removed, this, _1));
    bk->account_removed.connect (boost::bind (&Opal::Sip::EndPoint::account_updated_or_removed, this, _1));
    account_updated_or_removed (Ekiga::AccountPtr ()/* unused*/);
  }
}

void
Opal::Sip::EndPoint::account_updated_or_removed (Ekiga::AccountPtr /*account*/)
{
  /* we don't remember what the account information was, so we need
   * to clear our current information everytime something changed
   * (hopefully nobody has hundreds of opal accounts that get updated
   * often, so performance shouldn't be an issue!
   */

  { // keep the mutex only to clear the accounts variable...
    PWaitAndSignal m(aorMutex);
    accounts.clear ();
  }
  { // ... because here we call something which will want that very same mutex!
    bank = core.get<Opal::Bank> ("opal-account-store");
    if (boost::shared_ptr<Opal::Bank> bk = bank.lock ()) { // should always happen, but still

      bk->visit_accounts (boost::bind (&Opal::Sip::EndPoint::visit_account, this, _1));
    }
  }
}

bool
Opal::Sip::EndPoint::visit_account (Ekiga::AccountPtr _account)
{
  account_added (_account);
  return true;
}

void
Opal::Sip::EndPoint::account_added (Ekiga::AccountPtr _account)
{
  Opal::AccountPtr account = boost::dynamic_pointer_cast<Opal::Account> (_account);
  PWaitAndSignal m(aorMutex);
  accounts[account->get_host ()] = account->get_aor ();
}
