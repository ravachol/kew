##

## **Windows Install**

1. Download a fresh installer from https://www.msys2.org and install it

2. Open the MSYS2 UCRT64 terminal (important — not MinGW64, not the base MSYS2).

3. Run these commands:

```sh
pacman -Syu

pacman -S --noconfirm --needed mingw-w64-ucrt-x86_64-pkg-config mingw-w64-ucrt-x86_64-faad2 mingw-w64-ucrt-x86_64-taglib mingw-w64-ucrt-x86_64-fftw mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-chafa mingw-w64-ucrt-x86_64-glib2 mingw-w64-ucrt-x86_64-opus mingw-w64-ucrt-x86_64-opusfile mingw-w64-ucrt-x86_64-libvorbis mingw-w64-ucrt-x86_64-libogg git

git clone https://codeberg.org/ravachol/kew.git

cd kew

mingw32-make -j

mingw32-make install PREFIX=/ucrt64

kew
```

## Windows Terminal profile

Add this to your Windows Terminal. Choose Settings, then at the bottom "Open JSON file".

Add this in the part under profiles -> lists:

```
{
    "name": "bash",
    "commandline": "C:\\msys64\\usr\\bin\\bash.exe --login",
    "startingDirectory": "C:\\msys64\\home\\YOUR_USERNAME",
    "environment": {
        "CHERE_INVOKING": "1",
        "MSYSTEM": "UCRT64"
    }
},
```

## Add a shortcut to your desktop

Right click desktop, Choose new, shortcut

Put this in the location:
wt.exe -p "kew UCRT64" C:\msys64\usr\bin\bash.exe -li -c "kew"

Use the .ico in the kew folder for the icon.

## Location of the executable

The kew executable will likely be in c:\msys64\ucrt64\bin\kew.exe. You can double-click on it to run it, but it works better if you first start c:\msys64\ucrt64.exe and run it from that terminal emulator. The Windows Terminal in Windows 11 also works.

## Location of settings

Settings will likely be in c:\users\YOUR_USERNAME\AppData\Roaming\kew.

## Things that are different

Immediate play (Enqueue and play) is Ctrl+g instead of Ctrl+Enter.
