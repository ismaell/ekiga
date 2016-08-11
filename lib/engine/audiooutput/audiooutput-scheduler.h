
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
 *                         audiooutput-scheduler.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : Declaration of a scheduler for sound events that is run in 
 *                          a separate thread.
 *
 */

#ifndef __AUDIOOUTPUT_SCHEDULER_H__
#define __AUDIOOUTPUT_SCHEDULER_H__

#include "services.h"

#include "audiooutput-info.h"

#include <glib.h>
#include <vector>

#include "ptlib.h"
#include <ptclib/pwavfile.h>

namespace Ekiga
{
  class AudioOutputCore;

  typedef struct AudioEvent {
    std::string name;
    bool is_file_name;
    unsigned interval;
    unsigned repetitions;
    unsigned long time;
  } AudioEvent;

  typedef struct EventFileName {
    std::string event_name;
    std::string file_name;
    bool enabled;
    AudioOutputPS ps;
  } EventFileName;

  class AudioEventScheduler : public PThread
  {
    PCLASSINFO(AudioEventScheduler, PThread);

  public:
    AudioEventScheduler(Ekiga::AudioOutputCore& _audio_output_core);
    void quit ();
    void add_event_to_queue(const std::string & name, bool is_file_name, unsigned interval, unsigned repetitions);
    void remove_event_from_queue(const std::string & name);
    void set_file_name(const std::string & event_name, const std::string & file_name, AudioOutputPS ps, bool enabled);
  
  protected:
    void Main (void);
    void get_pending_event_list (std::vector<AudioEvent> & pending_event_list);
    unsigned long get_time_ms();
    unsigned get_time_to_next_event();
    bool get_file_name(const std::string & event_name, std::string & file_name, AudioOutputPS & ps);
    void load_wav(const std::string & event_name, bool is_file_name, char* & buffer, unsigned long & len, unsigned & channels, unsigned & sample_rate, unsigned & bps, AudioOutputPS & ps);

    PSyncPoint run_thread;
    bool end_thread;

    PMutex thread_ended;
    PSyncPoint thread_created;

    PMutex event_list_mutex;
    std::vector <AudioEvent> event_list;

    PMutex event_file_list_mutex;
    std::vector <EventFileName> event_file_list;

    Ekiga::AudioOutputCore& audio_output_core;
  };
};
#endif
