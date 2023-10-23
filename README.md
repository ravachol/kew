
# cue music command
[![cue-git](https://img.shields.io/aur/version/cue-git?color=1793d1&label=cue-git&logo=arch-linux&style=for-the-badge)](https://aur.archlinux.org/packages/cue-git/)
[![GitHub license](https://img.shields.io/github/license/ravachol/cue?color=333333&style=for-the-badge)](https://github.com/ravachol/cue/blob/master/LICENSE)

Listen to music in the terminal.

<div align="center">
    <img src="cue-screenshot.png" />
</div>

cue is a command-line music player for Linux.

## Features
 
 * Search a music library with partial titles.
 * Creates a playlist based on a matched directory. 
 * Display album covers as ASCII art or as a normal image.
 * Control the player with previous, next and pause.
 * Supports gapless playback and 24-bit audio.


## Reviews

cue was reviewed by Linuxlinks.com: 

https://www.linuxlinks.com/cue-command-line-music-player-gapless-playback/ 

And Tecmint.com: 

https://www.tecmint.com/command-line-music-players-for-linux/

Thank you!


## Installing

### Installing via AUR

On Arch Linux, and Arch-based distributions, cue can be found in the AUR. Install with pamac or an AUR helper like yay:

```bash
yay cue-git
```

That's it!


### Installing with quick install script

To quickly install cue, just copy and paste this to your terminal (if you have curl installed):

```bash
sudo bash -c "curl https://raw.githubusercontent.com/ravachol/cue/main/install.sh | bash"
```

Please note that this script might do a system update before installing cue.

### Installing everything manually

cue dependencies are:

* FFmpeg
* FFTW
* Chafa
* FreeImage
* glib2.0 and AVFormat. These should be installed with the others, if not install them.

Install FFmpeg, FFTW, Chafa and FreeImage using your distro's package manager. For instance:

```bash
apt install ffmpeg libfftw3-dev git libglib2.0-dev libchafa-dev libfreeimage-dev libavformat-dev
```
Or:

```bash
pacman -Syu ffmpeg fftw git glib2 chafa freeimage
```

Then run this (either git clone or unzip a release zip into a folder of your choice):

```bash
git clone https://github.com/ravachol/cue.git
```
```bash
cd cue
```
```bash
make
```
```bash
sudo make install
```

(Optional) Update the man db:

```bash
sudo mandb  # Update the man page database (may not be required on all systems)
sudo makewhatis /usr/local/share/man  # Update the man page index

```

A TrueColor capable terminal is recommended, like Konsole, kitty or st, to display colors properly.

For a complete list of capable terminals, see this page: [Colors in Terminal](https://gist.github.com/CMCDragonkai/146100155ecd79c7dac19a9e23e6a362) (github.com).

### Uninstalling

```bash
sudo make uninstall
```

## Usage

First thing to do is to tell cue the path to your music library (you only need to do this once):

```bash
cue path "/home/joe/Music/"
```
Now run cue and provide a partial name of a track or directory:

```bash
cue cure great
```

This command plays all songs from "The Cure Greatest Hits" directory, provided it's in your music library.

cue returns the first directory or file whose name matches the string you provide. It works best when your music library is organized in this way: artist folder->album folder(s)->track(s).

#### Some Examples:

 ```
cue (starting cue with no arguments plays all songs (up to 20 000) in your library, shuffled)

cue moonlight son (finds and plays moonlight sonata)

cue moon (finds and plays moonlight sonata)

cue beet (finds and plays all music files under "beethoven" directory)

cue dir <album name> (sometimes it's neccessary to specify it's a directory you want)

cue song <song> (or a song)

cue list <playlist> (or a playlist)

cue shuffle <album name> (shuffles the playlist)

cue artistA:artistB:artistC (plays all three artists, shuffled)

cue --help, -? or -h

cue --version or -v

cue --nocover

cue --noui (completely hides the UI)

cue . (loads the main cue playlist, see 'Other Functions')

 ```

Put single-quotes inside quotes "guns n' roses"

#### Key Bindings
* Use <kbd>+</kbd>, <kbd>-</kbd> keys to adjust the volume. 
* Use <kbd>←</kbd>, <kbd>→</kbd> or <kbd>h</kbd>, <kbd>l</kbd> keys to switch tracks.
* <kbd>Space</kbd>, <kbd>P</kbd> to toggle pause.
* <kbd>F2</kbd> to show/hide the playlist and information about cue.
* <kbd>F3</kbd> to show/hide key bindings.
* <kbd>v</kbd> to toggle the spectrum visualizer.
* <kbd>c</kbd> to toggle album covers.
* <kbd>i</kbd> to switch between using your regular color scheme or colors derived from the track cover.
* <kbd>b</kbd> to toggle album covers drawn in ascii or as a normal image.
* <kbd>r</kbd> to repeat the current song.
* <kbd>s</kbd> to shuffle the playlist.
* <kbd>a</kbd> add current song to main cue playlist.
* <kbd>x</kbd> to save the currently loaded playlist to a m3u file in your music folder.
* <kbd>gg</kbd> go to first song.
* number +<kbd>G</kbd>, <kbd>g</kbd> or <kbd>Enter</kbd>, go to specific song number in the playlist.
* <kbd>g</kbd> go to last song.
* <kbd>q</kbd> to quit.

## Configuration

cue will create a config file, .cue.conf, in your home directory. There you can change for instance starting directory, number of bars in the visualizer and whether to use the album cover for color (default), or your regular color scheme. To edit this file please make sure you quit cue first.

## License

Licensed under GPL. [See LICENSE for more information](https://github.com/ravachol/cue/blob/main/LICENSE).

Comments? Suggestions? Send mail to cuemusiccommand@proton.me.