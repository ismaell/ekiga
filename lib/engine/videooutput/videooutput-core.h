
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
 *                         videooutput-core.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2007 by Matthias Schneider
 *   copyright            : (c) 2007 by Matthias Schneider
 *   description          : Declaration of the interface of a videooutput core.
 *                          A videooutput core manages VideoOutputManagers.
 *
 */

#ifndef __VIDEOOUTPUT_CORE_H__
#define __VIDEOOUTPUT_CORE_H__

#include "services.h"

#include "videooutput-gmconf-bridge.h"
#include "videooutput-manager.h"

#include <boost/signals.hpp>
#include <boost/bind.hpp>
#include <set>
#include <map>

#include <glib.h>

#include "ptlib.h"

namespace Ekiga
{

/**
 * @defgroup videooutput
 * @{
 */

  /** Core object for the video display support
   *
   * The VideoOutputCore will control the different VideoOutputManagers and pass pointers to 
   * the frames to all of them.
   * Before passing the first frame, start() has to be called. In order to close the video,
   * stop() has to be called. The video output core interacts with the GUI when switching to fullscreen,
   * when the size of the video has been changed and when a device is opened and closed.
   */
  class VideoOutputCore
    : public Service
    {

  public:

      /** The constructor
      */
      VideoOutputCore ();

      /** The destructor
      */
      ~VideoOutputCore ();

      /** Set up gmconf bridge
       */
      void setup_conf_bridge();


      /*** Service Implementation ***/

      /** Returns the name of the service.
       * @return The service name.
       */
      const std::string get_name () const
        { return "videooutput-core"; }


      /** Returns the description of the service.
       * @return The service description.
       */
      const std::string get_description () const
        { return "\tVideoOutput Core managing VideoOutput Manager objects"; }


      /** Adds a VideoOutputManager to the VideoOutputCore service.
       * @param The manager to be added.
       */
      void add_manager (VideoOutputManager &manager);

      /** Triggers a callback for all Ekiga::VideoOutputManager sources of the
       * VideoOutputCore service.
       */
      void visit_managers (boost::function1<bool, VideoOutputManager &> visitor) const;

      /** This signal is emitted when a Ekiga::VideoOutputManager has been
       * added to the VideoOutputCore Service.
       */
      boost::signal1<void, VideoOutputManager &> manager_added;


      /*** Videooutput Management ***/

      /** Start the video output
       * Must be called before outputting frames via set_frame_data()
       */
      void start ();

      /** Stop the video output
       * 
       */
      void stop ();

      /** Display a single frame
       * Pass the pointer to the frame to all registered mangers.
       * The video output must have been started before.
       * @param data a pointer to the buffer with the data to be written. It will not be freed.
       * @param width the width in pixels of the frame to be written.
       * @param height the height in pixels of the frame to be written.
       * @param type the type of the frame: 0 - local video source or >0 from the remote end.
       * @param devices_nbr 1 if only local or remote device has been opened, 2 if both have been opened.
       */

      void set_frame_data (const char *data,
                           unsigned width,
                           unsigned height,
                           unsigned type,
                           int devices_nbr);

      void set_display_info (const DisplayInfo & _display_info);


      /*** Statistics ***/

      /** Get the current video output statistics from the core
       *
       * @param _videooutput_stats the struct to be filled with the current values..
       */
      void get_videooutput_stats (VideoOutputStats & _videooutput_stats) {
        _videooutput_stats = videooutput_stats;
      };


      /*** Signals ***/

      /** See videooutput-manager.h for the API
       */
      boost::signal6<void, VideoOutputManager &, VideoOutputAccel, VideoOutputMode, unsigned, bool, bool> device_opened;
      boost::signal1<void, VideoOutputManager &> device_closed;
      boost::signal2<void, VideoOutputManager &, VideoOutputErrorCodes> device_error;
      boost::signal2<void, VideoOutputManager &, VideoOutputFSToggle> fullscreen_mode_changed;
      boost::signal3<void, VideoOutputManager &, unsigned, unsigned> size_changed;


  private:
      void on_device_opened (VideoOutputAccel videooutput_accel,
                             VideoOutputMode mode,
                             unsigned zoom,
                             bool both_streams,
                             bool ext_stream,
                             VideoOutputManager *manager);
      void on_device_closed (VideoOutputManager *manager);
      void on_device_error (VideoOutputErrorCodes error_code, VideoOutputManager *manager);
      void on_size_changed ( unsigned width, unsigned height, VideoOutputManager *manager);
      void on_fullscreen_mode_changed (VideoOutputFSToggle toggle, VideoOutputManager *manager);

      std::set<VideoOutputManager *> managers;

      VideoOutputStats videooutput_stats;
      GTimeVal last_stats;
      int number_times_started;

      PMutex core_mutex;

      VideoOutputCoreConfBridge* videooutput_core_conf_bridge;
    };
/**
 * @}
 */
};

#endif
