
# cue

Listen to music in the terminal.

<div align="center">
    <img src="cue-screenshot.png" />
</div>

cue is a command-line music player for Linux.

## Features
 
 * Search a music library with partial titles
 * Display album covers as ASCII art.
 * Creates a playlist automatically based on matched directory name
 * Control the player with previous, next and pause.

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

cue song <song name> (or a song)

cue shuffle <dir name> (random and rand works too)

cue cure:depeche (plays the cure and depeche mode shuffled)

cue --nocover <words> (doesn't display a cover)

cue --help, -? or -h

cue --version or -v
 ```

#### Other Functions:

* Use <kbd>↑</kbd>, <kbd>↓</kbd> keys to raise or lower volume. 
* Use <kbd>→</kbd>, <kbd>←</kbd> keys to play the next or previous track in the playlist. 
* <kbd>Space</kbd> to toggle pause.
* <kbd>v</kbd> to toggle visualization.
* <kbd>c</kbd> to toggle album covers. (starting from next track)
* <kbd>b</kbd> to toggle album covers drawn with solid blocks. (starting from next track)
* <kbd>r</kbd> to repeat current song.
* <kbd>s</kbd> to shuffle playlist.
* <kbd>q</kbd> to quit.

## License

Licensed under GPL. [See LICENSE for more information](https://github.com/ravachol/cue/blob/main/LICENSE).