package Hamlib;

use 5.006;
use strict;
use warnings;
use Errno;
use Carp;

require Exporter;
require DynaLoader;
use AutoLoader;

our @ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Hamlib ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	CHANLSTSIZ
	FILPATHLEN
	FLTLSTSIZ
	FRQRANGESIZ
	MAXCHANDESC
	MAXDBLSTSIZ
	RIGNAMSIZ
	RIGVERSIZ
	RIG_ANN_ALL
	RIG_ANN_FREQ
	RIG_ANN_NONE
	RIG_ANN_OFF
	RIG_ANN_RXMODE
	RIG_COMBO_MAX
	RIG_CONF_CHECKBUTTON
	RIG_CONF_COMBO
	RIG_CONF_NUMERIC
	RIG_CONF_STRING
	RIG_CTRL_MAIN
	RIG_CTRL_SUB
	RIG_ECONF
	RIG_EINTERNAL
	RIG_EINVAL
	RIG_EIO
	RIG_ENAVAIL
	RIG_ENIMPL
	RIG_ENOMEM
	RIG_ENTARGET
	RIG_EPROTO
	RIG_ERJCTED
	RIG_ETIMEOUT
	RIG_ETRUNC
	RIG_FLAG_APRS
	RIG_FLAG_COMPUTER
	RIG_FLAG_DXCLUSTER
	RIG_FLAG_HANDHELD
	RIG_FLAG_MOBILE
	RIG_FLAG_RECEIVER
	RIG_FLAG_SCANNER
	RIG_FLAG_TNC
	RIG_FLAG_TRANSCEIVER
	RIG_FLAG_TRANSMITTER
	RIG_FLAG_TRUNKING
	RIG_FREQ_NONE
	RIG_FUNC_ABM
	RIG_FUNC_AIP
	RIG_FUNC_ANF
	RIG_FUNC_APF
	RIG_FUNC_ARO
	RIG_FUNC_BC
	RIG_FUNC_COMP
	RIG_FUNC_FAGC
	RIG_FUNC_FBKIN
	RIG_FUNC_LMP
	RIG_FUNC_LOCK
	RIG_FUNC_MBC
	RIG_FUNC_MN
	RIG_FUNC_MON
	RIG_FUNC_MUTE
	RIG_FUNC_NB
	RIG_FUNC_NONE
	RIG_FUNC_NR
	RIG_FUNC_REV
	RIG_FUNC_RNF
	RIG_FUNC_SBKIN
	RIG_FUNC_SQL
	RIG_FUNC_TONE
	RIG_FUNC_TSQL
	RIG_FUNC_VOX
	RIG_FUNC_VSC
	RIG_ITU_REGION1
	RIG_ITU_REGION2
	RIG_ITU_REGION3
	RIG_LEVEL_AF
	RIG_LEVEL_AGC
	RIG_LEVEL_ALC
	RIG_LEVEL_APF
	RIG_LEVEL_ATT
	RIG_LEVEL_BALANCE
	RIG_LEVEL_BKINDL
	RIG_LEVEL_COMP
	RIG_LEVEL_CWPITCH
	RIG_LEVEL_FLOAT_LIST
	RIG_LEVEL_IF
	RIG_LEVEL_KEYSPD
	RIG_LEVEL_METER
	RIG_LEVEL_MICGAIN
	RIG_LEVEL_NONE
	RIG_LEVEL_NOTCHF
	RIG_LEVEL_NR
	RIG_LEVEL_PBT_IN
	RIG_LEVEL_PBT_OUT
	RIG_LEVEL_PREAMP
	RIG_LEVEL_READONLY_LIST
	RIG_LEVEL_RF
	RIG_LEVEL_RFPOWER
	RIG_LEVEL_SQL
	RIG_LEVEL_SQLSTAT
	RIG_LEVEL_STRENGTH
	RIG_LEVEL_SWR
	RIG_LEVEL_VOX
	RIG_MODE_AM
	RIG_MODE_CW
	RIG_MODE_FM
	RIG_MODE_LSB
	RIG_MODE_NONE
	RIG_MODE_RTTY
	RIG_MODE_SSB
	RIG_MODE_USB
	RIG_MODE_WFM
	RIG_OK
	RIG_OP_BAND_DOWN
	RIG_OP_BAND_UP
	RIG_OP_CPY
	RIG_OP_DOWN
	RIG_OP_FROM_VFO
	RIG_OP_LEFT
	RIG_OP_MCL
	RIG_OP_NONE
	RIG_OP_RIGHT
	RIG_OP_TO_VFO
	RIG_OP_UP
	RIG_OP_XCHG
	RIG_PARM_ANN
	RIG_PARM_APO
	RIG_PARM_BACKLIGHT
	RIG_PARM_BAT
	RIG_PARM_BEEP
	RIG_PARM_FLOAT_LIST
	RIG_PARM_NONE
	RIG_PARM_READONLY_LIST
	RIG_PARM_TIME
	RIG_PASSBAND_NORMAL
	RIG_SCAN_DELTA
	RIG_SCAN_MEM
	RIG_SCAN_NONE
	RIG_SCAN_PRIO
	RIG_SCAN_PROG
	RIG_SCAN_SLCT
	RIG_SCAN_STOP
	RIG_SETTING_MAX
	RIG_TARGETABLE_ALL
	RIG_TARGETABLE_FREQ
	RIG_TARGETABLE_MODE
	RIG_TARGETABLE_NONE
	RIG_TRN_OFF
	RIG_TRN_POLL
	RIG_TRN_RIG
	RIG_TYPE_COMPUTER
	RIG_TYPE_HANDHELD
	RIG_TYPE_MASK
	RIG_TYPE_MOBILE
	RIG_TYPE_OTHER
	RIG_TYPE_PCRECEIVER
	RIG_TYPE_RECEIVER
	RIG_TYPE_SCANNER
	RIG_TYPE_TRANSCEIVER
	RIG_TYPE_TRUNKSCANNER
	RIG_VFO1
	RIG_VFO2
	RIG_VFO_A
	RIG_VFO_ALL
	RIG_VFO_B
	RIG_VFO_C
	RIG_VFO_CURR
	RIG_VFO_MAIN
	RIG_VFO_MEM
	RIG_VFO_NONE
	RIG_VFO_SUB
	RIG_VFO_VFO
	TSLSTSIZ
	__BEGIN_DECLS
	__END_DECLS
	ptr_t
	check_backend
	cleanup
	close
	confparam_lookup
	debug
	get_ant
	get_caps
	get_channel
	get_conf
	get_ctcss_sql
	get_ctcss_tone
	get_dcd
	get_dcs_code
	get_dcs_sql
	get_freq
	get_func
	get_info
	get_level
	get_mem
	get_mode
	get_parm
	get_powerstat
	get_ptt
	get_range
	get_resolution
	get_rit
	get_rptr_offs
	get_rptr_shift
	get_split
	get_split_freq
	get_split_mode
	get_trn
	get_ts
	get_vfo
	get_xit
	has_get_func
	has_get_level
	has_get_parm
	has_scan
	has_set_func
	has_set_level
	has_set_parm
	has_vfo_op
	init
	list_foreach
	load_all_backends
	load_backend
	mW2power
	need_debug
	open
	passband_narrow
	passband_normal
	passband_wide
	power2mW
	probe
	probe_all
	recv_dtmf
	register
	reset
	restore_channel
	rigerror
	save_channel
	scan
	send_dtmf
	send_morse
	set_ant
	set_bank
	set_channel
	set_conf
	set_ctcss_sql
	set_ctcss_tone
	set_dcs_code
	set_dcs_sql
	set_debug
	set_freq
	set_func
	set_level
	set_mem
	set_mode
	set_parm
	set_powerstat
	set_ptt
	set_rit
	set_rptr_offs
	set_rptr_shift
	set_split
	set_split_freq
	set_split_mode
	set_trn
	set_ts
	set_vfo
	set_xit
	setting2idx
	token_foreach
	token_lookup
	unregister
	vfo_op
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	CHANLSTSIZ
	FILPATHLEN
	FLTLSTSIZ
	FRQRANGESIZ
	MAXCHANDESC
	MAXDBLSTSIZ
	RIGNAMSIZ
	RIGVERSIZ
	RIG_ANN_ALL
	RIG_ANN_FREQ
	RIG_ANN_NONE
	RIG_ANN_OFF
	RIG_ANN_RXMODE
	RIG_COMBO_MAX
	RIG_CONF_CHECKBUTTON
	RIG_CONF_COMBO
	RIG_CONF_NUMERIC
	RIG_CONF_STRING
	RIG_CTRL_MAIN
	RIG_CTRL_SUB
	RIG_ECONF
	RIG_EINTERNAL
	RIG_EINVAL
	RIG_EIO
	RIG_ENAVAIL
	RIG_ENIMPL
	RIG_ENOMEM
	RIG_ENTARGET
	RIG_EPROTO
	RIG_ERJCTED
	RIG_ETIMEOUT
	RIG_ETRUNC
	RIG_FLAG_APRS
	RIG_FLAG_COMPUTER
	RIG_FLAG_DXCLUSTER
	RIG_FLAG_HANDHELD
	RIG_FLAG_MOBILE
	RIG_FLAG_RECEIVER
	RIG_FLAG_SCANNER
	RIG_FLAG_TNC
	RIG_FLAG_TRANSCEIVER
	RIG_FLAG_TRANSMITTER
	RIG_FLAG_TRUNKING
	RIG_FREQ_NONE
	RIG_FUNC_ABM
	RIG_FUNC_AIP
	RIG_FUNC_ANF
	RIG_FUNC_APF
	RIG_FUNC_ARO
	RIG_FUNC_BC
	RIG_FUNC_COMP
	RIG_FUNC_FAGC
	RIG_FUNC_FBKIN
	RIG_FUNC_LMP
	RIG_FUNC_LOCK
	RIG_FUNC_MBC
	RIG_FUNC_MN
	RIG_FUNC_MON
	RIG_FUNC_MUTE
	RIG_FUNC_NB
	RIG_FUNC_NONE
	RIG_FUNC_NR
	RIG_FUNC_REV
	RIG_FUNC_RNF
	RIG_FUNC_SBKIN
	RIG_FUNC_SQL
	RIG_FUNC_TONE
	RIG_FUNC_TSQL
	RIG_FUNC_VOX
	RIG_FUNC_VSC
	RIG_ITU_REGION1
	RIG_ITU_REGION2
	RIG_ITU_REGION3
	RIG_LEVEL_AF
	RIG_LEVEL_AGC
	RIG_LEVEL_ALC
	RIG_LEVEL_APF
	RIG_LEVEL_ATT
	RIG_LEVEL_BALANCE
	RIG_LEVEL_BKINDL
	RIG_LEVEL_COMP
	RIG_LEVEL_CWPITCH
	RIG_LEVEL_FLOAT_LIST
	RIG_LEVEL_IF
	RIG_LEVEL_KEYSPD
	RIG_LEVEL_METER
	RIG_LEVEL_MICGAIN
	RIG_LEVEL_NONE
	RIG_LEVEL_NOTCHF
	RIG_LEVEL_NR
	RIG_LEVEL_PBT_IN
	RIG_LEVEL_PBT_OUT
	RIG_LEVEL_PREAMP
	RIG_LEVEL_READONLY_LIST
	RIG_LEVEL_RF
	RIG_LEVEL_RFPOWER
	RIG_LEVEL_SQL
	RIG_LEVEL_SQLSTAT
	RIG_LEVEL_STRENGTH
	RIG_LEVEL_SWR
	RIG_LEVEL_VOX
	RIG_MODE_AM
	RIG_MODE_CW
	RIG_MODE_FM
	RIG_MODE_LSB
	RIG_MODE_NONE
	RIG_MODE_RTTY
	RIG_MODE_SSB
	RIG_MODE_USB
	RIG_MODE_WFM
	RIG_OK
	RIG_OP_BAND_DOWN
	RIG_OP_BAND_UP
	RIG_OP_CPY
	RIG_OP_DOWN
	RIG_OP_FROM_VFO
	RIG_OP_LEFT
	RIG_OP_MCL
	RIG_OP_NONE
	RIG_OP_RIGHT
	RIG_OP_TO_VFO
	RIG_OP_UP
	RIG_OP_XCHG
	RIG_PARM_ANN
	RIG_PARM_APO
	RIG_PARM_BACKLIGHT
	RIG_PARM_BAT
	RIG_PARM_BEEP
	RIG_PARM_FLOAT_LIST
	RIG_PARM_NONE
	RIG_PARM_READONLY_LIST
	RIG_PARM_TIME
	RIG_PASSBAND_NORMAL
	RIG_SCAN_DELTA
	RIG_SCAN_MEM
	RIG_SCAN_NONE
	RIG_SCAN_PRIO
	RIG_SCAN_PROG
	RIG_SCAN_SLCT
	RIG_SCAN_STOP
	RIG_SETTING_MAX
	RIG_TARGETABLE_ALL
	RIG_TARGETABLE_FREQ
	RIG_TARGETABLE_MODE
	RIG_TARGETABLE_NONE
	RIG_TRN_OFF
	RIG_TRN_POLL
	RIG_TRN_RIG
	RIG_TYPE_COMPUTER
	RIG_TYPE_HANDHELD
	RIG_TYPE_MASK
	RIG_TYPE_MOBILE
	RIG_TYPE_OTHER
	RIG_TYPE_PCRECEIVER
	RIG_TYPE_RECEIVER
	RIG_TYPE_SCANNER
	RIG_TYPE_TRANSCEIVER
	RIG_TYPE_TRUNKSCANNER
	RIG_VFO1
	RIG_VFO2
	RIG_VFO_A
	RIG_VFO_ALL
	RIG_VFO_B
	RIG_VFO_C
	RIG_VFO_CURR
	RIG_VFO_MAIN
	RIG_VFO_MEM
	RIG_VFO_NONE
	RIG_VFO_SUB
	RIG_VFO_VFO
	TSLSTSIZ
	__BEGIN_DECLS
	__END_DECLS
	ptr_t
);
our $VERSION = '0.01';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "& not defined" if $constname eq 'constant';
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($!{EINVAL}) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    croak "Your vendor has not defined Hamlib macro $constname";
	}
    }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
	if ($] >= 5.00561) {
	    *$AUTOLOAD = sub () { $val };
	}
	else {
	    *$AUTOLOAD = sub { $val };
	}
    }
    goto &$AUTOLOAD;
}

bootstrap Hamlib $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

Hamlib - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Hamlib;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Hamlib, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.

=head2 Exportable constants

  CHANLSTSIZ
  FILPATHLEN
  FLTLSTSIZ
  FRQRANGESIZ
  MAXCHANDESC
  MAXDBLSTSIZ
  RIGNAMSIZ
  RIGVERSIZ
  RIG_ANN_ALL
  RIG_ANN_FREQ
  RIG_ANN_NONE
  RIG_ANN_OFF
  RIG_ANN_RXMODE
  RIG_COMBO_MAX
  RIG_CONF_CHECKBUTTON
  RIG_CONF_COMBO
  RIG_CONF_NUMERIC
  RIG_CONF_STRING
  RIG_CTRL_MAIN
  RIG_CTRL_SUB
  RIG_ECONF
  RIG_EINTERNAL
  RIG_EINVAL
  RIG_EIO
  RIG_ENAVAIL
  RIG_ENIMPL
  RIG_ENOMEM
  RIG_ENTARGET
  RIG_EPROTO
  RIG_ERJCTED
  RIG_ETIMEOUT
  RIG_ETRUNC
  RIG_FLAG_APRS
  RIG_FLAG_COMPUTER
  RIG_FLAG_DXCLUSTER
  RIG_FLAG_HANDHELD
  RIG_FLAG_MOBILE
  RIG_FLAG_RECEIVER
  RIG_FLAG_SCANNER
  RIG_FLAG_TNC
  RIG_FLAG_TRANSCEIVER
  RIG_FLAG_TRANSMITTER
  RIG_FLAG_TRUNKING
  RIG_FREQ_NONE
  RIG_FUNC_ABM
  RIG_FUNC_AIP
  RIG_FUNC_ANF
  RIG_FUNC_APF
  RIG_FUNC_ARO
  RIG_FUNC_BC
  RIG_FUNC_COMP
  RIG_FUNC_FAGC
  RIG_FUNC_FBKIN
  RIG_FUNC_LMP
  RIG_FUNC_LOCK
  RIG_FUNC_MBC
  RIG_FUNC_MN
  RIG_FUNC_MON
  RIG_FUNC_MUTE
  RIG_FUNC_NB
  RIG_FUNC_NONE
  RIG_FUNC_NR
  RIG_FUNC_REV
  RIG_FUNC_RNF
  RIG_FUNC_SBKIN
  RIG_FUNC_SQL
  RIG_FUNC_TONE
  RIG_FUNC_TSQL
  RIG_FUNC_VOX
  RIG_FUNC_VSC
  RIG_ITU_REGION1
  RIG_ITU_REGION2
  RIG_ITU_REGION3
  RIG_LEVEL_AF
  RIG_LEVEL_AGC
  RIG_LEVEL_ALC
  RIG_LEVEL_APF
  RIG_LEVEL_ATT
  RIG_LEVEL_BALANCE
  RIG_LEVEL_BKINDL
  RIG_LEVEL_COMP
  RIG_LEVEL_CWPITCH
  RIG_LEVEL_FLOAT_LIST
  RIG_LEVEL_IF
  RIG_LEVEL_KEYSPD
  RIG_LEVEL_METER
  RIG_LEVEL_MICGAIN
  RIG_LEVEL_NONE
  RIG_LEVEL_NOTCHF
  RIG_LEVEL_NR
  RIG_LEVEL_PBT_IN
  RIG_LEVEL_PBT_OUT
  RIG_LEVEL_PREAMP
  RIG_LEVEL_READONLY_LIST
  RIG_LEVEL_RF
  RIG_LEVEL_RFPOWER
  RIG_LEVEL_SQL
  RIG_LEVEL_SQLSTAT
  RIG_LEVEL_STRENGTH
  RIG_LEVEL_SWR
  RIG_LEVEL_VOX
  RIG_MODE_AM
  RIG_MODE_CW
  RIG_MODE_FM
  RIG_MODE_LSB
  RIG_MODE_NONE
  RIG_MODE_RTTY
  RIG_MODE_SSB
  RIG_MODE_USB
  RIG_MODE_WFM
  RIG_OK
  RIG_OP_BAND_DOWN
  RIG_OP_BAND_UP
  RIG_OP_CPY
  RIG_OP_DOWN
  RIG_OP_FROM_VFO
  RIG_OP_LEFT
  RIG_OP_MCL
  RIG_OP_NONE
  RIG_OP_RIGHT
  RIG_OP_TO_VFO
  RIG_OP_UP
  RIG_OP_XCHG
  RIG_PARM_ANN
  RIG_PARM_APO
  RIG_PARM_BACKLIGHT
  RIG_PARM_BAT
  RIG_PARM_BEEP
  RIG_PARM_FLOAT_LIST
  RIG_PARM_NONE
  RIG_PARM_READONLY_LIST
  RIG_PARM_TIME
  RIG_PASSBAND_NORMAL
  RIG_SCAN_DELTA
  RIG_SCAN_MEM
  RIG_SCAN_NONE
  RIG_SCAN_PRIO
  RIG_SCAN_PROG
  RIG_SCAN_SLCT
  RIG_SCAN_STOP
  RIG_SETTING_MAX
  RIG_TARGETABLE_ALL
  RIG_TARGETABLE_FREQ
  RIG_TARGETABLE_MODE
  RIG_TARGETABLE_NONE
  RIG_TRN_OFF
  RIG_TRN_POLL
  RIG_TRN_RIG
  RIG_TYPE_COMPUTER
  RIG_TYPE_HANDHELD
  RIG_TYPE_MASK
  RIG_TYPE_MOBILE
  RIG_TYPE_OTHER
  RIG_TYPE_PCRECEIVER
  RIG_TYPE_RECEIVER
  RIG_TYPE_SCANNER
  RIG_TYPE_TRANSCEIVER
  RIG_TYPE_TRUNKSCANNER
  RIG_VFO1
  RIG_VFO2
  RIG_VFO_A
  RIG_VFO_ALL
  RIG_VFO_B
  RIG_VFO_C
  RIG_VFO_CURR
  RIG_VFO_MAIN
  RIG_VFO_MEM
  RIG_VFO_NONE
  RIG_VFO_SUB
  RIG_VFO_VFO
  TSLSTSIZ
  __BEGIN_DECLS
  __END_DECLS
  ptr_t

=head2 Exportable functions

  int     rig_check_backend  (rig_model_t rig_model)  
  int     rig_cleanup  (RIG *rig)  
  int     rig_close  (RIG *rig)  
  const struct confparams*     rig_confparam_lookup  (RIG *rig, const char *name)  
  void     rig_debug  (enum rig_debug_level_e debug_level, const char *fmt, ...)  
  int     rig_get_ant  (RIG *rig, vfo_t vfo, ant_t *ant)  
  const struct rig_caps *     rig_get_caps  (rig_model_t rig_model)  
  int     rig_get_channel  (RIG *rig, channel_t *chan)  
  int     rig_get_conf  (RIG *rig, token_t token, char *val)  
  int     rig_get_ctcss_sql  (RIG *rig, vfo_t vfo, tone_t *tone)  
  int     rig_get_ctcss_tone  (RIG *rig, vfo_t vfo, tone_t *tone)  
  int     rig_get_dcd  (RIG *rig, vfo_t vfo, dcd_t *dcd)  
  int     rig_get_dcs_code  (RIG *rig, vfo_t vfo, tone_t *code)  
  int     rig_get_dcs_sql  (RIG *rig, vfo_t vfo, tone_t *code)  
  int     rig_get_freq  (RIG *rig, vfo_t vfo, freq_t *freq)  
  int     rig_get_func  (RIG *rig, vfo_t vfo, setting_t func, int *status)  
  const char *     rig_get_info  (RIG *rig)  
  int     rig_get_level  (RIG *rig, vfo_t vfo, setting_t level, value_t *val)  
  int     rig_get_mem  (RIG *rig, vfo_t vfo, int *ch)  
  int     rig_get_mode  (RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)  
  int     rig_get_parm  (RIG *rig, setting_t parm, value_t *val)  
  int     rig_get_powerstat  (RIG *rig, powerstat_t *status)  
  int     rig_get_ptt  (RIG *rig, vfo_t vfo, ptt_t *ptt)  
  const freq_range_t *     rig_get_range  (const freq_range_t *range_list, freq_t freq, rmode_t mode)  
  shortfreq_t     rig_get_resolution  (RIG *rig, rmode_t mode)  
  int     rig_get_rit  (RIG *rig, vfo_t vfo, shortfreq_t *rit)  
  int     rig_get_rptr_offs  (RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)  
  int     rig_get_rptr_shift  (RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)  
  int     rig_get_split  (RIG *rig, vfo_t vfo, split_t *split)  
  int     rig_get_split_freq  (RIG *rig, vfo_t vfo, freq_t *tx_freq)  
  int     rig_get_split_mode  (RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)  
  int     rig_get_trn  (RIG *rig, int *trn)  
  int     rig_get_ts  (RIG *rig, vfo_t vfo, shortfreq_t *ts)  
  int     rig_get_vfo  (RIG *rig, vfo_t *vfo)  
  int     rig_get_xit  (RIG *rig, vfo_t vfo, shortfreq_t *xit)  
  setting_t     rig_has_get_func  (RIG *rig, setting_t func)  
  setting_t     rig_has_get_level  (RIG *rig, setting_t level)  
  setting_t     rig_has_get_parm  (RIG *rig, setting_t parm)  
  scan_t     rig_has_scan  (RIG *rig, scan_t scan)  
  setting_t     rig_has_set_func  (RIG *rig, setting_t func)  
  setting_t     rig_has_set_level  (RIG *rig, setting_t level)  
  setting_t     rig_has_set_parm  (RIG *rig, setting_t parm)  
  vfo_op_t     rig_has_vfo_op  (RIG *rig, vfo_op_t op)  
  RIG *     rig_init  (rig_model_t rig_model)  
  int     rig_list_foreach  (int (*cfunc)(const struct rig_caps*, void* ), void*  data)  
  int     rig_load_all_backends  ()  
  int     rig_load_backend  (const char *be_name)  
  int     rig_mW2power  (RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode)  
  int     rig_need_debug  (enum rig_debug_level_e debug_level)  
  int     rig_open  (RIG *rig)  
  pbwidth_t     rig_passband_narrow  (RIG *rig, rmode_t mode)  
  pbwidth_t     rig_passband_normal  (RIG *rig, rmode_t mode)  
  pbwidth_t     rig_passband_wide  (RIG *rig, rmode_t mode)  
  int     rig_power2mW  (RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode)  
  rig_model_t     rig_probe  (port_t *p)  
  rig_model_t     rig_probe_all  (port_t *p)  
  int     rig_recv_dtmf  (RIG *rig, vfo_t vfo, char *digits, int *length)  
  int     rig_register  (const struct rig_caps *caps)  
  int     rig_reset  (RIG *rig, reset_t reset)  
  int     rig_restore_channel  (RIG *rig, const channel_t *chan)  
  int     rig_save_channel  (RIG *rig, channel_t *chan)  
  int     rig_scan  (RIG *rig, vfo_t vfo, scan_t scan, int ch)  
  int     rig_send_dtmf  (RIG *rig, vfo_t vfo, const char *digits)  
  int     rig_send_morse  (RIG *rig, vfo_t vfo, const char *msg)  
  int     rig_set_ant  (RIG *rig, vfo_t vfo, ant_t ant)  
  int     rig_set_bank  (RIG *rig, vfo_t vfo, int bank)  
  int     rig_set_channel  (RIG *rig, const channel_t *chan)  
  int     rig_set_conf  (RIG *rig, token_t token, const char *val)  
  int     rig_set_ctcss_sql  (RIG *rig, vfo_t vfo, tone_t tone)  
  int     rig_set_ctcss_tone  (RIG *rig, vfo_t vfo, tone_t tone)  
  int     rig_set_dcs_code  (RIG *rig, vfo_t vfo, tone_t code)  
  int     rig_set_dcs_sql  (RIG *rig, vfo_t vfo, tone_t code)  
  void     rig_set_debug  (enum rig_debug_level_e debug_level)  
  int     rig_set_freq  (RIG *rig, vfo_t vfo, freq_t freq)  
  int     rig_set_func  (RIG *rig, vfo_t vfo, setting_t func, int status)  
  int     rig_set_level  (RIG *rig, vfo_t vfo, setting_t level, value_t val)  
  int     rig_set_mem  (RIG *rig, vfo_t vfo, int ch)  
  int     rig_set_mode  (RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)  
  int     rig_set_parm  (RIG *rig, setting_t parm, value_t val)  
  int     rig_set_powerstat  (RIG *rig, powerstat_t status)  
  int     rig_set_ptt  (RIG *rig, vfo_t vfo, ptt_t ptt)  
  int     rig_set_rit  (RIG *rig, vfo_t vfo, shortfreq_t rit)  
  int     rig_set_rptr_offs  (RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)  
  int     rig_set_rptr_shift  (RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)  
  int     rig_set_split  (RIG *rig, vfo_t vfo, split_t split)  
  int     rig_set_split_freq  (RIG *rig, vfo_t vfo, freq_t tx_freq)  
  int     rig_set_split_mode  (RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)  
  int     rig_set_trn  (RIG *rig, int trn)  
  int     rig_set_ts  (RIG *rig, vfo_t vfo, shortfreq_t ts)  
  int     rig_set_vfo  (RIG *rig, vfo_t vfo)  
  int     rig_set_xit  (RIG *rig, vfo_t vfo, shortfreq_t xit)  
  int     rig_setting2idx  (setting_t s)  
  int     rig_token_foreach  (RIG *rig, int (*cfunc)(const struct confparams *, void* ), void*  data)  
  token_t     rig_token_lookup  (RIG *rig, const char *name)  
  int     rig_unregister  (rig_model_t rig_model)  
  int     rig_vfo_op  (RIG *rig, vfo_t vfo, vfo_op_t op)  
  const char *     rigerror  (int errnum)  


=head1 AUTHOR

A. U. Thor, E<lt>a.u.thor@a.galaxy.far.far.awayE<gt>

=head1 SEE ALSO

L<perl>.

=cut
