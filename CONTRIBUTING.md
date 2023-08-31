# Welcome to cue music contributing guide

Thanks for stopping by, here are some points about contributing to cue.

### Goal of the project

The goal of cue is to provide a quick and easy way for people to listen to music with the absolute minimum fuss and bother. 
It's a small app, limited in scope and it shouldn't be everything to all people. It should continue to be a very light weight app.
For instance, it's not imagined as a software for dj'ing or as a busy music file manager with all the features. 

We want to keep the codebase easy to manage and free of bloat, so might reject a feature out of that reason only.

### Bugs

Please report any bugs directly on github, with as much relevant detail as possible. 
If there's a crash or stability issue, the audio file details are interesting, but also the details of the previous and next file on the playlist. You can extract these details by running:
ffprobe -i AUDIO FILE -show_streams -select_streams a:0 -v quiet -print_format json

### Pull Requests

- Please contact me (ravacholf@proton.me) before doing a big change, or risk the whole thing getting rejected. 

- Try to keep commits fairly small so that they are easy to review.

- If you can, use https://editorconfig.org/. There is a file with settings for it: .editorconfig.
