
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
 *                         pcssendpoint.h  -  description
 *                         ------------------------------
 *   begin                : Sun Oct 24 2004
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : This file contains the PCSS Endpoint class.
 *
 */


#ifndef _PCSS_ENDPOINT_H_
#define _PCSS_ENDPOINT_H_

#include "services.h"
#include "runtime.h"

#include <opal/opal.h>
#include <ep/pcss.h>

namespace Opal {
  class CallManager;
}

class GMPCSSEndpoint : public OpalPCSSEndPoint
{
  PCLASSINFO (GMPCSSEndpoint, OpalPCSSEndPoint);

public:
  GMPCSSEndpoint (Opal::CallManager &manager, Ekiga::ServiceCore & _core);

  bool OnShowIncoming (const OpalPCSSConnection &connection);

  bool OnShowOutgoing (const OpalPCSSConnection &connection);  

private:
  Ekiga::ServiceCore & core;
};

#endif
