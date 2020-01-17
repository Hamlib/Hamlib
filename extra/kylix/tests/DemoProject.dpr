program DemoProject;

uses
  QForms,
  hamlib_rigapi in '../hamlib_rigapi.pas',
  hamlib_rotapi in '../hamlib_rotapi.pas',
  HamlibComponents in '../HamlibComponents.pas',
  TestForm in 'TestForm.pas' {HamlibTestForm},  
  HamlibSelectionDlg in 'HamlibSelectionDlg.pas' {ModelSelectionDlg},
  HamlibRadioForm in 'HamlibRadioForm.pas' {RadioForm};

{$R *.res}

begin
  Application.Initialize;
  Application.Title := 'Hamlib Test Program';
  Application.CreateForm(THamlibTestForm, HamlibTestForm);
  Application.CreateForm(TModelSelectionDlg, ModelSelectionDlg);
  Application.CreateForm(TRadioForm, RadioForm);
  Application.Run;
end.
