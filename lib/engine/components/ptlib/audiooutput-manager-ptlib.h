
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
 *                         audioinput-manager-ptlib.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : declaration of the interface of a audioinput core.
 *                          A audioinput core manages AudioInputManagers.
 *
 */


#ifndef __AUDIOINPUT_MANAGER_PTLIB_H__
#define __AUDIOINPUT_MANAGER_PTLIB_H__

#include "audiooutput-manager.h"
#include "runtime.h"

#include <ptlib/sound.h>

/**
 * @addtogroup audioinput
 * @{
 */

  class GMAudioOutputManager_ptlib
   : public Ekiga::AudioOutputManager
    {
  public:

       GMAudioOutputManager_ptlib (Ekiga::ServiceCore & core);

      ~GMAudioOutputManager_ptlib ();

      virtual void get_devices (std::vector <Ekiga::AudioOutputDevice> & devices);

      virtual bool set_device (Ekiga::AudioOutputPS ps, const Ekiga::AudioOutputDevice & device);

      virtual bool open (Ekiga::AudioOutputPS ps, unsigned channels, unsigned samplerate, unsigned bits_per_sample);

      virtual void close(Ekiga::AudioOutputPS ps);

      virtual void set_buffer_size (Ekiga::AudioOutputPS ps, unsigned buffer_size, unsigned num_buffers);

      virtual bool set_frame_data (Ekiga::AudioOutputPS ps, 
                                   const char *data, 
                                   unsigned size,
                                   unsigned & bytes_written);

      virtual void set_volume     (Ekiga::AudioOutputPS ps, unsigned volume );

      virtual bool has_device     (const std::string & sink, const std::string & device_name, Ekiga::AudioOutputDevice & device);

    protected:
      Ekiga::ServiceCore & core;

      PSoundChannel *output_device[2];

    private:
      void device_opened_in_main (Ekiga::AudioOutputPS ps,
				  Ekiga::AudioOutputDevice device,
				  Ekiga::AudioOutputSettings settings);
      void device_closed_in_main (Ekiga::AudioOutputPS ps,
				  Ekiga::AudioOutputDevice device);
      void device_error_in_main (Ekiga::AudioOutputPS ps,
				 Ekiga::AudioOutputDevice device,
				 Ekiga::AudioOutputErrorCodes code);
  };

/**
 * @}
 */

#endif
