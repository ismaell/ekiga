
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
 *                         opal-account.cpp  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Damien Sandras
 *   copyright            : (c) 2008 by Damien Sandras
 *   description          : implementation of an opal account
 *
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <sstream>

#include <glib.h>
#include <glib/gi18n.h>

#include <ptlib.h>
#include <ptclib/guid.h>

#include <opal/opal.h>
#include <sip/sippres.h>

#include "opal-account.h"
#include "form-request-simple.h"
#include "toolbox.h"

#include "presence-core.h"
#include "personal-details.h"
#include "audiooutput-core.h"

#include "sip-endpoint.h"
#ifdef HAVE_H323
#include "h323-endpoint.h"
#endif

Opal::Account::Account (Ekiga::ServiceCore & _core,
                        const std::string & account)
  : core (_core)
{
  notification_core = core.get<Ekiga::NotificationCore> ("notification-core");
  state = Unregistered;
  status = _("Unregistered");
  message_waiting_number = 0;
  failed_registration_already_notified = false;
  dead = false;

  int i = 0;
  char *pch = strtok ((char *) account.c_str (), "|");
  while (pch != NULL) {

    switch (i) {

    case 0:
      enabled = atoi (pch);
      break;

    case 2:
      aid = pch;
      break;

    case 3:
      name = pch;
      break;

    case 4:
      protocol_name = pch;
      break;

    case 5:
      host = pch;
      break;

    case 7:
      username = pch;
      break;

    case 8:
      auth_username = pch;
      break;

    case 9:
      password = pch;
      // Could be empty, it is the only field allowed to be empty
      if (password == " ")
        password = "";
      break;

    case 10:
      timeout = atoi (pch);
      break;

    case 1:
    case 6:
    case 11:
    default:
      break;
    }
    pch = strtok (NULL, "|");
    i++;
  }

  if (host == "ekiga.net")
    type = Account::Ekiga;
  else if (host == "sip.diamondcard.us")
    type = Account::DiamondCard;
  else if (protocol_name == "SIP")
    type = Account::SIP;
  else
    type = Account::H323;

#ifdef HAVE_H323
  if (type == Account::H323)
    h323_endpoint = core.get<H323::EndPoint> ("opal-h323-endpoint");
  else {
#endif
    sip_endpoint = core.get<Sip::EndPoint> ("opal-sip-endpoint");

    if (name.find ("%limit") != std::string::npos)
      compat_mode = SIPRegister::e_CannotRegisterMultipleContacts;  // start registration in this compat mode
    else
      compat_mode = SIPRegister::e_FullyCompliant;
#ifdef HAVE_H323
  }
#endif

  setup_presentity ();
}


Opal::Account::Account (Ekiga::ServiceCore & _core,
                        Type t,
                        std::string _name,
                        std::string _host,
                        std::string _username,
                        std::string _auth_username,
                        std::string _password,
                        bool _enabled,
                        unsigned _timeout)
  : core (_core)
{
  notification_core = core.get<Ekiga::NotificationCore> ("notification-core");

  state = Unregistered;
  status = "";
  message_waiting_number = 0;
  enabled = _enabled;
  aid = (const char *) PGloballyUniqueID ().AsString ();
  name = _name;
  protocol_name = (t == H323) ? "H323" : "SIP";
  host = _host;
  username = _username;
  if (!_auth_username.empty())
    auth_username = _auth_username;
  else
    auth_username = _username;
  password = _password;
  timeout = _timeout;
  type = t;
  failed_registration_already_notified = false;
  dead = false;

#ifdef HAVE_H323
  if (type == Account::H323)
    h323_endpoint = core.get<H323::EndPoint> ("opal-h323-endpoint");
  else
#endif
    sip_endpoint = core.get<Sip::EndPoint> ("opal-sip-endpoint");

  setup_presentity ();

  if (enabled)
    enable ();
}

Opal::Account::~Account ()
{
  if (presentity)
    presentity->SetPresenceChangeNotifier (OpalPresentity::PresenceChangeNotifier(0));
}

const std::string Opal::Account::as_string () const
{
  std::stringstream str;

  if (dead)
    return "";

  str << enabled << "|1|"
      << aid << "|"
      << name << "|"
      << protocol_name << "|"
      << host << "|"
      << host << "|"
      << username << "|"
      << auth_username << "|"
      << (password.empty () ? " " : password) << "|"
      << timeout;

  return str.str ();
}

const std::string Opal::Account::get_name () const
{
  return name;
}

const std::string
Opal::Account::get_status () const
{
  std::string result;
  if (message_waiting_number > 0) {

    gchar* str = NULL;
    /* translators : the result will look like :
     * "registered (with 2 voice mail messages)"
     */
    str = g_strdup_printf (ngettext ("%s (with %d voice mail message)",
                                     "%s (with %d voice mail messages)",
                                     message_waiting_number),
                           status.c_str (), message_waiting_number);
    result = str;
    g_free (str);
  } else {

    result = status;
  }

  return result;
}

const std::string Opal::Account::get_aor () const
{
  std::stringstream str;

  str << (protocol_name == "SIP" ? "sip:" : "h323:") << username;

  if (username.find ("@") == string::npos)
    str << "@" << host;

  return str.str ();
}

const std::string Opal::Account::get_protocol_name () const
{
  return protocol_name;
}


const std::string Opal::Account::get_host () const
{
  return host;
}


const std::string Opal::Account::get_username () const
{
  return username;
}


const std::string Opal::Account::get_authentication_username () const
{
  return auth_username;
}


const std::string Opal::Account::get_password () const
{
  return password;
}


unsigned Opal::Account::get_timeout () const
{
  return timeout;
}


void Opal::Account::set_authentication_settings (const std::string & _username,
                                                 const std::string & _password)
{
  username = _username;
  auth_username = _username;
  password = _password;

  enable ();
}


void Opal::Account::enable ()
{
  enabled = true;

  state = Processing;
  status = _("Processing...");
#ifdef HAVE_H323
  if (type == Account::H323)
    h323_endpoint->subscribe (*this, presentity);
  else
#endif
    sip_endpoint->subscribe (*this, presentity);

  updated ();
  trigger_saving ();
}


void Opal::Account::disable ()
{
  enabled = false;

  if (presentity) {

    for (std::set<std::string>::iterator iter = watched_uris.begin ();
         iter != watched_uris.end (); ++iter) {
      presentity->UnsubscribeFromPresence (PString (*iter));
      Ekiga::Runtime::run_in_main (boost::bind (&Opal::Account::presence_status_in_main, this, *iter, "unknown", ""));
    }
  }
#ifdef HAVE_H323
  if (type == Account::H323)
    h323_endpoint->unsubscribe (*this, presentity);
  else
#endif
    {
      sip_endpoint->unsubscribe (*this, presentity);
      sip_endpoint->Unsubscribe (SIPSubscribe::MessageSummary, get_aor ());
    }
  // Translators: this is a state, not an action, i.e. it should be read as
  // "(you are) unregistered", and not as "(you have been) unregistered"
  status = _("Unregistered");

  updated ();
  trigger_saving ();
}


bool Opal::Account::is_enabled () const
{
  return enabled;
}


bool Opal::Account::is_active () const
{
  if (!enabled)
    return false;

  return (state == Registered);
}


SIPRegister::CompatibilityModes Opal::Account::get_compat_mode () const
{
  return compat_mode;
}


void Opal::Account::remove ()
{
  dead = true;
  if (state == Registered || state == Processing) {
    disable();
    return;
  }

  trigger_saving ();
  removed ();
}


bool Opal::Account::populate_menu (Ekiga::MenuBuilder &builder)
{
  if (enabled)
    builder.add_action ("user-offline", _("_Disable"),
                        boost::bind (&Opal::Account::disable, this));
  else
    builder.add_action ("user-available", _("_Enable"),
                        boost::bind (&Opal::Account::enable, this));

  builder.add_separator ();

  builder.add_action ("edit", _("_Edit"),
		      boost::bind (&Opal::Account::edit, this));
  builder.add_action ("remove", _("_Remove"),
		      boost::bind (&Opal::Account::remove, this));

  if (type == DiamondCard) {

    std::stringstream str;
    std::stringstream url;
    str << "https://www.diamondcard.us/exec/voip-login?accId=" << get_username () << "&pinCode=" << get_password () << "&spo=ekiga";

    builder.add_separator ();

    url.str ("");
    url << str.str () << "&act=rch";
    builder.add_action ("recharge",
			_("Recharge the account"),
                        boost::bind (&Opal::Account::on_consult, this, url.str ()));
    url.str ("");
    url << str.str () << "&act=bh";
    builder.add_action ("balance",
                        _("Consult the balance history"),
                        boost::bind (&Opal::Account::on_consult, this, url.str ()));
    url.str ("");
    url << str.str () << "&act=ch";
    builder.add_action ("history",
                        _("Consult the call history"),
                        boost::bind (&Opal::Account::on_consult, this, url.str ()));
  }

  return true;
}


void Opal::Account::edit ()
{
  boost::shared_ptr<Ekiga::FormRequestSimple> request = boost::shared_ptr<Ekiga::FormRequestSimple> (new Ekiga::FormRequestSimple (boost::bind (&Opal::Account::on_edit_form_submitted, this, _1, _2)));
  std::stringstream str;

  str << get_timeout ();

  request->title (_("Edit account"));

  request->instructions (_("Please update the following fields:"));

  request->text ("name", _("Name:"), get_name (), _("Account name, e.g. MyAccount"));
  if (get_protocol_name () == "SIP")
    request->text ("host", _("Registrar:"), get_host (), _("The registrar, e.g. ekiga.net"));
  else
    request->text ("host", _("Gatekeeper:"), get_host (), _("The gatekeeper, e.g. ekiga.net"));
  request->text ("user", _("User:"), get_username (), _("The user name, e.g. jim"));
  if (get_protocol_name () == "SIP")
    /* Translators:
     * SIP knows two usernames: The name for the client ("User") and the name
     * for the authentication procedure ("Authentication user") */
    request->text ("authentication_user", _("Authentication user:"), get_authentication_username (), _("The user name used during authentication, if different than the user name; leave empty if you do not have one"));
  request->private_text ("password", _("Password:"), get_password (), _("Password associated to the user"));
  request->text ("timeout", _("Timeout:"), str.str (), _("Time in seconds after which the account registration is automatically retried"));
  request->boolean ("enabled", _("Enable account"), enabled);

  questions (request);
}


void Opal::Account::on_edit_form_submitted (bool submitted,
					    Ekiga::Form &result)
{
  if (!submitted)
    return;

  std::string new_name = result.text ("name");
  std::string new_host = result.text ("host");
  std::string new_user = result.text ("user");
  std::string new_authentication_user;
  if (get_protocol_name () == "SIP")
    new_authentication_user = result.text ("authentication_user");
  if (new_authentication_user.empty ())
    new_authentication_user = new_user;
  std::string new_password = result.private_text ("password");
  bool new_enabled = result.boolean ("enabled");
  bool should_enable = false;
  bool should_disable = false;
  unsigned new_timeout = atoi (result.text ("timeout").c_str ());
  std::string error;

  if (new_name.empty ())
    error = _("You did not supply a name for that account.");
  else if (new_host.empty ())
    error = _("You did not supply a host to register to.");
  else if (new_user.empty ())
    error = _("You did not supply a user name for that account.");
  else if (new_timeout < 10)
    error = _("The timeout should be at least 10 seconds.");

  if (!error.empty ()) {

    boost::shared_ptr<Ekiga::FormRequestSimple> request = boost::shared_ptr<Ekiga::FormRequestSimple> (new Ekiga::FormRequestSimple (boost::bind (&Opal::Account::on_edit_form_submitted, this, _1, _2)));
    result.visit (*request);
    request->error (error);

    questions (request);
  }
  else {

    // Account was enabled and is now disabled
    // Disable it
    if (enabled != new_enabled && !new_enabled) {
      should_disable = true;
    }
    // Account was disabled and is now enabled
    // or account was already enabled
    else if (new_enabled) {
      // Some critical setting just changed
      if (host != new_host || username != new_user
          || auth_username != new_authentication_user
          || password != new_password
          || timeout != new_timeout
          || enabled != new_enabled) {

        should_enable = true;
      }
    }

    enabled = new_enabled;
    name = new_name;
    host = new_host;
    username = new_user;
    auth_username = new_authentication_user;
    password = new_password;
    timeout = new_timeout;
    enabled = new_enabled;

    if (should_enable)
      enable ();
    else if (should_disable)
      disable ();

    updated ();
    trigger_saving ();
  }
}


void
Opal::Account::on_consult (const std::string url)
{
  gm_open_uri (url.c_str ());
}


bool
Opal::Account::is_myself (const std::string uri) const
{
  size_t pos = uri.find ("@");
  if (pos == string::npos)
    return false;

  std::string uri_host = uri.substr (++pos);
  if (uri_host != get_host ())
    return false;

  return true;
}


void
Opal::Account::publish (const Ekiga::PersonalDetails& details)
{
  std::string presence = details.get_presence ();

  personal_state = OpalPresenceInfo::Available;
  presence_status = details.get_status ();

  if (presentity) {
    OpalPresenceInfo opi = OpalPresenceInfo (OpalPresenceInfo::Available);
    opi.m_activities = PString (presence);
    opi.m_note = presence_status;
    presentity->SetLocalPresence (opi);
    PTRACE (4, "Ekiga\tSent its own presence (publish) for " << get_aor() << ": " << presence << ", note " << presence_status);
  }
}


void
Opal::Account::fetch (const std::string uri)
{
  // Check if this is a presentity we watch
  if (!is_myself (uri))
    return;
  watched_uris.insert (uri);

  // Account is disabled, bye
  if (!is_enabled ())
    return;

  // Subscribe now
  if (state == Registered) {
    PTRACE(4, "Ekiga\tSubscribeToPresence for " << uri.c_str () << " (fetch)");
    presentity->SubscribeToPresence (PString (uri));
  }
}

void
Opal::Account::unfetch (const std::string uri)
{
  if (is_myself (uri) && presentity) {
    presentity->UnsubscribeFromPresence (PString (uri));
    watched_uris.erase (uri);
    Ekiga::Runtime::run_in_main (boost::bind (&Opal::Account::presence_status_in_main, this, uri, "unknown", ""));
  }
}

void
Opal::Account::handle_registration_event (RegistrationState state_,
					  const std::string info) const
{
  switch (state_) {

  case Registered:

    if (state != Registered) {

      // Translators: this is a state, not an action, i.e. it should be read as
      // "(you are) registered", and not as "(you have been) registered"
      status = _("Registered");
      boost::shared_ptr<Ekiga::PresenceCore> presence_core = core.get<Ekiga::PresenceCore> ("presence-core");
      boost::shared_ptr<Ekiga::PersonalDetails> personal_details = core.get<Ekiga::PersonalDetails> ("personal-details");
      if (presentity) {
        for (std::set<std::string>::iterator iter = watched_uris.begin ();
             iter != watched_uris.end (); ++iter) {
          PTRACE(4, "Ekiga\tSubscribeToPresence for " << iter->c_str () << " (Account Registered)");
          presentity->SubscribeToPresence (PString (*iter));
        }
        presentity->SetLocalPresence (personal_state, presence_status);
        if (type != Account::H323) {
          sip_endpoint->Subscribe (SIPSubscribe::MessageSummary, 3600, get_aor ());
        }
      }
      if (presence_core && personal_details)
	presence_core->publish (personal_details);

      state = state_;
      failed_registration_already_notified = false;
      updated ();
    }
    break;

  case Unregistered:

    // Translators: this is a state, not an action, i.e. it should be read as
    // "(you are) unregistered", and not as "(you have been) unregistered"
    status = _("Unregistered");
    failed_registration_already_notified = false;
    state = state_;

    updated ();
    /* delay destruction of this account until the
       unsubscriber thread has called back */
    if (dead)
      removed ();
    break;

  case UnregistrationFailed:

    state = state_;
    status = _("Could not unregister");
    failed_registration_already_notified = false;
    if (!info.empty ())
      status = status + " (" + info + ")";
    updated ();
    break;

  case RegistrationFailed:

    state = state_;
#ifdef HAVE_H323
    if (type == Account::H323) {
        std::stringstream msg;
        msg << _("Could not register to ") << get_name ();
        boost::shared_ptr<Ekiga::Notification> notif (new Ekiga::Notification (Ekiga::Notification::Warning, msg.str (), info, _("Edit"), boost::bind (&Opal::Account::edit, (Opal::Account*) this)));
        notification_core->push_notification (notif);
    }
    else {
#endif
      switch (compat_mode) {
      case SIPRegister::e_FullyCompliant:
        // FullyCompliant did not work, try next compat mode
        compat_mode = SIPRegister::e_CannotRegisterMultipleContacts;
        PTRACE (4, "Register failed in FullyCompliant mode, retrying in CannotRegisterMultipleContacts mode");
        sip_endpoint->subscribe (*this, presentity);
        break;
      case SIPRegister::e_CannotRegisterMultipleContacts:
        // CannotRegMC did not work, try next compat mode
        compat_mode = SIPRegister::e_CannotRegisterPrivateContacts;
        PTRACE (4, "Register failed in CannotRegisterMultipleContacts mode, retrying in CannotRegisterPrivateContacts mode");
        sip_endpoint->subscribe (*this, presentity);
        break;
      case SIPRegister::e_CannotRegisterPrivateContacts:
        // CannotRegPC did not work, try next compat mode
        compat_mode = SIPRegister::e_HasApplicationLayerGateway;
        PTRACE (4, "Register failed in CannotRegisterPrivateContacts mode, retrying in HasApplicationLayerGateway mode");
        sip_endpoint->subscribe (*this, presentity);
        break;
      case SIPRegister::e_HasApplicationLayerGateway:
        // HasAppLG did not work, stop registration with error
        compat_mode = SIPRegister::e_FullyCompliant;
        PTRACE (4, "Register failed in HasApplicationLayerGateway mode, aborting registration");
        status = _("Could not register");
        if (!info.empty ())
          status = status + " (" + info + ")";
        if (!failed_registration_already_notified) {
          std::stringstream msg;
          msg << _("Could not register to ") << get_name ();
          boost::shared_ptr<Ekiga::Notification> notif (new Ekiga::Notification (Ekiga::Notification::Warning, msg.str (), info, _("Edit"), boost::bind (&Opal::Account::edit, (Opal::Account*) this)));
          notification_core->push_notification (notif);
        }
        updated ();
        failed_registration_already_notified = true;
        break;
      default:

        state = state_;
        updated();
        break;
      }
#ifdef HAVE_H323
    }
#endif
    break;

  case Processing:

    state = state_;
    status = _("Processing...");
    updated ();
  default:

    state = state_;
    updated();
    break;
  }
}

void
Opal::Account::handle_message_waiting_information (const std::string info)
{
  std::string::size_type loc = info.find ("/", 0);

  if (loc != std::string::npos) {

    boost::shared_ptr<Ekiga::AudioOutputCore> audiooutput_core = core.get<Ekiga::AudioOutputCore> ("audiooutput-core");
    std::stringstream new_messages;
    new_messages << info.substr (0, loc);
    new_messages >> message_waiting_number;
    if (message_waiting_number > 0)
      audiooutput_core->play_event ("new_voicemail_sound");
    updated ();
  }
}

Opal::Account::Type
Opal::Account::get_type () const
{
  return type;
}

void
Opal::Account::setup_presentity ()
{
  boost::shared_ptr<CallManager> manager = core.get<CallManager> ("opal-component");
  PURL url = PString (get_aor ());
  presentity = manager->AddPresentity (url);

  if (presentity) {

    presentity->SetPresenceChangeNotifier (PCREATE_PresenceChangeNotifier (OnPresenceChange));
    presentity->GetAttributes().Set(OpalPresentity::AuthNameKey, username);
    presentity->GetAttributes().Set(OpalPresentity::AuthPasswordKey, password);
    if (type != H323) {
      presentity->GetAttributes().Set(SIP_Presentity::SubProtocolKey, "Agent");
    }
    PTRACE (4, "Created presentity for " << get_aor());
  } else
    PTRACE (4, "Error: cannot create presentity for " << get_aor());
}


void
Opal::Account::OnPresenceChange (OpalPresentity& /*presentity*/,
                                 const std::auto_ptr<OpalPresenceInfo> info)
{
  std::string new_presence;
  std::string new_status = "";

  SIPURL sip_uri = SIPURL (info->m_entity);
  sip_uri.Sanitise (SIPURL::ExternalURI);
  std::string uri = sip_uri.AsString ();
  PCaselessString note = info->m_note;

  PTRACE (4, "Ekiga\tReceived a presence change (notify) for " << info->m_entity << ": state " << info->m_state << ", activities " << info->m_activities << ", note " << info->m_note);

  if (!uri.compare (0, 5, "pres:"))
    uri.replace (0, 5, "sip:");  // replace "pres:" sith "sip:" FIXME

  new_status = (const char*) info->m_note;
  switch (info->m_state) {

  case OpalPresenceInfo::Unchanged:
    // do not change presence
    break;
  case OpalPresenceInfo::Available:
    new_presence = "available";
      if (note.Find ("dnd") != P_MAX_INDEX
          || note.Find ("meeting") != P_MAX_INDEX
          || note.Find ("do not disturb") != P_MAX_INDEX
          || note.Find ("busy") != P_MAX_INDEX) {
        new_presence = "busy";
      }
      else if (note.Find ("away") != P_MAX_INDEX
               || note.Find ("out") != P_MAX_INDEX
               || note.Find ("vacation") != P_MAX_INDEX
               || note.Find ("holiday") != P_MAX_INDEX
               || note.Find ("lunch") != P_MAX_INDEX) {
        new_presence = "away";
      }
      else if (note.Find ("phone") != P_MAX_INDEX
               || note.Find ("ringing") != P_MAX_INDEX
               || note.Find ("call") != P_MAX_INDEX) {
        new_presence = "inacall";
      }
    break;
  case OpalPresenceInfo::NoPresence:
    new_presence = "offline";
    break;
  case OpalPresenceInfo::InternalError:
  case OpalPresenceInfo::Forbidden:
  case OpalPresenceInfo::Unavailable:
    new_presence = "unknown";
    break;
  #if 0
  case OpalPresenceInfo::Away:
    new_presence = "away";
    break;
  case OpalPresenceInfo::Busy:
    new_presence = "busy";
    break;
  case OpalPresenceInfo::Appointment:
    new_presence = "away";
    // Translators: see RFC 4480 for more information about activities
    if (new_status.empty ())
      new_status = _("Appointment");
    break;
  case OpalPresenceInfo::Breakfast:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Breakfast");
    break;
  case OpalPresenceInfo::Dinner:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Dinner");
    break;
  case OpalPresenceInfo::Vacation:
  case OpalPresenceInfo::Holiday:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Holiday");
    break;
  case OpalPresenceInfo::InTransit:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("In transit");
    break;
  case OpalPresenceInfo::LookingForWork:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Looking for work");
    break;
  case OpalPresenceInfo::Lunch:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Lunch");
    break;
  case OpalPresenceInfo::Meal:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Meal");
    break;
  case OpalPresenceInfo::Meeting:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Meeting");
    break;
  case OpalPresenceInfo::OnThePhone:
    new_presence = "inacall";
    if (new_status.empty ())
      new_status = _("On the phone");
    break;
  case OpalPresenceInfo::Playing:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Playing");
    break;
  case OpalPresenceInfo::Shopping:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Shopping");
    break;
  case OpalPresenceInfo::Sleeping:
    new_presence = "away";
    if (new_status.empty ())
      new_status = _("Sleeping");
    break;
  case OpalPresenceInfo::Working:
    new_presence = "busy";
    if (new_status.empty ())
      new_status = _("Working");
    break;
  case OpalPresenceInfo::Other:
  case OpalPresenceInfo::Performance:
  case OpalPresenceInfo::PermanentAbsence:
  case OpalPresenceInfo::Presentation:
  case OpalPresenceInfo::Spectator:
  case OpalPresenceInfo::Steering:
  case OpalPresenceInfo::Travel:
  case OpalPresenceInfo::TV:
  case OpalPresenceInfo::Worship:
    new_presence = "away";
    break;
  #endif
  default:
    break;
  }

  Ekiga::Runtime::run_in_main (boost::bind (&Opal::Account::presence_status_in_main, this, uri, new_presence, new_status));
}


void
Opal::Account::presence_status_in_main (std::string uri,
					std::string uri_presence,
					std::string uri_status)
{
  presence_received (uri, uri_presence);
  status_received (uri, uri_status);
}
