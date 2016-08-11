
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
 *                         opal-call.h  -  description
 *                         ------------------------------------------
 *   begin                : written in 2007 by Damien Sandras
 *   copyright            : (c) 2007 by Damien Sandras
 *   description          : declaration of the interface of a call handled by
 *                          Opal.
 *
 */


#include <opal/opal.h>
#include <opal/call.h>
#include <ep/pcss.h>

#include "runtime.h"
#include "services.h"
#include "call.h"

#include "notification-core.h"

#ifndef __OPAL_CALL_H__
#define __OPAL_CALL_H__

class GMManager;

namespace Opal {

  class CallManager;
  class Call
    : public OpalCall,
      public Ekiga::Call,
      public boost::signals::trackable
  {

public:

    Call (CallManager &_manager,
          Ekiga::ServiceCore& _core,
          const std::string & uri);

    ~Call ();

    /*
     * Call Management
     */

    /** Hangup the call
    */
    void hangup ();

    /** Answer an incoming call
    */
    void answer ();

    /** Transfer the call to the specified uri
     * @param: uri: where to transfer the call
     */
    void transfer (std::string uri);

    /** Put the call on hold or retrieve it
    */
    void toggle_hold ();

    /** Toggle stream transmission (if any)
     * @param type the stream type
     */
    void toggle_stream_pause (StreamType type);

    /** Send the given DTMF
     * @param the dtmf (one char)
     */
    void send_dtmf (const char dtmf);

    /** Forward an incoming call after the given delay
     * @param delay the delay after which we forward
     * @param uri is the uri to forward to
     */
    void set_no_answer_forward (unsigned delay, const std::string & uri);

    /** Reject an incoming call after the given delay
     * @param delay the delay after which reject the call
     */
    void set_reject_delay (unsigned delay);


    /*
     * Call Information
     */

    /** Return the call id
     * @return: the call id
     */
    const std::string get_id () const;

    /** Return the local party name
     * @return: the local party name
     */
    const std::string get_local_party_name () const;

    /** Return the remote party name
     * @return: the remote party name
     */
    const std::string get_remote_party_name () const;

    /** Return the remote application
     * @return: the remote application
     */
    const std::string get_remote_application () const;


    /** Return the remote callback uri
     * @return: the remote uri
     */
    const std::string get_remote_uri () const;


    /** Return the call duration
     * @return: the current call duration
     */
    const std::string get_duration () const;


    /** Return the call start date and time
     * @return the current call start date and time
     */
    time_t get_start_time () const;


public:

    /* Implementation of inherited methods
    */

    bool is_outgoing () const;
    double get_received_audio_bandwidth () const { return re_a_bw; }
    double get_transmitted_audio_bandwidth () const { return tr_a_bw; }
    double get_received_video_bandwidth () const { return re_v_bw; }
    double get_transmitted_video_bandwidth () const { return tr_v_bw; }
    unsigned get_jitter_size () const { return jitter; }
    double get_lost_packets () const { return lost_packets; }
    double get_late_packets () const { return late_packets; }
    double get_out_of_order_packets () const { return out_of_order_packets; }


    /*
     * Opal Callbacks
     */
    void OnHold (OpalConnection & connection, bool from_remote, bool on_hold);

    void OnOpenMediaStream (OpalMediaStream & stream);

    void OnClosedMediaStream (OpalMediaStream & stream);

    void OnRTPStatistics2 (const OpalConnection & connection, const OpalRTPSession & session);

    void DoSetUp (OpalConnection & connection);

private:

    PBoolean OnEstablished (OpalConnection & connection);

    void OnReleased (OpalConnection & connection);

    void OnCleared ();

    OpalConnection::AnswerCallResponse OnAnswerCall (OpalConnection & connection, const PString & caller);

    PBoolean OnSetUp (OpalConnection & connection);

    PBoolean OnAlerting (OpalConnection & connection);

    /*
     * Helper methods
     */
    void parse_info (OpalConnection & connection);

    PSafePtr<OpalConnection> get_remote_connection ()
    {
      PSafePtr<OpalConnection> connection;
      for (PSafePtr<OpalConnection> iterConn (connectionsActive, PSafeReference); iterConn != NULL; ++iterConn) {
        if (PSafePtrCast<OpalConnection, OpalPCSSConnection> (iterConn) == NULL) {
          connection = iterConn;
          if (!connection.SetSafetyMode(PSafeReadWrite))
            connection.SetNULL();
          break;
        }
      }
      return connection;
    }

    /*
     *
     */
    PDECLARE_NOTIFIER(PTimer, Opal::Call, OnNoAnswerTimeout);
    PTimer NoAnswerTimer;

    /*
     * Variables
     */
    Ekiga::ServiceCore& core;
    CallManager & manager;
    boost::shared_ptr<Ekiga::NotificationCore> notification_core;

    std::string local_party_name;
    std::string remote_party_name;
    std::string remote_uri;
    std::string remote_application;

    bool call_setup;

    std::string forward_uri;

    double re_a_bw;
    double tr_a_bw;
    double re_v_bw;
    double tr_v_bw;
    unsigned re_v_fps;
    unsigned tr_v_fps;
    unsigned tr_width;
    unsigned tr_height;
    unsigned re_width;
    unsigned re_height;

    unsigned jitter;

    double lost_packets;
    double late_packets;
    double out_of_order_packets;

    PMutex stats_mutex;
    double re_a_bytes;
    double tr_a_bytes;
    double re_v_bytes;
    double tr_v_bytes;

    PTime last_a_tick;
    PTime last_v_tick;
    PTime start_time;

    unsigned lost_a;
    unsigned too_late_a;
    unsigned out_of_order_a;
    unsigned total_a;
    unsigned lost_v;
    unsigned too_late_v;
    unsigned out_of_order_v;
    unsigned total_v;

    bool outgoing;

private:

    void emit_established_in_main ();
    void emit_missed_in_main ();
    void emit_cleared_in_main (const std::string reason);
    void emit_setup_in_main ();
    void emit_ringing_in_main ();
    void emit_held_in_main ();
    void emit_retrieved_in_main ();
  };
};

#endif
