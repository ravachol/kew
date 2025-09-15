# CONTRIBUTING

## Welcome to kew contributing guide

Thank you for your interest in contributing to kew!

### Goal of the project

The goal of kew is to provide a quick and easy way for people to listen to music with the absolute minimum of inconvenience.
It's a small app, limited in scope and it shouldn't be everything to all people. It should continue to be a very light weight app.
For instance, it's not imagined as a software for dj'ing or as a busy music file manager with all the features.

We want to keep the codebase easy to manage and free of bloat, so might reject a feature out of that reason only.

### Bugs

Please report any bugs directly on github, with as much relevant detail as possible.
If there's a crash or stability issue, the audio file details are interesting, but also the details of the previous and next file on the playlist. You can extract these details by running:
ffprobe -i AUDIO FILE -show_streams -select_streams a:0 -v quiet -print_format json

### Create a pull request

After making any changes, open a pull request on Github.

- Please contact me (kew-player@proton.me) before doing a big change, or risk the whole thing getting rejected.

- Try to keep commits fairly small so that they are easy to review.

- If you're fixing a particular bug in the issue list, please explicitly say "Fixes #" in your description".

Once your PR has been reviewed and merged, you will be proudly listed as a contributor in the [contributor chart](https://github.com/ravachol/kew/graphs/contributors)!

### Issue assignment

We don't have a process for assigning issues to contributors. Please feel free to jump into any issues in this repo that you are able to help with. Our intention is to encourage anyone to help without feeling burdened by an assigned task. Life can sometimes get in the way, and we don't want to leave contributors feeling obligated to complete issues when they may have limited time or unexpected commitments.

We also recognize that not having a process could lead to competing or duplicate PRs. There's no perfect solution here. We encourage you to communicate early and often on an Issue to indicate that you're actively working on it. If you see that an Issue already has a PR, try working with that author instead of drafting your own.
