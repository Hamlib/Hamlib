/* unsupported Hamlib's calls */

/* useless macros */
%ignore AMP_DUMMY;
%ignore AMP_ELECRAFT;
%ignore AMP_GEMINI;
%ignore AMP_EXPERT;

%ignore RIG_ADAT;
%ignore RIG_ALINCO;
%ignore RIG_ANYTONE;
%ignore RIG_AOR;
%ignore RIG_BARRETT;
%ignore RIG_CODAN;
%ignore RIG_COMMRADIO;
%ignore RIG_DORJI;
%ignore RIG_DRAKE;
%ignore RIG_DUMMY;
%ignore RIG_EK;
%ignore RIG_ELAD;
%ignore RIG_FLEXRADIO;
%ignore RIG_GNURADIO;
%ignore RIG_GOMSPACE;
%ignore RIG_ICMARINE;
%ignore RIG_ICOM;
%ignore RIG_JRC;
%ignore RIG_KACHINA;
%ignore RIG_KENWOOD;
%ignore RIG_KIT;
%ignore RIG_LOWE;
%ignore RIG_MDS;
%ignore RIG_MICROTUNE;
%ignore RIG_MOTOROLA;
%ignore RIG_PCR;
%ignore RIG_PRM80;
%ignore RIG_RACAL;
%ignore RIG_RADIOSHACK;
%ignore RIG_RFT;
%ignore RIG_RS;
%ignore RIG_SKANTI;
%ignore RIG_TAPR;
%ignore RIG_TENTEC;
%ignore RIG_TUNER;
%ignore RIG_UNIDEN;
%ignore RIG_WINRADIO;
%ignore RIG_WJ;
%ignore RIG_YAESU;

%ignore ROT_AMSAT;
%ignore ROT_ANDROIDSENSOR;
%ignore ROT_APEX;
%ignore ROT_ARRAYSOLUTIONS;
%ignore ROT_ARS;
%ignore ROT_CELESTRON;
%ignore ROT_CNCTRK;
%ignore ROT_DUMMY;
%ignore ROT_EASYCOMM;
%ignore ROT_ETHER6;
%ignore ROT_FLIR;
%ignore ROT_FODTRACK;
%ignore ROT_GRBLTRK;
%ignore ROT_GS232A;
%ignore ROT_HEATHKIT;
%ignore ROT_INDI;
%ignore ROT_IOPTRON;
%ignore ROT_KIT;
%ignore ROT_M2;
%ignore ROT_MEADE;
%ignore ROT_PROSISTEL;
%ignore ROT_RADANT;
%ignore ROT_ROTOREZ;
%ignore ROT_SAEBRTRACK;
%ignore ROT_SARTEK;
%ignore ROT_SATEL;
%ignore ROT_SKYWATCHER;
%ignore ROT_SPID;
%ignore ROT_TS7400;

%ignore PRIfreq;
%ignore SCNfreq;
%ignore FREQFMT;

// remove defines from Hamlib class
%rename("$ignore", regexmatch$name="^_") "";
%rename("$ignore", regexmatch$name="^AMP_BACKEND") "";
%rename("$ignore", regexmatch$name="^RIG_BACKEND") "";
%rename("$ignore", regexmatch$name="^ROT_BACKEND") "";

// remove methods from Hamlib class
%rename("$ignore", regexmatch$name="^amp_", %$isfunction) "";
%rename("$ignore", regexmatch$name="^rig_", %$isfunction) "";
%rename("$ignore", regexmatch$name="^rot_", %$isfunction) "";
// re-add some methods
%rename("%s") rig_set_debug;
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

#ifdef SWIGLUA
%ignore Rig::set_level(setting_t,int,vfo_t);
%ignore Rig::set_ext_level(setting_t,value_t,vfo_t);
%ignore Rig::set_level(char const *,int,vfo_t);
%ignore Rig::set_parm(setting_t,int);
%ignore Rig::set_parm(char const *,int);
#endif

