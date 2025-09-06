/* Unsupported Hamlib's methods, properties and constants */

// remove defines from Hamlib class
%rename("$ignore", regexmatch$name="^_") "";
%rename("$ignore", regexmatch$name="^AMP_") "";
%rename("$ignore", regexmatch$name="^RIG_") "";
%rename("$ignore", regexmatch$name="^ROT_") "";
%rename("$ignore", regexmatch$name="deprecated") "";

// re-add useful defines

// Amplifiers
%rename("%s", regexmatch$name="^AMP_FLAG_") "";
%rename("%s", regexmatch$name="^AMP_LEVEL_") "";
%rename("%s", regexmatch$name="^AMP_MODEL_") "";
%rename("%s", regexmatch$name="^AMP_RESET_") "";
%rename("%s", regexmatch$name="^AMP_TYPE_") "";

// Rigs
%rename("%s", regexmatch$name="^RIG_AGC_") "";
%rename("%s", regexmatch$name="^RIG_ANN_") "";
%rename("%s", regexmatch$name="^RIG_ANT_") "";
%rename("%s", regexmatch$name="^RIG_BANDSELECT_") "";
%rename("%s", regexmatch$name="^RIG_BAND_") "";
%rename("%s") RIG_BUSBUSY;
%rename("%s") RIG_BUSERROR;
%rename("%s", regexmatch$name="^RIG_CAPS_") "";
%rename("%s", regexmatch$name="^RIG_CHFLAG_") "";
%rename("%s", regexmatch$name="^RIG_CLIENT_") "";
%rename("%s", regexmatch$name="^RIG_COMM_") "";
%rename("%s", regexmatch$name="^RIG_CONF_") "";
%rename("%s", regexmatch$name="^RIG_COOKIE_") "";
%rename("%s", regexmatch$name="^RIG_DCD_") "";
%rename("%s", regexmatch$name="^RIG_DEBUG_") "";
%rename("%s", regexmatch$name="^RIG_E") "";
%rename("%s", regexmatch$name="^RIG_FLAG_") "";
%rename("%s", regexmatch$name="^RIG_FLT_") "";
%rename("%s", regexmatch$name="^RIG_FUNCTION_") "";
%rename("%s", regexmatch$name="^RIG_FUNC_") "";
%rename("%s", regexmatch$name="^RIG_HANDSHAKE_") "";
%rename("%s", regexmatch$name="^RIG_KEYERTYPE_") "";
%rename("%s", regexmatch$name="^RIG_LEVEL_") "";
%rename("%s", regexmatch$name="^RIG_MEM_CAPS_") "";
%rename("%s", regexmatch$name="^RIG_METER_") "";
%rename("%s", regexmatch$name="^RIG_MODEL_") "";
%rename("%s", regexmatch$name="^RIG_MODE_") "";
%rename("%s", regexmatch$name="^RIG_MTYPE_") "";
%rename("%s", regexmatch$name="^RIG_MULTICAST_") "";
%rename("%s", regexmatch$name="^RIG_OP_") "";
%rename("%s") RIG_OK;
%rename("%s", regexmatch$name="^RIG_PARITY_") "";
%rename("%s", regexmatch$name="^RIG_PARM_") "";
%rename("%s", regexmatch$name="^RIG_PORT_") "";
%rename("%s", regexmatch$name="^RIG_POWER_") "";
%rename("%s", regexmatch$name="^RIG_PTRX_") "";
%rename("%s", regexmatch$name="^RIG_PTT_") "";
%rename("%s", regexmatch$name="^RIG_RESET_") "";
%rename("%s", regexmatch$name="^RIG_RPT_SHIFT_") "";
%rename("%s", regexmatch$name="^RIG_SCAN_") "";
%rename("%s", regexmatch$name="^RIG_SETTING_") "";
%rename("%s", regexmatch$name="^RIG_SIGNAL_") "";
%rename("%s", regexmatch$name="^RIG_SPECTRUM_MODE_") "";
%rename("%s", regexmatch$name="^RIG_SPLIT_") "";
%rename("%s", regexmatch$name="^RIG_STATUS_") "";
%rename("%s", regexmatch$name="^RIG_TARGETABLE_") "";
%rename("%s", regexmatch$name="^RIG_TRN_") "";
%rename("%s", regexmatch$name="^RIG_TS_") "";
%rename("%s", regexmatch$name="^RIG_TYPE_") "";
%rename("%s", regexmatch$name="^RIG_VFO_") "";
%ignore RIG_EK; // an internal macro
%ignore RIG_ELAD; // an internal macro

// Rotators

%rename("%s", regexmatch$name="^ROT_FLAG_") "";
%rename("%s", regexmatch$name="^ROT_FUNC_") "";
%rename("%s", regexmatch$name="^ROT_LEVEL_") "";
%rename("%s", regexmatch$name="^ROT_MODEL_") "";
%rename("%s", regexmatch$name="^ROT_MOVE_") "";
%rename("%s", regexmatch$name="^ROT_PARM_") "";
%rename("%s", regexmatch$name="^ROT_RESET_") "";
%rename("%s", regexmatch$name="^ROT_SPEED_") "";
%rename("%s", regexmatch$name="^ROT_STATUS_") "";
%rename("%s", regexmatch$name="^ROT_TYPE_") "";

// remove methods from Hamlib class
%rename("$ignore", regexmatch$name="^amp_", %$isfunction) "";
%rename("$ignore", regexmatch$name="^rig_", %$isfunction) "";
%rename("$ignore", regexmatch$name="^rot_", %$isfunction) "";

// re-add some methods
%rename("%s") rig_get_debug;
%rename("%s") rig_set_debug;
%rename("%s") rig_set_debug_time_stamp;
%rename("%s", regexmatch$name="^rig_str", %$isfunction) "";

// remove typedefs
%ignore s_rig;
%ignore s_rot;

%ignore amp;
%ignore debugmsgsave;
%ignore debugmsgsave2;
%ignore debugmsgsave3;
%ignore hamlib_copyright2;
%ignore hamlib_version2;
%ignore macro_name;
%ignore fd_sync_write;
%ignore fd_sync_read;
%ignore fd_sync_error_write;
%ignore fd_sync_error_read;

%ignore FIFO_RIG;
%ignore FREQFMT;
%ignore PRIfreq;
%ignore SCNfreq;

// others
%rename("$ignore", regexmatch$name="python_callbacks") ""; // internal structs and methods used by bindings
%ignore rig_state::cache; // deprecated
%ignore rig_set_trn; // deprecated
%ignore rig_get_trn; // deprecated

#ifdef SWIGLUA
%ignore Rig::set_level(setting_t,int,vfo_t);
%ignore Rig::set_ext_level(setting_t,value_t,vfo_t);
%ignore Rig::set_level(char const *,int,vfo_t);
%ignore Rig::set_parm(setting_t,int);
%ignore Rig::set_parm(char const *,int);
#endif

