## How to make your own kew

kew provides a very simple and minimal way to make kew look the way you want it.

## Version first

First, every layout file needs to have a version. Otherwise it gets backed up and replaced by the current vanilla layout.

The version needs to be the same as the one in the vanilla current.layout. The version will only change if there are breaking changes.


## The Basics

Put panes inside rows. A pane can have a child layout.


## Rows

A row has a height size, a starting column and numbered panes.


## Panes

A pane has a component, an optional dirty flag (when is it redrawn), a width size, and optionally a child layout.

There's also offsetX and offsetY.


## Size

Size values are:

fixed:value

percent:value

auto (fills available space, or divides it among other auto sized regions)

indent (the indentation kew is normally adjusted with)

indent_wide  (the extended indentation when using a side cover)

from_width (sets the height from width)

from_height (sets the width from the height)

window_minus:value (the full width or height minus the value)


## Dirty values

Dirty values determine what makes this region re-render. The allowed values are:

visualizer, progress, song, playlist, library, search, footer, layout


## Components

Every region that does anything is a component. The allowed components are:

empty, logo, now_playing, now_playing_and_artist, footer, error_row,

side_cover, cover, cover_centered, landscape_cover

playlist_header, playlist_rows,

library_header, library_rows

track, track_portrait, track_header, track_portrait_normal, track_portrait_lyrics,

track_landscape_normal, track_landscape_lyrics, track_landscape, lyrics_page

metadata, time, timestamped_lyrics, vis_and_progress_bar, visualizer, progress_bar

search, search_results, search_header, search_box

help, version


## Example

The following is a library with the logo, a header, the library itself, an error row and the footer. See the included .layout files for more examples.

```
[library]

row
height=fixed:4
col=0

pane
component=logo
dirty=song
width=auto
offsetX=1

row
height=fixed:4
col=0

pane
component=library_header
dirty=all
width=auto
offsetX=2

row
height=auto
col=0

pane
component=library_rows
dirty=library
width=auto

row
height=fixed:1
col=0

pane
component=error_row
dirty=footer
width=auto

row
height=fixed:1
col=0

pane
component=footer
dirty=footer
width=auto
```

