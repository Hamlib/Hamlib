This is a test of creating a portable Hamlib GUI controller using dotnet and GTK
Should be able to compile on Windows, Linux, and MacOS
No guarantee this will go anywhere depending on ability to talk to Hamlib via C#

On Windows
dotnet new install GtkSharp.Template.CSharp
dotnet build




On Ubuntu I don't see the GtkSharp package -- but you can build the code generated on the Windows side.
apt install dotnet-sdk-6.0
dotnet build


Note: On Windows you can create a skeleton GTK app 
dotnet new gtkapp
