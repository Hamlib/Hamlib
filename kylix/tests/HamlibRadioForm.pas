unit HamlibRadioForm;

interface

uses
  SysUtils, Types, Classes, Variants, QGraphics, QControls, QForms, QDialogs,
  QStdCtrls, QExtCtrls,
  HamlibComponents, hamlib_rigapi;

type
  TRadioForm = class(TForm)
    FreqPanel_vfoa: TPanel;
    FreqEdit_vfoa: TEdit;
    FreqLabel_vfoa: TLabel;
    Label2: TLabel;
    Panel1: TPanel;
    FreqLabel_vfob: TLabel;
    FreqEdit_vfob: TEdit;
    MHzLabel_vfob: TLabel;
    CloseButton: TButton;
    MagicCheckButton: TButton;
    FreqUpButton_vfoA: TButton;
    FreqDnButton_vfoa: TButton;
    FreqUpButton_vfob: TButton;
    FreqDnButton_vfob: TButton;
    procedure MagicCheckButtonClick(Sender: TObject);
    procedure FreqButton_Click(Sender: TObject);
  private
    { Private declarations }
    FRigComponent: TRigComponent;
    procedure RadioFreqCallback(Sender: TObject; vfo: vfo_t; Freq: freq_t);
  public
    { Public declarations }
    function Execute(model: integer): integer;
  end;

var
  RadioForm: TRadioForm;

implementation

{$R *.xfm}

procedure TRadioForm.RadioFreqCallback(Sender: TObject; vfo: Integer; Freq: Int64);
var
    edit: TEdit;
begin
    if (vfo = RIG_VFO_A)
    then edit := FreqEdit_vfoa
    else edit := FreqEdit_vfob;
    edit.Text := Format('%.6f', [freq / 1.0e6]);
end;

function TRadioForm.Execute(model: integer): integer;
begin
    result := 0;
    try
        FRigComponent := TRigComponent.CreateRig(Self, model);
        FRigComponent.OnFrequency := RadioFreqCallback;
        FRigComponent.OpenRig;
        FRigComponent.Transceive := RIG_TRN_RIG;

        rig_debug(RIG_DEBUG_TRACE, 'This is where the window is shown...'+chr($a));
        result := ShowModal;
        rig_debug(RIG_DEBUG_TRACE, 'Finished. Window is closed.'+chr($a));

        FRigComponent.CloseRig;
        FRigComponent.Free;
    except
        on e: ERigException do
            begin
            ShowMessage('Error occured while using the rig: %s', [e.Message]);
            FRigComponent.CloseRig;
            FRigComponent.Free;
            end;
    end;
end;

procedure TRadioForm.FreqButton_Click(Sender: TObject);
var
    newfreq : freq_t;
    deffreq: float;
    delta: freq_t;
    vfo: vfo_t;
    edit: TEdit;
begin
    deffreq := 145.825;

    if (Sender = FreqUpButton_vfoA) or (Sender = FreqDnButton_vfoA)
    then begin
         vfo := RIG_VFO_A;
         edit := FreqEdit_vfoa;
         end
    else begin
         vfo := RIG_VFO_B;
         edit := FreqEdit_vfob;
         end;

    if (Sender = FreqUpButton_vfoA) or (Sender = FreqUpButton_vfoB)
    then delta := +5000  { Hz }
    else delta := -5000; { Hz }

    newfreq := trunc( 1.0e6 * StrToFloatDef(edit.Text, deffreq));
    newfreq := newfreq + delta;
    try
        FRigComponent.SetFreq(vfo, newfreq);
        edit.text := Format('%.6f', [newfreq / 1.0e6]);
    except
        on e: ERigException do
            ShowMessage('Unable to set rig');
    end;
end;

procedure TRadioForm.MagicCheckButtonClick(Sender: TObject);
begin
    if FRigComponent.checkMagic
    then ShowMessage('Magic check succeded.')
    else ShowMessage('Magic check FAILED!!!');
end;


end.

