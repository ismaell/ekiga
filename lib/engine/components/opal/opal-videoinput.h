
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
 *                         videograbber.h  -  description
 *                         ------------------------------
 *   begin                : Mon Feb 12 2001
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : Video4Linux compliant functions to manipulate the 
 *                          webcam device.
 *
 */


#ifndef _EKIGA_VIDEO_INPUT_H_
#define _EKIGA_VIDEO_INPUT_H_

#include <ptlib.h>
#include <opal/manager.h>

#include "videoinput-core.h"

class PVideoInputDevice_EKIGA : public PVideoInputDevice 
{
  PCLASSINFO(PVideoInputDevice_EKIGA, PVideoInputDevice);
  
 public:
  
  /* DESCRIPTION  :  The constructor.
   * BEHAVIOR     :  Creates the Fake Input Device.
   * PRE          :  /
   */
  PVideoInputDevice_EKIGA (Ekiga::ServiceCore & core);


  /* DESCRIPTION  :  The destructor
   * BEHAVIOR     :  /
   * PRE          :  /
   */
  ~PVideoInputDevice_EKIGA ();

  
  virtual bool Open (const PString &name,
     	             bool start_immediate = TRUE);

  
  /**Determine of the device is currently open.
   */
  virtual bool IsOpen() ;

  
  /**Close the device.
   */
  virtual bool Close();

  
  /**Start the video device I/O.
   */
  virtual bool Start();

  
  /**Stop the video device I/O capture.
   */
  virtual bool Stop();


  /**Determine if the video device I/O capture is in progress.
   */
  virtual bool IsCapturing();

  
  virtual bool SetFrameSize (unsigned int width,
       		             unsigned int height);
  
  
  /* DESCRIPTION  :  The destructor
   * BEHAVIOR     :  /
   * PRE          :  /
   */
  virtual bool GetFrameData (BYTE *frame, PINDEX *i = NULL);


  /* DESCRIPTION  :  The destructor
   * BEHAVIOR     :  /
   * PRE          :  /
   */
  virtual bool GetFrameDataNoDelay (BYTE *frame, PINDEX *i = NULL);

  
  virtual bool TestAllFormats ();

  
  /**Get the maximum frame size in bytes.
  */
  virtual PINDEX GetMaxFrameBytes();

  
  /** Given a preset interval of n milliseconds, this function
      returns n msecs after the previous frame capture was initiated.
  */
//  virtual void WaitFinishPreviousFrame();

  
  /**Set the video format to be used.

  Default behaviour sets the value of the videoFormat variable and then
  returns the IsOpen() status.
  */
  virtual bool SetVideoFormat (VideoFormat newFormat);
  
  /**Get the number of video channels available on the device.

  Default behaviour returns 1.
  */
  virtual int GetNumChannels() ;

  
  /**Set the video channel to be used on the device.

  Default behaviour sets the value of the channelNumber variable and then
  returns the IsOpen() status.
  */
  virtual bool SetChannel (int newChannel);
			

  /**Set the colour format to be used.

  Default behaviour sets the value of the colourFormat variable and then
  returns the IsOpen() status.
  */
  virtual bool SetColourFormat (const PString &newFormat);

  
  /**Set the video frame rate to be used on the device.

  Default behaviour sets the value of the frameRate variable and then
  return the IsOpen() status.
  */
  virtual bool SetFrameRate (unsigned rate);

  
  virtual bool GetFrameSizeLimits (unsigned &minWidth,
			           unsigned &minHeight,
			           unsigned &maxWidth,
			           unsigned &maxHeight);
  
#if 0
  virtual bool GetParameters (int *whiteness,
		              int *brightness,
		              int *colour,
		              int *contrast,
		              int *hue);
#endif


  virtual PStringArray GetDeviceNames() const;

  static int devices_nbr;
  bool is_active;
  
protected:
  Ekiga::ServiceCore & core;
  boost::shared_ptr<Ekiga::VideoInputCore> videoinput_core;

  bool opened;
};

#endif
