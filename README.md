<div align="center">
  <img src="images/logo.png" alt="kew Logo">
</div>

<br>

<div align="center">
  <a href="https://jenova7.bandcamp.com/album/lost-sci-fi-movie-themes">
    <img src="images/kew.gif" alt="Screenshot">
  </a>
</div>
<br><br>

[![License](https://img.shields.io/github/license/ravachol/kew?color=333333&style=for-the-badge)](./LICENSE)
<br>

kew (/kjuː/) is a terminal music player.

## Features

 * Search a music library with partial titles from the command-line.
 * Creates a playlist automatically based on matched song, album or artist.
 * Private, no data is collected by kew.
 * Full color covers in sixel-capable terminals.
 * Visualizer with various settings.
 * Edit the playlist by adding, removing and reordering songs.
 * Gapless playback.
 * Explore the library and enqueue files or folders.
 * Search your music library and add to the queue.
 * Supports MP3, FLAC, MPEG-4/M4A (AAC), OPUS, OGG, Webm and WAV audio.
 * Supports desktop events through MPRIS.
 * Use themes or colors derived from covers.

## Installing

<a href="https://repology.org/project/kew/versions"><img src="https://repology.org/badge/vertical-allrepos/kew.svg" alt="Packaging status" align="right"></a>

Install through your package manager or homebrew (macOS). If you can't find it on your distro, or you want the bleeding edge, follow the [Manual Installation Instructions](docs/MANUAL-INSTALL-INSTRUCTIONS.md).

## Usage

kew creates a playlist with the contents of the first directory or file whose name matches the arguments you provide in the command-line.

```bash
kew cure great
```

This creates and starts playing a playlist with 'The cure greatest hits' if it's in your music library.

It works best when your music library is organized this way:

artist folder->album folder(s)->track(s).

<details>
<summary>Example commands</summary>

```
kew (starting kew with no arguments opens the library view where you can choose what to play)

kew all (plays all songs, up to 20 000, in your library, shuffled)

kew albums (plays all albums, up to 2000, randomly one after the other)

kew moonlight son (finds and plays moonlight sonata)

kew moon (finds and plays moonlight sonata)

kew beet (finds and plays all music files under "beethoven" directory)

kew dir <album name> (sometimes, if names collide, it's necessary to specify it's a directory you want)

kew song <song> (or a song)

kew list <playlist> (or a playlist)

kew theme midnight (sets the 'midnight.theme' theme).

kew shuffle <album name> (shuffles the playlist. shuffle needs to come first.)

kew artistA:artistB:artistC (plays all three artists, shuffled)

kew --help, -? or -h

kew --version or -v

kew --nocover

kew --noui (completely hides the UI)

kew -q <song>, --quitonstop (exits after finishing playing the playlist)

kew -e <song>, --exact (specifies you want an exact (but not case sensitive) match, of for instance an album)

kew . loads kew favorites.m3u

kew path "/home/joe/Musik/" (changes the path)

 ```
</details>

<details>
<summary>Key Bindings</summary>

* <kbd>Enter</kbd> to select or replay a song.
* Use <kbd>+</kbd> (or <kbd>=</kbd>), <kbd>-</kbd> keys to adjust the volume.
* Use <kbd>←</kbd>, <kbd>→</kbd> or <kbd>h</kbd>, <kbd>l</kbd> keys to switch tracks.
* <kbd>Space</kbd>, <kbd>p</kbd> or right mouse to play or pause.
* <kbd>Alt+s</kbd> to stop.
* <kbd>F2</kbd> or <kbd>Shift+z</kbd> (macOS/Android) to show/hide playlist view.
* <kbd>F3</kbd> or <kbd>Shift+x</kbd> (macOS/Android) to show/hide library view.
* <kbd>F4</kbd> or <kbd>Shift+c</kbd> (macOS/Android) to show/hide track view.
* <kbd>F5</kbd> or <kbd>Shift+v</kbd> (macOS/Android) to show/hide search view.
* <kbd>F6</kbd> or <kbd>Shift+b</kbd> (macOS/Android) to show/hide key bindings view.
* <kbd>u</kbd> to update the library.
* <kbd>v</kbd> to toggle the visualizer.
* <kbd>i</kbd> to cycle colors derived from kewrc, theme or track cover.
* <kbd>t</kbd> to cycle themes.
* <kbd>b</kbd> to toggle album covers drawn in ascii or as a normal image.
* <kbd>n</kbd> to toggle notifications.
* <kbd>r</kbd> to repeat the current song after playing.
* <kbd>s</kbd> to shuffle the playlist.
* <kbd>a</kbd> to seek back.
* <kbd>d</kbd> to seek forward.
* <kbd>x</kbd> to save the currently loaded playlist to a m3u file in your music folder.
* <kbd>Tab</kbd> to switch to next view.
* <kbd>Shift+Tab</kbd> to switch to previous view.
* <kbd>Backspace</kbd> to clear the playlist.
* <kbd>Delete</kbd> to remove a single playlist entry.
* <kbd>f</kbd>, <kbd>g</kbd> to move songs up or down the playlist.
* number + <kbd>G</kbd> or <kbd>Enter</kbd> to go to specific song number in the playlist.
* <kbd>.</kbd> to add currently playing song to kew favorites.m3u (run with "kew .").
* <kbd>Esc</kbd> to quit.

</details>

## Configuration

Linux: ~/.config/kew/

macOS: ~/Library/Preferences/kew/

## Themes

Press t to cycle available themes.

To set a theme from the command-line, run:

```bash
kew theme <themename> (ie 'kew theme midnight')
```

Put themes in \~/.config/kew/themes (\~/Library/Preferences/kew/themes on macOS).

## If Colors Look Wrong

Cycle <kbd>i</kbd> until they look right.

Press <kbd>v</kbd> to turn off visualizer.

Press <kbd>b</kbd> for ASCII covers.

A terminal emulator that can handle TrueColor and sixels is recommended. See [Sixels in Terminal](https://www.arewesixelyet.com/).

## Favorites Playlist

Add current song: press <kbd>.</kbd>

To load 'kew favorites.m3u':

```bash
kew .
```

## Join The Development Effort

Join the development of kew if you can handle writing C with camelCase. We are also looking for beta testers and bug reporters. See [Contributing](docs/CONTRIBUTING.md) and [Developers](docs/DEVELOPERS.md).

## License

Licensed under GPL. [See LICENSE for more information](./LICENSE).

<details>
<summary>Attributions</summary>
kew makes use of the following great open source projects:

Chafa by Hans Petter Jansson - https://hpjansson.org/chafa/

TagLib by TagLib Team - https://taglib.org/

faad2 by fabian_deb, knik, menno - https://sourceforge.net/projects/faac/

FFTW by Matteo Frigo and Steven G. Johnson - https://www.fftw.org/

Libopus by Opus - https://opus-codec.org/

Libvorbis by Xiph.org - https://xiph.org/

Miniaudio by David Reid - https://github.com/mackron/miniaudio

Minimp4 by Lieff - https://github.com/lieff/minimp4

Nestegg by Mozilla - https://github.com/mozilla/nestegg

Img_To_Txt by Danny Burrows - https://github.com/danny-burrows/img_to_txt
</details>

## Contact

Comments? Suggestions? Send mail to kew-player@proton.me.
