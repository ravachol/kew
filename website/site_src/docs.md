
## Documentation

kew creates a playlist with the contents of the first directory or file whose name matches the arguments you provide in the command-line.

```bash
kew cure great
```

This creates and starts playing a playlist with 'The cure greatest hits' if it's in your music library.

It works best when your music library is organized this way:

artist folder->album folder(s)->track(s).

## Example commands

 ```
 kew (starting kew with no arguments opens the library view where you can choose what to play)

 kew all (plays all songs, up to 50 000, in your library, shuffled)

 kew albums (plays all albums, up to 2000, randomly one after the other)

 kew moonlight son (finds and plays moonlight sonata)

 kew moon (finds and plays moonlight sonata)

 kew beet (finds and plays all music files under "beethoven" directory)

 kew dir <album name> (sometimes, if names collide, it's necessary to specify it's a directory you want)

 kew song <song> (or a song)

 kew play "/home/you/Music/Jimi Hendrix/Are You Experienced/" (Plays this album, location can be anywhere)

 kew play "/home/you/Music/Jimi Hendrix/Are You Experienced/Purple Haze.flac" (Plays this song, location can be anywhere)

 kew play <album path> <album path> <song path> (play can take multiple album paths or song paths and add them all into
 temporary playlist)

 kew list <playlist> (or a playlist)

 kew theme midnight (sets the 'midnight.theme' theme)

 kew shuffle <album name> (shuffles the playlist. shuffle needs to come first.)

 kew artistA:artistB:artistC (plays all three artists, shuffled)

 kew --help, -? or -h

 kew --version or -v

 kew --nocover

 kew --noui (completely hides the UI)

 kew -q <song>, --quitonstop (exits after finishing playing the playlist)

 kew -e <song>, --exact (specifies you want an exact (but not case sensitive) match, of for instance an album)

 kew .  (loads kew favorites.m3u)

 kew path "/home/you/Music/" (changes the path)
 ```


## Key Bindings

### Basic

* <kbd>Enter</kbd> to play or enqueue/dequeue.
* <kbd>Space</kbd>, <kbd>p</kbd> or right mouse to play or pause.
* Use <kbd>+</kbd> (or <kbd>=</kbd>), <kbd>-</kbd> keys to adjust the volume.
* Use <kbd>←</kbd>, <kbd>→</kbd> or <kbd>h</kbd>, <kbd>l</kbd> keys to switch tracks.
* <kbd>Alt+s</kbd> to stop.
* <kbd>F2</kbd> or <kbd>Shift+z</kbd> (macOS/Android) to show/hide playlist view.
* <kbd>F3</kbd> or <kbd>Shift+x</kbd> (macOS/Android) to show/hide library view.
* <kbd>F4</kbd> or <kbd>Shift+c</kbd> (macOS/Android) to show/hide track view.
* <kbd>F5</kbd> or <kbd>Shift+v</kbd> (macOS/Android) to show/hide search view.
* <kbd>F6</kbd> or <kbd>Shift+b</kbd> (macOS/Android) to show/hide key bindings view.
* <kbd>i</kbd> to cycle colors derived from kewrc, theme or track cover.
* <kbd>t</kbd> to cycle themes.
* <kbd>Backspace</kbd> to clear the playlist.
* <kbd>Delete</kbd> to remove a single playlist entry.
* <kbd>r</kbd> to cycle repeat settings (repeat, repeat list, off).
* <kbd>s</kbd> to shuffle the playlist.

### Advanced

* <kbd>u</kbd> to update the library.
* <kbd>m</kbd> show full page lyrics in track view.
* <kbd>v</kbd> to toggle the visualizer.
* <kbd>b</kbd> to toggle album covers drawn in ascii or as a normal image.
* <kbd>n</kbd> to toggle notifications.
* <kbd>a</kbd> to seek back. keep pressed for a longer duration, it executes the seek when you release the key.
* <kbd>d</kbd> to seek forward. keep pressed for a longer duration, it executes the seek when you release the key.
* <kbd>x</kbd> to save the currently loaded playlist to a m3u file in your music folder.
* <kbd>Tab</kbd> to switch to next view.
* <kbd>Shift+Tab</kbd> to switch to previous view.

* <kbd>f</kbd>, <kbd>g</kbd> to move songs up or down the playlist.
* number + <kbd>G</kbd> or <kbd>Enter</kbd> to go to specific song number in the playlist.
* <kbd>.</kbd> to add currently playing song to kew favorites.m3u (run with "kew .").
* <kbd>Esc</kbd> to quit.

## Configuration

Linux: ~/.config/kew/

macOS: ~/Library/Preferences/kew/

Key bindings can be added like this:

bind = +, volUp, +5%

If you have an old install of kew, delete the kewrc file to make this style of
bindings appear.

kew state (for settings that can be changed in-app) is kept in ~/.config/kew/ke
wstaterc.

If you change a setting in-app it will be tracked by kewstaterc and not kewrc.

kewrc is never changed by kew with the exception of when you run kew path.

If you delete your kewrc a new default one will be generated. You wont get newe
r config options listed in your config file unless you do this.

## Playlists

To load a playlist: type `kew list <name>`

To export a playlist, press `x`. This will save a file in your music path with the name of the first song in the queue.

There is also a favorites playlist function:

Add current song: press <kbd>.</kbd>

To load 'kew list fav': kew .

## Layouts

It's possible to define your own layout in kew. How to make one is described in [LAYOUTS-HOWTO.md](https://codeberg.org/ravachol/kew/src/branch/main/layouts/LAYOUTS-HOWTO.md)

The layout used by kew is called current.layout.

Replace current.layout to change the layout. If breaking changes are introduced, current.layout will be renamed to current.layout.bak, before the new one is copied over.

Layouts are in \~/.config/kew/layouts (\~/Library/Preferences/kew/layouts on macOS).

If layouts aren't working, try re-installing kew or running 'sudo make install' if you ran make yourself.

## Themes

Press t to cycle available themes.

To set a theme from the command-line, run:

```bash
kew theme <themename> (ie 'kew theme midnight')
```

Put themes in \~/.config/kew/themes (\~/Library/Preferences/kew/themes on macOS).

Do not edit the included themes as they are managed by kew. Instead make a copy with a different name and edit that.

Try the theme editor (by @bholroyd): [https://bholroyd.github.io/Kew-tip/](https://bholroyd.github.io/Kew-tip/).

## If Colors or Graphics Look Wrong

Cycle <kbd>i</kbd> until they look right.

Press <kbd>v</kbd> to turn off visualizer.

Press <kbd>b</kbd> for ASCII covers.

A terminal emulator that can handle TrueColor and sixels is recommended. See [Sixels in Terminal](https://www.arewesixelyet.com/).

## Visulizations / Chroma

Starting with kew 4.0, you can add visualizations to kew by installing Chroma, an app by another developer.

You'll need to install a specific commit, which is the latest one that works.

How to install Chroma:

```
git clone https://github.com/yuri-xyz/chroma.git

cd chroma

git checkout fb00b6e

cargo install --path . --features audio
```

Enable and cycle through the visualizations by pressing <kbd>c</kbd> in track view.

Disable by pressing <kbd>b</kbd>.

This works by kew being fed frames from Chroma and does not add bloat to kew.

#### Chroma Configuration

You can customize Chroma's behavior in your `kewrc` file:

- `chromaPath`: Path to a custom Chroma preset file. If set, this preset will be used instead of the built-in ones.
- `chromaDevice`: Specify the audio device for Chroma to capture from (e.g., `PipeWire Sound Server`). Use `chroma --list-audio-devices` to find available devices.

```ini
[chroma]
chromaPath=/path/to/your/custom.preset
chromaDevice=PipeWire Sound Server
```

## Scrobbling

kew's private and offline nature means we don't support Scrobbling/last.fm dire
ctly. Instead tools such as PanoScrobbler are recommended. See: https://github.
com/kawaiiDango/pano-scrobbler.

## How to make sure kew is offline and private

kew itself does not have telemetry, it does not collect data and send to kew servers, it does not access the internet.

However, if Discord RPC is enabled and Discord is running, data about what you are listening to will end up on Discord servers.

To ensure the best privacy, two settings have to be set as following in the settings file kewrc:

```bash
allowNotifications=0
discordRPCEnabled=0
```

in kewstaterc, make sure this is set:

```bash
allowNotifications=0
```

kewstaterc tracks in-app choices so if you press 'n' for toggling notifications, the value will change.

Notifications means desktop notifications (MPRIS/DBUS) which is local, but is broadcast to other apps on your machine that are listening. It's generally not logged unless an app on your machine that is listening logs it.

To force notifications to be always off, compile with:

```bash
make USE_DBUS=0 -j
```

To erase traces of your kew listening after uninstall, you need to delete the library.dat file in ~/.config/kew.

## License

Licensed under GPL. [See LICENSE for more information](https://codeberg.org/ravachol/kew/src/branch/main/LICENSE).
