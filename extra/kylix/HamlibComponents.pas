unit HamlibComponents;

interface

uses
  SysUtils, Types, Classes, QGraphics, QControls, QForms, QDialogs,
  hamlib_rigapi, hamlib_rotapi;


type
  TFrequencyEvent = procedure(Sender: TObject; vfo: vfo_t; Freq: freq_t) of object;
  TVfoEvent = procedure(Sender: TObject; vfo: vfo_t) of object;
  TModeEvent = procedure(Sender: TObject; vfo: vfo_t; mode: rmode_t) of object;

  TRigComponent = class(TComponent)
  private
    { Private declarations }
    FRig: PRig;
    FOnFrequency: TFrequencyEvent;
    FOnVfo : TVfoEvent;
    FOnMode : TModeEvent;
    function GetModel: integer;
    function GetModelName: string;
    function GetMfgName: string;
    function GetTrn: integer;
    procedure SetTrn(Value: integer);
  protected
    { Protected declarations }
  public
    { Public declarations }
    constructor Create(AOwner: TComponent); override;
    constructor CreateRig(AOwner: TComponent; AModel: integer);
    destructor Destroy; override;

    procedure OpenRig;
    procedure CloseRig;

    procedure SetTransceive(trn: integer);
    function GetFreq(vfo: vfo_t): freq_t;
    procedure SetFreq(vfo: vfo_t; freq: freq_t);

    function checkMagic: boolean;
  published
    { Published declarations }
    property Model: integer read GetModel;
    property ModelName: string read GetModelName;
    property MfgName: string read GetMfgName;

    property Transceive: integer read GetTrn write SetTrn default RIG_TRN_OFF;

    property OnFrequency: TFrequencyEvent read FOnFrequency write FOnFrequency;
    property OnVFO: TVfoEvent read FOnVfo write FOnVfo;
    property OnMode: TModeEvent read FOnMode write FOnMode;
  end;

type
  TRotatorComponent = class(TComponent)
  private
    { Private declarations }
    FRot: PRot;
    procedure SetMinAzim(const Value: float);
    procedure SetMaxAzim(const Value: float);
    procedure SetMinElev(const Value: float);
    procedure SetMaxElev(const Value: float);
    function GetMinAzim: float;
    function GetMaxAzim: float;
    function GetMinElev: float;
    function GetMaxElev: float;
    function GetModel: integer;
  protected
    { Protected declarations }
  public
    { Public declarations }
    constructor Create(AOwner : TComponent); override;
    constructor CreateRotator(AOwner : TComponent; AModel : Integer);
    destructor Destroy; override;

    procedure OpenRotator;
    procedure CloseRotator;

    procedure GetPosition(var Az: azimuth_t; var El: elevation_t);
    procedure SetPosition(var Az: azimuth_t; var El: elevation_t);
    procedure Stop;
    procedure Park;
    procedure Reset;

  published
    { Published declarations }
    property Model: integer read GetModel;
    property MinAzimuth: float read GetMinAzim write SetMinAzim;
    property MaxAzimuth: float read GetMaxAzim write SetMaxAzim;
    property MinElevation: float read GetMinElev write SetMinElev;
    property MaxElevation: float read GetMaxElev write SetMaxElev;
  end;

type
  ERigException = class(Exception);
  ERotatorException = class(Exception);

procedure Register;

implementation

procedure Register;
begin
  RegisterComponents('User', [TRigComponent, TRotatorComponent]);
end;

constructor TRotatorComponent.Create(AOwner: TComponent);
begin
    CreateRotator(AOwner, 1);
end;

constructor TRotatorComponent.CreateRotator(AOwner: TComponent; AModel: rot_model_t);
begin
    inherited Create(AOwner);

    {* Initialize the library for the spesified model *}
    FRot := rot_init(AModel);
    if (FRot = nil)
    then raise ERotatorException.Create('Rig initialization error');

    with FRot^ do
        begin
        state.obj := Self; { Pointer back to TRotatorComponent }
        //callbacks.position_event := @position_callback; // TODO: have to implement this
        end;
end;

destructor TRotatorComponent.Destroy;
begin
    rot_cleanup(FRot);
    FRot := nil;
    inherited Destroy;
end;

procedure TRotatorComponent.OpenRotator;
var
    retval: integer;
begin
    retval := rot_open(FRot);
    if (retval <> RIG_OK)
    then raise ERotatorException.Create('rot_open: ' + StrPas(rigerror(retval)));
end;

procedure TRotatorComponent.CloseRotator;
var
    retval: integer;
begin
    retval := rot_close(FRot);
    if (retval <> RIG_OK)
    then raise ERotatorException.Create('rot_close: ' + StrPas(rigerror(retval)));
end;

procedure TRotatorComponent.GetPosition;
var
    retval: integer;
begin
    retval := rot_get_position(FRot, az, el);
    if (retval <> RIG_OK)
    then raise ERotatorException.Create('rot_get_position: ' + StrPas(rigerror(retval)));
end;

procedure TRotatorComponent.SetPosition;
var
    retval: integer;
begin
    retval := rot_set_position(FRot, az, el);
    if (retval <> RIG_OK)
    then raise ERotatorException.Create('rot_set_position: ' + StrPas(rigerror(retval)));
end;

procedure TRotatorComponent.Stop;
var
    retval: integer;
begin
    retval := rot_stop(FRot);
    if (retval <> RIG_OK)
    then raise ERotatorException.Create('rot_stop: ' + StrPas(rigerror(retval)));
end;

procedure TRotatorComponent.Park;
var
    retval: integer;
begin
    retval := rot_park(FRot);
    if (retval <> RIG_OK)
    then raise ERotatorException.Create('rot_park: ' + StrPas(rigerror(retval)));
end;

procedure TRotatorComponent.Reset;
var
    retval: integer;
begin
    retval := rot_reset(FRot, 0);
    if (retval <> RIG_OK)
    then raise ERotatorException.Create('rot_reset: ' + StrPas(rigerror(retval)));
end;

function TRotatorComponent.GetModel: integer;
begin
    with FRot^ do
        result := Caps^.rot_model;
end;

function TRotatorComponent.GetMinAzim: float;
begin
    with FRot^ do
        result := State.min_az;
end;

function TRotatorComponent.GetMaxAzim: float;
begin
    with FRot^ do
        result := State.max_az;
end;

function TRotatorComponent.GetMinElev: float;
begin
    with FRot^ do
        result := State.min_el;
end;

function TRotatorComponent.GetMaxElev: float;
begin
    with FRot^ do
        result := State.max_el;
end;

procedure TRotatorComponent.SetMinAzim(const Value: float);
begin
    with FRot^ do
        State.min_az := Value;
end;

procedure TRotatorComponent.SetMaxAzim(const Value: float);
begin
    with FRot^ do
        State.max_az := Value;
end;

procedure TRotatorComponent.SetMinElev(const Value: float);
begin
    with FRot^ do
        State.min_el := Value;
end;

procedure TRotatorComponent.SetMaxElev(const Value: float);
begin
    with FRot^ do
        State.max_el := Value;
end;

(*
 *
 *
 *
 *
 *
 *
 *
 *
 * TRigComponent
 *
 *)

function freq_callback(rig: PRig; vfo: vfo_t; freq: freq_t): integer; cdecl;
var
    component: TRigComponent;
begin
    rig_debug(RIG_DEBUG_TRACE, 'Inside the callback: freq_callback...'+chr($a));
    component := rig^.state.obj;
    with component do
        begin
        if Assigned(FOnFrequency)
        then FOnFrequency(component, vfo, freq);
        end;
    result := 1;
end;

{* These callback still have to be implemented 
 *
function mode_callback(rig: PRig; vfo: vfo_t; mode: rmode; width: pbwidth_t): integer; cdecl;
function vfo_callback(rig: PRig; vfo: vfo_t; freq_t: freq_t): integer; cdecl;
function ptt_callback(rig: PRig; mode: ptt_t): integer; cdecl;
function dcd_callback(rig: PRig; mode: dcd_t): integer; cdecl;
 *
 *}

constructor TRigComponent.CreateRig(AOwner: TComponent; AModel: integer);
begin
    inherited Create(AOwner);

    {* Initialize the library for the spesified model *}
    FRig := rig_init(AModel);
    if (FRig = nil)
    then raise ERigException.Create('Rig initialization error');

    with FRig^ do
        begin
        state.obj := Self; { Pointer back to TRigComponent }
        callbacks.freq_event := @freq_callback;
        end;
end;

constructor TRigComponent.Create(AOwner: TComponent);
begin
    CreateRig(AOwner, 1);   { Use Dummy by default }
end;


destructor TRigComponent.Destroy;
begin
    rig_cleanup(FRig);
    FRig := nil;
    inherited Destroy;
end;

{*
 * Because FRig is initialized with constructor and cleared with the
 * destructor, all methods will assume that FRig != NIL during the life
 * of the object.
 *}

procedure TRigComponent.OpenRig;
var
    retval: integer;
begin
    retval := rig_open(FRig);
    if (retval <> RIG_OK)
    then raise ERigException.Create('rig_open: ' + StrPas(rigerror(retval)));
end;

procedure TRigComponent.CloseRig;
var
    retval: integer;
begin
    retval := rig_close(FRig);
    if (retval <> RIG_OK)
    then raise ERigException.Create('rig_close: ' + StrPas(rigerror(retval)));
end;

function TRigComponent.GetModel: integer;
begin
    result := FRig^.caps^.rig_model;
end;

function TRigComponent.GetModelName: string;
begin
    result := StrPas(FRig^.caps^.model_name);
end;

function TRigComponent.GetMfgName: string;
begin
    result := StrPas(FRig^.caps^.mfg_name);
end;

function TRigComponent.GetTrn: integer;
var
    retval : integer;
    trn : integer;
begin
    retval := rig_get_trn(FRig, trn);
    if (retval <> RIG_OK)
    then raise ERigException.Create('rig_get_trn: ' + StrPas(rigerror(retval)));
    result := trn;
end;

procedure TRigComponent.SetTrn(Value: integer);
var
    retval: integer;
begin
    retval := rig_set_trn(FRig, Value);
    if (retval <> RIG_OK) and (retval <> -RIG_ENAVAIL)
    then raise ERigException.Create('rig_set_trn: ' + StrPas(rigerror(retval)));
end;

function TRigComponent.GetFreq(vfo: vfo_t): freq_t;
var
    freq: freq_t;
    retval: integer;
begin
    freq := 0;

    retval := rig_get_freq(FRig, vfo, freq);
    if (retval <> RIG_OK)
    then raise ERigException.Create('rig_get_freq: ' + StrPas(rigerror(retval)));

    result := freq;
end;

procedure TRigComponent.SetFreq(vfo: vfo_t; freq: freq_t);
var
    retval: integer;
begin
    vfo:=1;
    writeln('vfo=',vfo,' freq=',freq);
    retval := rig_set_freq(FRig, vfo, freq);
    if (retval <> RIG_OK)
    then
    begin
       writeln('Return=',retval);
       raise ERigException.Create('rig_set_freq: ' + StrPas(rigerror(retval)));
       end;
end;

procedure TRigComponent.SetTransceive(trn: integer);
var
    retval: integer;
begin
    retval := rig_set_trn(FRig, trn);
    if (retval <> RIG_OK)
    then raise ERigException.Create('rig_set_trn: ' + StrPas(rigerror(retval)));

    { Note: This will cause a SIGIO interruption.  The application will handle
            the SIGIO.  Please disable SIGIO handling in the debugger, else the
            debugger will interrupt the program.
      Menu: Tools|Debugger Options|Signals
    }
end;

function TRigComponent.checkMagic: boolean;
begin
{$IFDEF HAVE_MAGIC_NUMBERS}
    result := rigapi_check_magic(FRig) = 0;
{$ELSE}
    result := true;
{$ENDIF}
end;


end.
