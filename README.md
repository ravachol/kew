
# cue

Listen to music in the terminal.

<div align="center">
    <img src="cue-screenshot.png" />
</div>

cue is a command-line music player for Linux.

## Features
 
 * Search a music library with partial titles
 * Display album covers as ASCII art or as a normal (blocky) image.
 * Creates a playlist automatically based on matched directory name
 * Control the player with previous, next and pause.
 * A main playlist that you can add to by pressing 'a' when listening to any song. Load the playlist by running cue with no arguments.

## Quick Installation

To quickly install cue, just copy and paste it to your terminal:

```bash
sudo bash -c "curl https://raw.githubusercontent.com/ravachol/cue/main/install.sh | bash"
```

## Manual Installation

cue dependencies are:

* FFmpeg
* FFTW 

A TrueColor capable terminal is recommended, like Konsole, kitty or st, to display colors properly.

For a complete list of capable terminals, see this page: [Colors in Terminal](https://gist.github.com/CMCDragonkai/146100155ecd79c7dac19a9e23e6a362) (github.com).

Install FFmpeg and FFTW using your distro's package manager. Then run:

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

## Usage

IMPORTANT! Tell cue the path to your music library (you only need to do this once):

```bash
cue path "/home/joe/Music/"
```
Now run cue and provide a partial name of a track or directory:

```bash
cue cure great
```

This command plays all songs from "The Cure Greatest Hits" directory, provided it's in your music library, and prints out the album cover in colorful ASCII on the screen!

cue returns the first directory or file whose name matches the string you provide.

#### Some Examples:

 ```
cue moonlight son (finds and plays moonlight sonata)

cue moon (finds and plays moonlight sonata)

cue beet (finds and plays all music files under "beethoven" directory)

cue dir <album name> (sometimes it's neccessary to specify it's a directory you want)

cue song <song> (or a song)

cue shuffle wu-tang (shuffles everything under wu-tang directory, random and rand works too)

cue list de la (searches for and starts playing a a .m3u playlist that contains "de la")

cue cure:depeche (plays the cure and depeche mode shuffled)

cue --nocover fear of the dark (doesn't display a cover)

cue --help, -? or -h

cue --version or -v

cue (starting cue with no arguments loads the main cue playlist, see 'Other Functions')
 ```

#### Other Functions:

* Use <kbd>↑</kbd>, <kbd>↓</kbd> keys to raise or lower volume. 
* Use <kbd>→</kbd>, <kbd>←</kbd> keys to play the next or previous track in the playlist. 
* <kbd>Space</kbd> to toggle pause.
* <kbd>e</kbd> to toggle equalizer.
* <kbd>c</kbd> to toggle album covers.
* <kbd>b</kbd> to toggle album covers drawn with solid blocks.
* <kbd>r</kbd> to repeat current song.
* <kbd>s</kbd> to shuffle playlist.
* <kbd>a</kbd> add current song to main cue playlist.
* <kbd>d</kbd> delete current song from main cue playlist. This only works if you started cue with no arguments, which loads the main cue playlist.
* <kbd>q</kbd> to quit.

## License

Licensed under GPL. [See LICENSE for more information](https://github.com/ravachol/cue/blob/main/LICENSE).