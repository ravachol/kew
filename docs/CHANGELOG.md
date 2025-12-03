# CHANGELOG

### 3.7.0

Biggest news in this release: Optimisations for large music collections/slow disks and a key binding/input handling overhaul.

- Optimisations: Faster loading of previous playlist. So if you are used to running kew all for instance this will be significantly faster, especially for people with large collections. The new method should be up to 20 times faster for loading a big last used playlist.

- The library is now always cached. It scans the library only if the files have changed, which it checks at the top level. This will be much faster if you have music on a slow disk. But it's all still very simple, no database dependencies or anything, just a flat binary file with the bare essentials. It's not a lot of data: 1k songs = 60KiB on disk.

- Key binding overhaul: This allows for more advanced key bindings. You can now bind more keys and key combinations. The new format was suggested by @jaoh.

Som examples:

bind = +, volUp, +5%
bind = Shift+s, stop
bind = mouseMiddle, enqueueAndPlay

The old format config files will still work for the most part, but to see examples of the new style, delete your kewrc file.

- The help page will now fully reflect the keys you have bound.

- Handle both synchronized and unsynchronized vorbis lyrics. Suggested by @hiruocha.

- Auto-scrolling lyrics in the lyrics page for synced lyrics. Also added the ability to scroll lyrics manually. By @noiamnote.

- The app and readme has been translated into japanese and chinese. Thank you for help with the chinese translation @hiruocha.

- New theme: catpuccin mocha by @pixel-peeper.

- New theme: neutral, uses only the default foreground color.

- The cover is now visible on the left side on most views. Can be disabled by setting hideSideCover=1 in the config file.

- Reverted to being neutral in album color mode when no music is playing.

#### Bug Fixes

- Don't enqueue the .m3u files themselves when mass enqueueing.

- Fixes a bug related to certain types of mp3 files, where the song metadata wasn't switching in the UI. Found by: @Chromium-3-Oxide.

- Fixes a bug in library view where under some conditions, the position of the selected row could jump upwards.

- Made the path validation function less strict to avoid false positives. Reported by: DimaFyodorov.

- Removed hardcoded paths in Makefile to avoid conflicting paths. Found by @hpjansson.

- Fixed a few minor bugs with the library UI. Found by @bholroyd.

- Fixed full width characters not displayed in notifications. Found by @Chromium-3-Oxide.

#### Contributors

Thank you also especially to contributors @jaoh, @bholroyd, @LeahTheSlug, @Chromium-3-Oxide and @hpjansson, @Vafone for reporting many issues and helping out.

Thank you to @bholroyd for making the kew theme editor: https://bholroyd.github.io/Kew-tip/

Thank you to all the beta testers!

#### Sponsors

Thank you to Christian Mummelthey, imalee.sk and @sandrock. for their donations.

### 3.6.4

- Fixed 'kew theme' command.

- Add support for vorbis lyrics for FLAC/Ogg/Opus files. We now have .lrc file support, SYLT mp3 lyrics support and now these. Should cover most cases.

### 3.6.3

- Fixed issue on termux for people who don't have faad installed.

- If run with song arguments starts in track view like it's supposed to.

- Fixed song loading instability with flac and mp3.

### 3.6.2

- Fix kew not exiting cleanly on android.

- Fix update library from cache resulting in error.

### 3.6.1

- Fixed build issue on termux.

### 3.6.0

![kew logo](https://codeberg.org/ravachol/kew/media/branch/main/images/logo.png)

- kew now has a real logo and a tagline: "MUSIC FOR THE SHELL".

- We now also have a color associated with kew, which is red: #de2b4d.

- This color is now the default if you are not playing anything and are using album colors.

- The welcome screen that appears when the path is not set has been given much love.

- Song lyrics support through .lrc files. These need to already be on your computer. By @Rioprastyo17

- Song lyrics support through SYLT id3 tags. By @dandelion-75.

- Watch timestamped lyrics in track view or press 'm' in the same view for full page lyrics. By @ravachol.

- Bumped miniaudio to version v0.11.23 which among other things fixes a bug with some versions of mp3. By @hypercunx.

- Code cleanup, improved internal structure by A LOT and removed all globals among other things. There are still a bunch of little inconsistencies to work on, naming conventions and so on, but for the most part, I'm very pleased with the progress.

- There's now a diagram of kew's architecture included for devs who want to know how the internals are laid out.

- You can now finally set collaped paths such as ~/Music and they will be set that way in config. Suggested by @danielwerg.

- The config file is now respected and no longer changed by kew, only if you run kew path \<path\>. Instead there is a kewstaterc file in ~/.local/state that keeps the variables that can change in-app, these override what's in kewrc. Suggested by @danielwerg.

- Dropped Nerd Fonts in favor of ⇉,⇇,↻,⇄ which are all unicode symbols. While Nerdfonts is neat it's too much trouble for users to install things and troubleshoot for just 4 slightly better symbols.

- kew now restarts if it's already running in a different window. This replaces the ugly message that instructs the user to run 'kill'. Suggested by @amigthea.

- Gradients are now enabled when using themes, not just when using album colors. It's the little things that count.

- Playlist max files limit raised to 50k songs. Suggested by: @Saijin_Naib.

#### Bug Fixes

- Path is expanded correctly when providing it through the first screen that let's you choose a path.

- Don't create ~/.config/kew/themes dir if there are no themes to be copied (user hasn't done sudo make install).

- Fixed a bug with the library cache ids that was introduced in the last version.

- Fixed a build issue on some versions of macOS.

- Fixed an issue with replay gain being calculated in the wrong place.

### Sponsors

Thank you to for the generous donation @LTeder!

### 3.5.3

- Fixes a bug that affects the library cache. This bug has the effect of making startup times for kew be slower on already slow HDDs.


### 3.5.2

- Fixed line in cover being erased in landscape mode on some terminals. By @hartalex.

- Fixed long names no longer scrolling.

- Fixed cover in landscape mode jumping from line 1 to line 2 and back when resizing window.

### 3.5.1

- Fix issue/test on homebrew.

### 3.5.0

Now with themes and Android support

New in this release:

- Fully customizable colors

- Themes supporting both TrueColor RGB and the terminal 16-color palette

- Theme pack with 16 included themes

- Android support

- Fixed TTY flickering

- Improved search

#### Themes

Press t to cycle available themes.

To set a theme from the command-line, run:

kew theme themename (ie 'kew theme midnight')

Put themes in \~/.config/kew/themes (\~/Library/Preferences/kew/themes on macOS).

#### Android

I haven't looked at battery life aspects yet, but staying in library view will be easier on the battery than using track view with the visualizer. You can also press v to toggle the visualizer on or off.

#### TTY problems resolved

The flickering in TTY has been fixed. Btw, if you are on tty or have limited colors and font on your terminal, try pressing i (for other color modes), v (for visualizer off) and b (for ascii cover). That should make it look much more easy on the eye!

#### Move to Codeberg

We now have a repo on Codeberg and that will be the preferred repo going forward. But people will be welcome to contribute in whichever place they prefer. Except for PRs, PRs need to go to codeberg, develop branch.

#### OpenSuse

We are now back on openSuse, our package there hadn't been updated in a long time, due to openSuse not having faad lib.

We still need a Fedora package. We already have a RPM spec that @kazeevn added and everything.

Thank you to @welpyes for bringing up Termux and helping out with that, and @arcathrax for fixing the ultrawide monitor bug. Thank you to mantarimay for updating the openSuse library.

#### Sponsors and Donations Wanted

Thank you to a new sponsor, @BasedScience!

Please support this effort:
https://ko-fi.com/ravachol
https://github.com/sponsors/ravachol.

- Ravachol

#### New Features / Improvements

- Theme colors, both TrueColor and 16-color palette theming. Cycle by pressing 't'.


- Android compatibility! Please see ANDROID-INSTRUCTIONS.md for how to get kew on your phone. (@welpyes)

- Improved the search function so that albums are shown below an artist hit.

- Improved installation instructions for Fedora and openSuse in the README.

- Enabled the detection of FAAD2 (which handles m4a) on Fedora properly in the makefile.

- Made makefile compatible with openSuse Tumbleweed. The kew package has been updated on openSuse for the first time in a long time, thank you mantarimay (maintainer on openSuse).

- Added an icon indicating if the song is playing or paused before the title at the top when the logo is hidden.

- Shows the playlist from the first song (if it's in view), instead of always starting from the playing song. Suggested by @affhp.

- Improved the safety of various functions and addressed potential vulnerabilities.

- Don't make a space for the cover if there is none on landscape view.

- Improved the instructions in the help view.

#### Bug Fixes

- Fixed visualizer crashing the app on ultrawide monitors.

- Added null check for when exporting an empty playlist to .m3u.

- Prevent flickering when scrolling on TTY and likely on some other terminals as well.

- Search: fixed files being reordered when scrolling on macOS/kitty.

### 3.4.1

Adds a few minor bug fixes and you can now use playlists from the library view.

- Added ability to see and enqueue playlists (m3u and m3u8) from library view. By Ravachol. Suggested by Kansei994.

- Removed -flto from the makefile since it was causing compatibility problems, for instance Ubuntu 25.04.

- Removed ALAC file support due to CVEs in the Apple ALAC decoder: https://github.com/macosforge/alac/issues/22. By Ravachol. Reported by @werdahias.

#### Bug Fixes

- Fixed G key not bound for new config files. By @Chromium-3-Oxide.

- Fixed restarting from stop by pressing space bar only working once. By @ravachol. Found by @Chromium-3-Oxide.

- Fixed status/error message sometimes not being cleared. By @SynthSlayer. Found by @SynthSlayer.

- Fixed playlist sometimes starting from last song when enqueueing all by pressing "MUSIC LIBRARY". By @ravachol. Found by j-lakeman.

- Fixed fullwidth characters not being truncated correctly. By @ravachol. Found by Kuuuube.

- Fixed running kew --version to show the version screen along with the logo makes the logo have a really dark gradient that is barely readable on some terminals. By @ravachol. Found by @Chromium-3-Oxide.

### 3.4.0

- Landscape View (horizontal layout). Something that was long overdue. Widen the window and it automatically goes into landscape mode. By @Ravachol. Suggested by @Saijin-Naib.

- Added ability to switch views by using the mouse. By @Chromium-3-Oxide.

- Added ability to drag the progress bar using the mouse. Suggested by @icejix.

- Faster loading of library cache from file, for people with very large music collections.

- Show the actual keys bound to different views on the last row on macOS and show Shift+key instead of Sh+key for clarity. By @arcathrax.

- Added an indicator when the library is being updated. Suggested by @Saijin-Naib.

- Now (optionally) sets the currently playing track as the window title. By @Chromium-3-Oxide. Suggested by @Saijin-Naib.

- Added back the special playlist, but renamed as the kew favorites playlist. This is a playlist you can add songs to by pressing "."." while the song is playing. Requested by @nikolasdmtr.

#### Bug Fixes

- Don't strip numbers from titles when presenting the actual title taken from metadata. We still strip numbers like 01-01 from the beginning of filenames before presenting them though.

- Reset clock when resuming playback when stopped. Found by @Knusper.

- Better way of checking for embedded opus covers, some covers weren't detected. Reported by @LeahTheSlug.

- Better way of extracting ogg vorbis covers. Reported by @musselmandev.

- Fixed 'kew all' not being shuffled if 'save repeat and shuffle settings' was turned on. By @j-lakeman.

### 3.3.3

- Don't show zero's for hours unless the duration is more than an hour long.

- Show shuffle, repeat settings even if last row not visible.

- Better handling of comments in config file.

- Volume settings now follow a more conventional pattern where it increases or decreases based directly on the system output, instead of being relative to maximum output. Was able to remove a lot of code related to getting system volume on linux and macOS. Suggested by @arcathrax.

- Escape for quit is no longer hard coded and can be disabled or changed in the settings file. Suggested by @0023-119.

- Page Up and Page Down for scrolling are no longer hard coded and can be changed in the settings file. Suggested by The Linux Cast.

- Remove special playlist function. It's kew's most odd feature and confuses people because they think it's related to the normal saving playlist function. Plus nobody has ever mentioned using it.

- Tmux + kitty and Tmux + foot now displays images correctly. Reported by @amigthea and @acdcbyl.

#### Bug Fixes

- Fixed "ghost" visualizer bars showing up at higher frequencies in a zoomed out terminal window. Reported by @Chromium-3-Oxide.

- Fixed bug in library related to handling of sub-directories several levels deep.

- Fixed volume up/down not working when audio interface plugged in on macOS. Reported by @arcathrax.

- Fixed bug with library cache setting not being remembered. Reported by @Saijin-Naib.

### 3.3.2

- Remove -lcurl from makefile.

### 3.3.1

Removal of Internet Radio Feature:
We have decided to remove the internet radio feature from this release. This decision was made after careful consideration of stability and security concerns.

Why This Change:
To focus on core functionality. By removing the internet radio feature, we can concentrate on making the core music player really high quality, and making kew a more enjoyable experience.

Security and Stability:
It has been challenging addressing issues related to the internet radio feature. Removing it allows us to focus on other aspects of the application without compromising its stability and security.

To summarize: By removing internet access completely from kew, we can make it a simple, secure and robust tool much more easily.

- Also Fixes an issue with visualizer height on macOS.

### 3.3.0

- Reworked the visualizer to make it have more punch and to make higher frequency value changes more visible. The visualizer now also runs at 30fps instead of 60fps. 60 fps was just at odds with kew's very low system requirements profile, and we want kew to consume very little resources. The lower framerate cuts the cpu utilization in half when in track view with no big noticeable differences.

- Added webm support. Suggested by @Kuuuube.

- kew now remembers the playlist between sessions, unless you tell it to load something else.

- Added a new setting, visualizerBarWidth. 0=Thin,1=Fat (twice as wide bars) or 2=Auto (depends on the window size. the default). Also line progressbar is now the default, mainly because it looks better with fat bars.

- The appearance of the progress bar can now be configured in detail. There's even a pill muncher mode, where this round guy eats everything in his path. There are commented out examples in the kewrc so you can go back to the old look, if you don't like the new. By @Chromium-3-Oxide.

- Gradient in library and playlist that makes the bottom rows a bit darker when using album colors.

- Replay gain source can now be set in the config file. 0 = track, 1 = album or 2 = disabled. Suggested by @ksushagryaznaya.

- Logging to error.log is now enabled if you run make DEBUG=1.

- Prevent Z,X,C,V,B,N to trigger view changes in search or radio search if not on macOS. These are the shortcuts that are used instead of the F1-F7 keys on macOS, because there F-keys don't always mean F-functions. Delete the config file kewrc if you want to type uppercase letters in search.

- Now saves repeat and shuffle settings betweens sessions. This can be turned off in the settings file.

#### Bug Fixes

- Fixed a format conversion issue with the visualizer.

- The clock wasn't getting reset completely when restarting songs after seeking or when using alt+s to stop. Found by @Chromium-3-Oxide and @Kuuuube.

- Fixed ascii cover image being too narrow on gnome terminal.

- Fixed error (invalid read 8 bytes) when using backspace to clear a stopped playlist.

- Gave the last row more minimum space.

- Fixed bug where on some terminals when in a small window and visualizer disabled, the time progress row would get repeated.

- Removed a use-after-unlock race in radio search.

- Eliminated memory leak on radio search cancel by switching to cooperative thread cancellation (stop flag) instead of pthread_cancel.

### 3.2.0

Now with a mini-player mode, a braille visualizer mode, a favorites list for radio stations, scrolling names that don't fit, the visualizer running at 60 fps, and much more!

- New mini-player mode. Make the window small and it goes into this mode. Suggested by @HAPPIOcrz007.

- The visualizer now runs at 60 fps instead of 10, making it much smoother. The rest of the ui runs slower much like before to save on system resources.

- The visualizer can now be shown in braille mode (visualizerBrailleMode option in kewrc config file).

- Track progress can now be shown as a thin line instead of dots.

- Now shows sample rate and if relevant, bitrate, in track view.

- A favorites list for radio stations. It's visible when the radio search field is empty.

- Audio normalization for flac and mp3 files using replay gain metatags. Suggested by @endless-stupidity.

- Long song names now scroll in the library and playlist. Suggested by @HAPPIOcrz007.

- Press o to sort the library, either showing latest first or alphabetically. Suggested by @j-lakeman.

- Radio search now refreshes the list as radio stations are found, making it less "laggy".

- Track view works with radio now.

- Added a stop command (shift+s). Space bar is play as before.

- Removed the playback of tiny left overs of the previous song, when pausing and then switching song.

- Added bitrate field to radio station list.

- Added support for fullwidth characters.

- Added repeat playlist option. Suggested by @HAPPIOcrz007.

- Added option to set the visualizer so that the brightness of it depends on the height of the bar. By @Chromium-3-Oxide.

- Added config option to disable mouse (in kewrc file). By @Chromium-3-Oxide.

- Previous on first track now resets the track instead of stopping.

- Code cleanup. By @Chromium-3-Oxide.

#### Bug Fixes

- Fixed deadlock when quickly and repeatedly starting a radio station.

- Fixed bug with previous track with shuffle enabled. Found by @GuyInAShack.

- Fixed bug with moving songs around, there was a case where it wasn't rebuilding the chain and the wrong song would get played.

- Fixed bug with alt + mouse commands not working.  By @Chromium-3-Oxide.

### 3.1.2

- Fix radio search sometimes freezing because of an invalid radio station URL. Found by joel. by @ravachol.

- Added ability to play a song directly from the library (instead of just adding it to the playlist) by pressing Alt+Enter. Suggested by @PrivacyFriendlyMuffins. By @ravachol.

- Added ability to disable the glimmering (flashing) last row. By @Chromium-3-Oxide.

### 3.1.1

- Reverts the command `kew path` to its previous behavior (exit on completion), which enables some automated tests to function again. By @ravachol.

### 3.1.0

Now with internet radio, mouse support and ability to move songs around in the playlist.

#### Dependencies:

- New dependency on libcurl.

#### Changes:

- Added Internet radio support.
  MP3 streams only so far, but the vast majority of streams are MP3 streams in the database we are using, others are excluded.
  Press F6 for radio search or Shift+B on macOS. By @ravachol.

- Added mouse support.
  Use the middle button for playing or enqueueing a song. Right button to pause. This is configurable with plenty of options. By @Chromium-3-Oxide.

- Move songs up and down the playlist with t and g. By @ravachol. Suggested By @HAPPIOcrz007.

- Added support for m4a files using ALAC decoder. By @ravachol.

- When the program exits previous commands and outputs are restored. By @DNEGEL3125.

- Clear the entire playlist by pressing backspace. By @mechatour.

- Added support for wav file covers. By @DNEGEL3125.

- Made the app do less work when idle. By @ravachol.

- The currently playing track is now underlined as well as bolded, because bold weight wasn't working with some fonts. Found By @yurivict. By @ravachol.

- Added logic that enables running raw AAC files (but not HE-AAC). By @ravachol.

- Added debugging flag to the makefile. Now to run make with debug symbols, run:
  make DEBUG=1 -ij4.

- It's now possible to remove or alter the delay when printing the song title, in settings. By @Chromium-3-Oxide.

- Added the config option of quitting after playing the playlist, same as --quitonstop flag. By @Chromium-3-Oxide.

- Improved error message system. By @ravachol.

- Reenabled seeking in ogg files. By @ravachol.

#### Bug Fixes:

- Fixed cover sometimes not centered in wezterm terminal. By @ravachol.

- Fixed setting path on some machines doesn't work, returns 'path not found'. Found by @illnesse.

- Fixed crash when in shuffle mode and choosing previous song on empty playlist. Found by @DNEGEL3125.

- Fixed crash sometimes when pressing enter in track view. By @ravachol.

- Fixed ogg vorbis playback sometimes leading to crash because there was no reliable way to tell if the song had ended. By @ravachol.

- Fixed opus playback sometimes leading to crash because of a mixup with decoders. By @ravachol.

- Uses a different method for detecting if kew is already running since the previous method didn't work on macOS. By @DNEGEL3125.

- Prevent the cover from scrolling up on tmux+konsole. Found by @acdcbyl. By @ravachol.

#### Special Thanks To These Sponsors:

- @SpaceCheeseWizard
- @nikolasdmtr
- *one private sponsor*

### 3.0.3

- Fixed buffer redraw issue with cover images on ghostty.

- Last Row is shown in the same place across all views.

- The library text no longer shifts one char to the left sometimes when starting songs.

- Fixed minor bug related to scrolling in library.

- Fixed bug related to covers in ascii, on narrow terminal sizes it wouldn't print correctly. Found by @Hostuu.

- Minor UI improvements, style adjustments and cleaning up.

- Added play and stop icon, and replaced some nerdfont characters with unicode equivalents.

- Disabled desktop notifications on macOS. The macOS desktop notifications didn't really gel well with the app, and the method used was unsafe in the long run. A better way to do it is by using objective-c, which I want to avoid using.

### 3.0.2

- You can now enqueue and play all your music (shuffled) in library view, by pressing MUSIC LIBRARY at the top.

- Removed dependency on Libnotify because its' blocking in nature, and some users were experiencing freezes. Instead dbus is used directly if available and used with timeouts. Reported by @sina-salahshour.

- Fixed bug introduced in 3.0.1 where songs whose titles start with a number would be sorted wrong.

- Fixed music library folders containing spaces weren't being accepted. Found by @PoutineSyropErable.

- Fixed bug where after finishing playing a playlist and then choosing a song in it, the next song would play twice.

- Fixed kew all not being randomized properly. Found by @j-lakeman.

- Fixed useConfigColors setting not being remembered. Found by @j-lakeman.

- Added AUTHORS.md, DEVELOPERS.md and CHANGELOG.md files.

- Dependencies Removed: Libnotify.

### 3.0.1

- Uses safer string functions.

- Fixed bug where scrolling in the library would overshoot its place when it was scrolling through directories with lots of files.

- Fixed mpris/dbus bug where some widgets weren't able to pause/play.

- Fixed crash when playing very short samples in sequence. Found by @hampa.

- Fixed order of songs in library was wrong in some cases. Found by @vincentcomfy.

- Fixed bug related to switching songs while paused.

- Fixed bug with being unable to rewind tracks to the start. Found by @INIROBO.

- Seek while paused is now disabled. Problems found by @INIROBO.

### 3.0.0

This release comes with bigger changes than usual. If you have installed kew manually, you need to now install taglib, ogglib and, if you want, faad2 (for aac/m4a support) for this version (see the readme for instructions for your OS).

- kew now works on macOS. The default terminal doesn't support colors and sixels fully, so installing a more modern terminal like kitty or wezterm is recommended.

- Removed dependencies: FFmpeg, FreeImage.

- Added Dependencies: Faad2, TagLib, Libogg.

- These changes make kew lighter weight and makes it faster to install on macOS through brew.

- Faad2 (which provides AAC decoding) is optional. By default, the build system will automatically detect if faad2 is available and include it if found.

- More optimized and faster binary. Thank you @zamazan4ik for ideas.

- Better support of Unicode strings.

- Case-insensitive search for unicode strings. Thank you @valesnikov.

- Fixed makefile and other things for building on all arches in Debian. Thank you so much @werdahias.

- More efficient handling of input.

- Added support for .m3u8 files. Thank you @mendhak for the suggestion.

- Fixed bug where switching songs quickly, the cover in the desktop notification would remain the same.

- Fixed issue with searching for nothing being broken. Thank you @Markmark1!

Thank you so much @xplshn, @Vafone and @Markmark1 for help with testing.

### 3.0.0-rc1

This release comes with bigger changes than usual. If you have installed kew manually, you need to now install taglib, ogglib and, if you want, faad2 (for aac/m4a support) for this version (see the readme for instructions for your OS).

- kew now works on macOS. The default terminal doesn't support colors and sixels fully, so installing a more modern terminal like kitty or wezterm is recommended.

- Removed dependencies: FFmpeg, FreeImage

- Added Dependencies: Faad2, TagLib, Ogglib

- These changes makes kew lighter weight and makes it faster to install on macOS through brew.

- Faad2 (which provides AAC decoding) is optional! By default, the build system will automatically detect if faad2 is available and include it if found. Disable with make USE_FAAD=0.

- More optimized and faster binary. Thank you @zamazan4ik for ideas.

- Better support of Unicode strings.

- Fixed makefile and other things for building on all arches in Debian. Thank you @werdahias.

- More efficient handling of input.

- Added support for .m3u8 files. Thank you @mendhak for the suggestion.

- Fixed bug where switching songs quickly, the cover in the desktop notification would remain the same.

Thank you @xplshn and @Markmark1 for help with testing.

Big thanks to everyone who helps report bugs!

### 2.8.2


- Fixed issue with building when libnotify is not installed.

- Fixed build issue on FreeBSD.


### 2.8.1

New in this version:

- Much nicer way to set the music library path on first use.

- Checks at startup if the music library's modified time has changed when using cached library. If it has, update the library. Thanks @yurivict for the suggestion.

- Improved search: kew now also shows the album name (directory name) of search results, for clarity.

- You can now use TAB to cycle through the different views.

- There's now a standalone executable AppImage for musl x86_64 systems. Thank you to @xplshn and @Samueru-sama for help with this.

Bugfixes and other:

- Added missing include file. Thank you @yurivict.

- Don't repeat the song notification when enqueuing songs. A small but annoying bug that slipped into the last release.

- Fixed issue where kew sometimes couldn't find the cover image in the folder.

- Better handling of songs that cannot be initialized.

- Removed support for .mp4 files so as to not add a bunch of video folders to the music library. Thanks @yurivict for the suggestion.

- Made the makefile compatible with Void Linux. Thank you @itsdeadguy.

- Cursor was not reappearing in some cases on FreeBSD after program exited. Thank you @yurivict.

- Fixed slow loading UI on some machines, because of blocking desktop notification. Thanks @vdawg-git for reporting this.

Thank you to @vdawg-git for helping me test and debug!

Thank you also to @ZhongRuoyu!

### 2.8

New Features:

- Much nicer way to set the music library path on first use.

- Checks at startup if the music library's modified time has changed when using cached library. If it has, update the library. Thanks @yurivict for the suggestion.

- Improved search: kew now also shows the album name (directory name) of search results, for clarity.

- You can now use TAB to cycle through the different views.

- There's now a standalone executable AppImage for musl x86_64 systems. Thank you to @xplshn and @Samueru-sama for help with this.

Bugfixes and other:

- Don't repeat the song notification when enqueuing songs. A small but annoying bug that slipped into the last release.

- Fixed issue where kew sometimes couldn't find the cover image in the folder.

- Better handling of songs that cannot be initialized.

- Removed support for .mp4 files so as to not add a bunch of video folders to the music library. Thanks @yurivict for the suggestion.

- Made the makefile compatible with Void Linux. Thank you @itsdeadguy.

- Cursor was not reappearing in some cases on FreeBSD after program exited. Thank you @yurivict.

- Fixed slow loading UI on some machines, because of blocking desktop notification. Thanks @vdawg-git for reporting this.

Thank you to @vdawg-git for helping me test and debug!

### 2.7.2

- You can now remove the currently playing song from the playlist. Thank you @yurivict for the suggestion. You can then press space bar to play the next song in the list.

- Scrolling now stops immediately after the key is released.

- Better reset of the terminal at program exit.

- MPRIS widgets are now updated when switching songs while paused.

- When pressing update library ("u"), it now remembers which files are enqueued.

- No more ugly scroll back buffer in the terminal.

Btw, there is a bug in the KDE Media Player Widget which locks up plasmashell when certain songs   play (in any music player). If you are having that problem, I suggest not using that widget until you have plasmashell version 6.20 or later. Bug description: https://bugs.kde.org/show_bug.cgi?id=491946.

### 2.7.1

- Added missing #ifdef for libnotify. This fixes #157.

### 2.7

This release adds:
	- Complete and corrected MPRIS implementation and support of playerCtl, except for opening Uris through mpris.
	- Libnotify as a new optional dependency.
	- Fixes to many minor issues that have cropped up.

- Proper MPRIS and PlayerCtl support. Set volume, stop, seek and others now work as expected. You can also switch tracks while stopped or paused now. Everything should work except openUri and repeat playlist which are not available for now.

- New (optional) dependency: Libnotify. In practice, adding libnotify as a dependency means browsing through music will no longer make desktop notifications pile up, instead the one visible will update itself. Thank you, @kazemaksOG, this looks much better. kew uses libnotify only if you have it installed, so it should if possible be an optional thing during installation.

- Allows binding of other keys for the different ui views that you get with F2-F6.

- Removed the option to toggle covers on and off by pressing 'c'. This led to confusion.

- Removed build warning on systems with ffmpeg 4.4 installed.

- Only run one instance of kew at a time, thanks @werdahias for the suggestion.

- If you exit the kew with 0% volume, when you open it next time, volume will be at 10%. To avoid confusion.

- Handle SIGHUP not only SIGINT.

- Prints error message instead of crashing on Fedora (thanks @spambroo) when playing unsupported .m4a files. This problem is related to ffmpeg free/non-free versions. You need the non-free version.

- Fixed issue where special characters in the song title could cause mpris widgets to not work correctly.

### 2.6

- New command: "kew albums", similar to "kew all" but it queues one album randomly after the other. Thank you @the-boar for the suggestion.

- Fixed bug where sometimes kew couldn't find a suitable name for a playlist file (created by pressing x).

- Made it so that seeking jumps in more reasonable smaller increments when not in song view. Previously it could jump 30 seconds at a time.

- Rolled back code related to symlinked directories, it didn't work with freebsd, possibly others.

### 2.5.1

- Fixed bug where desktop notifications could stall the app if notify-send wasn't installed. Thank you @Visual-Dawg for reporting this and all the help testing!

- Search: Removed duplicate search result name variable. This means search results will now have a very low memory footprint.

- Symlinked directories should work better now. Works best if the symlink and the destination directory has the same name.

### 2.5

- Fuzzy search! Press F5 to search your library.

- You can now quit with Esc. Handy when you are in search view, because pressing 'q' will just add that letter to the search string.

- Fixed issue where after completing a playthrough of a playlist and then starting over, only the first song would be played.

- Fine tuning of the spectrum visualizer. Still not perfect but I think this one is better. I might be wrong though.

- Fixed issue where debian package tracker wasn't detecting LDFLAGS in the makefile.

- Made scrolling quicker.

### 2.4.4

- Fixed no sound playing when playing a flac or mp3 song twice, then enqueuing another.

- Don't save every change to the playlist when running the special playlist with 'kew .', only add the songs added by pressing '.'.

- Removed compiler warning and a few other minor fixes.

### 2.4.3

- Fixed covers not being perfectly square on some terminals.

- Fixed playlist selector could get 'stuck' after playing a long list.

- Code refactoring and minor improvements to playlist view.

- Moved the files kewrc and kewlibrary config files from XDG_CONFIG_HOME into XDG_CONFIG_HOME/kew/, typically ~/.config/kew.

### 2.4.2

- Fixed a few issues related to reading and writing the library.

### 2.4.1

- Improved album cover color mode. Press 'i' to try this.

- To accelerate startup times, there is now a library cache. This feature is optional and can be enabled in the settings file (kewrc). If the library loading process is slow, you'll be prompted to consider using the cache.

- You can now press 'u' to update the library in case you've added or removed songs.

- Faster "kew all". It now bases its playlist on the library instead of scanning everything a second time.

- Fixed when running the special playlist with "kew .", the app sometimes became unresponsive when adding / deleting.

- Code refactoring and cleanup.

### 2.4

- Much faster song loading/skipping.

- New settings: configurable colors. These are configured in the kewrc file (in ~/.config/ or wherever your config files are) with instructions there.

- New setting: hidehelp. Hides the help text on library view and playlist view.

- New setting: hidelogo. Prints the artist name as well as the song title at the top instead of a logo.

- Fixed an issue with shuffle that could lead to a crash.

- Fixed an issue where it could crash at the end of the playlist.

- Fixed an issue where in some types of music libraries you couldn't scroll all the way to the bottom.

- Fixed notifications not notifying on songs with spaces in cover art url.

- Fixed sometimes not being able to switch song.

- Further adjustments to the visualizer.

- .aac and .mp4 file support.

- New option: -q. Quits right after playing the playlist (same as --quitonstop).

- Improved help text.

### 2.3.1

- The visualizer now (finally!) works like it's supposed to for all formats.

- Proper clean up and restore cursor when using CTRL-C to quit the app.

- Don't refresh track view twice when skipping to the previous song.

### 2.3

- Notifications of currently playing song through notify-send. New setting: allowNotifications. Set to 0 to disable notifications.

- Fixed an issue that could lead to a crash when switching songs.

- Fixed an issue with switching opus songs that could lead to a crash.

- Plus other bug fixes.

### 2.2.1

- Fixed issue related to enqueuing/dequeuing the next song.

- Some adapting for FreeBSD.

### 2.2.1

- Fixed issue related to enqueuing/dequeuing the next song.

- Some adapting for FreeBSD.

### 2.2

- This update mostly contains improvements to stability.

- M4a file decoding is no longer done by calling ffmpeg externally, it's (finally) done like the other file formats. This should make kew more stable, responsive and it should consume less memory when playing m4a files.

- kew now starts the first time with your system volume as the volume, after that it remembers the last volume it exited with and uses that.

- kew now picks up and starts using the cover color without the user having to first go to track view.

### 2.1.1

- Fixed a few issues related to passing cover art url and length to mpris. Should now display cover and progress correctly in widgets on gnome/wayland.

### 2.0.4

- You can now add "-e" or "--exact" in your searches to return an exact (not case sensitive) match. This can be helpful when two albums have a similar name, and you want to specify you want one or the other.

  Example: kew -e basement popstar.

- Fixed issue where pressing del on the playlist changed view to track view.

### 2.0.3

- Fixed issue where sometimes the last of enqueued songs where being played instead of the first,

- F4 is bound to show track view, and shown on the last row, so that the track view isn't hidden from new users.

### 2.0.1

- New view: Library. Shows the contents of the music library. From there you can add songs to the playlist.

- Delete items from the playlist by pressing Del.

- You can flip through pages of the library or playlist by pressing Page Up and Page Down.

- Starting kew with no arguments now takes you to the library.

- After the playlist finishes playing, the library is shown, instead of the app exiting.

- To run kew with all songs shuffled like you could before, just type "kew all" in the terminal.

- Running kew  with the argument --quitonstop, enables the old behavior of exiting when finished.

- Removed the playlist duration counter. It caused problems when coupled with the new features of being able to remove and add songs while audio is playing.

- New ascii logo! This one takes up much less space.

- kew now shows which song is playing on top of the library and playlist views.

- Volume is now set at 50% at the start.

- Also many bug fixes.

### 1.11

- Now shows volume percentage.

- Fixed bug where on a small window size, the nerdfonts for seeking, repeat and shuffle when all three enabled could mess up the visualizer.

### 1.10

- Improved config file, with more information on how to make key bindings with special keys for instance.

- Changing the volume is now for just kew, not the master volume of your system.

- Switching songs now unpauses the player.

- Fixed issue of potential crash when uninitializing decoders.

### 1.9

- Fixed a potential dead-lock situation.

- Fixed one instance of wrong metadata/cover being displayed for a song on rare occasions.

- Fixed an issue that could lead to a crash when switching songs.

- Fixed issue of potential crash when closing audio file.

- Fixed playlist showing the previous track as the current one.

- Much improved memory allocation handling in visualizer.

- Playlist builder now ignores hidden directories.

### 1.8.1

- Fixed bugs relating to showing the playlist and seeking.

- Fixed bug where trying to seek on ogg files led to strange behavior. Now seeking in ogg is entirely disabled.

- Fixed bug where kew for no reason stopped playing audio but kept counting elapsed seconds.

- More colorful visualizer bars when using album cover colors.

### 1.8

- Visualizer bars now grow and decrease smoothly (if your terminal supports unicode).

### 1.7.4

- Kew is now interactive when paused.

- Fixed issue with crashing after a few plays with repeat enabled.

- Deletes cover images from cache after playing the file.

### 1.7.3

- Fixed issue with crash after seeking to the end of songs a few times. A lot was changed in 1.6.0 and 1.7.0 which led to some instability.

### 1.7.2

- Introduced Nerd Font glyphs for things like pause, repeat, fast forward and so on.

- More fixes.

### 1.7.1

- Fixes a few issues in 1.7.0

### 1.7.0

- Added decoders for ogg vorbis and opus. Seeking on ogg files doesn't yet work however.

### 1.6.0

- Now uses miniaudio's built-in decoders for mp3, flac and wav.

### 1.5.2

- Fix for unplayable songs.

### 1.5.1

- Misc issues with input handling resolved.

- Faster seeking.

### 1.5

- Name changed to kew to resolve a name conflict.

- Fixed bug with elapsed seconds being counted wrong if pausing multiple times.

- Fixed bug with segfault on exit.

### 1.4

- Seeking is now possible, with A and D.

- Config file now resides in your XDG_CONFIG_HOME, possibly ~/.config/kewrc. You can delete the one in your home folder after starting this version of cue once.

- Most key bindings are now configurable.

- Singing more visible in the visualizer.

- Better looking visualizer with smoother gradient.

- Misc fixes.

- You can no longer press A to add to kew.m3u. instead use period.

### 1.3

- Now skips drm'd files more gracefully.

- Improvements to thread safety and background process handling.

- Misc bug fixes.

- Using album colors is now the default again.

### 1.2

- It's now possible to scroll through the songs in the playlist menu.

- Unfortunately this means a few key binding changes. Adjusting volume has been changed to +, -, instead up and down arrow is used for scrolling in the playlist menu.

- h,l is now prev and next track (previously j and k). Now j,k is used for scrolling in the playlist menu.

- Added a better check that metadata is correct before printing it, hopefully this fixes an occasional but annoying bug where the wrong metadata was sometimes displayed.

- Using profile/theme colors is now the default choice for colors.

### 1.1

- Everything is now centered around the cover and left-aligned within that space.

- Better visibility for text on white backgrounds. If colors are still too bright you can always press "i" and use the terminal colors, for now.

- Playlist is now F2 and key bindings is F3 to help users who are using the terminator terminal and other terminals who might have help on the F1 key.

- Now looks better in cases where there is no metadata and/or when there is no cover.

- The window refreshes faster after resize.

### 1.0.9

- More colorful. It should be rarer for the color derived from the album cover to be gray/white

- Press I to toggle using colors from the album cover or using colors from your terminal color scheme.

- Smoother color transition on visualizer.

### 1.0.8

#### Features:

- New Setting: useProfileColors. If set to 1 will match cue with your terminal profile colors instead of the album cover which remains the default.

- It is now possible to switch songs a little quicker.

- It's now faster (instant) to switch to playlist and key bindings views.

#### Bug Fixes:

- Skip to numbered song wasn't clearing the number array correctly.

- Rapid typing of a song number wasn't being read correctly..
### 1.0.7

- Fixed a bug where mpris stuff wasn't being cleaned up correctly.

- More efficient printing to screen.

- Better (refactored) cleanup.

### 1.0.6

- Fixed a bug where mpris stuff wasn't being cleaned up correctly

### 1.0.5

- Added a slow decay to the bars and made some other changes that should make the visualizer appear better, but this is all still experimental.

- Some more VIM key bindings:
	 100G or 100g or 100+ENTER -> go to song 100
	 gg -> go to first song
	 G -> go to last song

### 1.0.4

- Added man pages.

- Added a few VIM key bindings (h,j,k,l) to use instead of arrow keys.

- Shuffle now behaves like in other players, and works with MPRIS. Previously the list would be reordered, instead of the song just jumping from place to place, in the same list. Starting cue with 'cue shuffle <text>' still works the old way.

- Now prints a R or S on the last line when repeat or shuffle is enabled.

### 1.0.3

- cue should now cleanly skip files it cannot play. Previously this caused instability to the app and made it become unresponsive.

- Fixed a bug where the app sometimes became unresponsive, in relation to pausing/unpausing and pressing buttons while paused.

### 1.0.2:

- Added support for MPRIS, which is the protocol used on Linux systems for controlling media players.

- Added --nocover option.

- Added --noui option. When it's used cue doesn't print anything to screen.

- Now you can press K to see key bindings.

- Fixed issue with long files being cut off.

- Hiding cover and changing other settings, now clears the screen first.

- Fixed installscript for opensuse.

- Feature or bug? You can no longer raise the volume above 100%

- New dependency: glib2. this was already required because chafa requires it, but it is now a direct dependency of cue.
