/**
 * @file queue_ui.[c]
 * @brief Handles high level enqueue
 *
 */

#include "queue_ui.h"

#include "common/appstate.h"
#include "common/common.h"

#include "input.h"
#include "player_ui.h"
#include "search_ui.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/trackmanager.h"

#include "sys/mpris.h"
#include "sys/systemintegration.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <locale.h>

void determineSongAndNotify(void)
{
        AppState *state = getAppState();
        SongData *currentSongData = NULL;

        bool isDeleted = determineCurrentSongData(&currentSongData);

        Node *current = getCurrentSong();

        if (currentSongData && current)
                current->song.duration = currentSongData->duration;

        if (state->uiState.lastNotifiedId != current->id)
        {
                if (!isDeleted)
                        notifySongSwitch(currentSongData);
        }
}

void updateNextSongIfNeeded(void)
{
        loadNextSong();
        determineSongAndNotify();
}

void resetListAfterDequeuingPlayingSong(void)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        state->uiState.startFromTop = true;

        if (ps->lastPlayedId < 0)
                return;

        Node *node = findSelectedEntryById(getPlaylist(), ps->lastPlayedId);

        if (getCurrentSong() == NULL && node == NULL)
        {
                playbackStop();

                AudioData *audioData = getAudioData();

                ps->loadedNextSong = false;
                audioData->endOfListReached = true;
                audioData->restart = true;

                emitMetadataChanged("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                emitPlaybackStoppedMpris();

                playbackCleanup();

                triggerRefresh();

                playbackSwitchDecoder();

                unloadSongA();
                unloadSongB();
                state->uiState.songWasRemoved = true;

                UserData *userData = playbackGetUserData();

                userData->currentSongData = NULL;

                audioData->currentFileIndex = 0;
                audioData->restart = true;
                ps->waitingForNext = true;

                PlaybackState *ps = getPlaybackState();

                ps->loadingdata.loadA = true;
                ps->usingSongDataA = false;

                ma_data_source_uninit(&audioData);

                audioData->switchFiles = false;

                if (getPlaylist()->count == 0)
                        setSongToStartFrom(NULL);
        }
}

FileSystemEntry *enqueueSongs(FileSystemEntry *entry, FileSystemEntry **chosenDir)
{
        bool hasEnqueued = false;
        bool shuffle = false;

        AppState *state = getAppState();
        FileSystemEntry *firstEnqueuedEntry = NULL;
        UIState *uis = &(state->uiState);
        PlaybackState *ps = getPlaybackState();

        if (entry != NULL)
        {
                if (entry->isDirectory)
                {
                        if (!hasSongChildren(entry) || entry->parent == NULL ||
                            ((*chosenDir) != NULL &&
                             strcmp(entry->fullPath, (*chosenDir)->fullPath) == 0))
                        {
                                if (hasDequeuedChildren(entry))
                                {
                                        if (entry->parent ==
                                            NULL) // Shuffle playlist if it's
                                                  // the root
                                                shuffle = true;

                                        entry->isEnqueued = 1;
                                        entry = entry->children;

                                        enqueueChildren(entry,
                                                        &firstEnqueuedEntry);

                                        ps->nextSongNeedsRebuilding = true;

                                        hasEnqueued = true;
                                }
                                else
                                {
                                        dequeueChildren(entry);

                                        entry->isEnqueued = 0;

                                        ps->nextSongNeedsRebuilding = true;
                                }
                        }
                        if ((*chosenDir) != NULL && entry->parent != NULL &&
                            isContainedWithin(entry, (*chosenDir)) &&
                            uis->allowChooseSongs == true)
                        {
                                // If the chosen directory is the same as the
                                // entry's parent and it is open
                                uis->openedSubDir = true;

                                FileSystemEntry *tmpc = (*chosenDir)->children;

                                uis->numSongsAboveSubDir = 0;

                                while (tmpc != NULL)
                                {
                                        if (strcmp(entry->fullPath,
                                                   tmpc->fullPath) == 0 ||
                                            isContainedWithin(entry, tmpc))
                                                break;
                                        tmpc = tmpc->next;
                                        uis->numSongsAboveSubDir++;
                                }
                        }

                        AppState *state = getAppState();

                        if (state->uiState.currentLibEntry && state->uiState.currentLibEntry->isDirectory)
                                *chosenDir = state->uiState.currentLibEntry;

                        if (uis->allowChooseSongs == true)
                        {
                                uis->collapseView = true;
                                triggerRefresh();
                        }
                        uis->allowChooseSongs = true;
                }
                else
                {
                        if (!entry->isEnqueued)
                        {
                                setNextSong(NULL);
                                ps->nextSongNeedsRebuilding = true;
                                firstEnqueuedEntry = entry;

                                enqueueSong(entry);

                                hasEnqueued = true;
                        }
                        else
                        {
                                setNextSong(NULL);
                                ps->nextSongNeedsRebuilding = true;

                                dequeueSong(entry);
                        }
                }
                triggerRefresh();
        }

        if (hasEnqueued)
        {
                autostartIfStopped(firstEnqueuedEntry);
        }

        if (shuffle)
        {
                PlayList *playlist = getPlaylist();

                shufflePlaylist(playlist);
                setSongToStartFrom(NULL);
        }

        if (ps->nextSongNeedsRebuilding)
        {
                reshufflePlaylist();
        }

        return firstEnqueuedEntry;
}

FileSystemEntry *enqueue(FileSystemEntry *entry)
{
        AppState *state = getAppState();
        FileSystemEntry *firstEnqueuedEntry = NULL;
        PlaybackState *ps = getPlaybackState();
        AudioData *audioData = getAudioData();

        if (audioData->restart)
        {
                Node *lastSong = findSelectedEntryById(getPlaylist(), ps->lastPlayedId);
                state->uiState.startFromTop = false;

                if (lastSong == NULL)
                {
                        if (getPlaylist()->tail != NULL)
                                ps->lastPlayedId = getPlaylist()->tail->id;
                        else
                        {
                                ps->lastPlayedId = -1;
                                state->uiState.startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(getPlaylist()->mutex));

        FileSystemEntry *chosenDir = getChosenDir();

        firstEnqueuedEntry = enqueueSongs(entry, &chosenDir);

        setChosenDir(chosenDir);

        resetListAfterDequeuingPlayingSong();

        pthread_mutex_unlock(&(getPlaylist()->mutex));

        return firstEnqueuedEntry;
}

FileSystemEntry *libraryEnqueue(PlayList *playlist)
{
        AppState *state = getAppState();
        FileSystemEntry *entry = state->uiState.currentLibEntry;
        FileSystemEntry *firstEnqueuedEntry = NULL;

        if (entry == NULL)
                return NULL;

        // Enqueue playlist
        if (pathEndsWith(entry->fullPath, "m3u") ||
            pathEndsWith(entry->fullPath, "m3u8"))
        {
                Node *prevTail = playlist->tail;

                readM3UFile(entry->fullPath, playlist, getLibrary());

                if (prevTail != NULL && prevTail->next != NULL)
                {
                        firstEnqueuedEntry = findCorrespondingEntry(
                            getLibrary(), prevTail->next->song.filePath);
                }
                else if (playlist->head != NULL)
                {
                        firstEnqueuedEntry = findCorrespondingEntry(
                            getLibrary(), playlist->head->song.filePath);
                }

                autostartIfStopped(firstEnqueuedEntry);

                markListAsEnqueued(getLibrary(), playlist);

                PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

                deepCopyPlayListOntoList(playlist, &unshuffledPlaylist);
                shufflePlaylist(playlist);
                setUnshuffledPlaylist(unshuffledPlaylist);
        }
        else
        {
                firstEnqueuedEntry = enqueue(entry); // Enqueue song
        }

        return firstEnqueuedEntry;
}

FileSystemEntry *viewEnqueue(PlayList *playlist)
{
        AppState *state = getAppState();
        FileSystemEntry *firstEnqueuedEntry = NULL;

        switch (state->currentView)
        {
        case LIBRARY_VIEW:
                firstEnqueuedEntry = libraryEnqueue(playlist);
                break;
        case SEARCH_VIEW:
                firstEnqueuedEntry = searchEnqueue(playlist);
                break;
        case PLAYLIST_VIEW:
                if (!isDigitsPressed())
                {
                        playlistPlay(playlist);
                }
                else
                {
                        state->uiState.resetPlaylistDisplay = true;
                        int songNumber = getNumberFromString(getDigitsPressed());

                        resetDigitsPressed();

                        PlaybackState *ps = getPlaybackState();
                        ps->nextSongNeedsRebuilding = false;

                        skipToNumberedSong(songNumber);
                }
                break;
        default:
                return NULL;
        }

        return firstEnqueuedEntry;
}

void handleGoToSong(void)
{
        AppState *state = getAppState();
        Node *currentSong = getCurrentSong();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        FileSystemEntry *library = getLibrary();
        PlaybackState *ps = getPlaybackState();
        AudioData *audioData = getAudioData();

        bool canGoNext = (currentSong != NULL && currentSong->next != NULL);

        if (state->currentView == LIBRARY_VIEW)
        {
                FileSystemEntry *entry = state->uiState.currentLibEntry;

                if (entry == NULL)
                        return;

                // Enqueue playlist
                if (pathEndsWith(entry->fullPath, "m3u") ||
                    pathEndsWith(entry->fullPath, "m3u8"))
                {
                        FileSystemEntry *firstEnqueuedEntry = NULL;

                        Node *prevTail = playlist->tail;

                        if (playlist != NULL)
                        {
                                readM3UFile(entry->fullPath, playlist, library);

                                if (prevTail != NULL && prevTail->next != NULL)
                                {
                                        firstEnqueuedEntry = findCorrespondingEntry(
                                            library, prevTail->next->song.filePath);
                                }
                                else if (playlist->head != NULL)
                                {
                                        firstEnqueuedEntry = findCorrespondingEntry(
                                            library, playlist->head->song.filePath);
                                }

                                autostartIfStopped(firstEnqueuedEntry);

                                markListAsEnqueued(library, playlist);

                                deepCopyPlayListOntoList(playlist, &unshuffledPlaylist);
                        }
                }
                else
                        enqueue(entry); // Enqueue song
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                pthread_mutex_lock(&(playlist->mutex));

                FileSystemEntry *entry = getCurrentSearchEntry();

                setChosenDir(entry);

                FileSystemEntry *chosen = getChosenDir();

                enqueueSongs(entry, &chosen);

                setChosenDir(chosen);

                resetListAfterDequeuingPlayingSong();

                pthread_mutex_unlock(&(playlist->mutex));
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                if (isDigitsPressed() == 0)
                {
                        if (playbackIsPaused() && currentSong != NULL &&
                            state->uiState.chosenNodeId == currentSong->id)
                        {
                                togglePause();
                        }
                        else
                        {
                                playbackSafeCleanup();

                                ps->loadedNextSong = true;

                                ps->nextSongNeedsRebuilding = false;

                                unloadSongA();
                                unloadSongB();

                                ps->usingSongDataA = false;
                                audioData->currentFileIndex = 0;
                                ps->loadingdata.loadA = true;

                                bool wasEndOfList = playPreProcessing();

                                playbackPlay();

                                Node *found = NULL;
                                findNodeInList(playlist,
                                               state->uiState.chosenNodeId,
                                               &found);
                                play(found);

                                playPostProcessing(wasEndOfList);

                                ps->skipOutOfOrder = false;
                                ps->usingSongDataA = true;
                        }
                }
                else
                {
                        state->uiState.resetPlaylistDisplay = true;
                        int songNumber = getNumberFromString(getDigitsPressed());
                        resetDigitsPressed();

                        ps->nextSongNeedsRebuilding = false;

                        skipToNumberedSong(songNumber);
                }
        }

        // Handle MPRIS CanGoNext
        bool couldGoNext = (currentSong != NULL && currentSong->next != NULL);
        if (canGoNext != couldGoNext)
        {
                emitBooleanPropertyChanged("CanGoNext", couldGoNext);
        }
}

void enqueueAndPlay(void)
{
        AppState *state = getAppState();
        FileSystemEntry *firstEnqueuedEntry = NULL;
        PlayList *playlist = getPlaylist();
        PlaybackState *ps = getPlaybackState();
        AudioData *audioData = getAudioData();

        bool wasEmpty = (playlist->count == 0);

        bool wasEndOfList = playPreProcessing();

        if (state->currentView == PLAYLIST_VIEW)
        {
                handleGoToSong();
                return;
        }

        if (state->currentView == LIBRARY_VIEW)
        {
                firstEnqueuedEntry = enqueue(state->uiState.currentLibEntry);
        }

        if (state->currentView == SEARCH_VIEW)
        {
                setChosenDir(getCurrentSearchEntry());
                firstEnqueuedEntry = enqueue(getCurrentSearchEntry());
        }

        if (firstEnqueuedEntry && !wasEmpty)
        {
                Node *song =
                    findPathInPlaylist(firstEnqueuedEntry->fullPath, playlist);

                ps->loadedNextSong = true;

                ps->nextSongNeedsRebuilding = false;

                playbackSafeCleanup();

                unloadSongA();
                unloadSongB();

                ps->usingSongDataA = false;
                audioData->currentFileIndex = 0;
                ps->loadingdata.loadA = true;

                playbackPlay();

                play(song);

                playPostProcessing(wasEndOfList);

                ps->skipOutOfOrder = false;
                ps->usingSongDataA = true;
        }
}

void init(void)
{
        AppState *state = getAppState();
        disableTerminalLineInput();
        setRawInputMode();
        initResize();
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &state->uiState.windowSize);
        enableScrolling();
        setNonblockingMode();
        state->tmpCache = createCache();

        PlaybackState *ps = getPlaybackState();
        UserData *userData = playbackGetUserData();
        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();

        c_strcpy(ps->loadingdata.filePath, "", sizeof(ps->loadingdata.filePath));
        ps->loadingdata.songdataA = NULL;
        ps->loadingdata.songdataB = NULL;
        ps->loadingdata.loadA = true;
        ps->loadingdata.loadingFirstDecoder = true;
        audioData->restart = true;
        userData->songdataADeleted = true;
        userData->songdataBDeleted = true;
        unsigned int seed = (unsigned int)time(NULL);
        srand(seed);
        pthread_mutex_init(&(ps->loadingdata.mutex), NULL);
        pthread_mutex_init(&(playlist->mutex), NULL);

        freeSearchResults();
        resetChosenDir();
        createLibrary(settings, getLibraryFilePath());
        setlocale(LC_ALL, "");
        setlocale(LC_CTYPE, "");
        fflush(stdout);

#ifdef DEBUG
        // g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
        state->uiState.logFile = freopen("error.log", "w", stderr);
        if (state->uiState.logFile == NULL)
        {
                fprintf(stdout, "Failed to redirect stderr to error.log\n");
        }
#else
        FILE *nullStream = freopen("/dev/null", "w", stderr);
        (void)nullStream;
#endif
}
