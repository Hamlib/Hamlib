/*
 *  Hamlib Interface - Rig state structure
 *  Copyright (c) 2000-2025 The Hamlib Group
 *  Copyright (c) 2025 George Baltz
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/* SPDX-License-Identifier: LGPL-2.1-or-later */

#ifndef _RIG_STATE_H
#define _RIG_STATE_H 1

//#include <hamlib/rig.h>
//#include <pthread.h>

__BEGIN_DECLS

/**
 * \addtogroup rig
 * @{
 */

/**
 *  \brief Hamlib rig state data structure.
 *
 *  \file rig_state.h
 *
 *  This file contains the live data structure of the rig (radio).
 */

/**
 * \brief Rig state containing live data and customized fields.
 *
 * This struct contains live data, as well as a copy of capability fields
 * that may be updated (ie. customized)
 *
 * It is NOT fine to move fields around as it can break shared library offset
 * As of 2024-03-03  freq_event_elapsed is the last known item being reference externally
 * So any additions or changes to this structure must be at the end of the structure
 */
struct rig_state {
    /********* ENSURE ANY NEW ITEMS ARE ADDED AT BOTTOM OF THIS STRUCTURE *********/
    /*
     * overridable fields
     */
    // moving the hamlib_port_t to the end of rig_state and making it a pointer
    // this should allow changes to hamlib_port_t without breaking shared libraries
    // these will maintain a copy of the new port_t for backwards compatibility
    // to these offsets -- note these must stay until a major version update is done like 5.0
    hamlib_port_t_deprecated rigport_deprecated;  /*!< \deprecated Rig port (internal use). */
    hamlib_port_t_deprecated pttport_deprecated;  /*!< \deprecated PTT port (internal use). */
    hamlib_port_t_deprecated dcdport_deprecated;  /*!< \deprecated DCD port (internal use). */

    double vfo_comp;        /*!< VFO compensation in PPM, 0.0 to disable */

    int deprecated_itu_region;         /*!< \deprecated ITU region to select among freq_range_t */
    freq_range_t rx_range_list[HAMLIB_FRQRANGESIZ];    /*!< Receive frequency range list */
    freq_range_t tx_range_list[HAMLIB_FRQRANGESIZ];    /*!< Transmit frequency range list */

    struct tuning_step_list tuning_steps[HAMLIB_TSLSTSIZ]; /*!< Tuning step list */

    struct filter_list filters[HAMLIB_FLTLSTSIZ];      /*!< Mode/filter table, at -6dB */

    cal_table_t str_cal;            /*!< S-meter calibration table */

    chan_t chan_list[HAMLIB_CHANLSTSIZ];   /*!< Channel list, zero ended */

    shortfreq_t max_rit;        /*!< max absolute RIT */
    shortfreq_t max_xit;        /*!< max absolute XIT */
    shortfreq_t max_ifshift;    /*!< max absolute IF-SHIFT */

    ann_t announces;            /*!< Announces bit field list */

    int preamp[HAMLIB_MAXDBLSTSIZ];    /*!< Preamp list in dB, 0 terminated */
    int attenuator[HAMLIB_MAXDBLSTSIZ];    /*!< Attenuator list in dB, 0 terminated */

    setting_t has_get_func;     /*!< List of get functions */
    setting_t has_set_func;     /*!< List of set functions */
    setting_t has_get_level;    /*!< List of get level */
    setting_t has_set_level;    /*!< List of set level */
    setting_t has_get_parm;     /*!< List of get parm */
    setting_t has_set_parm;     /*!< List of set parm */

    gran_t level_gran[RIG_SETTING_MAX]; /*!< level granularity */
    gran_t parm_gran[RIG_SETTING_MAX];  /*!< parm granularity */


    /*
     * non overridable fields, internal use
     */

    int transaction_active;    /*!< set to 1 to inform the async reader thread that a synchronous command transaction is waiting for a response, otherwise 0 */
    vfo_t current_vfo;  /*!< VFO currently set */
    int vfo_list;       /*!< Complete list of VFO for this rig */
    int comm_state;     /*!< Comm port state, opened/closed. */
    rig_ptr_t priv;     /*!< Pointer to private rig state data. */
    rig_ptr_t obj;      /*!< Internal use by hamlib++ for event handling. */

    int async_data_enabled;     /*!< Whether async data mode is enabled */
    int poll_interval;          /*!< Rig state polling period in milliseconds */
    freq_t current_freq;        /*!< Frequency currently set */
    rmode_t current_mode;       /*!< Mode currently set */
    //rmode_t current_modeB;      /*!< Mode currently set VFOB */
    pbwidth_t current_width;    /*!< Passband width currently set */
    vfo_t tx_vfo;               /*!< Tx VFO currently set */
    rmode_t mode_list;              /*!< Complete list of modes for this rig */
    // mode_list is used by some
    // so anything added to this structure must be below here
    int transmit;               /*!< rig should be transmitting i.e. hard
                                     wired PTT asserted - used by rigs that
                                     don't do CAT while in Tx */
    freq_t lo_freq;             /*!< Local oscillator frequency of any transverter */
    time_t twiddle_time;        /*!< time when vfo twiddling was detected */
    int twiddle_timeout;        /*!< timeout to resume from twiddling */
    // uplink allows gpredict to behave better by no reading the uplink VFO
    int uplink;                 /*!< uplink=1 will not read Sub, uplink=2 will not read Main */
    HL_DEPRECATED
    struct rig_cache_deprecated cache; /*!< \deprecated Only here for backward compatibility */
    int vfo_opt;                /*!< Is -o switch turned on? */
    int auto_power_on;          /*!< Allow Hamlib to power on rig
                                   automatically if supported */
    int auto_power_off;          /*!< Allow Hamlib to power off rig
                                   automatically if supported */
    int auto_disable_screensaver; /*!< Allow Hamlib to disable the
                                   rig's screen saver automatically if
                                   supported */
    int ptt_share;              /*!< Share ptt port by open/close during get_ptt, set_ptt hogs the port while active */
    int power_now;              /*!< Current RF power level in rig units */
    int power_min;              /*!< Minimum RF power level in rig units */
    int power_max;              /*!< Maximum RF power level in rig units */
    unsigned char disable_yaesu_bandselect; /*!< Disables Yaesu band select logic */
    int twiddle_rit;            /*!< Suppresses VFOB reading (cached value used) so RIT control can be used */
    int twiddle_state;          /*!< keeps track of twiddle status */
    vfo_t rx_vfo;               /*!< Rx VFO currently set */

    volatile unsigned int snapshot_packet_sequence_number;  /*!< Sequence number for JSON output. */

    volatile int multicast_publisher_run;           /*!< Multicast publisher run flag. */
    void *multicast_publisher_priv_data;            /*!< Pointer to multicast_publisher_priv_data. */
    volatile int async_data_handler_thread_run;     /*!< Async data handler thread run flag. */
    void *async_data_handler_priv_data;             /*!< Pointer to async_data_handler_priv_data. */
    volatile int poll_routine_thread_run;           /*!< Poll routine thread run flag. */
    void *poll_routine_priv_data;                   /*!< Pointer to rig_poll_routine_priv_data. */
    pthread_mutex_t mutex_set_transaction;          /*!< Thread mutex flag. */
    hamlib_port_t rigport;  /*!< Rig port (internal use). */
    hamlib_port_t pttport;  /*!< PTT port (internal use). */
    hamlib_port_t dcdport;  /*!< DCD port (internal use). */
    /********* DO NOT ADD or CHANGE anything (or than to rename) ABOVE THIS LINE *********/
    /********* ENSURE ANY NEW ITEMS ARE ADDED AFTER HERE *********/
    /* flags instructing the rig_get routines to use cached values when asyncio is in use */
    int use_cached_freq; /*!< flag instructing rig_get_freq to use cached values when asyncio is in use */
    int use_cached_mode; /*!< flag instructing rig_get_mode to use cached values when asyncio is in use */
    int use_cached_ptt;  /*!< flag instructing rig_get_ptt to use cached values when asyncio is in use */
    int depth; /*!< a depth counter to use for debug indentation and such */
    int lock_mode; /*!< flag that prevents mode changes if ~= 0 -- see set/get_lock_mode */
    powerstat_t powerstat; /*!< power status */
    char *tuner_control_pathname;  /*!< Path to external tuner control program that get 0/1 (Off/On) argument */
    char client_version[32];  /*!<! Allow client to report version for compatibility checks/capability */
    freq_t offset_vfoa; /*!< Offset to apply to VFOA/Main set_freq */
    freq_t offset_vfob; /*!< Offset to apply to VFOB/Sub set_freq */
    struct multicast_s *multicast; /*!< Pointer to multicast server data */
    // Adding a number of items so netrigctl can see the real rig information
    // Eventually may want to add these so that rigctld can see more of the backend
    // But that will come later if requested -- for now they just fill out dumpstate.c
    rig_model_t rig_model;      /*!< Rig model. */
    const char *model_name;     /*!< Model name. */
    const char *mfg_name;       /*!< Manufacturer. */
    const char *version;        /*!< Driver version. */
    const char *copyright;      /*!< Copyright info. */
    enum rig_status_e status;   /*!< Driver status. */
    int rig_type;               /*!< Rig type. */
    ptt_type_t ptt_type;        /*!< Type of the PTT port. */
    dcd_type_t dcd_type;        /*!< Type of the DCD port. */
    rig_port_t port_type;       /*!< Type of communication port. */
    int serial_rate_min;        /*!< Minimum serial speed. */
    int serial_rate_max;        /*!< Maximum serial speed. */
    int serial_data_bits;       /*!< Number of data bits. */
    int serial_stop_bits;       /*!< Number of stop bits. */
    enum serial_parity_e serial_parity;         /*!< Parity. */
    enum serial_handshake_e serial_handshake;   /*!< Handshake. */
    int write_delay;            /*!< Delay between each byte sent out, in mS */
    int post_write_delay;       /*!< Delay between each commands send out, in mS */
    int timeout;                /*!< Timeout, in mS */
    int retry;                  /*!< Maximum number of retries if command fails, 0 to disable */
    int targetable_vfo;         /*!< Bit field list of direct VFO access commands */
    int async_data_supported;       /*!< Indicates that rig is capable of outputting asynchronous data updates, such as transceive state updates or spectrum data. 1 if true, 0 otherwise. */
    int agc_level_count; /*!< Number of supported AGC levels. Zero indicates all modes should be available (for backwards-compatibility). */
    enum agc_level_e agc_levels[HAMLIB_MAX_AGC_LEVELS]; /*!< Supported AGC levels */
    tone_t *ctcss_list;   /*!< CTCSS tones list, zero ended */
    tone_t *dcs_list;     /*!< DCS code list, zero ended */
    vfo_op_t vfo_ops;           /*!< VFO op bit field list */
    scan_t scan_ops;            /*!< Scan bit field list */
    int transceive;             /*!< \deprecated Use async_data_supported instead */
    int bank_qty;               /*!< Number of banks */
    int chan_desc_sz;           /*!< Max length of memory channel name */
    freq_range_t rx_range_list1[HAMLIB_FRQRANGESIZ];   /*!< Receive frequency range list #1 */
    freq_range_t tx_range_list1[HAMLIB_FRQRANGESIZ];   /*!< Transmit frequency range list #1 */
    freq_range_t rx_range_list2[HAMLIB_FRQRANGESIZ];   /*!< Receive frequency range list #2 */
    freq_range_t tx_range_list2[HAMLIB_FRQRANGESIZ];   /*!< Transmit frequency range list #2 */
    freq_range_t rx_range_list3[HAMLIB_FRQRANGESIZ];   /*!< Receive frequency range list #3 */
    freq_range_t tx_range_list3[HAMLIB_FRQRANGESIZ];   /*!< Transmit frequency range list #3 */
    freq_range_t rx_range_list4[HAMLIB_FRQRANGESIZ];   /*!< Receive frequency range list #4 */
    freq_range_t tx_range_list4[HAMLIB_FRQRANGESIZ];   /*!< Transmit frequency range list #4 */
    freq_range_t rx_range_list5[HAMLIB_FRQRANGESIZ];   /*!< Receive frequency range list #5 */
    freq_range_t tx_range_list5[HAMLIB_FRQRANGESIZ];   /*!< Transmit frequency range list #5 */
    struct rig_spectrum_scope spectrum_scopes[HAMLIB_MAX_SPECTRUM_SCOPES]; /*!< Supported spectrum scopes. The array index must match the scope ID. Last entry must have NULL name. */
    enum rig_spectrum_mode_e spectrum_modes[HAMLIB_MAX_SPECTRUM_MODES]; /*!< Supported spectrum scope modes. Last entry must be RIG_SPECTRUM_MODE_NONE. */
    freq_t spectrum_spans[HAMLIB_MAX_SPECTRUM_SPANS];                   /*!< Supported spectrum scope frequency spans in Hz in center mode. Last entry must be 0. */
    struct rig_spectrum_avg_mode spectrum_avg_modes[HAMLIB_MAX_SPECTRUM_AVG_MODES]; /*!< Supported spectrum scope averaging modes. Last entry must have NULL name. */
    int spectrum_attenuator[HAMLIB_MAXDBLSTSIZ];    /*!< Spectrum attenuator list in dB, 0 terminated */
    volatile int morse_data_handler_thread_run;     /*!< Morse data handler thread flag. */
    void *morse_data_handler_priv_data;             /*!< Morse data handler private structure. */
    FIFO_RIG *fifo_morse;                           /*!< FIFO queue for Morse Code transmission. */
    int doppler;         /*!< True if doppler changing detected */
    char *multicast_data_addr;  /*!< Multicast data UDP address for publishing rig data and state */
    int multicast_data_port;  /*!< Multicast data UDP port for publishing rig data and state */
    char *multicast_cmd_addr;  /*!< Multicast command server UDP address for sending commands to rig */
    int multicast_cmd_port;  /*!< Multicast command server UDP port for sending commands to rig */
    volatile int multicast_receiver_run;    /*!< Multicast receiver run flag. */
    void *multicast_receiver_priv_data;     /*!< Multicast receiver private data structure, */
    rig_comm_status_t comm_status;          /*!< Detailed rig control status */
    char device_id[HAMLIB_RIGNAMSIZ];       /*!< Device name, */
    int dual_watch;         /*!< Boolean DUAL_WATCH status */
    int post_ptt_delay;     /*!< delay after PTT to allow for relays and such */
    struct timespec freq_event_elapsed;     /*!< Time struct used by various caches. */
    int freq_skip; /*!< allow frequency skip for gpredict RX/TX freq set */
    client_t client;        /*!< Client application of the library. */
    pthread_mutex_t api_mutex;      /*!< Lock for any API entry. */
// New rig_state items go before this line ============================================
};

//---Start cut here---
/**
 * \brief Deprecated Rig state containing live data and customized fields.
 *
 * \deprecated
 * Due to DLL problems this remains in-place in the rig_caps structure but is no
 * longer referred to.\n\n A new rig_state has been added at the end of the
 * structure instead of the middle.\n\n This struct contains no data and is just a
 * place holder for DLL alignment.\n\n It is NOT fine to touch this struct AT
 * ALL!!!\n\n Do not use in new code.
 */
struct rig_state_deprecated {
    /********* ENSURE YOU DO NOT EVER MODIFY THIS STRUCTURE *********/
    /********* It will remain forever to provide DLL backwards compatibility ******/
    /*
     * overridable fields
     */
    // moving the hamlib_port_t to the end of rig_state and making it a pointer
    // this should allow changes to hamlib_port_t without breaking shared libraries
    // these will maintain a copy of the new port_t for backwards compatibility
    // to these offsets -- note these must stay until a major version update is done like 5.0
    hamlib_port_t_deprecated rigport_deprecated;  /*!< Rig port (internal use). */
    hamlib_port_t_deprecated pttport_deprecated;  /*!< PTT port (internal use). */
    hamlib_port_t_deprecated dcdport_deprecated;  /*!< DCD port (internal use). */

    double vfo_comp;        /*!< VFO compensation in PPM, 0.0 to disable */

    int deprecated_itu_region;         /*!< ITU region to select among freq_range_t */
    freq_range_t rx_range_list[HAMLIB_FRQRANGESIZ];    /*!< Receive frequency range list */
    freq_range_t tx_range_list[HAMLIB_FRQRANGESIZ];    /*!< Transmit frequency range list */

    struct tuning_step_list tuning_steps[HAMLIB_TSLSTSIZ]; /*!< Tuning step list */

    struct filter_list filters[HAMLIB_FLTLSTSIZ];      /*!< Mode/filter table, at -6dB */

    cal_table_t str_cal;            /*!< S-meter calibration table */

    chan_t chan_list[HAMLIB_CHANLSTSIZ];   /*!< Channel list, zero ended */

    shortfreq_t max_rit;        /*!< max absolute RIT */
    shortfreq_t max_xit;        /*!< max absolute XIT */
    shortfreq_t max_ifshift;    /*!< max absolute IF-SHIFT */

    ann_t announces;            /*!< Announces bit field list */

    int preamp[HAMLIB_MAXDBLSTSIZ];    /*!< Preamp list in dB, 0 terminated */
    int attenuator[HAMLIB_MAXDBLSTSIZ];    /*!< Attenuator list in dB, 0 terminated */

    setting_t has_get_func;     /*!< List of get functions */
    setting_t has_set_func;     /*!< List of set functions */
    setting_t has_get_level;    /*!< List of get level */
    setting_t has_set_level;    /*!< List of set level */
    setting_t has_get_parm;     /*!< List of get parm */
    setting_t has_set_parm;     /*!< List of set parm */

    gran_t level_gran[RIG_SETTING_MAX]; /*!< level granularity */
    gran_t parm_gran[RIG_SETTING_MAX];  /*!< parm granularity */


    /*
     * non overridable fields, internal use
     */

    int transaction_active;    /*!< set to 1 to inform the async reader thread that a synchronous command transaction is waiting for a response, otherwise 0 */
    vfo_t current_vfo;  /*!< VFO currently set */
    int vfo_list;       /*!< Complete list of VFO for this rig */
    int comm_state;     /*!< Comm port state, opened/closed. */
    rig_ptr_t priv;     /*!< Pointer to private rig state data. */
    rig_ptr_t obj;      /*!< Internal use by hamlib++ for event handling. */

    int async_data_enabled;     /*!< Whether async data mode is enabled */
    int poll_interval;          /*!< Rig state polling period in milliseconds */
    freq_t current_freq;        /*!< Frequency currently set */
    rmode_t current_mode;       /*!< Mode currently set */
    //rmode_t current_modeB;      /*!< Mode currently set VFOB */
    pbwidth_t current_width;    /*!< Passband width currently set */
    vfo_t tx_vfo;               /*!< Tx VFO currently set */
    rmode_t mode_list;              /*!< Complete list of modes for this rig */
    // mode_list is used by some
    // so anything added to this structure must be below here
    int transmit;               /*!< rig should be transmitting i.e. hard
                                     wired PTT asserted - used by rigs that
                                     don't do CAT while in Tx */
    freq_t lo_freq;             /*!< Local oscillator frequency of any transverter */
    time_t twiddle_time;        /*!< time when vfo twiddling was detected */
    int twiddle_timeout;        /*!< timeout to resume from twiddling */
    // uplink allows gpredict to behave better by no reading the uplink VFO
    int uplink;                 /*!< uplink=1 will not read Sub, uplink=2 will not read Main */
    struct rig_cache_deprecated cache;  /*!< Here for backward compatibility. */
    int vfo_opt;                /*!< Is -o switch turned on? */
    int auto_power_on;          /*!< Allow Hamlib to power on rig
                                   automatically if supported */
    int auto_power_off;          /*!< Allow Hamlib to power off rig
                                   automatically if supported */
    int auto_disable_screensaver; /*!< Allow Hamlib to disable the
                                   rig's screen saver automatically if
                                   supported */
    int ptt_share;              /*!< Share ptt port by open/close during get_ptt, set_ptt hogs the port while active */
    int power_now;              /*!< Current RF power level in rig units */
    int power_min;              /*!< Minimum RF power level in rig units */
    int power_max;              /*!< Maximum RF power level in rig units */
    unsigned char disable_yaesu_bandselect; /*!< Disables Yaesu band select logic */
    int twiddle_rit;            /*!< Suppresses VFOB reading (cached value used) so RIT control can be used */
    int twiddle_state;          /*!< keeps track of twiddle status */
    vfo_t rx_vfo;               /*!< Rx VFO currently set */

    volatile unsigned int snapshot_packet_sequence_number;  /*!< Sequence number for JSON output. */

    volatile int multicast_publisher_run;           /*!< Multicast publisher run flag. */
    void *multicast_publisher_priv_data;            /*!< Pointer to multicast_publisher_priv_data. */
    volatile int async_data_handler_thread_run;     /*!< Async data handler thread run flag. */
    void *async_data_handler_priv_data;             /*!< Pointer to async_data_handler_priv_data. */
    volatile int poll_routine_thread_run;           /*!< Poll routine thread run flag. */
    void *poll_routine_priv_data;                   /*!< Pointer to rig_poll_routine_priv_data. */
    pthread_mutex_t mutex_set_transaction;          /*!< Thread mutex flag. */
    hamlib_port_t rigport;  /*!< Rig port (internal use). */
    hamlib_port_t pttport;  /*!< PTT port (internal use). */
    hamlib_port_t dcdport;  /*!< DCD port (internal use). */
    /********* DO NOT ADD or CHANGE anything (or than to rename) ABOVE THIS LINE *********/
    /********* ENSURE ANY NEW ITEMS ARE ADDED AFTER HERE *********/
    /* flags instructing the rig_get routines to use cached values when asyncio is in use */
    int use_cached_freq; /*!< flag instructing rig_get_freq to use cached values when asyncio is in use */
    int use_cached_mode; /*!< flag instructing rig_get_mode to use cached values when asyncio is in use */
    int use_cached_ptt;  /*!< flag instructing rig_get_ptt to use cached values when asyncio is in use */
    int depth; /*!< a depth counter to use for debug indentation and such */
    int lock_mode; /*!< flag that prevents mode changes if ~= 0 -- see set/get_lock_mode */
    powerstat_t powerstat; /*!< power status */
    char *tuner_control_pathname;  /*!< Path to external tuner control program that get 0/1 (Off/On) argument */
    char client_version[32];  /*!<! Allow client to report version for compatibility checks/capability */
    freq_t offset_vfoa; /*!< Offset to apply to VFOA/Main set_freq */
    freq_t offset_vfob; /*!< Offset to apply to VFOB/Sub set_freq */
    struct multicast_s *multicast; /*!< Pointer to multicast server data */
};
//---End cut here---

#if defined(IN_HAMLIB)
#define STATE(r) (&(r)->state)
#endif
/** Macro for application access to rig_state data structure using the #RIG
 * handle.
 *
 * Example code.
 * ```
 * RIG *my_rig;
 *
 * //Instantiate a rig
 * my_rig = rig_init(RIG_MODEL_DUMMY); // your rig (radio) model.
 *
 * const struct rig_state *my_rs = HAMLIB_STATE(my_rig);
 * ```
 */
#define HAMLIB_STATE(r) ((struct rig_state *)rig_data_pointer(r, RIG_PTRX_STATE))

__END_DECLS

#endif /* _RIG_STATE_H */

/** @} */
