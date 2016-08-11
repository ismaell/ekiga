
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
 *                         sipendpoint.h  -  description
 *                         -----------------------------
 *   begin                : Wed 24 Nov 2004
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : This file contains the SIP Endpoint class.
 *
 */


#ifndef _SIP_ENDPOINT_H_
#define _SIP_ENDPOINT_H_

#include <opal/opal.h>

#include "presence-core.h"
#include "call-manager.h"
#include "call-protocol-manager.h"
#include "opal-bank.h"
#include "sip-dialect.h"
#include "call-core.h"
#include "contact-core.h"
#include "runtime.h"
#include "services.h"

#include "opal-call-manager.h"

namespace Opal {

  namespace Sip {

    class EndPoint : public SIPEndPoint,
		     public Ekiga::Service,
		     public Ekiga::CallProtocolManager,
		     public Ekiga::PresentityDecorator,
		     public Ekiga::ContactDecorator
    {
      PCLASSINFO(EndPoint, SIPEndPoint);

    public:

      typedef std::list<std::string> domain_list;
      typedef std::list<std::string>::iterator domain_list_iterator;

      EndPoint (CallManager& ep,
		Ekiga::ServiceCore& core,
		unsigned listen_port);

      ~EndPoint ();

      /* Service */
      const std::string get_name () const
      { return "opal-sip-endpoint"; }

      const std::string get_description () const
      { return "\tObject managing SIP objects with the Opal library"; }

      /* ContactDecorator and PresentityDecorator */
      bool populate_menu (Ekiga::ContactPtr contact,
			  const std::string uri,
                          Ekiga::MenuBuilder &builder);

      bool populate_menu (Ekiga::PresentityPtr presentity,
			  const std::string uri,
                          Ekiga::MenuBuilder & builder);

      bool menu_builder_add_actions (const std::string & fullname,
                                     const std::string& uri,
                                     Ekiga::MenuBuilder & builder);


      /* Chat subsystem */
      bool send_message (const std::string & uri,
                         const std::string & message);


      /* CallProtocolManager */
      bool dial (const std::string & uri);

      const std::string & get_protocol_name () const;

      void set_dtmf_mode (unsigned mode);
      unsigned get_dtmf_mode () const;

      bool set_listen_port (unsigned port);
      const Ekiga::CallProtocolManager::Interface& get_listen_interface () const;


      /* SIP EndPoint */
      void set_nat_binding_delay (unsigned delay);
      unsigned get_nat_binding_delay ();

      void set_outbound_proxy (const std::string & uri);
      const std::string & get_outbound_proxy () const;

      void set_forward_uri (const std::string & uri);
      const std::string & get_forward_uri () const;


      /* AccountSubscriber */
      bool subscribe (const Opal::Account & account, const PSafePtr<OpalPresentity> & presentity);
      bool unsubscribe (const Opal::Account & account, const PSafePtr<OpalPresentity> & presentity);


      /* Helper */
      static std::string get_aor_domain (const std::string & aor);

      /* FIXME: doesn't look 100% right */
      void update_bank ();

      /* OPAL Methods */
      void Register (const std::string username,
		     const std::string host,
		     const std::string auth_username,
		     const std::string password,
		     bool is_enabled,
		     SIPRegister::CompatibilityModes compat_mode,
		     unsigned timeout);

      void OnRegistrationStatus (const RegistrationStatus & status);

      void OnMWIReceived (const PString & party,
                          OpalManager::MessageWaitingType type,
                          const PString & info);

      bool OnIncomingConnection (OpalConnection &connection,
                                 unsigned options,
                                 OpalConnection::StringOptions * stroptions);

      void OnDialogInfoReceived (const SIPDialogNotification & info);

      bool OnReceivedMESSAGE (SIP_PDU & pdu);

      void OnMESSAGECompleted (const SIPMessage::Params & params,
                               SIP_PDU::StatusCodes reason);

      SIPURL GetRegisteredPartyName (const SIPURL & host,
				     const OpalTransport & transport);


      /* Callbacks */
    private:
      void on_dial (std::string uri);
      void on_message (std::string uri,
		       std::string name);
      void on_transfer (std::string uri);

      void account_updated_or_removed (Ekiga::AccountPtr account);
      bool visit_account (Ekiga::AccountPtr account);
      void account_added (Ekiga::AccountPtr account);

      void registration_event_in_main (const std::string aor,
				       Account::RegistrationState state,
				       const std::string msg);

      void push_message_in_main (const std::string uri,
				 const std::string name,
				 const std::string msg);

      void push_notice_in_main (const std::string uri,
				const std::string name,
				const std::string msg);

      void mwi_received_in_main (const std::string aor,
				 const std::string info);

      PMutex aorMutex;
      std::map<std::string, std::string> accounts;

      // this object is really managed by opal,
      // so the way it is handled here is correct
      CallManager & manager;
      Ekiga::ServiceCore & core;

      std::map<std::string, PString> publications;

      Ekiga::CallProtocolManager::Interface listen_iface;

      std::string protocol_name;
      std::string uri_prefix;
      std::string forward_uri;
      std::string outbound_proxy;

      unsigned listen_port;

      boost::weak_ptr<Opal::Bank> bank;
      boost::shared_ptr<SIP::Dialect> dialect;
    };
  };
};
#endif
