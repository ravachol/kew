/**
 * @file settings.[c]
 * @brief Application settings management.
 *
 * Loads, saves, and applies configuration settings such as
 * playback behavior, UI preferences, and library paths.
 */

#include "settings.h"

#include "common/appstate.h"

#include "playback_state.h"
#include "input.h"

#include "sound/soundcommon.h"

#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <wchar.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

const char SETTINGS_FILE[] = "kewrc";

time_t lastTimeAppRan;

void enableMouse(UISettings *ui)
{
        if (ui->mouseEnabled)
                enableTerminalMouseButtons();
}

void initSettings(AppSettings *settings)
{
        AppState *state = getAppState();
        UserData *userData = playbackGetUserData();

        getConfig(settings, &(state->uiSettings));

        userData->replayGainCheckFirst =
            state->uiSettings.replayGainCheckFirst;

        initKeyMappings(settings);
        enableMouse(&(state->uiSettings));
}

void freeKeyValuePairs(KeyValuePair *pairs, int count)
{
        if (pairs == NULL || count <= 0)
        {
                return;
        }

        for (size_t i = 0; i < (size_t)count; i++)
        {
                free(pairs[i].key);
                free(pairs[i].value);
        }

        free(pairs);
}

AppSettings constructAppSettings(KeyValuePair *pairs, int count)
{
        AppSettings settings;
        memset(&settings, 0, sizeof(settings));
        c_strcpy(settings.coverEnabled, "1", sizeof(settings.coverEnabled));
        c_strcpy(settings.allowNotifications, "1",
                 sizeof(settings.allowNotifications));
        c_strcpy(settings.coverAnsi, "0", sizeof(settings.coverAnsi));
        c_strcpy(settings.quitAfterStopping, "0",
                 sizeof(settings.quitAfterStopping));
        c_strcpy(settings.hideGlimmeringText, "0",
                 sizeof(settings.hideGlimmeringText));
        c_strcpy(settings.mouseEnabled, "1", sizeof(settings.mouseEnabled));
        c_strcpy(settings.replayGainCheckFirst, "0",
                 sizeof(settings.replayGainCheckFirst));
        c_strcpy(settings.visualizerBarWidth, "2",
                 sizeof(settings.visualizerBarWidth));
        c_strcpy(settings.visualizerBrailleMode, "0",
                 sizeof(settings.visualizerBrailleMode));
        c_strcpy(settings.progressBarElapsedEvenChar, "━",
                 sizeof(settings.progressBarElapsedEvenChar));
        c_strcpy(settings.progressBarElapsedOddChar, "━",
                 sizeof(settings.progressBarElapsedOddChar));
        c_strcpy(settings.progressBarApproachingEvenChar, "━",
                 sizeof(settings.progressBarApproachingEvenChar));
        c_strcpy(settings.progressBarApproachingOddChar, "━",
                 sizeof(settings.progressBarApproachingOddChar));
        c_strcpy(settings.progressBarCurrentEvenChar, "━",
                 sizeof(settings.progressBarCurrentEvenChar));
        c_strcpy(settings.progressBarCurrentOddChar, "━",
                 sizeof(settings.progressBarCurrentOddChar));
        c_strcpy(settings.saveRepeatShuffleSettings, "1",
                 sizeof(settings.saveRepeatShuffleSettings));
        c_strcpy(settings.trackTitleAsWindowTitle, "1",
                 sizeof(settings.trackTitleAsWindowTitle));
#ifdef __APPLE__
        // Visualizer looks wonky in default terminal but let's enable it
        // anyway. People need to switch
        c_strcpy(settings.visualizerEnabled, "1",
                 sizeof(settings.visualizerEnabled));
        c_strcpy(settings.colorMode, "0",
                 sizeof(settings.colorMode));
#else
        c_strcpy(settings.visualizerEnabled, "1",
                 sizeof(settings.visualizerEnabled));
        c_strcpy(settings.colorMode, "1",
                 sizeof(settings.colorMode));
#endif
#ifdef __ANDROID__
        c_strcpy(settings.hideLogo, "1", sizeof(settings.hideLogo));
#else
        c_strcpy(settings.hideLogo, "0", sizeof(settings.hideLogo));
#endif
        c_strcpy(settings.hideHelp, "0", sizeof(settings.hideHelp));
        c_strcpy(settings.cacheLibrary, "-1", sizeof(settings.cacheLibrary));
        c_strcpy(settings.visualizerHeight, "6",
                 sizeof(settings.visualizerHeight));
        c_strcpy(settings.visualizerColorType, "2",
                 sizeof(settings.visualizerColorType));
        c_strcpy(settings.titleDelay, "9", sizeof(settings.titleDelay));
        c_strcpy(settings.nextView, "\t", sizeof(settings.nextView));
        c_strcpy(settings.prevView, "[Z", sizeof(settings.prevView));
        c_strcpy(settings.volumeUp, "+", sizeof(settings.volumeUp));
        c_strcpy(settings.volumeUpAlt, "=", sizeof(settings.volumeUpAlt));
        c_strcpy(settings.volumeDown, "-", sizeof(settings.volumeDown));
        c_strcpy(settings.previousTrackAlt, "h",
                 sizeof(settings.previousTrackAlt));
        c_strcpy(settings.nextTrackAlt, "l", sizeof(settings.nextTrackAlt));
        c_strcpy(settings.scrollUpAlt, "k", sizeof(settings.scrollUpAlt));
        c_strcpy(settings.scrollDownAlt, "j", sizeof(settings.scrollDownAlt));
        c_strcpy(settings.switchNumberedSong, "G",
                 sizeof(settings.switchNumberedSong));
        c_strcpy(settings.cycleColorsDerivedFrom, "i",
                 sizeof(settings.cycleColorsDerivedFrom));
        c_strcpy(settings.cycleThemes, "t",
                 sizeof(settings.cycleThemes));
        c_strcpy(settings.toggleNotifications, "n",
                 sizeof(settings.toggleNotifications));
        c_strcpy(settings.toggleVisualizer, "v",
                 sizeof(settings.toggleVisualizer));
        c_strcpy(settings.toggleAscii, "b", sizeof(settings.toggleAscii));
        c_strcpy(settings.toggleRepeat, "r", sizeof(settings.toggleRepeat));
        c_strcpy(settings.toggleShuffle, "s", sizeof(settings.toggleShuffle));
        c_strcpy(settings.togglePause, "p", sizeof(settings.togglePause));
        c_strcpy(settings.seekBackward, "a", sizeof(settings.seekBackward));
        c_strcpy(settings.seekForward, "d", sizeof(settings.seekForward));
        c_strcpy(settings.savePlaylist, "x", sizeof(settings.savePlaylist));
        c_strcpy(settings.updateLibrary, "u", sizeof(settings.updateLibrary));
        c_strcpy(settings.addToFavoritesPlaylist, ".",
                 sizeof(settings.addToFavoritesPlaylist));
        c_strcpy(settings.hardPlayPause, " ", sizeof(settings.hardPlayPause));
        c_strcpy(settings.hardSwitchNumberedSong, "\n",
                 sizeof(settings.hardSwitchNumberedSong));
        c_strcpy(settings.hardPrev, "[D", sizeof(settings.hardPrev));
        c_strcpy(settings.hardNext, "[C", sizeof(settings.hardNext));
        c_strcpy(settings.hardScrollUp, "[A", sizeof(settings.hardScrollUp));
        c_strcpy(settings.hardScrollDown, "[B",
                 sizeof(settings.hardScrollDown));
        c_strcpy(settings.hardShowPlaylist, "OQ",
                 sizeof(settings.hardShowPlaylist));
        c_strcpy(settings.hardShowPlaylistAlt, "[[B",
                 sizeof(settings.hardShowPlaylistAlt));

        c_strcpy(settings.hardShowKeys, "[17~", sizeof(settings.hardShowKeys));
        c_strcpy(settings.hardShowKeysAlt, "[17~",
                 sizeof(settings.hardShowKeysAlt));
#if defined(__ANDROID__) || defined(__APPLE__)
        c_strcpy(settings.showPlaylistAlt, "Z",
                 sizeof(settings.showPlaylistAlt));
        c_strcpy(settings.showTrackAlt, "C", sizeof(settings.showTrackAlt));
        c_strcpy(settings.showLibraryAlt, "X", sizeof(settings.showLibraryAlt));
        c_strcpy(settings.showSearchAlt, "V", sizeof(settings.showSearchAlt));
        c_strcpy(settings.showKeysAlt, "B", sizeof(settings.showKeysAlt));
#endif
        c_strcpy(settings.hardShowTrack, "OS", sizeof(settings.hardShowTrack));
        c_strcpy(settings.hardShowTrackAlt, "[[D",
                 sizeof(settings.hardShowTrackAlt));

        c_strcpy(settings.hardShowLibrary, "OR",
                 sizeof(settings.hardShowLibrary));
        c_strcpy(settings.hardShowLibraryAlt, "[[C",
                 sizeof(settings.hardShowLibraryAlt));

        c_strcpy(settings.hardShowSearch, "[15~",
                 sizeof(settings.hardShowSearch));
        c_strcpy(settings.hardShowSearchAlt, "[[E",
                 sizeof(settings.hardShowSearchAlt));

        c_strcpy(settings.nextPage, "[6~", sizeof(settings.nextPage));
        c_strcpy(settings.prevPage, "[5~", sizeof(settings.prevPage));
        c_strcpy(settings.hardRemove, "[3~", sizeof(settings.hardRemove));
        c_strcpy(settings.hardRemove2, "[P", sizeof(settings.hardRemove2));
        c_strcpy(settings.mouseLeftClick, "[<0",
                 sizeof(settings.mouseLeftClick));
        c_strcpy(settings.mouseMiddleClick, "[<1",
                 sizeof(settings.mouseMiddleClick));
        c_strcpy(settings.mouseRightClick, "[<2",
                 sizeof(settings.mouseRightClick));
        c_strcpy(settings.mouseScrollUp, "[<64",
                 sizeof(settings.mouseScrollUp));
        c_strcpy(settings.mouseScrollDown, "[<65",
                 sizeof(settings.mouseScrollDown));
        c_strcpy(settings.mouseAltScrollUp, "[<72",
                 sizeof(settings.mouseAltScrollUp));
        c_strcpy(settings.mouseAltScrollDown, "[<73",
                 sizeof(settings.mouseAltScrollDown));
        c_strcpy(settings.lastVolume, "100", sizeof(settings.lastVolume));
        c_strcpy(settings.color, "6", sizeof(settings.color));
        c_strcpy(settings.artistColor, "6", sizeof(settings.artistColor));
        c_strcpy(settings.titleColor, "6", sizeof(settings.titleColor));
        c_strcpy(settings.enqueuedColor, "6", sizeof(settings.enqueuedColor));
        c_strcpy(settings.mouseLeftClickAction, "0",
                 sizeof(settings.mouseLeftClickAction));
        c_strcpy(settings.mouseMiddleClickAction, "1",
                 sizeof(settings.mouseMiddleClickAction));
        c_strcpy(settings.mouseRightClickAction, "2",
                 sizeof(settings.mouseRightClickAction));
        c_strcpy(settings.mouseScrollUpAction, "3",
                 sizeof(settings.mouseScrollUpAction));
        c_strcpy(settings.mouseScrollDownAction, "4",
                 sizeof(settings.mouseScrollDownAction));
        c_strcpy(settings.mouseAltScrollUpAction, "7",
                 sizeof(settings.mouseAltScrollUpAction));
        c_strcpy(settings.mouseAltScrollDownAction, "8",
                 sizeof(settings.mouseAltScrollDownAction));
        c_strcpy(settings.moveSongUp, "f", sizeof(settings.moveSongUp));
        c_strcpy(settings.moveSongDown, "g", sizeof(settings.moveSongDown));
        c_strcpy(settings.enqueueAndPlay, "^M",
                 sizeof(settings.enqueueAndPlay));
        c_strcpy(settings.hardStop, "S", sizeof(settings.hardStop));
        c_strcpy(settings.sortLibrary, "o", sizeof(settings.sortLibrary));
        c_strcpy(settings.quit, "q", sizeof(settings.quit));
        c_strcpy(settings.altQuit, "\x1B", sizeof(settings.altQuit));
        c_strcpy(settings.hardClearPlaylist, "\b",
                 sizeof(settings.hardClearPlaylist));
        c_strcpy(settings.showLyricsPage, "m",
                 sizeof(settings.showLyricsPage));

        memcpy(settings.ansiTheme, "default", 8);

        if (pairs == NULL)
        {
                return settings;
        }

        bool foundCycleThemesSetting = false;

        for (int i = 0; i < count; i++)
        {
                KeyValuePair *pair = &pairs[i];

                char *lowercaseKey = stringToLower(pair->key);

                if (strcmp(lowercaseKey, "path") == 0)
                {
                        snprintf(settings.path, sizeof(settings.path), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "theme") == 0)
                {
                        snprintf(settings.theme, sizeof(settings.theme), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "coverenabled") == 0)
                {
                        snprintf(settings.coverEnabled,
                                 sizeof(settings.coverEnabled), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "coveransi") == 0)
                {
                        snprintf(settings.coverAnsi, sizeof(settings.coverAnsi),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "visualizerenabled") == 0)
                {
                        snprintf(settings.visualizerEnabled,
                                 sizeof(settings.visualizerEnabled), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "useconfigcolors") == 0)
                {
                        snprintf(settings.useConfigColors,
                                 sizeof(settings.useConfigColors), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "visualizerheight") == 0)
                {
                        snprintf(settings.visualizerHeight,
                                 sizeof(settings.visualizerHeight), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "visualizercolortype") == 0)
                {
                        snprintf(settings.visualizerColorType,
                                 sizeof(settings.visualizerColorType), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "titledelay") == 0)
                {
                        snprintf(settings.titleDelay,
                                 sizeof(settings.titleDelay), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "volumeup") == 0)
                {
                        snprintf(settings.volumeUp, sizeof(settings.volumeUp),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "volumeupalt") == 0)
                {
                        snprintf(settings.volumeUpAlt,
                                 sizeof(settings.volumeUpAlt), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "volumedown") == 0)
                {
                        snprintf(settings.volumeDown,
                                 sizeof(settings.volumeDown), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "previoustrackalt") == 0)
                {
                        snprintf(settings.previousTrackAlt,
                                 sizeof(settings.previousTrackAlt), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "nexttrackalt") == 0)
                {
                        snprintf(settings.nextTrackAlt,
                                 sizeof(settings.nextTrackAlt), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "scrollupalt") == 0)
                {
                        snprintf(settings.scrollUpAlt,
                                 sizeof(settings.scrollUpAlt), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "scrolldownalt") == 0)
                {
                        snprintf(settings.scrollDownAlt,
                                 sizeof(settings.scrollDownAlt), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "switchnumberedsong") == 0)
                {
                        snprintf(settings.switchNumberedSong,
                                 sizeof(settings.switchNumberedSong), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "togglepause") == 0)
                {
                        snprintf(settings.togglePause,
                                 sizeof(settings.togglePause), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "togglecolorsderivedfrom") == 0)
                {
                        snprintf(settings.cycleColorsDerivedFrom,
                                 sizeof(settings.cycleColorsDerivedFrom), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "cyclethemes") == 0)
                {
                        snprintf(settings.cycleThemes,
                                 sizeof(settings.cycleThemes), "%s",
                                 pair->value);
                        foundCycleThemesSetting = true;
                }
                else if (strcmp(lowercaseKey, "togglenotifications") == 0)
                {
                        snprintf(settings.toggleNotifications,
                                 sizeof(settings.toggleNotifications), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "togglevisualizer") == 0)
                {
                        snprintf(settings.toggleVisualizer,
                                 sizeof(settings.toggleVisualizer), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "toggleascii") == 0)
                {
                        snprintf(settings.toggleAscii,
                                 sizeof(settings.toggleAscii), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "togglerepeat") == 0)
                {
                        snprintf(settings.toggleRepeat,
                                 sizeof(settings.toggleRepeat), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "toggleshuffle") == 0)
                {
                        snprintf(settings.toggleShuffle,
                                 sizeof(settings.toggleShuffle), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "seekbackward") == 0)
                {
                        snprintf(settings.seekBackward,
                                 sizeof(settings.seekBackward), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "seekforward") == 0)
                {
                        snprintf(settings.seekForward,
                                 sizeof(settings.seekForward), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "saveplaylist") == 0)
                {
                        snprintf(settings.savePlaylist,
                                 sizeof(settings.savePlaylist), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "addtofavoritesplaylist") == 0)
                {
                        snprintf(settings.addToFavoritesPlaylist,
                                 sizeof(settings.addToFavoritesPlaylist), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "lastvolume") == 0)
                {
                        snprintf(settings.lastVolume,
                                 sizeof(settings.lastVolume), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "allownotifications") == 0)
                {
                        snprintf(settings.allowNotifications,
                                 sizeof(settings.allowNotifications), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "colormode") == 0)
                {
                        snprintf(settings.colorMode, sizeof(settings.colorMode), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "color") == 0)
                {
                        snprintf(settings.color, sizeof(settings.color), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "artistcolor") == 0)
                {
                        snprintf(settings.artistColor,
                                 sizeof(settings.artistColor), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "enqueuedcolor") == 0)
                {
                        snprintf(settings.enqueuedColor,
                                 sizeof(settings.enqueuedColor), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "titlecolor") == 0)
                {
                        snprintf(settings.titleColor,
                                 sizeof(settings.titleColor), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mouseenabled") == 0)
                {
                        snprintf(settings.mouseEnabled,
                                 sizeof(settings.mouseEnabled), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "repeatstate") == 0)
                {
                        snprintf(settings.repeatState,
                                 sizeof(settings.repeatState), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "shuffleenabled") == 0)
                {
                        snprintf(settings.shuffleEnabled,
                                 sizeof(settings.shuffleEnabled), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "saverepeatshufflesettings") == 0)
                {
                        snprintf(settings.saveRepeatShuffleSettings,
                                 sizeof(settings.saveRepeatShuffleSettings),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "tracktitleaswindowtitle") == 0)
                {
                        snprintf(settings.trackTitleAsWindowTitle,
                                 sizeof(settings.trackTitleAsWindowTitle), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "replaygaincheckfirst") == 0)
                {
                        snprintf(settings.replayGainCheckFirst,
                                 sizeof(settings.replayGainCheckFirst), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "visualizerbarwidth") == 0)
                {
                        snprintf(settings.visualizerBarWidth,
                                 sizeof(settings.visualizerBarWidth), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "visualizerbraillemode") == 0)
                {
                        snprintf(settings.visualizerBrailleMode,
                                 sizeof(settings.visualizerBrailleMode), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mouseleftclickaction") == 0)
                {
                        snprintf(settings.mouseLeftClickAction,
                                 sizeof(settings.mouseLeftClickAction), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mousemiddleclickaction") == 0)
                {
                        snprintf(settings.mouseMiddleClickAction,
                                 sizeof(settings.mouseMiddleClickAction), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mouserightclickaction") == 0)
                {
                        snprintf(settings.mouseRightClickAction,
                                 sizeof(settings.mouseRightClickAction), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mousescrollupaction") == 0)
                {
                        snprintf(settings.mouseScrollUpAction,
                                 sizeof(settings.mouseScrollUpAction), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mousescrolldownaction") == 0)
                {
                        snprintf(settings.mouseScrollDownAction,
                                 sizeof(settings.mouseScrollDownAction), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mousealtscrollupaction") == 0)
                {
                        snprintf(settings.mouseAltScrollUpAction,
                                 sizeof(settings.mouseAltScrollUpAction), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "mousealtscrolldownaction") == 0)
                {
                        snprintf(settings.mouseAltScrollDownAction,
                                 sizeof(settings.mouseAltScrollDownAction),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "hidelogo") == 0)
                {
                        snprintf(settings.hideLogo, sizeof(settings.hideLogo),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "hidehelp") == 0)
                {
                        snprintf(settings.hideHelp, sizeof(settings.hideHelp),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "cachelibrary") == 0)
                {
                        snprintf(settings.cacheLibrary,
                                 sizeof(settings.cacheLibrary), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "quitonstop") == 0)
                {
                        snprintf(settings.quitAfterStopping,
                                 sizeof(settings.quitAfterStopping), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "hideglimmeringtext") == 0)
                {
                        snprintf(settings.hideGlimmeringText,
                                 sizeof(settings.hideGlimmeringText), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "quit") == 0)
                {
                        snprintf(settings.quit, sizeof(settings.quit), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "altquit") == 0)
                {
                        snprintf(settings.altQuit, sizeof(settings.altQuit),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "prevpage") == 0)
                {
                        snprintf(settings.prevPage, sizeof(settings.prevPage),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "nextpage") == 0)
                {
                        snprintf(settings.nextPage, sizeof(settings.nextPage),
                                 "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "updatelibrary") == 0)
                {
                        snprintf(settings.updateLibrary,
                                 sizeof(settings.updateLibrary), "%s",
                                 pair->value);
                }
                else if (strcmp(lowercaseKey, "showplaylistalt") == 0)
                {
                        if (strcmp(pair->value, "") !=
                            0) // Don't set these to nothing
                                snprintf(settings.showPlaylistAlt,
                                         sizeof(settings.showPlaylistAlt), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "showlibraryalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showLibraryAlt,
                                         sizeof(settings.showLibraryAlt), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "showtrackalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showTrackAlt,
                                         sizeof(settings.showTrackAlt), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "showsearchalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showSearchAlt,
                                         sizeof(settings.showSearchAlt), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "showlyricspage") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showLyricsPage,
                                         sizeof(settings.showLyricsPage), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "movesongup") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.moveSongUp,
                                         sizeof(settings.moveSongUp), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "movesongdown") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.moveSongDown,
                                         sizeof(settings.moveSongDown), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "enqueueandplay") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.enqueueAndPlay,
                                         sizeof(settings.enqueueAndPlay), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "sort") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.sortLibrary,
                                         sizeof(settings.sortLibrary), "%s",
                                         pair->value);
                }
                else if (strcmp(lowercaseKey, "progressbarelapsedevenchar") ==
                         0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings.progressBarElapsedEvenChar,
                                    sizeof(settings.progressBarElapsedEvenChar),
                                    "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "progressbarelapsedoddchar") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings.progressBarElapsedOddChar,
                                    sizeof(settings.progressBarElapsedOddChar),
                                    "%s", pair->value);
                }
                else if (strcmp(lowercaseKey,
                                "progressbarapproachingevenchar") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings.progressBarApproachingEvenChar,
                                    sizeof(settings
                                               .progressBarApproachingEvenChar),
                                    "%s", pair->value);
                }
                else if (strcmp(lowercaseKey,
                                "progressbarapproachingoddchar") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings.progressBarApproachingOddChar,
                                    sizeof(
                                        settings.progressBarApproachingOddChar),
                                    "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "progressbarcurrentevenchar") ==
                         0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings.progressBarCurrentEvenChar,
                                    sizeof(settings.progressBarCurrentEvenChar),
                                    "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "progressbarcurrentoddchar") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings.progressBarCurrentOddChar,
                                    sizeof(settings.progressBarCurrentOddChar),
                                    "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "showkeysalt") == 0 &&
                         strcmp(pair->value, "N") != 0)
                {
                        // We need to prevent the previous key B or else config
                        // files wont get updated to the new key N and B for
                        // radio search on macOS

                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showKeysAlt,
                                         sizeof(settings.showKeysAlt), "%s",
                                         pair->value);
                }

                free(lowercaseKey);
        }

        freeKeyValuePairs(pairs, count);

        if (!foundCycleThemesSetting)
        {
                // moveSongUp is no longer t, it needs to be changed
                c_strcpy(settings.moveSongUp, "f", sizeof(settings.moveSongUp));
        }

        return settings;
}

KeyValuePair *readKeyValuePairs(const char *file_path, int *count,
                                time_t *lastTimeAppRan)
{
        FILE *file = fopen(file_path, "r");
        if (file == NULL)
        {
                return NULL;
        }

        struct stat file_stat;
        if (stat(file_path, &file_stat) == -1)
        {
                perror("stat");
                return NULL;
        }

        // Save the modification time (mtime) of the file
#ifdef __APPLE__
        *lastTimeAppRan = (file_stat.st_mtime > 0)
                              ? file_stat.st_mtime
                              : file_stat.st_mtimespec.tv_sec;
#else
        *lastTimeAppRan = (file_stat.st_mtime > 0) ? file_stat.st_mtime
                                                   : file_stat.st_mtim.tv_sec;
#endif
        KeyValuePair *pairs = NULL;
        int pair_count = 0;

        char line[256];
        while (fgets(line, sizeof(line), file))
        {
                // Remove trailing newline character if present
                line[strcspn(line, "\n")] = '\0';

                char *comment = strchr(line, '#');

                // Remove comments
                if (comment != NULL)
                        *comment = '\0';

                char *delimiter = strchr(line, '=');

                if (delimiter != NULL)
                {
                        *delimiter = '\0';
                        char *value = delimiter + 1;

                        pair_count++;
                        pairs =
                            realloc(pairs, pair_count * sizeof(KeyValuePair));
                        KeyValuePair *current_pair = &pairs[pair_count - 1];

                        current_pair->key = strdup(line);
                        current_pair->value = strdup(value);
                }
        }

        fclose(file);

        *count = pair_count;
        return pairs;
}

const char *getDefaultMusicFolder(void)
{
        const char *home = getHomePath();
        if (home != NULL)
        {
                static char musicPath[MAXPATHLEN];
                snprintf(musicPath, sizeof(musicPath), "%s/Music", home);
                return musicPath;
        }
        else
        {
                return NULL; // Return NULL if XDG home is not found.
        }
}

int getMusicLibraryPath(char *path)
{
        char expandedPath[MAXPATHLEN];

        if (path[0] != '\0' && path[0] != '\r')
        {
                if (expandPath(path, expandedPath) >= 0)
                {
                        c_strcpy(path, expandedPath, sizeof(expandedPath));
                }
        }

        return 0;
}

void mapSettingsToKeys(AppSettings *settings, UISettings *ui,
                       EventMapping *mappings)
{
        mappings[0] = (EventMapping){settings->scrollUpAlt, EVENT_SCROLLPREV};
        mappings[1] = (EventMapping){settings->scrollDownAlt, EVENT_SCROLLNEXT};
        mappings[2] = (EventMapping){settings->nextTrackAlt, EVENT_NEXT};
        mappings[3] = (EventMapping){settings->previousTrackAlt, EVENT_PREV};
        mappings[4] = (EventMapping){settings->volumeUp, EVENT_VOLUME_UP};
        mappings[5] = (EventMapping){settings->volumeUpAlt, EVENT_VOLUME_UP};
        mappings[6] = (EventMapping){settings->volumeDown, EVENT_VOLUME_DOWN};
        mappings[7] = (EventMapping){settings->togglePause, EVENT_PLAY_PAUSE};
        mappings[8] = (EventMapping){settings->quit, EVENT_QUIT};
        mappings[9] = (EventMapping){settings->altQuit, EVENT_QUIT};
        mappings[10] = (EventMapping){settings->toggleShuffle, EVENT_SHUFFLE};
        mappings[11] = (EventMapping){settings->toggleVisualizer, EVENT_TOGGLEVISUALIZER};
        mappings[12] = (EventMapping){settings->toggleAscii, EVENT_TOGGLEASCII};
        mappings[13] = (EventMapping){settings->switchNumberedSong, EVENT_ENQUEUE};
        mappings[14] = (EventMapping){settings->seekBackward, EVENT_SEEKBACK};
        mappings[15] = (EventMapping){settings->seekForward, EVENT_SEEKFORWARD};
        mappings[16] = (EventMapping){settings->toggleRepeat, EVENT_TOGGLEREPEAT};
        mappings[17] = (EventMapping){settings->savePlaylist, EVENT_EXPORTPLAYLIST};
        mappings[18] = (EventMapping){settings->cycleColorsDerivedFrom, EVENT_CYCLECOLORMODE};
        mappings[19] = (EventMapping){settings->addToFavoritesPlaylist, EVENT_ADDTOFAVORITESPLAYLIST};
        mappings[20] = (EventMapping){settings->updateLibrary, EVENT_UPDATELIBRARY};
        mappings[21] = (EventMapping){settings->hardPlayPause, EVENT_PLAY_PAUSE};
        mappings[22] = (EventMapping){settings->hardPrev, EVENT_PREV};
        mappings[23] = (EventMapping){settings->hardNext, EVENT_NEXT};
        mappings[24] = (EventMapping){settings->hardSwitchNumberedSong, EVENT_ENQUEUE};
        mappings[25] = (EventMapping){settings->hardScrollUp, EVENT_SCROLLPREV};
        mappings[26] = (EventMapping){settings->hardScrollDown, EVENT_SCROLLNEXT};
        mappings[27] = (EventMapping){settings->hardShowPlaylist, EVENT_SHOWPLAYLIST};
        mappings[28] = (EventMapping){settings->hardShowPlaylistAlt, EVENT_SHOWPLAYLIST};
        mappings[29] = (EventMapping){settings->showPlaylistAlt, EVENT_SHOWPLAYLIST};
        mappings[30] = (EventMapping){settings->hardShowKeys, EVENT_SHOWKEYBINDINGS};
        mappings[31] = (EventMapping){settings->hardShowKeysAlt, EVENT_SHOWKEYBINDINGS};
        mappings[32] = (EventMapping){settings->showKeysAlt, EVENT_SHOWKEYBINDINGS};
        mappings[33] = (EventMapping){settings->hardShowTrack, EVENT_SHOWTRACK};
        mappings[34] = (EventMapping){settings->hardShowTrackAlt, EVENT_SHOWTRACK};
        mappings[35] = (EventMapping){settings->showTrackAlt, EVENT_SHOWTRACK};
        mappings[36] = (EventMapping){settings->hardShowLibrary, EVENT_SHOWLIBRARY};
        mappings[37] = (EventMapping){settings->hardShowLibraryAlt, EVENT_SHOWLIBRARY};
        mappings[38] = (EventMapping){settings->showLibraryAlt, EVENT_SHOWLIBRARY};
        mappings[39] = (EventMapping){settings->hardShowSearch, EVENT_SHOWSEARCH};
        mappings[40] = (EventMapping){settings->hardShowSearchAlt, EVENT_SHOWSEARCH};
        mappings[41] = (EventMapping){settings->showSearchAlt, EVENT_SHOWSEARCH};
        mappings[42] = (EventMapping){settings->nextPage, EVENT_NEXTPAGE};
        mappings[43] = (EventMapping){settings->prevPage, EVENT_PREVPAGE};
        mappings[44] = (EventMapping){settings->hardRemove, EVENT_REMOVE};
        mappings[45] = (EventMapping){settings->hardRemove2, EVENT_REMOVE};
        mappings[46] = (EventMapping){settings->nextView, EVENT_NEXTVIEW};
        mappings[47] = (EventMapping){settings->prevView, EVENT_PREVVIEW};
        mappings[48] = (EventMapping){settings->mouseLeftClick, ui->mouseLeftClickAction};
        mappings[49] = (EventMapping){settings->mouseMiddleClick, ui->mouseMiddleClickAction};
        mappings[50] = (EventMapping){settings->mouseRightClick, ui->mouseRightClickAction};
        mappings[51] = (EventMapping){settings->mouseScrollUp, ui->mouseScrollUpAction};
        mappings[52] = (EventMapping){settings->mouseScrollDown, ui->mouseScrollDownAction};
        mappings[53] = (EventMapping){settings->mouseAltScrollUp, ui->mouseAltScrollUpAction};
        mappings[54] = (EventMapping){settings->mouseAltScrollDown, ui->mouseAltScrollDownAction};
        mappings[55] = (EventMapping){settings->hardClearPlaylist, EVENT_CLEARPLAYLIST};
        mappings[56] = (EventMapping){settings->moveSongUp, EVENT_MOVESONGUP};
        mappings[57] = (EventMapping){settings->moveSongDown, EVENT_MOVESONGDOWN};
        mappings[58] = (EventMapping){settings->enqueueAndPlay, EVENT_ENQUEUEANDPLAY};
        mappings[59] = (EventMapping){settings->hardStop, EVENT_STOP};
        mappings[60] = (EventMapping){settings->sortLibrary, EVENT_SORTLIBRARY};
        mappings[61] = (EventMapping){settings->cycleThemes, EVENT_CYCLETHEMES};
        mappings[62] = (EventMapping){settings->toggleNotifications, EVENT_TOGGLENOTIFICATIONS};
        mappings[63] = (EventMapping){settings->showLyricsPage, EVENT_SHOWLYRICSPAGE};
}

char *getConfigFilePath(char *configdir)
{
        size_t configdir_length = strnlen(configdir, MAXPATHLEN - 1);
        size_t settings_file_length =
            strnlen(SETTINGS_FILE, sizeof(SETTINGS_FILE) - 1);

        if (configdir_length + 1 + settings_file_length + 1 > MAXPATHLEN)
        {
                fprintf(stderr, "Error: File path exceeds maximum length.\n");
                exit(1);
        }

        char *filepath = (char *)malloc(MAXPATHLEN);
        if (filepath == NULL)
        {
                perror("malloc");
                exit(1);
        }

        int written =
            snprintf(filepath, MAXPATHLEN, "%s/%s", configdir, SETTINGS_FILE);
        if (written < 0 || written >= MAXPATHLEN)
        {
                fprintf(stderr,
                        "Error: snprintf failed or filepath truncated.\n");
                free(filepath);
                exit(1);
        }
        return filepath;
}

int getBytesInFirstChar(const char *str)
{
        if (str == NULL || str[0] == '\0')
        {
                return 0;
        }

        mbstate_t state;
        memset(&state, 0, sizeof(state));
        wchar_t wc;
        int numBytes = mbrtowc(&wc, str, MB_CUR_MAX, &state);

        return numBytes;
}

enum EventType getMouseAction(int num)
{
        enum EventType value = EVENT_NONE;

        switch (num)
        {
        case 0:
                value = EVENT_NONE;
                break;
        case 1:
                value = EVENT_ENQUEUE;
                break;
        case 2:
                value = EVENT_PLAY_PAUSE;
                break;
        case 3:
                value = EVENT_SCROLLPREV;
                break;
        case 4:
                value = EVENT_SCROLLNEXT;
                break;
        case 5:
                value = EVENT_SEEKFORWARD;
                break;
        case 6:
                value = EVENT_SEEKBACK;
                break;
        case 7:
                value = EVENT_VOLUME_UP;
                break;
        case 8:
                value = EVENT_VOLUME_DOWN;
                break;
        case 9:
                value = EVENT_NEXTVIEW;
                break;
        case 10:
                value = EVENT_PREVVIEW;
                break;
        default:
                value = EVENT_NONE;
                break;
        }

        return value;
}

int mkdir_p(const char *path, mode_t mode)
{
        if (path == NULL)
                return -1;

        if (path[0] == '~')
        {
                // Just try a plain mkdir if there's a tilde
                if (mkdir(path, mode) == -1)
                {
                        if (errno != EEXIST)
                                return -1;
                }
                return 0;
        }

        char tmp[PATH_MAX];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp), "%s", path);
        len = strnlen(tmp, MAXPATHLEN);
        if (len > 0 && tmp[len - 1] == '/')
                tmp[len - 1] = 0;

        for (p = tmp + 1; *p; p++)
        {
                if (*p == '/')
                {
                        *p = 0;
                        if (mkdir(tmp, mode) == -1)
                        {
                                if (errno != EEXIST)
                                        return -1;
                        }
                        *p = '/';
                }
        }
        if (mkdir(tmp, mode) == -1)
        {
                if (errno != EEXIST)
                        return -1;
        }
        return 0;
}

void getConfig(AppSettings *settings, UISettings *ui)
{
        int pair_count;
        char *configdir = getConfigPath();

        setlocale(LC_ALL, "");

        // Create the directory if it doesn't exist
        struct stat st = {0};
        if (stat(configdir, &st) == -1)
        {
                if (mkdir_p(configdir, 0700) != 0)
                {
                        perror("mkdir");
                        exit(1);
                }
        }

        char *filepath = getConfigFilePath(configdir);

        KeyValuePair *pairs =
            readKeyValuePairs(filepath, &pair_count, &(ui->lastTimeAppRan));

        free(filepath);
        *settings = constructAppSettings(pairs, pair_count);

        int tmpNumBytes =
            getBytesInFirstChar(settings->progressBarElapsedEvenChar);
        if (tmpNumBytes != 0)
                settings->progressBarElapsedEvenChar[tmpNumBytes] = '\0';

        tmpNumBytes = getBytesInFirstChar(settings->progressBarElapsedOddChar);
        if (tmpNumBytes != 0)
                settings->progressBarElapsedOddChar[tmpNumBytes] = '\0';

        tmpNumBytes =
            getBytesInFirstChar(settings->progressBarApproachingEvenChar);
        if (tmpNumBytes != 0)
                settings->progressBarApproachingEvenChar[tmpNumBytes] = '\0';

        tmpNumBytes =
            getBytesInFirstChar(settings->progressBarApproachingOddChar);
        if (tmpNumBytes != 0)
                settings->progressBarApproachingOddChar[tmpNumBytes] = '\0';

        tmpNumBytes = getBytesInFirstChar(settings->progressBarCurrentEvenChar);
        if (tmpNumBytes != 0)
                settings->progressBarCurrentEvenChar[tmpNumBytes] = '\0';

        tmpNumBytes = getBytesInFirstChar(settings->progressBarCurrentOddChar);
        if (tmpNumBytes != 0)
                settings->progressBarCurrentOddChar[tmpNumBytes] = '\0';

        ui->allowNotifications = (settings->allowNotifications[0] == '1');
        ui->coverEnabled = (settings->coverEnabled[0] == '1');
        ui->coverAnsi = (settings->coverAnsi[0] == '1');
        ui->visualizerEnabled = (settings->visualizerEnabled[0] == '1');
        bool useConfigColors = (settings->useConfigColors[0] == '1');
        ui->quitAfterStopping = (settings->quitAfterStopping[0] == '1');
        ui->hideGlimmeringText = (settings->hideGlimmeringText[0] == '1');
        ui->mouseEnabled = (settings->mouseEnabled[0] == '1');
        ui->shuffleEnabled = (settings->shuffleEnabled[0] == '1');

        ui->visualizerBrailleMode = (settings->visualizerBrailleMode[0] == '1');
        ui->hideLogo = (settings->hideLogo[0] == '1');
        ui->hideHelp = (settings->hideHelp[0] == '1');
        ui->saveRepeatShuffleSettings =
            (settings->saveRepeatShuffleSettings[0] == '1');
        ui->trackTitleAsWindowTitle =
            (settings->trackTitleAsWindowTitle[0] == '1');

        int tmp = getNumber(settings->repeatState);
        if (tmp >= 0)
                ui->repeatState = tmp;

        tmp = getNumber(settings->colorMode);
        if (tmp >= 0 && tmp < 3)
        {
                ui->colorMode = tmp;
        }
        else
        {
                if (useConfigColors)
                        ui->colorMode = COLOR_MODE_DEFAULT;
                else
                        ui->colorMode = COLOR_MODE_ALBUM;
        }

        tmp = getNumber(settings->replayGainCheckFirst);
        if (tmp >= 0)
                ui->replayGainCheckFirst = tmp;

        tmp = getNumber(settings->mouseLeftClickAction);
        enum EventType tmpEvent = getMouseAction(tmp);
        if (tmp >= 0)
                ui->mouseLeftClickAction = tmpEvent;

        tmp = getNumber(settings->mouseMiddleClickAction);
        tmpEvent = getMouseAction(tmp);
        if (tmp >= 0)
                ui->mouseMiddleClickAction = tmpEvent;

        tmp = getNumber(settings->mouseRightClickAction);
        tmpEvent = getMouseAction(tmp);
        if (tmp >= 0)
                ui->mouseRightClickAction = tmpEvent;

        tmp = getNumber(settings->mouseScrollUpAction);
        tmpEvent = getMouseAction(tmp);
        if (tmp >= 0)
                ui->mouseScrollUpAction = tmpEvent;

        tmp = getNumber(settings->mouseScrollDownAction);
        tmpEvent = getMouseAction(tmp);
        if (tmp >= 0)
                ui->mouseScrollDownAction = tmpEvent;

        tmp = getNumber(settings->mouseAltScrollUpAction);
        tmpEvent = getMouseAction(tmp);
        if (tmp >= 0)
                ui->mouseAltScrollUpAction = tmpEvent;

        tmp = getNumber(settings->mouseAltScrollDownAction);
        tmpEvent = getMouseAction(tmp);
        if (tmp >= 0)
                ui->mouseAltScrollDownAction = tmpEvent;

        tmp = getNumber(settings->visualizerHeight);
        if (tmp > 0)
                ui->visualizerHeight = tmp;

        tmp = getNumber(settings->visualizerBarWidth);
        if (tmp >= 0)
                ui->visualizerBarWidth = tmp;

        tmp = getNumber(settings->visualizerColorType);
        if (tmp >= 0)
                ui->visualizerColorType = tmp;

        tmp = getNumber(settings->titleDelay);
        if (tmp >= 0)
                ui->titleDelay = tmp;

        tmp = getNumber(settings->lastVolume);
        if (tmp >= 0)
                setVolume(tmp);

        tmp = getNumber(settings->cacheLibrary);
        if (tmp >= 0)
                ui->cacheLibrary = tmp;

        getMusicLibraryPath(settings->path);
        snprintf(ui->themeName, sizeof(ui->themeName), "%s", settings->theme);
        free(configdir);
}

void setConfig(AppSettings *settings, UISettings *ui)
{
        // Create the file path
        char *configdir = getConfigPath();

        char *filepath = getConfigFilePath(configdir);

        setlocale(LC_ALL, "");

        FILE *file = fopen(filepath, "w");
        if (file == NULL)
        {
                fprintf(stderr, "Error opening file: %s\n", filepath);
                free(filepath);
                free(configdir);
                return;
        }

        // Make sure strings are valid before writing settings to the file
        if (settings->allowNotifications[0] == '\0')
                ui->allowNotifications
                    ? c_strcpy(settings->allowNotifications, "1",
                               sizeof(settings->allowNotifications))
                    : c_strcpy(settings->allowNotifications, "0",
                               sizeof(settings->allowNotifications));
        if (settings->coverEnabled[0] == '\0')
                ui->coverEnabled ? c_strcpy(settings->coverEnabled, "1",
                                            sizeof(settings->coverEnabled))
                                 : c_strcpy(settings->coverEnabled, "0",
                                            sizeof(settings->coverEnabled));
        if (settings->coverAnsi[0] == '\0')
                ui->coverAnsi ? c_strcpy(settings->coverAnsi, "1",
                                         sizeof(settings->coverAnsi))
                              : c_strcpy(settings->coverAnsi, "0",
                                         sizeof(settings->coverAnsi));
        if (settings->visualizerEnabled[0] == '\0')
                ui->visualizerEnabled
                    ? c_strcpy(settings->visualizerEnabled, "1",
                               sizeof(settings->visualizerEnabled))
                    : c_strcpy(settings->visualizerEnabled, "0",
                               sizeof(settings->visualizerEnabled));
        if (settings->quitAfterStopping[0] == '\0')
                ui->quitAfterStopping
                    ? c_strcpy(settings->quitAfterStopping, "1",
                               sizeof(settings->quitAfterStopping))
                    : c_strcpy(settings->quitAfterStopping, "0",
                               sizeof(settings->quitAfterStopping));
        if (settings->hideGlimmeringText[0] == '\0')
                ui->hideGlimmeringText
                    ? c_strcpy(settings->hideGlimmeringText, "1",
                               sizeof(settings->hideGlimmeringText))
                    : c_strcpy(settings->hideGlimmeringText, "0",
                               sizeof(settings->hideGlimmeringText));
        if (settings->mouseEnabled[0] == '\0')
                ui->mouseEnabled ? c_strcpy(settings->mouseEnabled, "1",
                                            sizeof(settings->mouseEnabled))
                                 : c_strcpy(settings->mouseEnabled, "0",
                                            sizeof(settings->mouseEnabled));
        if (settings->trackTitleAsWindowTitle[0] == '\0')
                ui->trackTitleAsWindowTitle
                    ? c_strcpy(settings->trackTitleAsWindowTitle, "1",
                               sizeof(settings->trackTitleAsWindowTitle))
                    : c_strcpy(settings->trackTitleAsWindowTitle, "0",
                               sizeof(settings->trackTitleAsWindowTitle));

        snprintf(settings->repeatState, sizeof(settings->repeatState), "%d",
                 ui->repeatState);

        ui->shuffleEnabled ? c_strcpy(settings->shuffleEnabled, "1",
                                      sizeof(settings->shuffleEnabled))
                           : c_strcpy(settings->shuffleEnabled, "0",
                                      sizeof(settings->shuffleEnabled));

        if (settings->visualizerBarWidth[0] == '\0')
                snprintf(settings->visualizerBarWidth,
                         sizeof(settings->visualizerBarWidth), "%d",
                         ui->visualizerBarWidth);

        if (settings->visualizerBrailleMode[0] == '\0')
                ui->visualizerBrailleMode
                    ? c_strcpy(settings->visualizerBrailleMode, "1",
                               sizeof(settings->visualizerBrailleMode))
                    : c_strcpy(settings->visualizerBrailleMode, "0",
                               sizeof(settings->visualizerBrailleMode));
        if (settings->hideLogo[0] == '\0')
                ui->hideLogo ? c_strcpy(settings->hideLogo, "1",
                                        sizeof(settings->hideLogo))
                             : c_strcpy(settings->hideLogo, "0",
                                        sizeof(settings->hideLogo));
        if (settings->hideHelp[0] == '\0')
                ui->hideHelp ? c_strcpy(settings->hideHelp, "1",
                                        sizeof(settings->hideHelp))
                             : c_strcpy(settings->hideHelp, "0",
                                        sizeof(settings->hideHelp));

        if (settings->visualizerHeight[0] == '\0')
                snprintf(settings->visualizerHeight,
                         sizeof(settings->visualizerHeight), "%d",
                         ui->visualizerHeight);
        if (settings->visualizerColorType[0] == '\0')
                snprintf(settings->visualizerColorType,
                         sizeof(settings->visualizerColorType), "%d",
                         ui->visualizerColorType);
        if (settings->titleDelay[0] == '\0')
                snprintf(settings->titleDelay, sizeof(settings->titleDelay),
                         "%d", ui->titleDelay);

        snprintf(settings->cacheLibrary, sizeof(settings->cacheLibrary), "%d",
                 ui->cacheLibrary);

        int currentVolume = getCurrentVolume();
        currentVolume = (currentVolume <= 0) ? 10 : currentVolume;
        snprintf(settings->lastVolume, sizeof(settings->lastVolume), "%d",
                 currentVolume);

        if (settings->replayGainCheckFirst[0] == '\0')
                snprintf(settings->replayGainCheckFirst,
                         sizeof(settings->replayGainCheckFirst), "%d",
                         ui->replayGainCheckFirst);

        if (settings->mouseLeftClickAction[0] == '\0')
                snprintf(settings->mouseLeftClickAction,
                         sizeof(settings->mouseLeftClickAction), "%d",
                         ui->mouseLeftClickAction);
        if (settings->mouseMiddleClickAction[0] == '\0')
                snprintf(settings->mouseMiddleClickAction,
                         sizeof(settings->mouseMiddleClickAction), "%d",
                         ui->mouseMiddleClickAction);
        if (settings->mouseRightClickAction[0] == '\0')
                snprintf(settings->mouseRightClickAction,
                         sizeof(settings->mouseRightClickAction), "%d",
                         ui->mouseRightClickAction);
        if (settings->mouseScrollUpAction[0] == '\0')
                snprintf(settings->mouseScrollUpAction,
                         sizeof(settings->mouseScrollUpAction), "%d",
                         ui->mouseScrollUpAction);
        if (settings->mouseScrollDownAction[0] == '\0')
                snprintf(settings->mouseScrollDownAction,
                         sizeof(settings->mouseScrollDownAction), "%d",
                         ui->mouseScrollDownAction);
        if (settings->mouseAltScrollUpAction[0] == '\0')
                snprintf(settings->mouseAltScrollUpAction,
                         sizeof(settings->mouseAltScrollUpAction), "%d",
                         ui->mouseAltScrollUpAction);
        if (settings->mouseAltScrollDownAction[0] == '\0')
                snprintf(settings->mouseAltScrollDownAction,
                         sizeof(settings->mouseAltScrollDownAction), "%d",
                         ui->mouseAltScrollDownAction);

        // Write the settings to the file
        fprintf(file, "# Configuration file for kew terminal music player.\n");
        fprintf(file, "# Make sure that kew is not running before editing this "
                      "file in order for changes to take effect.\n\n");

        fprintf(file, "\n[miscellaneous]\n\n");
        fprintf(file, "path=%s\n", settings->path);
        fprintf(file, "version=%s\n", ui->VERSION);
        fprintf(file, "allowNotifications=%s\n", settings->allowNotifications);
        fprintf(file, "hideLogo=%s\n", settings->hideLogo);
        fprintf(file, "hideHelp=%s\n", settings->hideHelp);
        fprintf(file, "lastVolume=%s\n\n", settings->lastVolume);

        fprintf(file, "# Cache: Set to 1 to use cache of the music library "
                      "directory tree for faster startup times.\n");
        fprintf(file, "cacheLibrary=%s\n\n", settings->cacheLibrary);

        fprintf(file, "# Delay when drawing title in track view, set to 0 to "
                      "have no delay.\n");
        fprintf(file, "titleDelay=%s\n\n", settings->titleDelay);

        fprintf(file, "# Same as '--quitonstop' flag, exits after playing the "
                      "whole playlist.\n");
        fprintf(file, "quitOnStop=%s\n\n", settings->quitAfterStopping);

        fprintf(file, "# Glimmering text on the bottom row.\n");
        fprintf(file, "hideGlimmeringText=%s\n\n",
                settings->hideGlimmeringText);

        fprintf(file, "# Replay gain check first, can be either 0=track, "
                      "1=album or 2=disabled.\n");
        fprintf(file, "replayGainCheckFirst=%s\n\n",
                settings->replayGainCheckFirst);

        fprintf(file, "# Save Repeat and Shuffle Settings.\n");
        fprintf(file, "saveRepeatShuffleSettings=%s\n\n",
                settings->saveRepeatShuffleSettings);

        fprintf(file, "repeatState=%s\n\n", settings->repeatState);
        fprintf(file, "shuffleEnabled=%s\n\n", settings->shuffleEnabled);

        fprintf(file, "# Set the window title to the title of the currently "
                      "playing track\n");
        fprintf(file, "trackTitleAsWindowTitle=%s\n\n",
                settings->trackTitleAsWindowTitle);

        fprintf(file, "\n[colors]\n\n");

        fprintf(file, "# Theme's go in ~/.config/kew/themes (on Linux/FreeBSD/Android), \n");
        fprintf(file, "# and ~/Library/Preferences/kew/themes (on macOS), \n");
        fprintf(file, "theme=%s\n\n", settings->theme);

        fprintf(file, "# Color Mode is:\n");
        fprintf(file, "# 0 = 16-bit color palette from default theme, \n");
        fprintf(file, "# 1 = Colors derived from track cover, \n");
        fprintf(file, "# 2 = Colors derived from TrueColor theme, \n\n");
        fprintf(file, "# Color Mode:\n");
        fprintf(file, "colorMode=%d\n\n", ui->colorMode);

        fprintf(file, "# Terminal color theme is default.theme in \n");
        fprintf(file, "# ~/.config/kew/themes (on Linux/FreeBSD/Android), \n");
        fprintf(file, "# and ~/Library/Preferences/kew/themes (on macOS).\n\n");

        fprintf(file, "\n[track cover]\n\n");
        fprintf(file, "coverEnabled=%s\n", settings->coverEnabled);
        fprintf(file, "coverAnsi=%s\n\n", settings->coverAnsi);

        fprintf(file, "\n[mouse]\n\n");
        fprintf(file, "mouseEnabled=%s\n\n", settings->mouseEnabled);

        fprintf(file, "\n[visualizer]\n\n");
        fprintf(file, "visualizerEnabled=%s\n", settings->visualizerEnabled);
        fprintf(file, "visualizerHeight=%s\n", settings->visualizerHeight);
        fprintf(file, "visualizerBrailleMode=%s\n\n",
                settings->visualizerBrailleMode);

        fprintf(file, "# How colors are laid out in the spectrum visualizer. "
                      "0=lighten, 1=brightness depending on bar height, "
                      "2=reversed, 3=reversed darken.\n");
        fprintf(file, "visualizerColorType=%s\n\n",
                settings->visualizerColorType);

        fprintf(file, "# 0=Thin bars, 1=Bars twice the width, 2=Auto (depends "
                      "on window size).\n");
        fprintf(file, "visualizerBarWidth=%s\n\n",
                settings->visualizerBarWidth);

        fprintf(file, "\n[progress bar]\n\n");

        fprintf(file, "# Progress bar in track view\n");
        fprintf(file, "# The progress bar can be configured in many ways.\n");
        fprintf(file,
                "# When copying the values below, be sure to include values "
                "that are empty spaces or things will get messed up.\n");
        fprintf(file,
                "# Be sure to have the actual uncommented values last.\n");
        fprintf(
            file,
            "# For instance use the below values for a pill muncher mode:\n\n");

        fprintf(file, "#progressBarElapsedEvenChar= \n");
        fprintf(file, "#progressBarElapsedOddChar= \n");
        fprintf(file, "#progressBarApproachingEvenChar=•\n");
        fprintf(file, "#progressBarApproachingOddChar=·\n");
        fprintf(file, "#progressBarCurrentEvenChar=ᗧ\n");
        fprintf(file, "#progressBarCurrentOddChar=ᗧ\n\n");

        fprintf(file, "# To have a thick line: \n\n");

        fprintf(file, "#progressBarElapsedEvenChar=━\n");
        fprintf(file, "#progressBarElapsedOddChar=━\n");
        fprintf(file, "#progressBarApproachingEvenChar=━\n");
        fprintf(file, "#progressBarApproachingOddChar=━\n");
        fprintf(file, "#progressBarCurrentEvenChar=━\n");
        fprintf(file, "#progressBarCurrentOddChar=━\n\n");

        fprintf(file, "# To have dots (the original): \n\n");

        fprintf(file, "#progressBarElapsedEvenChar=■\n");
        fprintf(file, "#progressBarElapsedOddChar= \n");
        fprintf(file, "#progressBarApproachingEvenChar==\n");
        fprintf(file, "#progressBarApproachingOddChar= \n");
        fprintf(file, "#progressBarCurrentEvenChar=■\n");
        fprintf(file, "#progressBarCurrentOddChar= \n\n");

        fprintf(file, "# Current values: \n\n");

        fprintf(file, "progressBarElapsedEvenChar=%s\n",
                settings->progressBarElapsedEvenChar);
        fprintf(file, "progressBarElapsedOddChar=%s\n",
                settings->progressBarElapsedOddChar);
        fprintf(file, "progressBarApproachingEvenChar=%s\n",
                settings->progressBarApproachingEvenChar);
        fprintf(file, "progressBarApproachingOddChar=%s\n",
                settings->progressBarApproachingOddChar);
        fprintf(file, "progressBarCurrentEvenChar=%s\n",
                settings->progressBarCurrentEvenChar);
        fprintf(file, "progressBarCurrentOddChar=%s\n\n",
                settings->progressBarCurrentOddChar);

        fprintf(file, "# Mouse actions are 0=None, 1=Select song, 2=Toggle "
                      "pause, 3=Scroll up, 4=Scroll down, 5=Seek forward, "
                      "6=Seek backward, 7=Volume up, 8=Volume down, 9=Switch "
                      "to next view, 10=Switch to previous view\n");
        fprintf(file, "mouseLeftClickAction=%s\n",
                settings->mouseLeftClickAction);
        fprintf(file, "mouseMiddleClickAction=%s\n",
                settings->mouseMiddleClickAction);
        fprintf(file, "mouseRightClickAction=%s\n",
                settings->mouseRightClickAction);
        fprintf(file, "mouseScrollUpAction=%s\n",
                settings->mouseScrollUpAction);
        fprintf(file, "mouseScrollDownAction=%s\n\n",
                settings->mouseScrollDownAction);

        fprintf(file, "# Mouse action when using mouse scroll + alt\n");
        fprintf(file, "mouseAltScrollUpAction=%s\n",
                settings->mouseAltScrollUpAction);
        fprintf(file, "mouseAltScrollDownAction=%s\n\n",
                settings->mouseAltScrollDownAction);

        fprintf(file, "\n[key bindings]\n\n");
        fprintf(file, "volumeUp=%s\n", settings->volumeUp);
        fprintf(file, "volumeUpAlt=%s\n", settings->volumeUpAlt);
        fprintf(file, "volumeDown=%s\n", settings->volumeDown);
        fprintf(file, "previousTrackAlt=%s\n", settings->previousTrackAlt);
        fprintf(file, "nextTrackAlt=%s\n", settings->nextTrackAlt);
        fprintf(file, "scrollUpAlt=%s\n", settings->scrollUpAlt);
        fprintf(file, "scrollDownAlt=%s\n", settings->scrollDownAlt);
        fprintf(file, "switchNumberedSong=%s\n", settings->switchNumberedSong);
        fprintf(file, "togglePause=%s\n", settings->togglePause);
        fprintf(file, "toggleNotifications=%s\n", settings->toggleNotifications);
        fprintf(file, "toggleColorsDerivedFrom=%s\n",
                settings->cycleColorsDerivedFrom);
        fprintf(file, "cycleThemes=%s\n",
                settings->cycleThemes);
        fprintf(file, "toggleVisualizer=%s\n", settings->toggleVisualizer);
        fprintf(file, "toggleAscii=%s\n", settings->toggleAscii);
        fprintf(file, "toggleRepeat=%s\n", settings->toggleRepeat);
        fprintf(file, "toggleShuffle=%s\n", settings->toggleShuffle);
        fprintf(file, "seekBackward=%s\n", settings->seekBackward);
        fprintf(file, "seekForward=%s\n", settings->seekForward);
        fprintf(file, "savePlaylist=%s\n", settings->savePlaylist);
        fprintf(file, "addToFavoritesPlaylist=%s\n",
                settings->addToFavoritesPlaylist);
        fprintf(file, "updateLibrary=%s\n", settings->updateLibrary);
        fprintf(file, "moveSongUp=%s\n", settings->moveSongUp);
        fprintf(file, "moveSongDown=%s\n", settings->moveSongDown);
        fprintf(file, "enqueueAndPlay=%s\n", settings->enqueueAndPlay);
        fprintf(file, "sortLibrary=%s\n", settings->sortLibrary);
        fprintf(file, "quit=%s\n", settings->quit);
        fprintf(file, "altQuit=%s\n\n", settings->altQuit);

        fprintf(file,
                "# Alt keys for the different main views, normally F2-F7:\n");
        fprintf(file, "showPlaylistAlt=%s\n", settings->showPlaylistAlt);
        fprintf(file, "showLibraryAlt=%s\n", settings->showLibraryAlt);
        fprintf(file, "showTrackAlt=%s\n", settings->showTrackAlt);
        fprintf(file, "showSearchAlt=%s\n", settings->showSearchAlt);
        fprintf(file, "showKeysAlt=%s\n\n", settings->showKeysAlt);
        fprintf(file, "showLyricsPage=%s\n\n", settings->showLyricsPage);

        fprintf(file, "# Keys for scrolling page up and page down:\n");
        fprintf(file, "prevPage=%s\n", settings->prevPage);
        fprintf(file, "nextPage=%s\n\n", settings->nextPage);

        fprintf(file, "# For special keys use terminal codes: OS, for F4 for "
                      "instance. This can depend on the terminal.\n");
        fprintf(file, "# You can find out the codes for the keys by using "
                      "tools like showkey.\n");
        fprintf(file, "# For special keys, see the key value after the bracket "
                      "\"[\" after typing \"showkey -a\" in the terminal and "
                      "then pressing a key you want info about.\n");

        fclose(file);
        free(filepath);
        free(configdir);
}
