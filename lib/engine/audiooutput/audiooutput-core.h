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
 *                         audiooutput-core.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : Declaration of the interface of a audiooutput core.
 *                          A audioout core manages AudioOutputManagers.
 *
 */

#ifndef __AUDIOOUTPUT_CORE_H__
#define __AUDIOOUTPUT_CORE_H__

#include "services.h"
#include "runtime.h"
#include "hal-core.h"
#include "notification-core.h"

#include "audiooutput-manager.h"
#include "audiooutput-gmconf-bridge.h"
#include "audiooutput-scheduler.h"

#include "ptlib.h"

#define AUDIO_OUTPUT_FALLBACK_DEVICE_TYPE "Ekiga"
#define AUDIO_OUTPUT_FALLBACK_DEVICE_SOURCE "Ekiga"
#define AUDIO_OUTPUT_FALLBACK_DEVICE_NAME   "SILENT"

namespace Ekiga
{
/**
 * @defgroup audiooutput
 * @{
 */

  /** Core object for the audio output support
   * The audio output core abstracts all functionality related to audio output
   * in a thread safe manner. Typically, most of the functions except start(),
   * stop(), set_buffer_size() and set_frame_data() will be called from a UI thread,
   * while the three mentioned funtions will be used by an audio streaming thread.
   * 
   * The audio output core abstracts different audio output managers, which can 
   * represent different backends like PTLIB, from the application and can 
   * switch the output device transparently for the audio streaming thread,
   * even while audio output is in progress.
   *
   * If the removal of an audio output device is detected by a failed
   * write or by a message from the HalCore, the audio output core will 
   * determine the responsible audio output manager and send a signal to the UI,
   * which can be used to update device lists. Also, if the removed device was the 
   * currently used one, the core falls back to the backup device.
   * 
   * A similar procedure is performed on the addition of a device. In case we fell 
   * back due to a removed device, and the respective device is re-added to the system,
   * it will be automatically activated.
   *

   */
  class AudioOutputCore
    : public Service
    {

  public:

      /** The constructor
       */
      AudioOutputCore (Ekiga::ServiceCore & core);

      /** The destructor
      */
      ~AudioOutputCore ();

      /** Set up gmconf bridge
       */
      void setup_conf_bridge();


      /*** Service Implementation ***/

      /** Returns the name of the service.
       * @return The service name.
       */
      const std::string get_name () const
        { return "audiooutput-core"; }

      /** Returns the description of the service.
       * @return The service description.
       */
      const std::string get_description () const
        { return "\tAudioOutput Core managing AudioOutput Manager objects"; }

      /** Adds a AudioOutputManager to the AudioOutputCore service.
       * @param The manager to be added.
       */
      void add_manager (AudioOutputManager &manager);

      /** Triggers a callback for all Ekiga::AudioOutputManager sources of the
       * AudioOutputCore service.
       */
      void visit_managers (boost::function1<bool, AudioOutputManager &> visitor) const;

      /** This signal is emitted when a Ekiga::AudioOutputManager has been
       * added to the AudioOutputCore Service.
       */
      boost::signal1<void, AudioOutputManager &> manager_added;


      /** Get a list of all devices supported by all managers registered to the core.
       * @param devices a vector of device names to be filled by the core.
       */
      void get_devices(std::vector <AudioOutputDevice> & devices);

      /** Set a specific device
       * This function sets the current primary or secondary audio output device. This function can
       * also be used while in a stream or in preview mode. In that case the old
       * device is closed and the new device opened automatically.
       * @param ps whether referring to the primary or secondary device.
       * @param device the new device to be used.
       */
      void set_device(AudioOutputPS ps, const AudioOutputDevice & device);

      /** Inform the core of an added audiooutput device
       * This function is called by the HalCore when an audio output device is added.
       * It determines responsible managers for that specific device and informs the 
       * GUI about the device that was added (via device_added signal). 
       * In case the added device was the desired device and we fell back, 
       * we will reactivate it. MUST be called from main thread,
       * @param sink the device sink (e.g. alsa).
       * @param device_name the name of the added device.
       * @param manager the HalManger detected the addition.
       */
      void add_device (const std::string & sink, const std::string & device_name, HalManager* manager);

      /** Inform the core of a removed audiooutput device
       * This function is called by the HalCore when an audio output device is removed.
       * It determines responsible managers for that specific device and informs the 
       * GUI about the device that was removed (via device_removed signal). 
       * In case the removed device was the current device we fall back to the
       * fallback device. MUST be called from main thread,
       * @param sink the device sink (e.g. alsa).
       * @param device_name the name of the removed device.
       * @param manager the HalManger detected the removal.
       */
      void remove_device (const std::string & sink, const std::string & device_name, HalManager* manager);


      /*** Event Management ***/

      /** Add or update a mapping between event and file name to the event list
       * An event shall refer to a specific sound file. This mapping is set here.
       * @param event_name the name of the event.
       * @param file_name the name of the file.
       * @param ps whether the event shall be played on the primary or secondary device preferrably.
       * @param enabled if the event is enabled.
       */
      void map_event (const std::string & event_name, const std::string & file_name, AudioOutputPS ps, bool enabled);

      /** Play a sound specified by a file name
       * Play a sound file once.
       * The sound will be played in the background as soon as the Scheduler 
       * schedules it.
       * This function only adds the sound to the Scheduler queue and returns immediately.
       * The sound will be played on the primary device 
       * @param file_name the name of the file.
       */
      void play_file (const std::string & file_name);

      /** Play a sound specified by an event name
       * Play a sound associated to the event speficied by its name once.
       * The sound will be played in the background as soon as the Scheduler 
       * schedules it.
       * This function only adds the sound to the Scheduler queue and returns immediately.
       * The sound will be played on the primary or seconday device depending on
       * how this specific event was configured. In case it was to be played on the secondary device
       * and not secondary device is available or configured, it will be played on the primary device.
       * The event will only be played if it is enabled.
       * @param event_name the name of the event.
       */
      void play_event (const std::string & event_name);

      /** Play a sound specified by an event name
       * Play a sound associated to the event specified by its name repeatingly.
       * The sound will be played in the background as soon as the Scheduler 
       * schedules it.
       * This function only adds the sound to the Scheduler queue and returns immediately.
       * The sound will be played on the primary or seconday device depending on
       * how this specific event was configured. In case it was to be played on the secondary device
       * and not secondary device is available or configured, it will be played on the primary device.
       * The event will only be played if it is enabled.
       * The event will be removed from the scheduler queue once it has been repeated "repetitions" times
       * or if it has been removd from the queue via stop_play_event.
       * @param event_name the name of the event.
       * @param interval the interval of the repetitions in ms.
       * @param repetitions the maximum number of repetitions.
       */
      void start_play_event (const std::string & event_name, unsigned interval, unsigned repetitions);

      /** Stop playing a sound specified by an event name
       * Stop playing sound associated to the event specified by its name.
       * If the sound is currently playing, it will not be cut short.
       * @param event_name the name of the event.
       */
      void stop_play_event (const std::string & event_name);

      /** Play a sound event buffer
       * This function is called by the Scheduler in order to play an already loaded sound.
       * @param ps whether to play the sound on the primary or secondary device.
       * @param buffer pointer to the sound in raw format.
       * @param len the length in bytes of the sound.
       * @param channels the number of channels.
       * @param sample_rate the samplerate.
       * @param bps bits per sample.
       */
      void play_buffer(AudioOutputPS ps, const char* buffer, unsigned long len, unsigned channels, unsigned sample_rate, unsigned bps);


      /*** Stream Management ***/

      /** Set the number and size of buffers 
       * Will be applied the next time the device is opened.
       * @param buffer_size the size of each buffer in byte.
       * @param num_buffers the number of buffers.
       */
      void set_buffer_size (unsigned buffer_size, unsigned num_buffers);

     /** Start the audio output on the primary device
       * @param channels the number of channels (1 or 2).
       * @param samplerate the samplerate.
       * @param bits_per_sample the number of bits per sample (e.g. 8, 16).
       */ 
      void start (unsigned channels, unsigned samplerate, unsigned bits_per_sample);

      /** Stop the audio output of the primary device.
       */
      void stop ();

     /** Set one audio buffer in the current manager.
       * This function will pass one buffer to the current manager. 
       * Requires the audio output to be started.
       * In case the device returns an error writing the frame, set_frame_data()
       * falls back to the fallback device and writes the frame there. Thus
       * set_frame_data() always be succesful.
       * In case a new volume has bee set, it will be applied here.
       * @param data a pointer to the buffer that is to be written to the device.
       * @param size the number of bytes to be written.
       * @param bytes_written number of bytes actually written.
       */
      void set_frame_data (const char *data, unsigned size, unsigned & bytes_written); 

     /** Set the volume of the next opportunity
       * Sets the volume to the specified value the next time
       * get_frame_data() is called.
       * @param ps whether the volume of the primary or seconday device shall be set.
       * @param volume the new volume level (0..255).
       */
      void set_volume (AudioOutputPS ps, unsigned volume);

      /** Turn average collecion on and off
       * The average values can be collected via get_average_level()
       * This applies to primary device only.
       * @param on_off whether to turn the collection on or off.
       */
      void set_average_collection (bool on_off) { calculate_average = on_off; }

      /** Get the average volume level
       * Get the average volume level ove the last read buffer of the primary device.
       * @return the average volume level.
       */
      float get_average_level () { return average_level; }


      /*** Signals ***/

      /** See audiooutput-manager.h for the API
       */
      boost::signal4<void, AudioOutputManager &, AudioOutputPS, AudioOutputDevice&, AudioOutputSettings&> device_opened;
      boost::signal3<void, AudioOutputManager &, AudioOutputPS, AudioOutputDevice&> device_closed;
      boost::signal4<void, AudioOutputManager &, AudioOutputPS, AudioOutputDevice&, AudioOutputErrorCodes> device_error;

      /** This signal is emitted when an audio output device has been added to the system.
       * This signal will be emitted if add_device was called with a device name and
       * a manager claimed support for this device.
       * @param device the audio output device that was added.
       */
      boost::signal2<void, AudioOutputDevice, bool> device_added;

      /** This signal is emitted when an audio output device has been removed from the system.
       * This signal will be emitted if remove_device was called with a device name and
       * a manager claimed support for this device.
       * @param device the audio output device that was removed.
       */
      boost::signal2<void, AudioOutputDevice, bool> device_removed;

  private:
      void on_set_device (const AudioOutputDevice & device);
      void on_device_opened (AudioOutputPS ps,
                             AudioOutputDevice device,
                             AudioOutputSettings settings,
                             AudioOutputManager *manager);
      void on_device_closed (AudioOutputPS ps, AudioOutputDevice device, AudioOutputManager *manager);
      void on_device_error  (AudioOutputPS ps, AudioOutputDevice device, AudioOutputErrorCodes error_code, AudioOutputManager *manager);

      void internal_set_primary_device(const AudioOutputDevice & device);
      void internal_set_manager (AudioOutputPS ps, const AudioOutputDevice & device);
      void internal_set_primary_fallback();

      bool internal_open (AudioOutputPS ps, unsigned channels, unsigned samplerate, unsigned bits_per_sample);
      void internal_close(AudioOutputPS ps);

      void internal_play(AudioOutputPS ps, const char* buffer, unsigned long len, unsigned channels, unsigned sample_rate, unsigned bps);

      void calculate_average_level (const short *buffer, unsigned size);

      std::set<AudioOutputManager *> managers;

      typedef struct DeviceConfig {
        bool active;
        unsigned channels;
        unsigned samplerate;
        unsigned bits_per_sample;
        unsigned buffer_size;
        unsigned num_buffers;
      } DeviceConfig;

      DeviceConfig current_primary_config;

      AudioOutputManager* current_manager[2];
      AudioOutputDevice desired_primary_device;
      AudioOutputDevice current_device[2];
      unsigned desired_primary_volume;
      unsigned current_primary_volume;

      PMutex core_mutex[2];
      PMutex volume_mutex;

      AudioOutputCoreConfBridge* audiooutput_core_conf_bridge;
      AudioEventScheduler* audio_event_scheduler;

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
