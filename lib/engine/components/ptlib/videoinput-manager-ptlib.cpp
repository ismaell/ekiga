
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
 *                         videoinput-manager-ptlib.cpp  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : declaration of the interface of a vidinput core.
 *                          A vidinput core manages VideoInputManagers.
 *
 */

#include "videoinput-manager-ptlib.h"
#include "ptlib.h"
#include "utils.h"

#define DEVICE_TYPE "PTLIB"

GMVideoInputManager_ptlib::GMVideoInputManager_ptlib (Ekiga::ServiceCore & _core)
: core (_core)
{
  current_state.opened = false;
  input_device = NULL;
  expectedFrameSize = 0;
}

GMVideoInputManager_ptlib::~GMVideoInputManager_ptlib ()
{
}

void GMVideoInputManager_ptlib::get_devices(std::vector <Ekiga::VideoInputDevice> & devices)
{
  PStringArray video_sources;
  PStringArray video_devices;
  char **sources_array;
  char **devices_array;

  Ekiga::VideoInputDevice device;
  device.type   = DEVICE_TYPE;

  video_sources = PVideoInputDevice::GetDriverNames ();
  sources_array = video_sources.ToCharArray ();
  for (PINDEX i = 0; sources_array[i] != NULL; i++) {

    device.source = sources_array[i];

    if ( (device.source != "YUVFile") &&
         (device.source != "Shm") &&
         (device.source != "FakeVideo") &&
         (device.source != "EKIGA") &&
         (device.source != "FFMPEG") &&
         (device.source != "VideoForWindows") ) {
      video_devices = PVideoInputDevice::GetDriversDeviceNames (device.source);
      devices_array = video_devices.ToCharArray ();

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

bool GMVideoInputManager_ptlib::set_device (const Ekiga::VideoInputDevice & device, int channel, Ekiga::VideoInputFormat format)
{
  if ( device.type == DEVICE_TYPE ) {

    PTRACE(4, "GMVideoInputManager_ptlib\tSetting Device " << device);
    current_state.device = device;
    current_state.channel = channel;
    current_state.format = format;
    return true;
  }

  return false;
}

bool GMVideoInputManager_ptlib::open (unsigned width, unsigned height, unsigned fps)
{
  PVideoDevice::VideoFormat pvideo_format;

  PTRACE(4, "GMVideoInputManager_ptlib\tOpening Device " << current_state.device);
  PTRACE(4, "GMVideoInputManager_ptlib\tOpening Device with " << width << "x" << height << "/" << fps);

  current_state.width  = width;
  current_state.height = height;
  current_state.fps    = fps;
  expectedFrameSize = (width * height * 3) >> 1;

  pvideo_format = (PVideoDevice::VideoFormat)current_state.format;
  input_device = PVideoInputDevice::CreateOpenedDevice (current_state.device.source,
#ifdef WIN32
                 utf2codepage (current_state.device.name),  // reencode back to codepage
#else
                 current_state.device.name,
#endif
                 FALSE);

  Ekiga::VideoInputErrorCodes error_code = Ekiga::VI_ERROR_NONE;
  if (!input_device)
    error_code = Ekiga::VI_ERROR_DEVICE;
  else if (!input_device->SetVideoFormat (pvideo_format))
    error_code = Ekiga::VI_ERROR_FORMAT;
  else if (!input_device->SetChannel (current_state.channel))
    error_code = Ekiga::VI_ERROR_CHANNEL;
  else if (!input_device->SetColourFormatConverter ("YUV420P"))
    error_code = Ekiga::VI_ERROR_COLOUR;
  else if (!input_device->SetFrameRate (current_state.fps))
    error_code = Ekiga::VI_ERROR_FPS;
  else if (!input_device->SetFrameSizeConverter (current_state.width, current_state.height, PVideoFrameInfo::eScale))
    error_code = Ekiga::VI_ERROR_SCALE;
  else input_device->Start ();

  if (error_code != Ekiga::VI_ERROR_NONE) {
    PTRACE(1, "GMVideoInputManager_ptlib\tEncountered error " << error_code << " while opening device ");
    Ekiga::Runtime::run_in_main (boost::bind (&GMVideoInputManager_ptlib::device_error_in_main, this, current_state.device, error_code));
    return false;
  }

  struct PVideoDevice::Attributes attr;
  input_device->GetAttributes (attr);
  current_state.opened = true;

  Ekiga::VideoInputSettings settings;
  settings.whiteness = attr.m_gamma >> 8;
  settings.brightness = attr.m_brightness >> 8;
  settings.colour = attr.m_saturation >> 8;
  settings.contrast = attr.m_contrast >> 8;
  settings.modifyable = true;

  Ekiga::Runtime::run_in_main (boost::bind (&GMVideoInputManager_ptlib::device_opened_in_main, this, current_state.device, settings));

  return true;
}

void GMVideoInputManager_ptlib::close()
{
  PTRACE(4, "GMVideoInputManager_ptlib\tClosing device " << current_state.device);
  if (input_device) {
    delete input_device;
    input_device = NULL;
  }
  current_state.opened = false;
  Ekiga::Runtime::run_in_main (boost::bind (&GMVideoInputManager_ptlib::device_closed_in_main, this, current_state.device));
}

bool GMVideoInputManager_ptlib::get_frame_data (char *data)
{
  bool ret = false;
  if (!current_state.opened) {
    PTRACE(1, "GMVideoInputManager_ptlib\tTrying to get frame from closed device");
    return false;
  }

  PINDEX I = 0;

  if (input_device)
    ret = input_device->GetFrameData ((BYTE*)data, &I);

  if ((unsigned) I != expectedFrameSize) {
    PTRACE(1, "GMVideoInputManager_ptlib\tExpected a frame of " << expectedFrameSize << " bytes but got " << I << " bytes");
  }
  return ret;
}

void GMVideoInputManager_ptlib::set_colour (unsigned colour)
{
  PTRACE(4, "GMVideoInputManager_ptlib\tSetting colour to " << colour);
  if (input_device) {
    struct PVideoDevice::Attributes attr;
    input_device->GetAttributes (attr);
    attr.m_saturation = colour << 8;
    input_device->SetAttributes (attr);
  }
}

void GMVideoInputManager_ptlib::set_brightness (unsigned brightness)
{
  PTRACE(4, "GMVideoInputManager_ptlib\tSetting brightness to " << brightness);
  if (input_device) {
    struct PVideoDevice::Attributes attr;
    input_device->GetAttributes (attr);
    attr.m_brightness = brightness << 8;
    input_device->SetAttributes (attr);
  }
}

void GMVideoInputManager_ptlib::set_whiteness (unsigned whiteness)
{
  PTRACE(4, "GMVideoInputManager_ptlib\tSetting whiteness to " << whiteness);
  if (input_device) {
    struct PVideoDevice::Attributes attr;
    input_device->GetAttributes (attr);
    attr.m_gamma = whiteness << 8;
    input_device->SetAttributes (attr);
  }
}

void GMVideoInputManager_ptlib::set_contrast (unsigned contrast)
{
  PTRACE(4, "GMVideoInputManager_ptlib\tSetting contrast to " << contrast);
  if (input_device) {
    struct PVideoDevice::Attributes attr;
    input_device->GetAttributes (attr);
    attr.m_contrast = contrast << 8;
    input_device->SetAttributes (attr);
  }
}

bool GMVideoInputManager_ptlib::has_device(const std::string & source, const std::string & device_name, unsigned capabilities, Ekiga::VideoInputDevice & device)
{
  if (source == "video4linux") {
    if (capabilities & 0x02) {
      device.type = DEVICE_TYPE;
      device.source = "V4L2";
      device.name = device_name;
      return true;
    }
    return false;
  }
  return false;
}

void
GMVideoInputManager_ptlib::device_opened_in_main (Ekiga::VideoInputDevice device,
						  Ekiga::VideoInputSettings settings)
{
  device_opened (device, settings);
}

void
GMVideoInputManager_ptlib::device_closed_in_main (Ekiga::VideoInputDevice device)
{
  device_closed (device);
}

void
GMVideoInputManager_ptlib::device_error_in_main (Ekiga::VideoInputDevice device,
						 Ekiga::VideoInputErrorCodes code)
{
  device_error (device, code);
}
