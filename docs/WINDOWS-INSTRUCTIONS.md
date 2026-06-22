## **Windows Install**

IMPORTANT! This is experimental and not production ready. It's available mainly for other devs in case they want to help out or check it out.

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

The kew executable will likely be in c:\msys64\ucrt64\bin\kew.exe. You can double-click on it to run it, but it works better if you first start c:\msys64\ucrt64.exe and run it from that terminal emulator.

## TODO, Things that don't yet work:

-Animations (title animation, name scrolling and glimmering footer). Because they are too slow on windows.

-The Spectrum Visualizer is too slow, disable by pressing v a few times.

-.m4a files don't work. I haven't tried all formats but mp3 work so flacs should work as well.

-Chafa covers don't work, although chafa is installed and so on. So ASCII covers is the default in
Windows.

-Resizing sometimes crashes the app.

-Unicode characters are broken, sometimes files don't show up.
