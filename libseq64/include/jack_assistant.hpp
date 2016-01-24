#ifndef SEQ64_JACK_ASSISTANT_HPP
#define SEQ64_JACK_ASSISTANT_HPP

/*
 *  This file is part of seq24/sequencer64.
 *
 *  seq24 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  seq24 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with seq24; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          jack_assistant.hpp
 *
 *  This module declares/defines the base class for handling many facets
 *  of performing (playing) a full MIDI song using JACK.
 *
 * \library       sequencer64 application
 * \author        Chris Ahlstrom
 * \date          2015-09-17
 * \updates       2016-01-24
 * \license       GNU GPLv2 or above
 *
 *  This class contains a number of functions that used to reside in the
 *  still-large perform module.
 */

#include "globals.h"                    /* globals, nullptr, and more       */

#ifdef SEQ64_JACK_SUPPORT
#include <jack/jack.h>
#include <jack/transport.h>

#ifdef SEQ64_JACK_SESSION
#include <jack/session.h>
#endif

#else       // ! SEQ64_JACK_SUPPORT

#undef SEQ64_JACK_SESSION

#endif      // SEQ64_JACK_SUPPORT

/*
 *  We don't really need to be a slow-sync client, as far as we can tell.
 *  In fact, our sync code may interfere with getting a valid frame rate.
 *  However, we still can't JACK working exactly the way it does in seq24,
 *  so we leave the callback in place.  Plus, it does things important to the
 *  setup of JACK.
 */

#define USE_JACK_SYNC_CALLBACK

namespace seq64
{

class perform;                          /* jack_assistant parent is perform */

/**
 *  Provide a temporary structure for passing data and results between a
 *  perform and jack_assistant object.  The jack_assistant class already
 *  has access to the members of perform, but it needs access to and
 *  modification of "local" variables in perform::output_func().
 *  This scratchpad is useful even if JACK support is not enabled.
 */

class jack_scratchpad
{

public:

    double js_current_tick;
    double js_total_tick;
    double js_clock_tick;
    bool js_jack_stopped;
    bool js_dumping;
    bool js_init_clock;
    bool js_looping;                    /* perform::m_looping       */
    bool js_playback_mode;              /* perform::m_playback_mode */
    double js_ticks_converted_last;

};

#ifdef SEQ64_JACK_SUPPORT

/**
 *  Provides an internal type to make it easier to display a specific and
 *  accurate human-readable message when a JACK operation fails.
 *
 * \var jf_bit
 *      Holds one of the bit-values from jack_status_t, which is defined as
 *      an "enum JackStatus" type.
 *
 * \var jf_meaning
 *      Holds a textual description of the corresponding status bit.
 */

typedef struct
{
    unsigned jf_bit;
    std::string jf_meaning;

} jack_status_pair_t;

/**
 *  This class provides the performance mode JACK support.
 *
 *  WHY PERFORMANCE MODE?  Only works in that mode???  
 */

class jack_assistant
{

#ifdef USE_JACK_SYNC_CALLBACK
    friend int jack_sync_callback
    (
        jack_transport_state_t state,
        jack_position_t * pos,
        void * arg
    );
#endif  // USE_JACK_SYNC_CALLBACK

#ifdef SEQ64_JACK_SESSION
    friend void jack_session_callback (jack_session_event_t * ev, void * arg);
#endif

    friend void jack_shutdown_callback (void * arg);
    friend void jack_timebase_callback
    (
        jack_transport_state_t state,
        jack_nframes_t nframes,
        jack_position_t * pos,
        int new_pos,
        void * arg
    );

private:

    static jack_status_pair_t sm_status_pairs [];

    perform & m_jack_parent;
    jack_client_t * m_jack_client;
    jack_nframes_t m_jack_frame_current;
    jack_nframes_t m_jack_frame_last;
    jack_position_t m_jack_pos;
    jack_transport_state_t m_jack_transport_state;
    jack_transport_state_t m_jack_transport_state_last;
    double m_jack_tick;

#ifdef SEQ64_JACK_SESSION
    jack_session_event_t * m_jsession_ev;
#endif

    bool m_jack_running;
    bool m_jack_master;
    int m_ppqn;

public:

    jack_assistant (perform & parent, int ppqn = SEQ64_USE_DEFAULT_PPQN);
    ~jack_assistant ();

    /**
     * \getter m_jack_running
     */

    bool is_running () const
    {
        return m_jack_running;
    }

    /**
     * \getter m_jack_master
     */

    bool is_master () const
    {
        return m_jack_master;
    }

    /**
     * \getter m_jack_parent
     *      Needed for external callbacks.
     */

    perform & parent ()
    {
        return m_jack_parent;
    }

    bool init ();                       // init_jack ();
    bool restart ();                    // NEW 2015-01-23
    void deinit ();                     // deinit_jack ();

#ifdef SEQ64_JACK_SESSION
    bool session_event ();              // jack_session_event ();
#endif

    void start ();                      // start_jack();
    void stop ();                       // stop();
    void position                       // position_jack();
    (
        bool to_left_tick,              // instead of current tick
        bool relocate = false           // enable "dead code" EXPERIMENTAL
    );
    bool output (jack_scratchpad & pad);

private:

    /**
     * \setter m_ppqn
     *      For the future, changing the PPQN internally.
     */

    void set_ppqn (int ppqn)
    {
        m_ppqn = ppqn;
    }

    double get_jack_ticks() const;                      // @new ca 2016-01-21
    double frame_to_ticks (jack_nframes_t frame) const; // @new ca 2016-01-21
    bool info_message (const std::string & msg);
    bool error_message (const std::string & msg);
    jack_client_t * client_open (const std::string & clientname);
    void show_statuses (unsigned bits);
    int sync (jack_transport_state_t state = (jack_transport_state_t)(-1));
    void set_position (midipulse currenttick);

#ifdef SEQ64_USE_DEBUG_OUTPUT
    void jack_debug_print
    (
        double current_tick,
        double ticks_delta
    );
#endif

};

/**
 *  Global functions for JACK support and JACK sessions.
 */

#ifdef USE_JACK_SYNC_CALLBACK
extern int jack_sync_callback
(
    jack_transport_state_t state,
    jack_position_t * pos,
    void * arg
);
#endif  // USE_JACK_SYNC_CALLBACK

extern void jack_shutdown_callback (void * arg);
extern void jack_timebase_callback
(
    jack_transport_state_t state,
    jack_nframes_t nframes,
    jack_position_t * pos,
    int new_pos,
    void * arg
);

/*
 * ca 2015-07-23
 * Implemented second patch for JACK Transport from freddix/seq24
 * GitHub project.  Added the following function.
 */

extern int jack_process_callback (jack_nframes_t nframes, void * arg);

#ifdef SEQ64_JACK_SESSION
extern void jack_session_callback (jack_session_event_t * ev, void * arg);
#endif

#ifdef ALLOW_PLATFORM_DEBUG
extern void print_jack_pos (jack_position_t & jack_pos, const std::string & tag);
#endif

#endif  // SEQ64_JACK_SUPPORT

}           // namespace seq64

#endif      // SEQ64_JACK_ASSISTANT_HPP

/*
 * jack_assistant.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

