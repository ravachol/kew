## How to install

## Linux and macOS Packages

Install through your package manager or homebrew (macOS). If you can't find it on your distro, or you want the bleeding edge, follow the [Manual Installation Instructions](#Manual-Installation-Instructions).

For NixOS, there is an official package but also a [Nix Flake](flake.nix) that is tied to the latest commit.

For Artix it's available in the [artist repository](https://wiki.artixlinux.org/Main/Repositories#artist_repository).

For Fedora there is an [unofficial repo](https://copr.fedorainfracloud.org/coprs/fed500/kew).

## Manually Installing kew

kew dependencies are:

* FFTW
* Chafa
* libopus
* opusfile
* libvorbis
* TagLib
* faad2 (optional)
* libogg
* pkg-config
* glib2.0

Install the necessary dependencies using your distro's package manager and then install kew. Below are some examples.

<details>
<summary>Debian/Ubuntu</summary>

Install dependencies:

```bash
sudo apt install -y pkg-config libfaad-dev libtag1-dev libfftw3-dev libopus-dev libopusfile-dev libvorbis-dev libogg-dev git gcc make libchafa-dev libglib2.0-dev libgdk-pixbuf-2.0-dev
```

[Install kew](#install-kew)
</details>

<details>
<summary>Arch Linux</summary>

Install dependencies:

```bash
sudo pacman -Syu --noconfirm --needed pkg-config faad2 taglib fftw git gcc make chafa glib2 opus opusfile libvorbis libogg
```

[Install kew](#install-kew)
</details>

<details>
<summary>Android</summary>

Follow the [Android instructions](#android).

</details>

<details>
<summary>macOS</summary>

Install git:

```bash
xcode-select --install
```

Install dependencies:

```bash
brew install gettext faad2 taglib chafa fftw opus opusfile libvorbis libogg glib pkg-config make
```
Notes for mac users:
1) A sixel-capable terminal like kitty or WezTerm is recommended for macOS.
2) The visualizer and album colors are disabled by default on macOS, because the default terminal doesn't handle them too well. To enable press v and i respectively.

[Install kew](#install-kew)
</details>

<details>
<summary>Fedora</summary>

Install dependencies:

```bash
sudo dnf install -y pkg-config taglib-devel fftw-devel opus-devel opusfile-devel libvorbis-devel libogg-devel git gcc make chafa-devel libatomic gcc-c++ glib2-devel
```
Option: add faad2-devel for AAC, M4A support.

```bash
sudo dnf install faad2-devel faad2
```

[Install kew manually](#install-kew)/[Build an RPM package](#fedora-rpm-instructions)
</details>

<details>
<summary>OpenSUSE</summary>

Install dependencies:

```bash
sudo zypper install -y pkgconf taglib fftw3-devel opusfile-devel libvorbis-devel libogg-devel git chafa-devel gcc make glib2-devel faad2 faad2-devel gcc-c++ libtag-devel
```

[Install kew](#install-kew)
</details>

<details>
<summary>CentOS/Red Hat</summary>

Install dependencies:

```bash
sudo dnf config-manager --set-enabled crb

sudo dnf install -y pkgconfig taglib taglib-devel fftw-devel opus-devel opusfile-devel libvorbis-devel libogg-devel git gcc make chafa-devel glib2-devel gcc-c++
```

Option: add faad2-devel for AAC,M4A support (Requires RPM-fusion to be enabled).

Enable RPM Fusion Free repository:

```bash
sudo dnf install https://download1.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm
```

Install faad2:

```bash
sudo dnf install faad2-devel
```

[Install kew manually](#install-kew)/[Build an RPM package](#fedora-rpm-instructions)
</details>

<details>
<summary>Void Linux</summary>

Install dependencies:

```bash
sudo xbps-install -y pkg-config faad2 taglib taglib-devel fftw-devel git gcc make chafa chafa-devel opus opusfile opusfile-devel libvorbis-devel libogg glib-devel
```

[Install kew](#install-kew)
</details>

<details>
<summary>Alpine Linux</summary>

Install dependencies:

```bash
sudo apk add pkgconfig faad2-dev taglib-dev fftw-dev opus-dev opusfile-dev libvorbis-dev libogg-dev git build-base chafa-dev glib-dev
```

[Install kew](#install-kew)

</details>

<details>
<summary>Windows (WSL)</summary>

1) Install Windows Subsystem for Linux (WSL).

2) Install kew using the instructions for Ubuntu.

3) If you are running Windows 11, Pulseaudio should work out of the box, but if you are running Windows 10, use the instructions below for installing PulseAudio:
https://www.reddit.com/r/bashonubuntuonwindows/comments/hrn1lz/wsl_sound_through_pulseaudio_solved/

4) To install Pulseaudio as a service on Windows 10, follow the instructions at the bottom in this guide: https://www.linuxuprising.com/2021/03/how-to-get-sound-pulseaudio-to-work-on.html

</details>

#### Install kew

Download the latest release (recommended) or, if you are feeling adventurous, clone from the latest in main:

```bash
git clone https://codeberg.org/ravachol/kew.git
```
```bash
cd kew
```

Then run:

```bash
make -j4
```

```bash
sudo make install
```

### Uninstalling

If you installed kew manually, simply run:

```bash
sudo make uninstall
```

If you want, you can also delete the settings files:

Linux: ~/.config/kew/
macOS: ~/Library/Preferences/kew/

Then delete the kewstaterc file:

Linux: ~/.config/kew/kewstaterc
macOS: ~/Library/Preferences/kew/kewstaterc

#### Faad2 is optional

By default, the build system will automatically detect if `faad2` is available and includes it if found.

## Android

### **Basic Installation Requirements :**

To run kew on Android please install the following applications :

- **Termux** : A terminal emulator for Android that allows you to run Linux commands on your device.

  [![Download Termux](https://img.shields.io/badge/Download-Termux-brightgreen?style=for-the-badge&logo=android)](https://github.com/termux/termux-app/releases/) - click to go to downloads page

- **Termux-Api** : A plugin for Termux that executes Termux-api package commands.

  [![Download Termux-Api](https://img.shields.io/badge/Download-Termux--API-blue?style=for-the-badge&logo=android)](https://github.com/termux/termux-api/releases/download/v0.53.0/termux-api-app_v0.53.0+github.debug.apk) - click to download

### **Termux Setup:**

1. **Update and install dependencies**
```sh
pkg install tur-repo -y && yes | pkg upgrade -y && pkg install clang pkg-config taglib fftw git make chafa glib libopus opusfile libvorbis libogg dbus termux-api
```

2. **Make sure termux has sound:**

On your phone, go to Settings -> Sound and Vibration -> Volume and make sure the level for Media is not 0.

<details>
<summary><b>Building Faad2 from source (needed to run .m4a files)</b></summary>

```sh
pkg install cmake make clang
git clone https://github.com/knik0/faad2
cd faad2
cmake -DCMAKE_EXE_LINKER_FLAGS="-lm" . -D CMAKE_INSTALL_PREFIX=/data/data/com.termux/files/usr
make install
```

</details>

3. **Enable storage permissions**
```sh
termux-setup-storage
```
Tap allow for the setup to finish

### **Compiling Kew:**

```sh
git clone https://codeberg.org/ravachol/kew.git
cd kew
make -j4
make install
```

This step is all you will need to do later to update the version.

### **Run kew:**

1. **Set kew's music library path**

This could be ~/storage/music for instance:
```
kew path <music path>
```

2. **Run kew**

```
kew
```

### To get volume buttons on your phone to work in Termux:

run
nano ~/.termux/termux.properties

Uncomment this line:

volume-keys = volume

Save (ctrl+x)
