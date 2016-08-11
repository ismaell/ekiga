
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
 *                         videoinput-core.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : Declaration of the interface of a videoinput core.
 *                          A videoinput core manages VideoInputManagers.
 *
 */

#ifndef __VIDEOINPUT_CORE_H__
#define __VIDEOINPUT_CORE_H__

#include "services.h"
#include "runtime.h"
#include "videooutput-core.h"
#include "hal-core.h"
#include "notification-core.h"
#include "videoinput-manager.h"
#include "videoinput-gmconf-bridge.h"

#include <boost/signals.hpp>
#include <boost/bind.hpp>
#include <glib.h>
#include <set>

#include "ptlib.h"

#define VIDEO_INPUT_FALLBACK_DEVICE_TYPE   "Moving Logo"
#define VIDEO_INPUT_FALLBACK_DEVICE_SOURCE "Moving Logo"
#define VIDEO_INPUT_FALLBACK_DEVICE_NAME   "Moving Logo"

namespace Ekiga
{
/**
 * @defgroup videoinput
 * @{
 */

  /** Core object for the video input support
   * The video input core abstracts all functionality related to video input
   * in a thread safe manner. Typically, most of the functions except start_stream(),
   * stop_stream(), and get_frame_data() will be called from 
   * a UI thread, while the three mentioned funtions will be used by a video
   * streaming thread.
   * 
   * The video output core abstracts different video input managers, which can 
   * represent different backends like PTLIB, from the application and can 
   * switch the output device transparently for the video streaming thread,
   * even while capturing is in progress.
   *
   * If the removal of an video input device is detected by a failed
   * read or by a message from the HalCore, the video input core will 
   * determine the responsible video input manager and send a signal to the UI,
   * which can be used to update device lists. Also, if the removed device was the 
   * currently used one, the core falls back to the backup device.
   * 
   * A similar procedure is performed on the addition of a device. In case we fell 
   * back due to a removed device, and the respective device is re-added to the system,
   * it will be automatically activated.
   *
   * The video input core can also be used in a preview mode, where it starts a separate
   * thread (represented by the VideoPreviewManager), which grabs frames from the video 
   * input core and passes them to the video output core. This can be used for displaying
   * the local camera signal while not being in a call. If the preview is active and them
   * and the stream is started, the core will automatically determined if the device needs
   * to be reinitialized (the stream settings may be different from the preview settings
   * due to the capabilities negotiation). In case preview is set to active and them
   * the streaming is ended, the core will automatically switch back to preview mode,
   * also reinitializing the device if preview settings differ from stream settings.
   */
  class VideoInputCore
    : public Service
    {

  public:

      /** The constructor
       * @param _runtime reference to Ekiga runtime.
       * @param _videooutput_core reference ot the video output core.
       */
      VideoInputCore (Ekiga::ServiceCore & core,
                      boost::shared_ptr<VideoOutputCore> _videooutput_core);

      /** The destructor
       */
      ~VideoInputCore ();

      /** Set up gmconf bridge
       */
      void setup_conf_bridge();


      /*** Service Implementation ***/

      /** Returns the name of the service.
       * @return The service name.
       */
      const std::string get_name () const
        { return "videoinput-core"; }


      /** Returns the description of the service.
       * @return The service description.
       */
      const std::string get_description () const
        { return "\tVideoInput Core managing VideoInput Manager objects"; }

      /** Adds a VideoInputManager to the VideoInputCore service.
       * @param The manager to be added.
       */
       void add_manager (VideoInputManager &manager);

      /** Triggers a callback for all Ekiga::VideoInputManager sources of the
       * VideoInputCore service.
       */
       void visit_managers (boost::function1<bool, VideoInputManager &> visitor) const;

      /** This signal is emitted when a Ekiga::VideoInputManager has been
       * added to the VideoInputCore Service.
       */
       boost::signal1<void, VideoInputManager &> manager_added;


      /*** VideoInput Device Management ***/

      /** Get a list of all devices supported by all managers registered to the core.
       * @param devices a vector of device names to be filled by the core.
       */
      void get_devices(std::vector <VideoInputDevice> & devices);

      /** Set a specific device
       * This function sets the current video input device. This function can
       * also be used while in a stream or in preview mode. In that case the old
       * device is closed and the new device opened automatically. 
       * @param device the new device to be used.
       * @param channel the new channel to be used.
       * @param format the new format to be used.
       */
      void set_device(const VideoInputDevice & device, int channel, VideoInputFormat format);

      /** Inform the core of an added videoinput device
       * This function is called by the HalCore when a video device is added.
       * It determines responsible managers for that specific device and informs the 
       * GUI about the device that was added (via device_added signal). 
       * In case the added device was the desired device and we fell back, 
       * we will reactivate it. MUST be called from main thread.
       * @param source the device source (e.g. video4linux).
       * @param device_name the name of the added device.
       * @param capabilities used for differentiating V4L1 and V4L2.
       * @param manager the HalManger detected the addition.
       */
      void add_device (const std::string & source, const std::string & device_name, unsigned capabilities, HalManager* manager);

      /** Inform the core of a removed videoinput device
       * This function is called by the HalCore when a video device is removed.
       * It determines responsible managers for that specific device and informs the 
       * GUI about the device that was removed (via device_removed signal). 
       * In case the removed device was the current device we fall back to the
       * fallback device. MUST be called from main thread.
       * @param source the device source (e.g. video4linux).
       * @param device_name the name of the removed device.
       * @param capabilities used for differentiating V4L1 and V4L2.
       * @param manager the HalManger detected the removal.
       */
      void remove_device (const std::string & source, const std::string & device_name, unsigned capabilities, HalManager* manager);

      /** Set still image data
       * In case there is a manager that allows trasmitting a still image, 
       * this function can be used to set the actual image data. All other 
       * managers can safely ignore this function.
       * @param width the width of the image in pixels.
       * @param height the height of the image in pixels.
       * @param data a pointer to the image data in YUV420P format.
       */
      void set_image_data (unsigned width, unsigned height, const char* data);


      /*** VideoInput Stream and Preview Management ***/

      /** Set the preview configuration
       * This function sets the resolution and framerate for the preview mode. In case
       * preview is not active (due to active stream or because it is simply off), it will 
       * be applied the next time it is (re)started. In case preview is active, 
       * the new configuration will be applied immediately by closing and reopening the device.
       * @param width the frame width.
       * @param height the frame height.
       * @param fps the frame rate.
       */
      void set_preview_config (unsigned width, unsigned height, unsigned fps);

      /** Start the preview mode
       * Start the preview thread implemented in the PreviewManager. 
       * In case a stream is active, this will not have an effect until the stream is stopped.
       */
      void start_preview ();

      /** Stop the preview mode
       */
      void stop_preview ();


      /** Set the stream configuration
       * This function sets the resolution and framerate for the stream mode, which 
       * can be different from the preview configuration due to negotiated capabilities. 
       * The configuration will be applied on the next call of start_stream(), in order 
       * not to confuse simple endpoints that do not support switching of the resolution in
       * mid-stream.
       * @param width the frame width.
       * @param height the frame height.
       * @param fps the frame rate.
       */
      void set_stream_config (unsigned width, unsigned height, unsigned fps);

      /** Start the stream mode
       * In case that the preview mode was active and had a different configuration,
       * the core will reopen the device automatically.
       */
      void start_stream ();

      /** Stop the stream mode
       * In case preview mode has been started, the core will switch to preview mode.
       * In that case, and if the preview mode has a different configuration,
       * the core will reopen the device automatically.
       */
      void stop_stream ();

      /** Get one video frame buffer from the current manager.
       * This function will block until the buffer is completely filled.
       * Requires the stream or the preview (when being called from the 
       * VideoPreviewManager) to be started.
       * In case the device returns an error reading the frame, get_frame_data()
       * falls back to the fallback device and reads the frame from there. Thus
       * get_frame_data() always returns a frame.
       * In case a new brightness, whiteness, etc. has bee set, it will be applied here.
       * @param data a pointer to the frame buffer that is to be filled. The memory has to be allocated already.
       */
      void get_frame_data (char *data);


      /** See vidinput-manager.h for the API
       */
      void set_colour     (unsigned colour);
      void set_brightness (unsigned brightness);
      void set_whiteness  (unsigned whiteness);
      void set_contrast   (unsigned contrast);


      /*** VideoInput related Signals ***/

      /** See videoinput-manager.h for the API
       */
      boost::signal3<void, VideoInputManager &, VideoInputDevice &, VideoInputSettings&> device_opened;
      boost::signal2<void, VideoInputManager &, VideoInputDevice &> device_closed;
      boost::signal3<void, VideoInputManager &, VideoInputDevice &, VideoInputErrorCodes> device_error;

      /** This signal is emitted when a video input has been added to the system.
       * This signal will be emitted if add_device was called with a device name and
       * a manager claimed support for this device.
       * @param device the video input device that was added.
       */
      boost::signal2<void, VideoInputDevice, bool> device_added;

      /** This signal is emitted when a video input has been removed from the system.
       * This signal will be emitted if remove_device was called with a device name and
       * a manager claimed support for this device.
       * @param device the video input device that was removed.
       */
      boost::signal2<void, VideoInputDevice, bool> device_removed;

  private:
      void on_set_device (const VideoInputDevice & device);
      void on_device_opened (VideoInputDevice device,  
                             VideoInputSettings settings, 
                             VideoInputManager *manager);
      void on_device_closed (VideoInputDevice device, VideoInputManager *manager);
      void on_device_error  (VideoInputDevice device, VideoInputErrorCodes error_code, VideoInputManager *manager);

      void internal_set_device(const VideoInputDevice & vidinput_device, int channel, VideoInputFormat format);
      void internal_set_manager (const VideoInputDevice & vidinput_device, int channel, VideoInputFormat format);
      void internal_set_fallback ();

      void internal_open (unsigned width, unsigned height, unsigned fps);
      void internal_close();

      void internal_apply_settings();

private:
      /** VideoPreviewManager thread.
        *
        * VideoPreviewManager represents a thread that gets frames from the 
        * video input core and passes them to the video output core. This is 
        * used for displaying the preview video. This thread will run only 
        * while preview is active. It is called from the VideoInputCore, which
        * has the interface to the application for enabling and disabling the preview.
        */
      class VideoPreviewManager : public PThread
      {
        PCLASSINFO(VideoPreviewManager, PThread);

      public:
        /** The constructor
        * @param _videoinput_core reference to the video input core.
        * @param _videooutput_core reference to the video output core.
        */
        VideoPreviewManager(VideoInputCore & _videoinput_core, boost::shared_ptr<VideoOutputCore> _videooutput_core);

        void quit();

        /** Start the preview thread.
        * Start the thread represented by the Main() function. Block until the thread is running.
        * Requires the the current device to be opened.
        * In case the resolution is changed, the preview manager has to be stopped and restarted.
        * @param width the frame width in pixels of the preview video.
        * @param height the frame width in pixels of the preview video.
        */
        virtual void start(unsigned _width, unsigned _height);

        /** Stop the preview thread.
        * Stop the thread represented by the Main() function. Blocks until the thread has terminated.
        */
        virtual void stop();

      protected:
        void Main ();
        char* frame;

        bool end_thread;
        bool pause_thread;
        PMutex     thread_ended;
        PSyncPoint thread_paused;
        PSyncPoint run_thread;

        VideoInputCore  & videoinput_core;
        boost::shared_ptr<VideoOutputCore> videooutput_core;
        unsigned width;
        unsigned height;
      };

      /** Class for storing the device configuration.
        *
        * This class is used for storing the device configuration when
        * streaming video or when in preview mode. The device configuration
        * consists of width, height and framerate. This class can be used to 
        * check whether stream and preview configuration is different and
        * thus if a device needs to be reopened when switching between stream
        * and preview mode.
        */
      class VideoDeviceConfig
      {
       public:
        bool active;
        unsigned width;
        unsigned height;
        unsigned fps;

        VideoDeviceConfig () {}
        VideoDeviceConfig (unsigned _width, unsigned _height, unsigned _fps) {
          width = _width;
          height = _height;
          fps = _fps;
        }

        friend std::ostream& operator <<(std::ostream & stream, const VideoDeviceConfig & config){
          stream << config.width << "x" << config.height << "/" << config.fps;
          return stream;
        }

        VideoDeviceConfig & operator= (const VideoDeviceConfig & rhs)
        {
            width  = rhs.width;
            height = rhs.height;
            fps    = rhs.fps;
            return *this;
        }

        bool operator==( const VideoDeviceConfig & rhs ) const
        {
          if ( (width  == rhs.width)   && 
               (height == rhs.height)  &&
               (fps    == rhs.fps)     )
          return true;
        else
          return false;
        }

        bool operator!=( const VideoDeviceConfig & rhs ) const
        {
          return (!(*this==rhs));
        }

      };

private:
      std::set<VideoInputManager *> managers;

      VideoDeviceConfig       preview_config;
      VideoDeviceConfig       stream_config;

      VideoInputManager*      current_manager;
      VideoInputDevice        desired_device;
      VideoInputDevice        current_device;
      VideoInputFormat        current_format;
      int                     current_channel;
      VideoInputSettings      current_settings; 
      VideoInputSettings      desired_settings;

      PMutex core_mutex;
      PMutex settings_mutex;

      Ekiga::ServiceCore & core;
      VideoPreviewManager* preview_manager;
      VideoInputCoreConfBridge* videoinput_core_conf_bridge;
      boost::shared_ptr<Ekiga::NotificationCore> notification_core;
    };
/**
 * @}
 */
};

#endif
