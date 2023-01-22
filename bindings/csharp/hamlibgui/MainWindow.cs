using System;
using Gtk;
using UI = Gtk.Builder.ObjectAttribute;

namespace hamlibgui
{
class MainWindow : Window
{
    [UI] private Label _label1 = null;
    [UI] private Button _button1 = null;

    private bool connectFlag;

    public MainWindow() : this(new Builder("MainWindow.glade")) { }

    private MainWindow(Builder builder) : base(
            builder.GetRawOwnedObject("MainWindow"))
    {
        builder.Autoconnect(this);

        DeleteEvent += Window_DeleteEvent;
        _button1.Clicked += Button1_Clicked;
    }

    private void Window_DeleteEvent(object sender, DeleteEventArgs a)
    {
        Application.Quit();
    }

    private void Button1_Clicked(object sender, EventArgs a)
    {
        connectFlag = !connectFlag;

        if (connectFlag)
        {
            String mytext = "Rig Connected (not really)";
            _label1.Text = mytext;
            _button1.Label = "Disconnect Rig";
        }
        else
        {
            String mytext = "Rig not Connected ";
            _label1.Text = mytext;
            _button1.Label = "Connect to Rig";
        }
    }
}
}
