
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
 *                         audiodev.h  -  description
 *                         --------------------------------
 *   begin                : Thu Mar 06 2008
 *   copyright            : (C) 2008 by Matthias Schneider
 *   description          : Audio dummy device to allow opal make use of 
 *                          the Ekiga Engine 
 *
 */

#ifndef _EKIGA_AUDIO_DEVICE_H_
#define _EKIGA_AUDIO_DEVICE_H_

#include <ptlib.h>
#include <ptlib/sound.h>

#include "audioinput-core.h"
#include "audiooutput-core.h"

class PSoundChannel_EKIGA : public PSoundChannel {
  PCLASSINFO(PSoundChannel_EKIGA, PSoundChannel); 
public:
  PSoundChannel_EKIGA(Ekiga::ServiceCore & core);
  PSoundChannel_EKIGA(const PString &device,
		   PSoundChannel::Directions dir,
		   unsigned numChannels,
		   unsigned sampleRate,
		   unsigned bitsPerSample,
                   Ekiga::ServiceCore & _core);
  ~PSoundChannel_EKIGA();
  static PString GetDefaultDevice(PSoundChannel::Directions);
  bool Open(const Params & params);
  bool Close();
  bool Write(const void * buf, PINDEX len);
  bool Read(void * buf, PINDEX len);
  unsigned GetChannels() const;
  unsigned GetSampleRate() const;
  unsigned GetSampleSize() const;
  bool SetBuffers(PINDEX size, PINDEX count);
  bool GetBuffers(PINDEX & size, PINDEX & count);
  bool IsOpen() const;

 private:

  PSoundChannel::Directions direction;
  PString device;
  unsigned mNumChannels;
  unsigned mSampleRate;
  unsigned mBitsPerSample;
  bool isInitialised;

  PMutex device_mutex;

  /**number of 30 (or 20) ms long sound intervals stored by EKIGA. Typically, 2.*/
  PINDEX storedPeriods;

  /**Total number of bytes of audio stored by EKIGA.  Typically, 2*480 or 960.*/
  PINDEX storedSize;

  PINDEX storedVolume;

  Ekiga::ServiceCore & core;
  boost::shared_ptr<Ekiga::AudioInputCore> audioinput_core;
  boost::shared_ptr<Ekiga::AudioOutputCore> audiooutput_core;
  bool opened;
};

#endif
