unit hamlib_rotapi;

interface

Uses
    hamlib_rigapi;

{$MINENUMSIZE 4}
{* The $MINENUMSIZE directive controls the
 * minimum storage size of enumerated types. *}

{$UNDEF HAVE_MAGIC_NUMBERS}

{$IFDEF HAVE_MAGIC_NUMBERS}
const
    ROT_MAGIC           = $12348889;
    ROT_MAGIC_CAPS      = $12348899;
    ROT_MAGIC_STATE     = $12348999;
    ROT_MAGIC_CALLBACKS = $12349999;
{$ENDIF}

const
    ROT_FLAG_AZIMUTH   = (1 shl 1);
    ROT_FLAG_ELEVATION = (1 shl 2);
    ROT_TYPE_OTHER     = 0;

type
    elevation_t = float;
    azimuth_t = float;

    rot_model_t = integer;
    rot_reset_t = integer;

    PRot = ^TRot;   // forward decleration

    rot_caps = packed record
        rot_model : rot_model_t;
        model_name : PChar;
        mfg_name : PChar;
        version : PChar;
        copyright : PChar;
        status : rig_status_e;

        rot_type : integer;
        port_type : rig_port_e;

        serial_rate_min : integer;
        serial_rate_max : integer;
        serial_stop_bits : integer;
        serial_parity : serial_parity_e;
        serial_handshake_e : serial_handshake_e;

        write_delay : integer;
        post_write_delay : integer;
        timeout : integer;
        retry : integer;

        {*
         * Movement range, az is relative to North
         * negative values allowed for overlap
         *}
        min_az : azimuth_t;
        max_az : azimuth_t;
        min_el : elevation_t;
        max_el : elevation_t;

        cfgparams : PConfparms;
        priv : rig_ptr_t;

        {*
         * Rot Admin API
         *
         *}

        rot_init : function(rot: PRot): integer; cdecl;
        rot_cleanup : function(rot: PRot): integer; cdecl;
        rot_open : function(rot: PRot): integer; cdecl;
        rot_close : function(rot: PRot): integer; cdecl;

        set_conf : function(rot: PRot; token: token_t; val : PChar): integer; cdecl;
        get_conf : function(rot: PRot; token: token_t; val : PChar): integer; cdecl;

        {*
         *  General API commands, from most primitive to least.. :()
         *  List Set/Get functions pairs
         *}
        set_position : function(rot: PRot; az: azimuth_t; el: elevation_t): integer; cdecl;
        get_position : function(rot: PRot; var az: azimuth_t; var el: elevation_t): integer; cdecl;

        stop : function(rot: PRot): integer; cdecl;
        park : function(rot: PRot): integer; cdecl;

        reset : function(rot: PRot; reset: rot_reset_t): integer; cdecl;
        move : function(rot: PRot; direction: integer; speed: integer): integer; cdecl;
       
        {* get firmware info, etc. *}
        get_info : function(rot: PRot): PChar; cdecl;

        {* more to come... *}
{$IFDEF HAVE_MAGIC_NUMBERS}
        magic : cardinal;
{$ENDIF}
    end;
    TRotCaps = rot_caps;
    PRotCaps = ^TRotCaps;

    {*
     * Rotator state
     *
     * This struct contains live data, as well as a copy of capability fields
     * that may be updated (ie. customized)
     *
     * It is fine to move fields around, as this kind of struct should
     * not be initialized like caps are.
     *}
    rot_state = packed record
    	{*
	     * overridable fields
    	 *}
        min_az : azimuth_t;
        max_az : azimuth_t;
        min_el : elevation_t;
        max_el : elevation_t;

	    {*
         * non overridable fields, internal use
    	 *}
        rotport : port_t;

        comm_state : integer;	{* opened or not *}
        {*
         * Pointer to private data
         *}
        priv : pointer;
        {*
         * internal use by hamlib++ & Kylix for event handling
         *}
        obj : pointer;

        {* etc... *}

{$IFDEF HAVE_MAGIC_NUMBERS}
        magic: cardinal;
{$ENDIF}
    end;
    TRotState = rot_state;
    PRotState = ^TRotState;

    rot_callbacks = packed record
        position_event : function(rot: PRot; azim: azimuth_t; elev: elevation_t): integer; cdecl;

        { more to come }

{$IFDEF HAVE_MAGIC_NUMBERS}
        magic: cardinal;
{$ENDIF}
    end;
    TRotCallbacks = rot_callbacks;
    PRotCallbacks = ^TRotCallbacks;

    {*
     * struct rot is the master data structure,
     * acting as a handle for the controlled rotator.
     *}
    rot = packed record
        caps : PRotCaps;
        state : TRotState;
        callbacks : TRotCallbacks;
{$IFDEF HAVE_MAGIC_NUMBERS}
        magic: cardinal;
{$ENDIF}
    end;
    TRot = rot;
    //PRot = ^TRot;  // declared above.

{* --------------- API function prototypes -----------------*}

function rot_init(rot_model: rot_model_t): PRot; cdecl;
function rot_open(rot: PRot): integer; cdecl;
function rot_close(rot: PRot): integer; cdecl;
function rot_cleanup(rot: PRot): integer; cdecl;

function rot_set_conf(rot: PRot; token: token_t; val : PChar): integer; cdecl;
function rot_get_conf(rot: PRot; token: token_t; val : PChar): integer; cdecl;

  {*
   *  General API commands, from most primitive to least.. )
   *  List Set/Get functions pairs
   *}

function rot_set_position(rot: PRot; az: azimuth_t; el: elevation_t): integer; cdecl;
function rot_get_position(rot: PRot; var az: azimuth_t; var el: elevation_t): integer; cdecl;

function rot_stop(rot: PRot): integer; cdecl;
function rot_park(rot: PRot): integer; cdecl;
function rot_reset(rot: PRot; reset: rot_reset_t): integer; cdecl;
function rot_move(rot: PRot; direction: integer; speed: integer): integer; cdecl;
function rot_set_position_at(rot: PRot; az: azimuth_t; el: elevation_t; when: longint): integer; cdecl;

function rot_get_info(rot: PRot): PChar; cdecl;

function rot_register(caps: PRotCaps): integer; cdecl;
function rot_unregister(rot_model: rot_model_t): integer; cdecl;

type __cfunc_1 = function(const _caps: TRotCaps; _data: pointer): integer; cdecl;
function rot_list_foreach(cfunc : __cfunc_1; data: pointer): integer; cdecl;

function rot_load_backend(be_name: PChar): integer; cdecl;
function rot_check_backend(rot_model: rot_model_t): integer; cdecl;
function rot_load_all_backends: integer; cdecl;
function rot_probe_all(p: port_t): rot_model_t; cdecl;

type __cfunc_2 = function(const _conf: TConfparms; _data: pointer): integer; cdecl;
function rot_token_foreach(rot : PRot; cfunc: __cfunc_2; data: pointer): integer; cdecl;

function rot_confparam_lookup(rot: PRot; name: PChar): PConfparms; cdecl;
function rot_token_lookup(rot: PRot; name: PChar): token_t; cdecl;

function rot_get_caps(rot_model: rot_model_t): PRotCaps; cdecl;

implementation

{* Note: hamlib_modulename is defined in hamlib_rigapi.pas *}

function rot_init;          external hamlib_modulename name 'rot_init';
function rot_open;          external hamlib_modulename name 'rot_open';
function rot_close;         external hamlib_modulename name 'rot_close';
function rot_cleanup;       external hamlib_modulename name 'rot_cleanup';
function rot_set_conf;      external hamlib_modulename name 'rot_set_conf';
function rot_get_conf;      external hamlib_modulename name 'rot_get_conf';
function rot_set_position;  external hamlib_modulename name 'rot_set_position';
function rot_get_position;  external hamlib_modulename name 'rot_get_position';
function rot_stop;          external hamlib_modulename name 'rot_stop';
function rot_park;          external hamlib_modulename name 'rot_park';
function rot_reset;         external hamlib_modulename name 'rot_reset';
function rot_move;          external hamlib_modulename name 'rot_move';
function rot_set_position_at; external hamlib_modulename name 'rot_set_position_at';
function rot_get_info;      external hamlib_modulename name 'rot_get_info';
function rot_register;      external hamlib_modulename name 'rot_register';
function rot_unregister;    external hamlib_modulename name 'rot_unregister';
function rot_list_foreach;  external hamlib_modulename name 'rot_list_foreach';
function rot_load_backend;  external hamlib_modulename name 'rot_load_backend';
function rot_check_backend; external hamlib_modulename name 'rot_check_backend';
function rot_load_all_backends; external hamlib_modulename name 'rot_load_all_backends';
function rot_probe_all;     external hamlib_modulename name 'rot_probe_all';
function rot_token_foreach; external hamlib_modulename name 'rot_token_foreach';
function rot_confparam_lookup;  external hamlib_modulename name 'rot_confparam_lookup';
function rot_token_lookup;  external hamlib_modulename name 'rot_token_lookup';
function rot_get_caps;      external hamlib_modulename name 'rot_get_caps';

end.

