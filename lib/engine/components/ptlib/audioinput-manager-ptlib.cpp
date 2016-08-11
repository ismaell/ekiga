
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
 *                         audioinput-manager-ptlib.cpp  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : declaration of a PTLIB audio input manager

 *
 */

#include "audioinput-manager-ptlib.h"
#include "ptlib.h"
#include "utils.h"

#define DEVICE_TYPE "PTLIB"


GMAudioInputManager_ptlib::GMAudioInputManager_ptlib (Ekiga::ServiceCore & _core)
: core (_core)
{
  current_state.opened = false;
  input_device = NULL;
  expectedFrameSize = 0;
}

GMAudioInputManager_ptlib::~GMAudioInputManager_ptlib ()
{
}

void GMAudioInputManager_ptlib::get_devices(std::vector <Ekiga::AudioInputDevice> & devices)
{
  PStringArray audio_sources;
  PStringArray audio_devices;
  char **sources_array;
  char **devices_array;

  Ekiga::AudioInputDevice device;
  device.type   = DEVICE_TYPE;

  audio_sources = PSoundChannel::GetDriverNames ();
  sources_array = audio_sources.ToCharArray ();
  for (PINDEX i = 0; sources_array[i] != NULL; i++) {

    device.source = sources_array[i];

    if ((device.source != "EKIGA") &&
        (device.source != "WAVFile") &&
        (device.source != "NullAudio")) {
      audio_devices = PSoundChannel::GetDeviceNames (device.source, PSoundChannel::Recorder);
      devices_array = audio_devices.ToCharArray ();

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

bool GMAudioInputManager_ptlib::set_device (const Ekiga::AudioInputDevice & device)
{
  if ( device.type == DEVICE_TYPE ) {

    PTRACE(4, "GMAudioInputManager_ptlib\tSetting Device " << device);
    current_state.device = device;
    return true;
  }

  return false;
}

bool GMAudioInputManager_ptlib::open (unsigned channels, unsigned samplerate, unsigned bits_per_sample)
{
  PTRACE(4, "GMAudioInputManager_ptlib\tOpening Device " << current_state.device);
  PTRACE(4, "GMAudioInputManager_ptlib\tOpening Device with " << channels << "-" << samplerate << "/" << bits_per_sample);

  current_state.channels        = channels;
  current_state.samplerate      = samplerate;
  current_state.bits_per_sample = bits_per_sample;

  input_device = PSoundChannel::CreateOpenedChannel (current_state.device.source,
#ifdef WIN32
                                                     utf2codepage (current_state.device.name),  // reencode back to codepage
#else
                                                     current_state.device.name,
#endif
                                                     PSoundChannel::Recorder,
                                                     channels,
                                                     samplerate,
                                                     bits_per_sample);

  Ekiga::AudioInputErrorCodes error_code = Ekiga::AI_ERROR_NONE;
  if (!input_device)
    error_code = Ekiga::AI_ERROR_DEVICE;

  if (error_code != Ekiga::AI_ERROR_NONE) {
    PTRACE(1, "GMAudioInputManager_ptlib\tEncountered error " << error_code << " while opening device ");
    Ekiga::Runtime::run_in_main (boost::bind (&GMAudioInputManager_ptlib::device_error_in_main, this, current_state.device, error_code));
    return false;
  }

  unsigned volume;
  input_device->GetVolume (volume);
  current_state.opened = true;

  Ekiga::AudioInputSettings settings;
  settings.volume = volume;
  settings.modifyable = true;
  Ekiga::Runtime::run_in_main (boost::bind (&GMAudioInputManager_ptlib::device_opened_in_main, this, current_state.device, settings));

  return true;
}

void GMAudioInputManager_ptlib::close()
{
  PTRACE(4, "GMAudioInputManager_ptlib\tClosing device " << current_state.device);
  if (input_device) {
     delete input_device;
     input_device = NULL;
  }
  current_state.opened = false;
  Ekiga::Runtime::run_in_main (boost::bind (&GMAudioInputManager_ptlib::device_closed_in_main, this, current_state.device));
}

void GMAudioInputManager_ptlib::set_buffer_size (unsigned buffer_size, unsigned num_buffers)
{
  PTRACE(4, "GMAudioInputManager_ptlib\tSetting buffer size to " << buffer_size << "/" <<  num_buffers);

  if (input_device)
    input_device->SetBuffers (buffer_size, num_buffers);
}


bool GMAudioInputManager_ptlib::get_frame_data (char *data, 
                                                unsigned size,
						unsigned & bytes_read)
{
  bool ret = false;
  bytes_read = 0;

  if (!current_state.opened) {
    PTRACE(1, "GMAudioInputManager_ptlib\tTrying to get frame from closed device");
    return false;
  }

  if (input_device) {
    ret = input_device->Read ((void*)data, size);
    if (ret) {
      bytes_read = input_device->GetLastReadCount();
    }
    if (bytes_read != size) {
      PTRACE(1, "GMAudioInputManager_ptlib\tRead " << bytes_read << " instead of " << size);
    }
  }

  return (ret);
}

void GMAudioInputManager_ptlib::set_volume (unsigned volume)
{
  PTRACE(4, "GMAudioInputManager_ptlib\tSetting volume to " << volume);
  if (input_device)
    input_device->SetVolume(volume);
}

bool GMAudioInputManager_ptlib::has_device(const std::string & source, const std::string & device_name, Ekiga::AudioInputDevice & device)
{
  if (source == "alsa") {
    device.type = DEVICE_TYPE;
    device.source = "ALSA";
    device.name = device_name;
    return true;
  }
/*  if (source == "oss") {
    device.type = DEVICE_TYPE;
    device.source = "OSS";
    device.device = device;
    return true;
  }*/
  return false;
}

void
GMAudioInputManager_ptlib::device_error_in_main (Ekiga::AudioInputDevice device,
						 Ekiga::AudioInputErrorCodes code)
{
  device_error (device, code);
}

void
GMAudioInputManager_ptlib::device_opened_in_main (Ekiga::AudioInputDevice device,
						  Ekiga::AudioInputSettings settings)
{
  device_opened (device, settings);
}

void
GMAudioInputManager_ptlib::device_closed_in_main (Ekiga::AudioInputDevice device)
{
  device_closed (device);
}
