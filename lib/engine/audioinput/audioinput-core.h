
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
 *                         audioinput-core.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : Declaration of the interface of a audioinput core.
 *                          A audioinput core manages AudioInputManagers.
 *
 */

#ifndef __AUDIOINPUT_CORE_H__
#define __AUDIOINPUT_CORE_H__

#include "services.h"
#include "runtime.h"

#include "audioinput-manager.h"
#include "notification-core.h"
#include "hal-core.h"
#include "audioinput-gmconf-bridge.h"

#include "ptlib.h"

#define AUDIO_INPUT_FALLBACK_DEVICE_TYPE   "Ekiga"
#define AUDIO_INPUT_FALLBACK_DEVICE_SOURCE "Ekiga"
#define AUDIO_INPUT_FALLBACK_DEVICE_NAME   "SILENT"

namespace Ekiga
{
/**
 * @defgroup audioinput
 * @{
 */

  /** Core object for the audio input support
   * The audio input core abstracts all functionality related to audio input
   * in a thread safe manner. Typically, most of the functions except start_stream(),
   * stop_stream(), set_stream_buffer_size() and get_frame_data() will be called from
   * a UI thread, while the three mentioned funtions will be used by an audio
   * streaming thread.
   *
   * The audio input core abstracts different audio input managers, which can
   * represent different backends like PTLIB, from the application and can
   * switch the input device transparently for the audio streaming thread
   * even while capturing is in progress.
   *
   * If the removal of an audio input device is detected by a failed
   * read or by a message from the HalCore, the audio input core will
   * determine the responsible audio input manager and send a signal to the UI,
   * which can be used to update device lists. Also, if the removed device was the
   * currently used one, the core falls back to the backup device.
   *
   * A similar procedure is performed on the addition of a device. In case we fell
   * back due to a removed device, and the respective device is re-added to the system,
   * it will be automatically activated.
   *
   * The audio input core can also be used in a preview mode, where it starts a separate
   * thread (represented by the AudioPreviewManager), which grabs frames from the audio
   * input core and passes them to the audio output core. This can be used for audio device
   * testing. Note that, contrary to the video preview, the audio preview does not support
   * direct switching between the preview and the streaming mode, which must tus be
   * be prevented by the UI.
   */
  class AudioInputCore
    : public Service
    {

  public:

      /** The constructor
       */
      AudioInputCore (Ekiga::ServiceCore & core);

      /** The destructor
      */
      ~AudioInputCore ();

      /** Set up gmconf bridge
       */
      void setup_conf_bridge();


      /*** Service Implementation ***/

      /** Returns the name of the service.
       * @return The service name.
       */
      const std::string get_name () const
        { return "audioinput-core"; }


      /** Returns the description of the service.
       * @return The service description.
       */
      const std::string get_description () const
        { return "\tAudioInput Core managing AudioInput Manager objects"; }


      /** Adds a AudioInputManager to the AudioInputCore service.
       * @param The manager to be added.
       */
      void add_manager (AudioInputManager &manager);

      /** Triggers a callback for all Ekiga::AudioInputManager sources of the
       * AudioInputCore service.
       */
      void visit_managers (boost::function1<bool, AudioInputManager &> visitor) const;

      /** This signal is emitted when a Ekiga::AudioInputManager has been
       * added to the AudioInputCore Service.
       */
      boost::signal1<void, AudioInputManager &> manager_added;


      /*** AudioInput Device Management ***/

      /** Get a list of all devices supported by all managers registered to the core.
       * @param devices a vector of device names to be filled by the core.
       */
      void get_devices(std::vector <AudioInputDevice> & devices);

      /** Set a specific device
       * This functions sets the current audio input device.
       * It can also be used while in a stream or in preview mode,
       * in such a case the old device gets closed and the new device is opened.
       * @param device_string the new device to be used, as a string
       */
      void set_device (const std::string& device_string);

      /** Inform the core of an added audioinout device
       * This function is called by the HalCore when an audio device is added.
       * It determines responsible managers for that specific device and informs the
       * GUI about the device that was added (via device_added signal).
       * In case the added device was the desired device and we fell back,
       * we will reactivate it. MUST be called from main thread.
       * @param source the device source (e.g. alsa).
       * @param device_name the name of the added device.
       * @param manager the HalManger detected the addition.
       */
      void add_device (const std::string & source, const std::string & device_name, HalManager* manager);

      /** Inform the core of a removed audioinput device
       * This function is called by the HalCore when an audio device is removed.
       * It determines responsible managers for that specific device and informs the
       * GUI about the device that was removed (via device_removed signal).
       * In case the removed device was the current device we fall back to the
       * fallback device. MUST be called from main thread.
       * @param source the device source (e.g. alsa).
       * @param device_name the name of the removed device.
       * @param manager the HalManger detected the removal.
       */
      void remove_device (const std::string & source, const std::string & device_name, HalManager* manager);


      /*** AudioInput Stream and Preview Management ***/

      /** Start the preview mode
       * Contrary to the video input core this can only be done if
       * the streaming mode NOT active (responsability of the UI).
       * @param channels the number of channels (1 or 2).
       * @param samplerate the samplerate.
       * @param bits_per_sample the number of bits per sample (e.g. 8, 16).
       */
      void start_preview (unsigned channels, unsigned samplerate, unsigned bits_per_sample);

      /** Stop the preview mode
       */
      void stop_preview ();


      /** Set the number and size of buffers for streaming mode
       * Will be applied the next time the device is opened.
       * @param buffer_size the size of each buffer in byte.
       * @param num_buffers the number of buffers.
       */
      void set_stream_buffer_size (unsigned buffer_size, unsigned num_buffers);

      /** Start the stream mode
       * Contrary to the video input core this can only be done if
       * preview is NOT active (responsability of the UI)
       * @param channels the number of channels (1 or 2).
       * @param samplerate the samplerate.
       * @param bits_per_sample the number of bits per sample (e.g. 8, 16).
       */
      void start_stream (unsigned channels, unsigned samplerate, unsigned bits_per_sample);

      /** Stop the stream mode
       */
      void stop_stream ();


      /** Get one audio buffer from the current manager.
       * This function will block until the buffer is completely filled.
       * Requires the stream or the preview (when being called from the
       * VideoPreviewManager) to be started.
       * In case the device returns an error reading the frame, get_frame_data()
       * falls back to the fallback device and reads the frame from there. Thus
       * get_frame_data() always returns a frame.
       * In case a new volume has bee set, it will be applied here.
       * @param data a pointer to the buffer that is to be filled. The memory has to be allocated already.
       * @param size the number of bytes to be read
       * @param bytes_read number of bytes actually read.
       */
      void get_frame_data (char *data, unsigned size, unsigned & bytes_read);

      /** Set the volume of the next opportunity
       * Sets the volume to the specified value the next time
       * get_frame_data() is called.
       * @param volume The new volume level (0..255).
       */
      void set_volume (unsigned volume);

      /** Turn average collecion on and off
       * The average values can be collected via get_average_level()
       * @param on_off whether to turn the collection on or off.
       */
      void set_average_collection (bool on_off) { calculate_average = on_off; }

      /** Get the average volume level
       * Get the average volume level ove the last read buffer.
       * @return the average volume level.
       */
      float get_average_level () { return average_level; }


      /*** VidInput Related Signals ***/

      /** See audioinput-manager.h for the API
       */
      boost::signal3<void, AudioInputManager &, AudioInputDevice &, AudioInputSettings&> device_opened;
      boost::signal2<void, AudioInputManager &, AudioInputDevice &> device_closed;
      boost::signal3<void, AudioInputManager &, AudioInputDevice &, AudioInputErrorCodes> device_error;

      /** This signal is emitted when an audio device input has been added to the system.
       * This signal will be emitted if add_device was called with a device name and
       * a manager claimed support for this device.
       * @param device the audio input device that was added.
       */
      boost::signal2<void, AudioInputDevice, bool> device_added;

      /** This signal is emitted when an audio input device has been removed from the system.
       * This signal will be emitted if remove_device was called with a device name and
       * a manager claimed support for this device.
       * @param device the audio input device that was removed.
       */
      boost::signal2<void, AudioInputDevice, bool> device_removed;

  private:
      void on_set_device (const AudioInputDevice & device);
      void on_device_opened (AudioInputDevice device,
                             AudioInputSettings settings,
                             AudioInputManager *manager);
      void on_device_closed (AudioInputDevice device, AudioInputManager *manager);
      void on_device_error  (AudioInputDevice device, AudioInputErrorCodes error_code, AudioInputManager *manager);

      void internal_set_device(const AudioInputDevice & device);
      void internal_set_manager (const AudioInputDevice & device);
      void internal_set_fallback();

      void internal_open (unsigned channels, unsigned samplerate, unsigned bits_per_sample);
      void internal_close();

      void calculate_average_level (const short *buffer, unsigned size);

  private:

      typedef struct DeviceConfig {
	bool active;

        unsigned channels;
        unsigned samplerate;
        unsigned bits_per_sample;

        unsigned buffer_size;
        unsigned num_buffers;
      } DeviceConfig;

      std::set<AudioInputManager *> managers;

      DeviceConfig preview_config;
      DeviceConfig stream_config;

      AudioInputManager* current_manager;
      AudioInputDevice desired_device;
      AudioInputDevice current_device;
      unsigned current_volume;
      unsigned desired_volume;

      PMutex core_mutex;
      PMutex volume_mutex;

      AudioInputCoreConfBridge* audioinput_core_conf_bridge;

      float average_level;
      bool calculate_average;
      bool yield;

      Ekiga::ServiceCore & core;
      boost::shared_ptr<Ekiga::NotificationCore> notification_core;
    };
/**
 * @}
 */
};

#endif
