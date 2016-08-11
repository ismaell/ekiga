
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
 *                         videoinput-manager-mlogo.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : Declaration of the interface of a videoinput core.
 *                          A vidinput core manages VideoInputManagers.
 *
 */


#ifndef __VIDEOINPUT_MANAGER_MLOGO_H__
#define __VIDEOINPUT_MANAGER_MLOGO_H__

#include "videoinput-core.h"
#include "videoinput-manager.h"
#include "runtime.h"

#include <ptclib/delaychan.h>

/**
 * @addtogroup videoinput
 * @{
 */

  class GMVideoInputManager_mlogo
   : public Ekiga::VideoInputManager
    {
  public:

      GMVideoInputManager_mlogo (Ekiga::ServiceCore & core);

      ~GMVideoInputManager_mlogo ();


      virtual void get_devices(std::vector <Ekiga::VideoInputDevice> & devices);

      virtual bool set_device (const Ekiga::VideoInputDevice & device, int channel, Ekiga::VideoInputFormat format);

      virtual bool open (unsigned width, unsigned height, unsigned fps);

      virtual void close();

      virtual bool get_frame_data (char *data);

      virtual bool has_device (const std::string & source, const std::string & device_name, unsigned capabilities, Ekiga::VideoInputDevice & device);

  protected:
      void CopyYUVArea (const char* srcFrame,
			unsigned srcWidth,
			unsigned srcHeight,
			char* dstFrame,
			unsigned dstX,
			unsigned dstY,
			unsigned dstWidth,
			unsigned dstHeight);

      char* background_frame;
      unsigned pos;
      unsigned increment;

      Ekiga::ServiceCore & core;

      PAdaptiveDelay adaptive_delay;

    private:
      void device_opened_in_main (Ekiga::VideoInputDevice device,
				  Ekiga::VideoInputSettings settings);
      void device_closed_in_main (Ekiga::VideoInputDevice device);

  };
/**
 * @}
 */


#endif
