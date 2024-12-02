Works on Windows and Linux

Requires you get dotnet installed of course

For Windows install Visual Studio or such to get dotnet

On Ubuntu 21.04 it was this
apt install snap
apt install mono-complete
snap install dotnet-sdk --classic --channel=6.0
snap alias dotnet-sdk.dotnet dotnet
snap install dotnet-runtime-60 --classic
snap alias dotnet-runtime-60.dotnet dotnet
export DOTNET_ROOT=/snap/dotnet-sdk/current

Once dotnet is OK

dotnet build

You should then be able to run

./bin/Debug/net6.0/multicast

======================================================
Waiting for Net 7.0/8.0 to be in Ubuntu main packages
Following did not work

sudo apt remove 'dotnet*' 'aspnet*' 'netstandard*'
touch /etc/apt/preferences
// add to preferences
Package: dotnet* aspnet* netstandard*
Pin: origin "archive.ubuntu.com"
Pin-Priority: -10
// end preferences
snap remove dotnet-sdk
snap install dotnet-sdk --classic --channel=7.0
snap alias dotnet-runtime-70.dotnet dotnet

