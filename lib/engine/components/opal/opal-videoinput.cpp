
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
 *                         videograbber.cpp  -  description
 *                         --------------------------------
 *   begin                : Mon Feb 12 2001
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : Video4Linux compliant functions to manipulate the 
 *                          webcam device.
 *
 */

#include "opal-videoinput.h"

int PVideoInputDevice_EKIGA::devices_nbr = 0;

PVideoInputDevice_EKIGA::PVideoInputDevice_EKIGA (Ekiga::ServiceCore & _core):
  core (_core)
{
  videoinput_core = core.get<Ekiga::VideoInputCore> ("videoinput-core");
  opened = false;
  is_active = false;
}


PVideoInputDevice_EKIGA::~PVideoInputDevice_EKIGA ()
{
  Close ();
}

bool
PVideoInputDevice_EKIGA::Open (const PString &/*name*/,
			       bool start_immediate)
{
  if (start_immediate) {
    if (!is_active) {
      if (devices_nbr == 0) {
        videoinput_core->set_stream_config(frameWidth, frameHeight, frameRate);
        videoinput_core->start_stream();
      }
      is_active = true;
      devices_nbr++;
    }
  }
  opened = true;

  return true;
}


bool
PVideoInputDevice_EKIGA::IsOpen ()
{
  return opened;
}


bool
PVideoInputDevice_EKIGA::Close ()
{
  if (is_active) {
    devices_nbr--;
    if (devices_nbr==0)
      videoinput_core->stop_stream();
    is_active = false;
  }
  opened = false;

  return true;
}

  
bool
PVideoInputDevice_EKIGA::Start ()
{
  if (!is_active) {
    if (devices_nbr == 0) {
      videoinput_core->set_stream_config(frameWidth, frameHeight, frameRate);
      videoinput_core->start_stream();
    }
    is_active = true;
    devices_nbr++;
  }

  return true;
}

  
bool
PVideoInputDevice_EKIGA::Stop ()
{
  return true;
}


bool
PVideoInputDevice_EKIGA::IsCapturing ()
{
  return IsCapturing ();
}


PStringArray
PVideoInputDevice_EKIGA::GetDeviceNames() const
{
  PStringArray  devlist;
  devlist.AppendString(GetDeviceName());

  return devlist;
}


bool
PVideoInputDevice_EKIGA::SetFrameSize (unsigned int width,
				       unsigned int height)
{
  if (!PVideoDevice::SetFrameSize (width, height))
    return false;

  return true;
}


bool
PVideoInputDevice_EKIGA::GetFrameData (BYTE *frame,
				       PINDEX *i)
{
  videoinput_core->get_frame_data((char*)frame);

  *i = frameWidth * frameHeight * 3 / 2;
 
  return true;
}

//FIXME
bool PVideoInputDevice_EKIGA::GetFrameDataNoDelay (BYTE *frame,
						   PINDEX *i)
{
  videoinput_core->get_frame_data((char*)frame);

  *i = frameWidth * frameHeight * 3 / 2;
  return true;
}


bool
PVideoInputDevice_EKIGA::TestAllFormats ()
{
  return true;
}


PINDEX
PVideoInputDevice_EKIGA::GetMaxFrameBytes ()
{
  return CalculateFrameBytes (frameWidth, frameHeight, colourFormat);
}


bool
PVideoInputDevice_EKIGA::SetVideoFormat (VideoFormat newFormat)
{
  return PVideoDevice::SetVideoFormat (newFormat);
}


int
PVideoInputDevice_EKIGA::GetNumChannels()
{
  return 1;
}


bool
PVideoInputDevice_EKIGA::SetChannel (int /*newChannel*/)
{
  return true;
}


bool
PVideoInputDevice_EKIGA::SetColourFormat (const PString &newFormat)
{
  if (newFormat == "YUV420P") 
    return PVideoDevice::SetColourFormat (newFormat);

  return false;  
}


bool
PVideoInputDevice_EKIGA::SetFrameRate (unsigned rate)
{
  PVideoDevice::SetFrameRate (rate);
 
  return true;
}


bool
PVideoInputDevice_EKIGA::GetFrameSizeLimits (unsigned & minWidth,
					       unsigned & minHeight,
					       unsigned & maxWidth,
					       unsigned & maxHeight)
{
  minWidth  = 10;
  minHeight = 10;
  maxWidth  = 1000;
  maxHeight =  800;

  return true;
}


#if 0
bool PVideoInputDevice_EKIGA::GetParameters (int *whiteness,
					       int *brightness,
					       int *colour,
					       int *contrast,
					       int *hue)
{
  *whiteness = 0;
  *brightness = 0;
  *colour = 0;
  *contrast = 0;
  *hue = 0;

  return true;
}
#endif