unit HamlibSelectionDlg;

interface

uses
  SysUtils, Types, Classes, Variants, QGraphics, QControls, QForms, QDialogs,
  QStdCtrls, QComCtrls, QExtCtrls,
  HamlibComponents, hamlib_rigapi, hamlib_rotapi;

type
  TModelSelectionDlg = class(TForm)
    BtnPanel: TPanel;
    ListView: TListView;
    OkBtn: TButton;
    CancelBtn: TButton;
  private
    { Private declarations }
    FModelNr: integer;
  protected
    { Protected declarations }
  public
    { Public declarations }
    function ExecuteRig: Boolean;
    function ExecuteRotator: Boolean;
  published
    property ModelNr: integer read FModelNr write FModelNr default 1;
  end;

var
  ModelSelectionDlg: TModelSelectionDlg;

implementation

{$R *.xfm}

function rig_list_models(const caps : TRigCaps; data: pointer): integer; cdecl;
var
    listview: TListView;
    listitem: TListItem;
begin
    listview := data;
    with listview, caps do
      begin
      ListItem := Items.Add;
      ListItem.Caption := mfg_name;
      ListItem.SubItems.Add(model_name);
      ListItem.SubItems.Add(version);
      ListItem.Data := @caps;
      end;
    result := 1;
end;

function TModelSelectionDlg.ExecuteRig: Boolean;
var
  caps: PRigCaps;
  status: integer;
begin
    result := false;
    ListView.Items.Clear;

    rig_load_all_backends;

    status := rig_list_foreach(@rig_list_models, ListView);
    if (status <> RIG_OK )
    then raise ERigException.Create('RigError: '+ StrPas(RigError(status)));

    status := ShowModal;
    if (status = mrOK)
    then begin
         if (ListView.Selected <> nil)
         then begin
              caps := ListView.Selected.Data;
              Modelnr := caps^.rig_model;
              end;
         result := true;
         end;
end;

function rotator_list_models(const caps : TRotCaps; data: pointer): integer; cdecl;
var
    listview: TListView;
    listitem: TListItem;
begin
    listview := data;
    with listview, caps do
      begin
      ListItem := Items.Add;
      ListItem.Caption := mfg_name;
      ListItem.SubItems.Add(model_name);
      ListItem.SubItems.Add(version);
      ListItem.Data := @caps;
      end;
    result := 1;
end;

function TModelSelectionDlg.ExecuteRotator: Boolean;
var
  caps: PRotCaps;
  status: integer;
begin
    result := false;
    ListView.Items.Clear;

    rot_load_all_backends;

    status := rot_list_foreach(@rotator_list_models, ListView);
    if (status <> RIG_OK )
    then raise ERigException.Create('RotatorError: '+ StrPas(RigError(status)));

    status := ShowModal;
    if (status = mrOK)
    then begin
         if (ListView.Selected <> nil)
         then begin
              caps := ListView.Selected.Data;
              Modelnr := caps^.rot_model;
              end;
         result := true;
         end;
end;

end.
