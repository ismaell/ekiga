
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2015 Damien Sandras <dsandras@seconix.com>
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
 *                         h323-call-manager.h  -  description
 *                         ----------------------------------
 *   begin                : Sun Mar 15 2014
 *   authors              : Damien Sandras
 *   description          : This file contains the engine H.323 CallManager.
 *
 */


#ifndef __H323_CALL_MANAGER_H_
#define __H323_CALL_MANAGER_H_

#include "opal-call-manager.h"
#include "opal-endpoint.h"

#include "ekiga-settings.h"

namespace Opal {

  namespace H323 {

    /* This is the engine CallManager implementation.
     * It uses the Opal::Manager object to implement the engine
     * CallManager interface.
     */
    class CallManager :
        public Opal::CallManager,
        public Ekiga::URIActionProvider
    {
  public:
      CallManager (Ekiga::ServiceCore& core,
                   Opal::EndPoint& endpoint);
      ~CallManager ();

      /* URIActionProvider Methods */
      void pull_actions (Ekiga::Actor & actor,
                         const std::string & name,
                         const std::string & uri);

      /* CallManager methods we implement */
      const std::string & get_protocol_name () const;

      const Ekiga::CallManager::InterfaceList get_interfaces () const;

      /*
         void set_dtmf_mode (unsigned mode);
         unsigned get_dtmf_mode ();
         bool set_listen_port (unsigned port);
         const InterfaceList & get_interfaces () const;
       */

      /* Set up endpoint: all options or a specific setting */
      void setup (const std::string & setting = "");

  private:
      Ekiga::SettingsPtr h323_settings;
      Ekiga::SettingsPtr video_codecs_settings;

      std::string protocol_name;
    };
  };
};
#endif