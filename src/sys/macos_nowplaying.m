/**
 * @file macos_nowplaying.m
 * @brief macOS Now Playing integration via MediaPlayer framework.
 *
 * Implements the C-callable bridge to macOS Objective-C APIs for
 * MPNowPlayingInfoCenter and MPRemoteCommandCenter. This enables
 * media key support, Control Center metadata display, AirPods
 * controls, and Touch Bar integration.
 */

#import <AppKit/AppKit.h>
#import <MediaPlayer/MediaPlayer.h>

#include "macos_nowplaying.h"

#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playlist_ops.h"

void macos_process_events(void)
{
        @autoreleasepool {
                NSEvent *event;
                while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                   untilDate:nil
                                                      inMode:NSDefaultRunLoopMode
                                                     dequeue:YES])) {
                        [NSApp sendEvent:event];
                }
        }
}

void init_macos_nowplaying(void)
{
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

        MPRemoteCommandCenter *commandCenter =
            [MPRemoteCommandCenter sharedCommandCenter];

        [commandCenter.playCommand addTargetWithHandler:
                                       ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
                                         (void)event;
                                         play();
                                         return MPRemoteCommandHandlerStatusSuccess;
                                       }];

        [commandCenter.pauseCommand addTargetWithHandler:
                                        ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
                                          (void)event;
                                          pause_song();
                                          return MPRemoteCommandHandlerStatusSuccess;
                                        }];

        [commandCenter.togglePlayPauseCommand addTargetWithHandler:
                                                  ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
                                                    (void)event;
                                                    ops_toggle_pause();
                                                    return MPRemoteCommandHandlerStatusSuccess;
                                                  }];

        [commandCenter.stopCommand addTargetWithHandler:
                                       ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
                                         (void)event;
                                         stop();
                                         return MPRemoteCommandHandlerStatusSuccess;
                                       }];

        [commandCenter.nextTrackCommand addTargetWithHandler:
                                            ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
                                              (void)event;
                                              skip_to_next_song();
                                              return MPRemoteCommandHandlerStatusSuccess;
                                            }];

        [commandCenter.previousTrackCommand addTargetWithHandler:
                                                ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
                                                  (void)event;
                                                  skip_to_prev_song();
                                                  return MPRemoteCommandHandlerStatusSuccess;
                                                }];

        [commandCenter.changePlaybackPositionCommand addTargetWithHandler:
                                                         ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
                                                           MPChangePlaybackPositionCommandEvent *positionEvent =
                                                               (MPChangePlaybackPositionCommandEvent *)event;
                                                           gint64 position_usec =
                                                               (gint64)(positionEvent.positionTime * G_USEC_PER_SEC);
                                                           set_position(position_usec, get_current_song_duration());
                                                           return MPRemoteCommandHandlerStatusSuccess;
                                                         }];
}

void cleanup_macos_nowplaying(void)
{
        MPRemoteCommandCenter *commandCenter =
            [MPRemoteCommandCenter sharedCommandCenter];

        [commandCenter.playCommand removeTarget:nil];
        [commandCenter.pauseCommand removeTarget:nil];
        [commandCenter.togglePlayPauseCommand removeTarget:nil];
        [commandCenter.stopCommand removeTarget:nil];
        [commandCenter.nextTrackCommand removeTarget:nil];
        [commandCenter.previousTrackCommand removeTarget:nil];
        [commandCenter.changePlaybackPositionCommand removeTarget:nil];

        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
}

void macos_set_now_playing_info(const char *title, const char *artist,
                                const char *album, const char *cover_art_path,
                                double duration)
{
        @autoreleasepool {
                NSMutableDictionary *nowPlayingInfo =
                    [NSMutableDictionary dictionary];

                if (title)
                        nowPlayingInfo[MPMediaItemPropertyTitle] =
                            [NSString stringWithUTF8String:title];

                if (artist)
                        nowPlayingInfo[MPMediaItemPropertyArtist] =
                            [NSString stringWithUTF8String:artist];

                if (album)
                        nowPlayingInfo[MPMediaItemPropertyAlbumTitle] =
                            [NSString stringWithUTF8String:album];

                nowPlayingInfo[MPMediaItemPropertyPlaybackDuration] =
                    @(duration);
                nowPlayingInfo[MPNowPlayingInfoPropertyElapsedPlaybackTime] =
                    @(0.0);
                nowPlayingInfo[MPNowPlayingInfoPropertyPlaybackRate] = @(0.0);

                if (cover_art_path && cover_art_path[0] != '\0') {
                        NSString *path =
                            [NSString stringWithUTF8String:cover_art_path];
                        NSImage *image =
                            [[NSImage alloc] initWithContentsOfFile:path];
                        if (image) {
                                MPMediaItemArtwork *artwork =
                                    [[MPMediaItemArtwork alloc]
                                        initWithBoundsSize:image.size
                                            requestHandler:^NSImage *(CGSize size) {
                                              (void)size;
                                              return image;
                                            }];
                                nowPlayingInfo[MPMediaItemPropertyArtwork] =
                                    artwork;
                        }
                }

                [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo =
                    nowPlayingInfo;
        }
}

void macos_update_playback_position(double position_seconds)
{
        @autoreleasepool {
                NSMutableDictionary *nowPlayingInfo =
                    [[MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo
                            mutableCopy];
                if (nowPlayingInfo) {
                        nowPlayingInfo
                            [MPNowPlayingInfoPropertyElapsedPlaybackTime] =
                                @(position_seconds);
                        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo =
                            nowPlayingInfo;
                }
        }
}

void macos_set_playback_state_playing(void)
{
        @autoreleasepool {
                [MPNowPlayingInfoCenter defaultCenter].playbackState =
                    MPNowPlayingPlaybackStatePlaying;
                NSMutableDictionary *nowPlayingInfo =
                    [[MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo
                            mutableCopy];
                if (nowPlayingInfo) {
                        nowPlayingInfo
                            [MPNowPlayingInfoPropertyPlaybackRate] =
                                @(1.0);
                        nowPlayingInfo
                            [MPNowPlayingInfoPropertyElapsedPlaybackTime] =
                                @(get_elapsed_seconds());
                        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo =
                            nowPlayingInfo;
                }
        }
}

void macos_set_playback_state_paused(void)
{
        @autoreleasepool {
                [MPNowPlayingInfoCenter defaultCenter].playbackState =
                    MPNowPlayingPlaybackStatePaused;
                NSMutableDictionary *nowPlayingInfo =
                    [[MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo
                            mutableCopy];
                if (nowPlayingInfo) {
                        nowPlayingInfo
                            [MPNowPlayingInfoPropertyPlaybackRate] =
                                @(0.0);
                        nowPlayingInfo
                            [MPNowPlayingInfoPropertyElapsedPlaybackTime] =
                                @(get_elapsed_seconds());
                        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo =
                            nowPlayingInfo;
                }
        }
}

void macos_set_playback_state_stopped(void)
{
        @autoreleasepool {
                [MPNowPlayingInfoCenter defaultCenter].playbackState =
                    MPNowPlayingPlaybackStateStopped;
                NSMutableDictionary *nowPlayingInfo =
                    [[MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo
                            mutableCopy];
                if (nowPlayingInfo) {
                        nowPlayingInfo
                            [MPNowPlayingInfoPropertyPlaybackRate] =
                                @(0.0);
                        nowPlayingInfo
                            [MPNowPlayingInfoPropertyElapsedPlaybackTime] =
                                @(get_elapsed_seconds());
                        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo =
                            nowPlayingInfo;
                }
        }
}
