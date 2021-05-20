Works on Windows and Linux

Requires you get dotnet installed of course

For Wiundows install Visual Studio or such to get dotnet

On Ubuntu 20.04 it was this
apt install snap
apt install mono-complete
snap install dotnet-sdk --classic --channel=5.0
snap alias dotnet-sdk.dotnet dotnet
snap install dotnet-runtime-50 --classic
snap alias dotnet-runtime-50.dotnet dotnet
export DOTNET_ROOT=/snap/dotnet-sdk/current

Once dotnet is OK

dotnet build

You should then be able to run

./bin/Debug/net5.0/multicast
