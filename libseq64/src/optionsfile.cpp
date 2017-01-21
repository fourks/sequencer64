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
 * \file          optionsfile.cpp
 *
 *  This module declares/defines the base class for managing the <code>
 *  ~/.seq24rc </code> legacy configuration file or the new <code>
 *  ~/.config/sequencer64/sequencer64.rc </code> ("rc") configuration file.
 *
 * \library       sequencer64 application
 * \author        Seq24 team; modifications by Chris Ahlstrom
 * \date          2015-07-24
 * \updates       2017-01-21
 * \license       GNU GPLv2 or above
 *
 *  The <code> ~/.seq24rc </code> or <code> ~/.config/sequencer64/sequencer64.rc
 *  </code> configuration file is fairly simple in layout.  The documentation
 *  for this module is supplemented by the following GitHub projects:
 *
 *      -   https://github.com/ahlstromcj/seq24-doc.git (legacy support)
 *      -   https://github.com/ahlstromcj/sequencer64-doc.git
 *
 *  Those documents also relate these file settings to the application's
 *  command-line options.
 *
 *  Note that these options are primarily read/written from/to the perform
 *  object that is passed to the parse() and write() functions.
 *
 *  Also note that the parse() and write() functions process sections in a
 *  different order!  The reason this does not mess things up is that the
 *  line_after() function always rescans from the beginning of the file.  As
 *  long as each section's sub-values are read and written in the same order,
 *  there will be no problem.
 *
 * Fixups:
 *
 *      As of version 0.9.11, a "Pause" key is added, if SEQ64_PAUSE_SUPPORT is
 *      defined.  One must fix up the sequencer64.rc file.  First, run
 *      Sequencer64.  Then open File / Options, and go to the Keyboard tab.
 *      Fix the Start, Stop, and Pause fields as desired.  The recommended
 *      character for Pause is the period (".").
 *
 *      Or better yet, add a Pause line to the sequencer.rc file after the
 *      "stop sequencer" line:
 *
 *      46   # period pause sequencer
 */

#include <string.h>                     /* memset()                         */

#include "gdk_basic_keys.h"             /* SEQ64_equal, SEQ64_minus         */
#include "midibus.hpp"
#include "optionsfile.hpp"
#include "perform.hpp"
#include "settings.hpp"                 /* seq64::rc() and choose_ppqn()    */

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq64
{

/**
 *  Principal constructor.
 *
 * \param name
 *      Provides the name of the options file; this is usually a full path
 *      file-specification.
 */

optionsfile::optionsfile (const std::string & name)
 :
    configfile  (name)               // base class constructor
{
    // Empty body
}

/**
 *  A rote destructor.
 */

optionsfile::~optionsfile ()
{
    // Empty body
}

/**
 *  Helper function for error-handling.
 *
 * \param sectionname
 *      Provides the name of the section for reporting the error.
 *
 * \return
 *      Always returns false.
 */

bool
optionsfile::error_message (const std::string & sectionname)
{
    std::string msg = "BAD DATA after [";
    msg += sectionname;
    msg += "], should ABORT";
    errprint(msg.c_str());
    return false;
}

/**
 *  Parse the ~/.seq24rc or ~/.config/sequencer64/sequencer64.rc file.
 *
 *  [midi-control]
 *
 *  Get the number of sequence definitions provided in the [midi-control]
 *  section.  Ranges from 32 on up.  Then read in all of the sequence
 *  lines.  The first 32 apply to the first screen set.  There can also be
 *  a comment line "# mute in group" followed by 32 more lines.  Then
 *  there are addditional comments and single lines for BPM up, BPM down,
 *  Screen Set Up, Screen Set Down, Mod Replace, Mod Snapshot, Mod Queue,
 *  Mod Gmute, Mod Glearn, and Screen Set Play.  These are all forms of
 *  MIDI automation useful to control the playback while not sitting near
 *  the computer.
 *
 *  [mute-group]
 *
 *  The mute-group starts with a line that indicates up to 32 mute-groups
 *  are defined. A common value is 1024, which means there are 32 groups
 *  times 32 keys.  But this value is currently thrown away.  This value
 *  is followed by 32 lines of data, each contained 4 sets of 8 settings.
 *  See the seq24-doc project on GitHub for a much more detailed
 *  description of this section.
 *
 *  [midi-clock]
 *
 *  The MIDI-clock section defines the clocking value for up to 16 output
 *  busses.  The first number, 16, indicates how many busses are
 *  specified.  Generally, these busses are shown to the user with names
 *  such as "[1] seq24 1".
 *
 *  [keyboard-control]
 *
 *  The keyboard control defines the keys that will toggle the stage of
 *  each of up to 32 patterns in a pattern/sequence box.  These keys are
 *  displayed in each box as a reminder.  The first number specifies the
 *  Key number, and the second number specifies the Sequence number.
 *
 *  [keyboard-group]
 *
 *  The keyboard group specifies more automation for the application.
 *  The first number specifies the Key number, and the second number
 *  specifies the Group number.  This section should be better described
 *  in the seq24-doc project on GitHub.
 *
 *  [extended-keys]
 *
 *  Additional keys (not yet represented in the Options dialog) to support
 *  additional keys for tempo-tapping, Seq32's new transport and connection
 *  functionality, and maybe a little more.
 *
 *  [New-keys]
 *
 *  Conditional support for reading Seq32 "rc" files.
 *
 *  [jack-transport]
 *
 *  This section covers various JACK settings, one setting per line.  In
 *  order, the following numbers are specfied:
 *
 *      -   jack_transport - Enable sync with JACK Transport.
 *      -   jack_master - Seq24 will attempt to serve as JACK Master.
 *      -   jack_master_cond - Seq24 will fail to be Master if there is
 *          already a Master set.
 *      -   song_start_mode:
 *          -   0 = Playback will be in Live mode.  Use this to allow
 *              muting and unmuting of loops.
 *          -   1 = Playback will use the Song Editor's data.
 *
 *  [midi-input]
 *
 *  This section covers the MIDI input busses, and has a format similar to
 *  "[midi-clock]".  Generally, these busses are shown to the user with
 *  names such as "[1] seq24 1", and currently there is only one input
 *  buss.  The first field is the port number, and the second number
 *  indicates whether it is disabled (0), or enabled (1).
 *
 *  [midi-clock-mod-ticks]
 *
 *  This section covers....  One common value is 64.
 *
 *  [manual-alsa-ports]
 *
 *  Set to 1 if you want seq24 to create its own ALSA ports and not
 *  connect to other clients.
 *
 *  [last-used-dir]
 *
 *  This section simply holds the last path-name that was used to read or
 *  write a MIDI file.  We still need to add a check for a valid path, and
 *  currently the path must start with a "/", so it is not suitable for
 *  Windows.
 *
 *  [interaction-method]
 *
 *  This section specified the kind of mouse interaction.
 *
 *  -   0 = 'seq24' (original Seq24 method).
 *  -   1 = 'fruity' (similar to a certain fruity sequencer we like).
 *
 *  The second data line is set to "1" if Mod4 can be used to keep seq24
 *  in note-adding mode even after the right-click is released, and "0"
 *  otherwise.
 *
 * \param p
 *      Provides the performance object to which all of these options apply.
 *
 * \return
 *      Returns true if the file was able to be opened for reading.
 *      Currently, there is no indication if the parsing actually succeeded.
 */

bool
optionsfile::parse (perform & p)
{
    std::ifstream file(m_name.c_str(), std::ios::in | std::ios::ate);
    if (! file.is_open())
    {
        printf("? error opening [%s] for reading\n", m_name.c_str());
        return false;
    }
    file.seekg(0, std::ios::beg);                           /* seek to start */

    /*
     * This call causes parsing to skip all of the header material.  Please note
     * that the line_after() function always starts from the beginning of the
     * file every time.  A lot a rescanning!  But it goes fast.
     */

    unsigned sequences = 0;
    line_after(file, "[midi-control]");                     /* find section  */
    sscanf(m_line, "%u", &sequences);

    /*
     * The above value is called "sequences", but what was written was the
     * value of c_midi_controls.  If we make that value non-constant, then
     * we should modify that value, instead of the throw-away "sequences"
     * values.  Note that c_midi_controls is, in a roundabout way, defined
     * as 74.  See the old "dot-seq24rc" file in the contrib directory.
     * We should do some testing on what happens if "sequences" is 0 or
     * greater than or equal to 74 (c_midi_controls)).
     */

    bool ok = false;
    if (sequences > c_midi_controls)                /* 1 to 74 are valid      */
    {
        return error_message("midi-control, too many values");
    }
    else if (sequences > 0)
    {
        ok = next_data_line(file);
        if (! ok)
            return error_message("midi-control");
        else
            ok = true;

        for (unsigned i = 0; i < sequences; ++i)    /* 0 to c_midi_controls-1 */
        {
            int sequence = 0;
            int a[6], b[6], c[6];
            sscanf
            (
                m_line,
                "%d [ %d %d %d %d %d %d ]"
                  " [ %d %d %d %d %d %d ]"
                  " [ %d %d %d %d %d %d ]",
                &sequence,
                &a[0], &a[1], &a[2], &a[3], &a[4], &a[5],
                &b[0], &b[1], &b[2], &b[3], &b[4], &b[5],
                &c[0], &c[1], &c[2], &c[3], &c[4], &c[5]
            );
            p.midi_control_toggle(i).set(a);
            p.midi_control_on(i).set(b);
            p.midi_control_off(i).set(c);
            ok = next_data_line(file);
            if (! ok && i < (sequences - 1))
                return error_message("midi-control data line");
            else
                ok = true;
        }
    }
    else
    {
        warnprint("[midi-controls] specifies a count of 0, so skipped");
    }

    line_after(file, "[mute-group]");               /* Group MIDI control   */
    int gtrack = 0;

    /*
     * @change ca 2016-07-07
     *  We used to throw this value away, but it is useful if
     *  no mute groups have been created.  So, if it reads 0 (instead of
     *  1024), we will assume there are no mute-group settings.
     */

    sscanf(m_line, "%d", &gtrack);

#ifdef SEQ64_STRIP_EMPTY_MUTES
    ok = gtrack == 0 ||
        gtrack == SEQ64_DEFAULT_SET_MAX * SEQ64_SET_KEYS_MAX;       /* 1024 */
#else
    ok = next_data_line(file);
    if (ok)
        ok = gtrack == SEQ64_DEFAULT_SET_MAX * SEQ64_SET_KEYS_MAX;  /* 1024 */

    if (! ok)
        (void) error_message("mute-group");         /* finish the parsing!  */
#endif

    if (ok && gtrack > 0)
    {
        int gm[c_seqs_in_set];

        /*
         * This loop is a bit odd.  We set groupmute, read it, increment it,
         * and then read it again.  Issue?  Will fix when we have time to test
         * it fully.
         */

        int groupmute = 0;
        for (int i = 0; i < c_seqs_in_set; ++i)
        {
            p.select_group_mute(groupmute);
            sscanf
            (
                m_line,
                "%d [%d %d %d %d %d %d %d %d]"
                  " [%d %d %d %d %d %d %d %d]"
                  " [%d %d %d %d %d %d %d %d]"
                  " [%d %d %d %d %d %d %d %d]",
    &groupmute,
    &gm[0],  &gm[1],  &gm[2],  &gm[3],  &gm[4],  &gm[5],  &gm[6],  &gm[7],
    &gm[8],  &gm[9],  &gm[10], &gm[11], &gm[12], &gm[13], &gm[14], &gm[15],
    &gm[16], &gm[17], &gm[18], &gm[19], &gm[20], &gm[21], &gm[22], &gm[23],
    &gm[24], &gm[25], &gm[26], &gm[27], &gm[28], &gm[29], &gm[30], &gm[31]
            );
            for (int k = 0; k < c_seqs_in_set; ++k)
                p.set_group_mute_state(k, gm[k]);

            ++groupmute;
            ok = next_data_line(file);
            if (! ok && i < (c_seqs_in_set - 1))
                return error_message("mute-group data line");
            else
                ok = true;
        }
    }

    ok = line_after(file, "[midi-clock]");
    long buses = 0;
    if (ok)
    {
        sscanf(m_line, "%ld", &buses);
        ok = next_data_line(file) && buses > 0 && buses <= SEQ64_DEFAULT_BUSS_MAX;
    }
    if (! ok)
        return error_message("midi-clock");

    for (int i = 0; i < buses; ++i)
    {
        long bus_on, bus;
        sscanf(m_line, "%ld %ld", &bus, &bus_on);
        p.add_clock(static_cast<clock_e>(bus_on));
        ok = next_data_line(file);
        if (! ok && i < (buses - 1))
            return error_message("midi-clock data line");
        else
        {
            ok = true;
            break;
        }
    }

    line_after(file, "[keyboard-control]");
    long keys = 0;
    sscanf(m_line, "%ld", &keys);
    ok = next_data_line(file) && keys > 0 && keys <= SEQ64_SET_KEYS_MAX;
    if (! ok)
        return error_message("keyboard-control");

    /*
     * Bug involving the optionsfile and perform modules:  At the 4th or 5th
     * line of data in the "rc" file, setting this key event results in the
     * size remaining at 4, so the final size is 31.  This bug is present even
     * in seq24 r.0.9.2, and occurs only if the Keyboard options are actually
     * edited.  Also, the size of the reverse container is constant at 32.
     * Clearing the latter container as well appears to fix both bugs.
     */

    p.get_key_events().clear();
    p.get_key_events_rev().clear();
    for (int i = 0; i < keys; ++i)
    {
        long key = 0, seq = 0;
        sscanf(m_line, "%ld %ld", &key, &seq);
        p.set_key_event(key, seq);
        ok = next_data_line(file);
        if (! ok && i < (keys - 1))
            return error_message("keyboard-control data line");
    }

    line_after(file, "[keyboard-group]");
    long groups = 0;
    sscanf(m_line, "%ld", &groups);
    ok = next_data_line(file) && groups > 0 && groups <= SEQ64_SET_KEYS_MAX;
    if (! ok)
        return error_message("keyboard-group");

    p.get_key_groups().clear();
    p.get_key_groups_rev().clear();       // \new ca 2015-09-16
    for (int i = 0; i < groups; ++i)
    {
        long key = 0, group = 0;
        sscanf(m_line, "%ld %ld", &key, &group);
        p.set_key_group(key, group);
        ok = next_data_line(file);
        if (! ok && i < (groups - 1))
            return error_message("keyboard-group data line");
    }

    keys_perform_transfer ktx;
    memset(&ktx, 0, sizeof(ktx));
    sscanf(m_line, "%u %u", &ktx.kpt_bpm_up, &ktx.kpt_bpm_dn);
    next_data_line(file);
    sscanf
    (
        m_line, "%u %u %u",
        &ktx.kpt_screenset_up,
        &ktx.kpt_screenset_dn,
        &ktx.kpt_set_playing_screenset
    );
    next_data_line(file);
    sscanf
    (
        m_line, "%u %u %u",
        &ktx.kpt_group_on,
        &ktx.kpt_group_off,
        &ktx.kpt_group_learn
    );
    next_data_line(file);
    sscanf
    (
        m_line, "%u %u %u %u %u",
        &ktx.kpt_replace,
        &ktx.kpt_queue,
        &ktx.kpt_snapshot_1,
        &ktx.kpt_snapshot_2,
        &ktx.kpt_keep_queue
    );

    int show_key = 0;
    next_data_line(file);
    sscanf(m_line, "%d", &show_key);
    ktx.kpt_show_ui_sequence_key = bool(show_key);
    next_data_line(file);
    sscanf(m_line, "%u", &ktx.kpt_start);
    next_data_line(file);
    sscanf(m_line, "%u", &ktx.kpt_stop);

    if (rc().legacy_format())               /* init "non-legacy" fields */
    {
        ktx.kpt_show_ui_sequence_number = false;
        ktx.kpt_pattern_edit = 0;
        ktx.kpt_event_edit = 0;
        ktx.kpt_pause = 0;
    }
    else
    {
        /*
         * We have removed the individual key fixups in this module, since
         * keyval_normalize() is called afterward to make sure all key values
         * are legitimate.  However, we do have an issue with all of these
         * conditional compile macros.  We will ultimately read and write them
         * all, even if the application is built to not actually use some of
         * them.
         */

        next_data_line(file);
        sscanf(m_line, "%u", &ktx.kpt_pause);
        if (ktx.kpt_pause <= 1)             /* no pause key value present   */
        {
            ktx.kpt_show_ui_sequence_number = bool(ktx.kpt_pause);
            ktx.kpt_pause = 0;              /* make key_normalize() fix it  */
        }
        else
        {
            /*
             * New feature for showing sequence numbers in the mainwnd GUI.
             */

            next_data_line(file);
            sscanf(m_line, "%d", &show_key);
            ktx.kpt_show_ui_sequence_number = bool(show_key);
        }

        /*
         * Might need to be fixed up for existing config files.  Will fix when
         * we see what the problem would be.  Right now, they both come up as
         * an apostrophe when the config file exists.  Actually, the integer
         * value for each is zero!  So, if it comes up zero, we force them the
         * SEQ64_equal and SEQ64_minus.  This might still screw up
         * configurations that have devoted those keys to other purposes.
         */

        next_data_line(file);
        sscanf(m_line, "%u", &ktx.kpt_pattern_edit);

        next_data_line(file);
        sscanf(m_line, "%u", &ktx.kpt_event_edit);

        if (line_after(file, "[New-keys]"))
        {
            sscanf(m_line, "%u", &ktx.kpt_song_mode);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_menu_mode);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_follow_transport);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_toggle_jack);
            next_data_line(file);
        }
        else if (line_after(file, "[extended-keys]"))
        {
            sscanf(m_line, "%u", &ktx.kpt_song_mode);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_toggle_jack);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_menu_mode);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_follow_transport);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_fast_forward);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_rewind);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_pointer_position);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_tap_bpm);
            next_data_line(file);
            sscanf(m_line, "%u", &ktx.kpt_toggle_mutes);
            next_data_line(file);
        }
        else
        {
            warnprint("WARNING:  no [extended-keys] section");
        }
    }

    keyval_normalize(ktx);                  /* fix any missing values   */
    p.keys().set_keys(ktx);                 /* copy into perform keys   */

    line_after(file, "[jack-transport]");
    long flag = 0;
    sscanf(m_line, "%ld", &flag);
    rc().with_jack_transport(bool(flag));

    next_data_line(file);
    sscanf(m_line, "%ld", &flag);
    rc().with_jack_master(bool(flag));

    next_data_line(file);
    sscanf(m_line, "%ld", &flag);
    rc().with_jack_master_cond(bool(flag));

    next_data_line(file);
    sscanf(m_line, "%ld", &flag);
    p.song_start_mode(bool(flag));

    line_after(file, "[midi-input]");
    buses = 0;
    sscanf(m_line, "%ld", &buses);
    next_data_line(file);
    for (int i = 0; i < buses; ++i)
    {
        long bus_on, bus;
        sscanf(m_line, "%ld %ld", &bus, &bus_on);
        p.add_input(bool(bus_on));
        next_data_line(file);
    }
    if (next_data_line(file))                       /* new 2016-08-20 */
    {
        sscanf(m_line, "%ld", &flag);
        rc().filter_by_channel(bool(flag));
    }
    line_after(file, "[midi-clock-mod-ticks]");

    long ticks = 64;
    sscanf(m_line, "%ld", &ticks);
    midibus::set_clock_mod(ticks);

    line_after(file, "[manual-alsa-ports]");
    sscanf(m_line, "%ld", &flag);
    rc().manual_alsa_ports(bool(flag));

    line_after(file, "[reveal-alsa-ports]");
    sscanf(m_line, "%ld", &flag);

    /*
     * If this flag is already raised, it was raised on the command line, and
     * we don't want to change it.  An ugly special case.
     */

    if (! rc().reveal_alsa_ports())
        rc().reveal_alsa_ports(bool(flag));

    line_after(file, "[last-used-dir]");
    if (strlen(m_line) > 0)
        rc().last_used_dir(m_line); // FIXME: check for valid path

    long method = 0;
    line_after(file, "[interaction-method]");
    sscanf(m_line, "%ld", &method);
    rc().interaction_method(interaction_method_t(method));
    if (! rc().legacy_format())
    {
        if (next_data_line(file))                   /* a new option */
        {
            sscanf(m_line, "%ld", &method);
            rc().allow_mod4_mode(method != 0);
        }
        if (next_data_line(file))                   /* a new option */
        {
            sscanf(m_line, "%ld", &method);
            rc().allow_snap_split(method != 0);
        }
        if (next_data_line(file))                   /* a new option */
        {
            sscanf(m_line, "%ld", &method);
            rc().allow_click_edit(method != 0);
        }

        line_after(file, "[lash-session]");
        sscanf(m_line, "%ld", &method);
        rc().lash_support(method != 0);

        method = 1;         /* preserve legacy seq24 option if not present */
        line_after(file, "[auto-option-save]");
        sscanf(m_line, "%ld", &method);
        rc().auto_option_save(method != 0);
    }
    file.close();           /* done parsing the "rc" configuration file */
    return true;
}

/**
 *  This options-writing function is just about as complex as the
 *  options-reading function.
 *
 * \param p
 *      Provides a const reference to the main perform object.  However,
 *      we have to cast away the constness, because too many of the
 *      perform getter functions are used in non-const contexts.
 *
 * \return
 *      Returns true if the write operations all succeeded.
 */

bool
optionsfile::write (const perform & p)
{
    std::ofstream file(m_name.c_str(), std::ios::out | std::ios::trunc);
    perform & ucperf = const_cast<perform &>(p);
    if (! file.is_open())
    {
        printf("? error opening [%s] for writing\n", m_name.c_str());
        return false;
    }

    /*
     * Initial comments and MIDI control section.  No more "global_xxx", yay!
     */

    if (rc().legacy_format())
    {
        file <<
           "# Sequencer64 user configuration file (legacy Seq24 0.9.2 format)\n";
    }
    else
    {
        file <<
            "# Sequencer64 0.90.0 (and above) rc configuration file\n"
            "#\n"
            "# This file holds the main configuration options for Sequencer64.\n"
            "# It follows the format of the legacy seq24 'rc' configuration\n"
            "# file, but adds some new options, such as LASH, Mod4 interaction\n"
            "# support, an auto-save-on-exit option, and more.  Also provided\n"
            "# is a legacy mode.\n"
            ;
    }
    file << "\n"
        "[midi-control]\n"
        "\n"
        "# The leftmost number on each line here is the pattern number, from\n"
        "# 0 to 31 (and beyond for the mute-groups). Next, there are three\n"
        "# groups of bracketed numbers that follow:\n"
        "#\n"
        "#    [toggle]  [on]  [off]\n"
        "#\n"
        "# In each group, there are six numbers:\n"
        "#\n"
        "#    [on/off invert status d0 d1min d1max]\n"
        "#\n"
        "# 'on/off' enables/disables (1/0) the MIDI control for the pattern.\n"
        "# 'invert' (1/0) causes the opposite if data is outside the range.\n"
        "# 'status' is by MIDI event to match (channel is ignored).\n"
        "# 'd0' is the first data value.  Example: if status is 144 (Note On),\n"
        "# then d0 represents Note 0.\n"
        "# 'd1min'/'d1max' are the range of second values that should match.\n"
        "# Example:  For a Note On for note 0, 0 and 127 indicate that any\n"
        "# Note On velocity will cause the MIDI control to take effect.\n"
        "\n"
        <<  c_midi_controls << "      # MIDI controls count\n" // constant count
        ;

    char outs[SEQ64_LINE_MAX];
    for (int mcontrol = 0; mcontrol < c_midi_controls; ++mcontrol)
    {
        /*
         * \tricky
         *      32 mutes for channel, 32 group mutes, 9 odds-and-ends.
         *      This first output item merely write a <i> comment </i> to
         *      the "rc" file to indicate what the next section describes.
         *      The first section of [midi-control] specifies 74 items.
         *      The first 32 are unlabelled by a comment, and run from 0
         *      to 31.  The next 32 are labelled "mute in group", and run
         *      from 32 to 63.  The next 10 are each labelled, starting
         *      with "bpm up" and ending with "screen set play", and are
         *      each one line long.
         */

        switch (mcontrol)
        {
        case c_seqs_in_set:                 // 32
            file << "# mute in group section:\n";
            break;

        case c_midi_control_bpm_up:         // 64
            file << "# bpm up:\n";
            break;

        case c_midi_control_bpm_dn:         // 65
            file << "# bpm down:\n";
            break;

        case c_midi_control_ss_up:          // 66
            file << "# screen set up:\n";
            break;

        case c_midi_control_ss_dn:          // 67
            file << "# screen set down:\n";
            break;

        case c_midi_control_mod_replace:    // 68
            file << "# mod replace:\n";
            break;

        case c_midi_control_mod_snapshot:   // 69
            file << "# mod snapshot:\n";
            break;

        case c_midi_control_mod_queue:      // 70
            file << "# mod queue:\n";
            break;

        case c_midi_control_mod_gmute:      // 71
            file << "# mod gmute:\n";
            break;

        case c_midi_control_mod_glearn:     // 72
            file << "# mod glearn:\n";
            break;

        case c_midi_control_play_ss:        // 73
            file << "# screen set play:\n";
            break;

        /*
         * case c_midi_controls:  74, the last value, not written.
         */

        default:
            break;
        }
        const midi_control & toggle = ucperf.midi_control_toggle(mcontrol);
        const midi_control & off = ucperf.midi_control_off(mcontrol);
        const midi_control & on = ucperf.midi_control_on(mcontrol);
        snprintf
        (
            outs, sizeof outs,
            "%d [%1d %1d %3d %3d %3d %3d]"
              " [%1d %1d %3d %3d %3d %3d]"
              " [%1d %1d %3d %3d %3d %3d]",
             mcontrol,
             toggle.active(), toggle.inverse_active(), toggle.status(),
                 toggle.data(), toggle.min_value(), toggle.max_value(),
             on.active(), on.inverse_active(), on.status(),
                 on.data(), on.min_value(), on.max_value(),
             off.active(), off.inverse_active(), off.status(),
                 off.data(), off.min_value(), off.max_value()
        );
        file << std::string(outs) << "\n";
    }

    /*
     * Group MIDI control
     */

    file << "\n[mute-group]\n\n";
    int gm[c_seqs_in_set];

    /*
     * We might as well save the empty mutes in the "rc" configuration file,
     * even if we don't save empty mutes to the MIDI file.  This is less
     * confusing to the user, especially if issues with the mute groups occur,
     * and is not a lot of space to waste, it's just one file.
     *
     * We've replaced c_gmute_tracks with c_max_sequence, since they're the
     * same concept and same number (1024).
     */

    file <<
        "# All mute-group values are saved, even if the all are zero, and will\n"
        "# be stripped out from the MIDI file by the new strip-empty-mutes\n"
        "# functionality (a build option).  This is less confusing to the user.\n"
        "\n"
        << c_max_sequence << "       # group mute count\n"
        ;

    for (int seqj = 0; seqj < c_seqs_in_set; ++seqj)
    {
        ucperf.select_group_mute(seqj);
        for (int seqi = 0; seqi < c_seqs_in_set; ++seqi)
            gm[seqi] = ucperf.get_group_mute_state(seqi);

        snprintf
        (
            outs, sizeof outs,
            "%d [%1d %1d %1d %1d %1d %1d %1d %1d]"
            " [%1d %1d %1d %1d %1d %1d %1d %1d]"
            " [%1d %1d %1d %1d %1d %1d %1d %1d]"
            " [%1d %1d %1d %1d %1d %1d %1d %1d]",
            seqj,
            gm[0],  gm[1],  gm[2],  gm[3],  gm[4],  gm[5],  gm[6],  gm[7],
            gm[8],  gm[9],  gm[10], gm[11], gm[12], gm[13], gm[14], gm[15],
            gm[16], gm[17], gm[18], gm[19], gm[20], gm[21], gm[22], gm[23],
            gm[24], gm[25], gm[26], gm[27], gm[28], gm[29], gm[30], gm[31]
        );
        file << std::string(outs) << "\n";
    }

    /*
     * Bus mute/unmute data.  At this point, we can use the master_bus()
     * accessor, even if a pointer dereference, because it was created at
     * application start-up, and here we are at application close-down.
     */

    int buses = ucperf.master_bus().get_num_out_buses();
    file
        << "\n[midi-clock]\n\n"
        << "# The first line indicates the number of MIDI busses defined.\n"
        << "\n"
        << "# Each buss line contains the buss (re 0) and the clock status of\n"
        << "# that buss.  0 = MIDI Clock is off; 1 = MIDI Clock on, and Song\n"
        << "# Position and MIDI Continue will be sent, if needed; and 2 = MIDI\n"
        << "# Clock Modulo, where MIDI clocking will not begin until the song\n"
        << "# position reaches the start modulo value [midi-clock-mod-ticks].\n"
        << "\n"
        ;

    file << buses << "    # number of MIDI clocks/busses\n\n";
    for (int bus = 0; bus < buses; ++bus)
    {
        file
            << "# Output buss name: "
            << ucperf.master_bus().get_midi_out_bus_name(bus)
            << "\n"
            ;
        snprintf
        (
            outs, sizeof(outs), "%d %d  # buss number, clock status",
            bus, (char) ucperf.master_bus().get_clock(bus)
        );
        file << outs << "\n";
    }

    /*
     * MIDI clock modulo value
     */

    file
        << "\n[midi-clock-mod-ticks]\n\n"
        << "# The Song Position (in 16th notes) at which clocking will begin\n"
        << "# if the buss is set to MIDI Clock mod setting.\n"
        << "\n"
        << midibus::get_clock_mod() << "\n"
        ;

    /*
     * Bus input data
     */

    buses = ucperf.master_bus().get_num_in_buses();
    file
        << "\n[midi-input]\n\n"
        << buses << "   # number of input MIDI busses\n\n"
           "# The first number is the port number, and the second number\n"
           "# indicates whether it is disabled (0), or enabled (1).\n"
           "\n"
        ;

    for (int i = 0; i < buses; ++i)
    {
        file << "# " << ucperf.master_bus().get_midi_in_bus_name(i) << "\n\n";
        snprintf
        (
            outs, sizeof(outs), "%d %d",
            i, static_cast<char>(ucperf.master_bus().get_input(i))
        );
        file << outs << "\n";
    }

    /*
     * Filter by channel, new option as of 2016-08-20
     */

    file
        << "\n"
        << "# If set to 1, this option allows the master MIDI bus to record\n"
           "# (filter) incoming MIDI data by channel, allocating each incoming\n"
           "# MIDI event to the sequence that is set to that channel.\n"
           "# This is an option adopted from the Seq32 project at GitHub.\n"
           "\n"
        << (rc().filter_by_channel() ? "1" : "0")
        << "   # flag to record incoming data by channel\n"
        ;

    /*
     * Manual ALSA ports
     */

    file
        << "\n[manual-alsa-ports]\n\n"
           "# Set to 1 to have sequencer64 create its own ALSA ports and not\n"
           "# connect to other clients.  Use 1 to expose all the MIDI ports to\n"
           "# JACK (e.g. via a2jmidid).  Use 0 to access the ALSA MIDI ports\n"
           "# already running on one's computer, or to use the MIDI Through\n"
           "# (capture) output alone in JACK.\n"
           "\n"
        << (rc().manual_alsa_ports() ? "1" : "0")
        << "   # flag for manual ALSA ports\n"
        ;

    /*
     * Reveal ALSA ports
     */

    file
        << "\n[reveal-alsa-ports]\n\n"
        << "# Set to 1 to have sequencer64 ignore any system port names\n"
        << "# declared in the 'user' configuration file.  Use this option if\n"
        << "# you want to be able to see the port names as detected by ALSA.\n"
        << "\n"
        << (rc().reveal_alsa_ports() ? "1" : "0")
        << "   # flag for reveal ALSA ports\n"
        ;

    /*
     * Interaction-method
     */

    int x = 0;
    file << "\n[interaction-method]\n\n";
    while (c_interaction_method_names[x] && c_interaction_method_descs[x])
    {
        file
            << "# " << x
            << " - '" << c_interaction_method_names[x]
            << "' (" << c_interaction_method_descs[x] << ")\n"
            ;
        ++x;
    }
    file
        << "\n" << rc().interaction_method() << "   # interaction_method\n\n"
        ;

    file
        << "# Set to 1 to allow Sequencer64 to stay in note-adding mode when\n"
           "# the right-click is released while holding the Mod4 (Super or\n"
           "# Windows) key.\n"
           "\n"
        << (rc().allow_mod4_mode() ? "1" : "0")     // @new 2015-08-28
        << "   # allow_mod4_mode\n\n"
        ;

    file
        << "# Set to 1 to allow Sequencer64 to split performance editor\n"
           "# triggers at the closest snap position, instead of splitting the\n"
           "# trigger exactly in its middle.  Remember that the split is\n"
           "# activated by a middle click.\n"
           "\n"
        << (rc().allow_snap_split() ? "1" : "0")    // @new 2016-08-17
        << "   # allow_snap_split\n\n"
        ;

    file
        << "# Set to 1 to allow a double-click on a slot to bring it up in\n"
           "# the pattern editor.  This is the default.  Set it to 0 if\n"
           "# it interferes with muting/unmuting a pattern.\n"
           "\n"
        << (rc().allow_click_edit() ? "1" : "0")    // @new 2016-10-30
        << "   # allow_click_edit\n"
        ;

    size_t kevsize = ucperf.get_key_events().size() < size_t(c_seqs_in_set) ?
         ucperf.get_key_events().size() : size_t(c_seqs_in_set)
         ;
    file
        << "\n[keyboard-control]\n\n"
        << kevsize << "     # number of keys\n\n"
        << "# Key #  Sequence #  Key name\n\n"
        ;

    for
    (
        keys_perform::SlotMap::const_iterator i = ucperf.get_key_events().begin();
        i != ucperf.get_key_events().end(); ++i
    )
    {
        snprintf
        (
            outs, sizeof(outs), "%u  %ld   # %s",
            i->first, i->second,
            ucperf.key_name(i->first).c_str()   // gdk_keyval_name(i->first)
        );
        file << std::string(outs) << "\n";
    }

    size_t kegsize = ucperf.get_key_groups().size() < size_t(c_seqs_in_set) ?
         ucperf.get_key_groups().size() : size_t(c_seqs_in_set)
         ;
    file
        << "\n[keyboard-group]\n\n"
        << kegsize << "     # number of key groups\n\n"
        << "# Key #  group # Key name\n\n"
        ;

    for
    (
        keys_perform::SlotMap::const_iterator i = ucperf.get_key_groups().begin();
        i != ucperf.get_key_groups().end(); ++i
    )
    {
        snprintf
        (
            outs, sizeof(outs), "%u  %ld   # %s",
            i->first, i->second,
            ucperf.key_name(i->first).c_str()   // gdk_keyval_name(i->first)
        );
        file << std::string(outs) << "\n";
    }

    keys_perform_transfer ktx;
    ucperf.keys().get_keys(ktx);      /* copy perform key to structure    */
    file
        << "\n"
        << "# bpm up and bpm down:\n"
        << ktx.kpt_bpm_up << " "
        << ktx.kpt_bpm_dn << "          # "
        << ucperf.key_name(ktx.kpt_bpm_up) << " "
        << ucperf.key_name(ktx.kpt_bpm_dn) << "\n"
        ;
    file
        << "# screen set up, screen set down, play:\n"
        << ktx.kpt_screenset_up << " "
        << ktx.kpt_screenset_dn << " "
        << ktx.kpt_set_playing_screenset << "    # "
        << ucperf.key_name(ktx.kpt_screenset_up) << " "
        << ucperf.key_name(ktx.kpt_screenset_dn) << " "
        << ucperf.key_name(ktx.kpt_set_playing_screenset) << "\n"
        ;
    file
        << "# group on, group off, group learn:\n"
        << ktx.kpt_group_on << " "
        << ktx.kpt_group_off << " "
        << ktx.kpt_group_learn << "   # "
        << ucperf.key_name(ktx.kpt_group_on) << " "
        << ucperf.key_name(ktx.kpt_group_off) << " "
        << ucperf.key_name(ktx.kpt_group_learn) << "\n"
        ;
    file
        << "# replace, queue, snapshot_1, snapshot 2, keep queue:\n"
        << ktx.kpt_replace << " "
        << ktx.kpt_queue << " "
        << ktx.kpt_snapshot_1 << " "
        << ktx.kpt_snapshot_2 << " "
        << ktx.kpt_keep_queue << "   # "
        << ucperf.key_name(ktx.kpt_replace) << " "
        << ucperf.key_name(ktx.kpt_queue) << " "
        << ucperf.key_name(ktx.kpt_snapshot_1) << " "
        << ucperf.key_name(ktx.kpt_snapshot_2) << " "
        << ucperf.key_name(ktx.kpt_keep_queue) << "\n"
        ;

    file
        << (ktx.kpt_show_ui_sequence_key ? 1 : 0)
        << "     # show_ui_sequence_key (1 = true / 0 = false)\n"
        ;

    file
        << ktx.kpt_start << "    # "
        << ucperf.key_name(ktx.kpt_start)
        << " start sequencer\n"
       ;

    file
        << ktx.kpt_stop << "    # "
        << ucperf.key_name(ktx.kpt_stop)
        << " stop sequencer\n"
        ;

    /**
     *  New boolean to show sequence numbers; ignored in legacy mode.
     */

    if (! rc().legacy_format())
    {
        file
            << ktx.kpt_pause << "    # "
            << ucperf.key_name(ktx.kpt_pause)
            << " pause sequencer\n"
            ;

        file
            << ktx.kpt_show_ui_sequence_number << "     #"
            << " show sequence numbers (1 = true / 0 = false);"
               " ignored in legacy mode\n"
            ;

        file
            << ktx.kpt_pattern_edit << "    # "
            << ucperf.key_name(ktx.kpt_pattern_edit)
            << " is the shortcut key to bring up the pattern editor\n"
            ;

        file
            << ktx.kpt_event_edit << "    # "
            << ucperf.key_name(ktx.kpt_event_edit)
            << " is the shortcut key to bring up the event editor\n"
            ;

        /*
         * This section writes all of the new additional keystrokes created by
         * seq32 (stazed) and sequencer64.  Eventually we will provide a
         * use-interface options page for them.  Note that the Pause key is
         * handled elsewhere; it was a much earlier option for Sequencer64.
         */

        file
            << "\n[extended-keys]\n\n"
            << "# Currently there is no user interface for this section.\n\n"
            << ktx.kpt_song_mode << "    # "
            << ucperf.key_name(ktx.kpt_song_mode)
            << " handles the Song/Live mode\n"
            << ktx.kpt_toggle_jack << "    # "
            << ucperf.key_name(ktx.kpt_toggle_jack)
            << " handles the JACK mode\n"
            << ktx.kpt_menu_mode << "    # "
            << ucperf.key_name(ktx.kpt_menu_mode)
            << " handles the menu mode\n"
            << ktx.kpt_follow_transport << "    # "
            << ucperf.key_name(ktx.kpt_follow_transport)
            << " handles the following of JACK transport\n"
            << ktx.kpt_fast_forward << "    # "
            << ucperf.key_name(ktx.kpt_fast_forward)
            << " handles the Fast-Forward function\n"
            << ktx.kpt_rewind << "    # "
            << ucperf.key_name(ktx.kpt_rewind)
            << " handles Rewind function\n"
            << ktx.kpt_pointer_position << "    # "
            << ucperf.key_name(ktx.kpt_pointer_position)
            << " handles song pointer-position function\n"
            << ktx.kpt_tap_bpm << "    # "
            << ucperf.key_name(ktx.kpt_tap_bpm)
            << " emulates clicking the Tap (BPM) button\n"
            << ktx.kpt_toggle_mutes << "    # "
            << ucperf.key_name(ktx.kpt_toggle_mutes)
            << " handles the toggling-all-pattern-mutes function\n"
            ;
    }

    file
        << "\n[jack-transport]\n\n"
        "# jack_transport - Enable slave synchronization with JACK Transport.\n\n"
        << rc().with_jack_transport() << "   # jack_transport\n\n"
        "# jack_master - Sequencer64 attempts to serve as JACK Master.\n"
        "# Also must enable jack_transport (the user interface forces this,\n"
        "# and also disables jack_master_cond).\n\n"
        << rc().with_jack_master() << "   # jack_master\n\n"
        "# jack_master_cond - Sequencer64 is JACK master if no other JACK\n"
        "# master exists. Also must enable jack_transport (the user interface\n"
        "# forces this, and disables jack_master).\n\n"
        << rc().with_jack_master_cond()  << "   # jack_master_cond\n\n"
        "# song_start_mode (applies mainly if JACK is enabled).\n\n"
        "# 0 = Playback in live mode. Allows muting and unmuting of loops.\n"
        "#     from the main (patterns) window.  Disables both manual and\n"
        "#     automatic muting and unmuting from the performance window.\n"
        "# 1 = Playback uses the song (performance) editor's data and mute\n"
        "#     controls, regardless of which window was used to start the\n"
        "#     playback.\n\n"
        << p.song_start_mode() << "   # song_start_mode\n"
        ;

    /*
     * New for sequencer64:  provide configurable LASH session management.
     * Ignored in legacy mode, for now.
     */

    if (! rc().legacy_format())
    {
        file << "\n"
            "[lash-session]\n\n"
            "# Set the following value to 0 to disable LASH session management.\n"
            "# Set the following value to 1 to enable LASH session management.\n"
            "# This value will have no effect if LASH support is not built into\n"
            "# the application.  Use --help option to see if LASH is part of\n"
            "# the options list.\n"
            "\n"
            << (rc().lash_support() ? "1" : "0")
            << "     # LASH session management support flag\n"
            ;
    }

    file << "\n"
        "[auto-option-save]\n\n"
        "# Set the following value to 0 to disable the automatic saving of the\n"
        "# current configuration to the 'rc' and 'user' files.  Set it to 1 to\n"
        "# follow legacy seq24 behavior of saving the configuration at exit.\n"
        "# Note that, if auto-save is set, many of the command-line settings,\n"
        "# such as the JACK/ALSA settings, are then saved to the configuration,\n"
        "# which can confuse one at first.  Also note that one currently needs\n"
        "# this option set to 1 to save the configuration, as there is not a\n"
        "# user-interface control for it at present.\n"
        "\n"
        << (rc().auto_option_save() ? "1" : "0")
        << "     # auto-save-options-on-exit support flag\n"
        ;


    file << "\n"
        "[last-used-dir]\n\n"
        "# Last used directory:\n\n"
        << rc().last_used_dir() << "\n\n"
        ;
    file
        << "# End of " << m_name << "\n#\n"
        << "# vim: sw=4 ts=4 wm=4 et ft=sh\n"   /* ft=sh for nice colors */
        ;
    file.close();
    return true;
}

}           // namespace seq64

/*
 * optionsfile.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

