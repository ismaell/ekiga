
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2006 Damien Sandras
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
 *                         sound_handling.h  -  description
 *                         --------------------------------
 *   begin                : Thu Nov 22 2001
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : This file contains sound handling functions.
 *
 */


#ifndef __GM_SOUND_HANDLING_H
#define __GM_SOUND_HANDLING_H

#include "common.h"
#include "manager.h"

#ifndef DISABLE_GNOME
#include <esd.h>
#endif


#define GM_AUDIO_TESTER(x) (GMAudioTester *)(x)

enum { SOURCE_AUDIO, SOURCE_MIC };


/* The functions */

/* DESCRIPTION   :  /
 * BEHAVIOR      : Puts ESD (and Artsd if support compiled in) into standby 
 *                 mode. An error message is displayed in the gnomemeeting
 *                 log if it failed. No message is displayed if it is
 *                 succesful.
 * PRE           : /
 */
void gnomemeeting_sound_daemons_suspend ();


/* DESCRIPTION   :  /
 * BEHAVIOR      : Puts ESD (and Artsd if support compiled in) into normal
 *                 mode. An error message is displayed in the gnomemeeting
 *                 log if it failed. No message is displayed if it is
 *                 succesful.
 * PRE           : /
 */
void gnomemeeting_sound_daemons_resume ();


/* GnomeMeeting Sound Event class */
class GMSoundEvent
{

 public:

  /* DESCRIPTION  :  The constructor.
   * BEHAVIOR     :  Builds the PSound corresponding to the event name
   *                 and plays it if it is valid.
   * PRE          :  The sound event name.
   */
  GMSoundEvent (PString);

 private:
  void Main ();

  PString event;
};


/* Audio Tester class */
class GMAudioTester : public PThread
{
  PCLASSINFO(GMAudioTester, PThread);


public:

  /* DESCRIPTION  :  The constructor.
   * BEHAVIOR     :  /
   * PRE          :  /
   */
  GMAudioTester (gchar *manager,
		 gchar *player,
		 gchar *recorder,
		 GMManager &endpoint);


  /* DESCRIPTION  :  The destructor.
   * BEHAVIOR     :  /
   * PRE          :  /
   */
  ~GMAudioTester ();


  void Main ();

protected:

  BOOL stop;

  PMutex quit_mutex;
  PSyncPoint thread_sync_point;

  PSoundChannel *player;
  PSoundChannel *recorder;

  char *buffer_ring;

  PString audio_manager;
  PString audio_player;
  PString audio_recorder;
  
  PMutex buffer_ring_access_mutex;

  GtkWidget *test_label;
  GtkWidget *test_dialog;
  GtkWidget *level_meter;
  
  GMManager & ep;

  friend class GMAudioRP;
};
#endif


class GMAudioRP : public PThread
{
  PCLASSINFO(GMAudioRP, PThread);

 public:

  GMAudioRP (BOOL enc, 
	     GMAudioTester &t);
  ~GMAudioRP ();

  void Main ();
  
  static gfloat GetAverageSignalLevel (const short *buffer, int size);

 private:
  
  BOOL is_encoding;
  
  PString driver_name;
  PString device_name;
  
  PMutex quit_mutex;
  PSyncPoint thread_sync_point;
  
  BOOL stop;
  
  GMAudioTester & tester;
};