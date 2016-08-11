
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
 *                         videooutput-manager-common.h  -  description
 *                         ------------------------------
 *   begin                : Sat Feb 17 2001
 *   copyright            : (C) 2000-2008 by Damien Sandras
 *                        : (C) 2007-2008 by Matthias Schneider
 *   description          : Generic class that represents 
 *                          a thread that can display a video image and defines.
 *                          generic functions for local/remote/pip/pip external 
 *                          window/fullscreen video display. Provides interface 
 *                          to the GUI for an embedded window, display mode 
 *                          control and feedback of information like the status
 *                          of the video acceleration. Also provides the 
 *                          copying and local storage of the video frame.
 */


#ifndef _VIDEOOUTPUT_MANAGER_COMMON_H_
#define _VIDEOOUTPUT_MANAGER_COMMON_H_

#include "videooutput-manager.h"
#include "runtime.h"

#include "ptlib.h"

/**
 * @addtogroup videooutput
 * @{
 */

  /** Common VideoOutputManager implementation for DX and XV
   *
   * This class features the shared functionality between the DX and XV output managers.
   * The GMVideoOutputManager consists of a special thread that will do all accesses 
   * to the dxWindow and xWindow/xvWindow classes (important for windows where all
   * accesses to a specific window not only have to be thread-safe, but also have to
   * come all from the same thread). The main task of that thread is to wait for triggers
   * and then act on them, the thread itself will be active for the whole lifetime of Ekiga.
   * A call to open() from the core will signal the thread to execute the initialize function,
   * with open() blocking until the thread has finished its task. 
   * The thread is now ready to accept frames, which will arrive from the core via
   * set_frame_data(). set_frame_data will make a local copy of the image via memcpy,
   * and signal the thread that a new remote or local frame has arrived and is ready to be
   * displayed. Then set_frame_data() will be left, while the actual display of the frame
   * will be performed in the thread.
   * Once the thread has been triggered with a new frame to display, it will first call redraw
   * for passing the frame to the graphics adaptor, and then do_sync to actually displaying it.
   * The reason these procedures are separate has the following motivation: do_sync can block for
   * quite some time if synv-to-vblank is active. If this would be executed in the mutex-protected
   * redraw procedure, we would be blocking the video processing threads, possibly leading to lost packets
   * and interfering with synchronous frame transmission.
   * redraw() will check whether the dxWindow/xvWindow classes need to be reinitialized (e.g. we want to 
   * switching from non-pip mode to pip mode, etc.), do that if desired and then pass pointers to the 
   * (copied) frames to the dxWindow/xvWindow classes via display_frame() and display_pip_frames(). It will
   * also determine if only one or both frames need to be synched to the screen.
   * Once the device has been opened/initialized and no new frame arrives, the frames will nevertheless
   * be updated every 250 ms.
   * In order to close the devices, an uninit request is sent to the thread, which performes the 
   * necessary cleanup. Like open(), close() blocks until the thread signals the cleanup has been completed.

   * the frames to all of them.
   * Before passing the first frame, start() has to be called. In order to close the video,
   * stop() has to be called. The video output core interacts with the GUI when switching to fullscreen,
   * when the size of the video has been changed and when a device is opened and closed.
   */
  class GMVideoOutputManager
    : public PThread,
      public Ekiga::VideoOutputManager
  {
    PCLASSINFO(GMVideoOutputManager, PThread); 

  public:
 
   /** See videooutput-manager.h for the API
     */

    GMVideoOutputManager (Ekiga::ServiceCore & core);

    virtual ~GMVideoOutputManager (void);

    virtual void open ();

    virtual void close ();

    virtual void set_frame_data (const char* data,
                                  unsigned width,
                                  unsigned height,
                                  unsigned type,
                                  int devices_nbr);

    virtual void set_display_info (const Ekiga::DisplayInfo & _display_info)
    {
      PWaitAndSignal m(display_info_mutex);
      display_info = _display_info;
    };

  protected:
    typedef struct {
      bool local;
      bool remote;
      bool extended;
    } UpdateRequired;

    /** The main video thread loop
     * The video output thread loop that waits for a frame and
     * then displays it
     */
    virtual void Main ();
  
    /** Check if the display needs to be reinitialized
     * Returns true if the given settings require a
     * reinitialization of the display, false
     * otherwise.
     * @return wether the display needs to be reopened.
     */
    virtual bool frame_display_change_needed ();
  
    /** Set up the display
     * Setup the display following the display type,
     * picture dimensions and zoom value.
     * Returns false in case of failure.
     */
    virtual void setup_frame_display () = 0;
  
    /** Close the display
     * Closes the frame display and returns false
     * in case of failure.
     */
    virtual void close_frame_display () = 0;
  
    /** Display one frame
     * Display the given frame on the correct display.
     * The display needs to be initialized first by using 
     * setup_frame_display().
     */
    virtual void display_frame (const char *frame,
                                unsigned width,
                                unsigned height) = 0;

    /** Display two frames as picture-in-picture
     * Display the given frames as Picture in Picture.
     * The display needs to be initialized first by using 
     * setup_frame_display().
     */
    virtual void display_pip_frames (const char *local_frame,
                                    unsigned lf_width,
                                    unsigned lf_height,
                                    const char *remote_frame,
                                    unsigned rf_width,
                                    unsigned rf_height) = 0;
  
    /** Draw the frame 
     * Draw the frame to a backbuffer, do not show it yet.
     * @return wether the local, the remote or both frames need to be synced to the screen.
     */
    virtual UpdateRequired redraw ();

    /** Sync the output of the frame
     * Bring the frame to the screen. May block for 
     * a while if sync-to-vblank is active
     * @param sync_required  wether the local, the remote or both frames need to be synced to the screen.
     */
    virtual void sync(UpdateRequired sync_required) = 0;

    /** Initialises the display
     */
    virtual void init ();
  
    /** Uninitialises the display
     */
    virtual void uninit ();

    /** Update the GUI about the new device state
     * Triggers a device_closed and device_opened callback.
     * Used to inform the GUI that the second video signal 
     * has arrived for the first time 
     */
    void update_gui_device();

    virtual void get_display_info (Ekiga::DisplayInfo & _display_info) {
          PWaitAndSignal m(display_info_mutex);
          _display_info = display_info;
    }
  
    /* This variable has to be protected by display_info_mutex */
    Ekiga::DisplayInfo display_info;
    PMutex display_info_mutex; /* To protect the DisplayInfo object */

    PBYTEArray lframeStore;
    PBYTEArray rframeStore;
    PBYTEArray eframeStore;

    typedef struct {
      Ekiga::VideoOutputMode mode;
      Ekiga::VideoOutputAccel accel;
      bool both_streams_active;
      bool ext_stream_active;

      unsigned int remote_width;
      unsigned int remote_height;

      unsigned int ext_width; // 2nd remote (extended video)
      unsigned int ext_height;

      unsigned int local_width;
      unsigned int local_height;

      unsigned int zoom;

      int embedded_x;
      int embedded_y;
    } FrameInfo;

    FrameInfo last_frame;
    FrameInfo current_frame;

    bool local_frame_received;
    bool remote_frame_received;
    bool ext_frame_received;
    bool video_disabled;

    UpdateRequired update_required;
  
    PSyncPoint run_thread;                  /* To signal the thread shall execute its tasks */
    bool       end_thread;
    bool       init_thread;
    bool       uninit_thread;
  
    PSyncPoint thread_created;              /* To signal that the thread has been created */
    PSyncPoint thread_initialised;          /* To signal that the thread has been initialised */
    PSyncPoint thread_uninitialised;        /* To signal that the thread has been uninitialised */
    PMutex     thread_ended;                /* To exit */
    
    PMutex var_mutex;      /* To protect variables that are read and written
                              from various threads */
  
    Ekiga::ServiceCore & core;

  private:

    void device_opened_in_main (Ekiga::VideoOutputAccel accel,
				Ekiga::VideoOutputMode mode,
				unsigned zoom,
				bool both, bool ext);

    void device_closed_in_main ();

  };

/**
 * @}
 */

#endif /* _VIDEOOUTPUT_MANAGER_COMMON_H_ */
