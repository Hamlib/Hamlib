unit TestForm;

interface

uses
  SysUtils, Types, Classes, Variants, QGraphics, QControls, QForms, QDialogs,
  QStdCtrls, QExtCtrls, QComCtrls,
  hamlib_rigapi, hamlib_rotapi, HamlibComponents,
  HamlibSelectionDlg, HamlibRadioForm;

type
  THamlibTestForm = class(TForm)
    Panel1: TPanel;
    Memo1: TMemo;
    Panel2: TPanel;
    RigButton: TButton;
    RotatorButton: TButton;
    APIcheckButton: TButton;
    AboutLabel: TLabel;
    procedure RigButtonClick(Sender: TObject);
    procedure RotatorButtonClick(Sender: TObject);
    procedure APIcheckButtonClick(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  HamlibTestForm: THamlibTestForm;

implementation

{$R *.xfm}

procedure THamlibTestForm.RigButtonClick(Sender: TObject);
begin
    ModelSelectionDlg.ModelNr := 6666;  { Default value }
    if ModelSelectionDlg.ExecuteRig
    then begin
         try
            Memo1.Lines.Add('Selected model number = '
                         + IntToStr(ModelSelectionDlg.ModelNr));

            RadioForm.Execute(ModelSelectionDlg.ModelNr);
         except
            on e: ERigException do
                begin
                rig_debug(RIG_DEBUG_ERR, 'Error while using rig...'+chr($a));
                memo1.Lines.add('Error while using rig...');
                ShowException(e, exceptaddr);
                end;
         end; {try}
         end;
end;

procedure THamlibTestForm.RotatorButtonClick(Sender: TObject);
begin
    ModelSelectionDlg.ModelNr := 1;     { Default value }
    if ModelSelectionDlg.ExecuteRotator
    then begin
         Memo1.Lines.Add('Selected Rotator model number = '
                         + inttostr(ModelSelectionDlg.ModelNr));
         ShowMessage('Not implemented yet');
         end;
end;

procedure THamlibTestForm.APIcheckButtonClick(Sender: TObject);
begin
    {* Test conducted on the API *}
    rig_check_types;
    rigapi_check_types;
end;

end.
