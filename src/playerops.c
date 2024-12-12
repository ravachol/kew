#include "playerops.h"
/*

playerops.c

 Related to features/actions of the player.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef ASK_IF_USE_CACHE_LIMIT_SECONDS
#define ASK_IF_USE_CACHE_LIMIT_SECONDS 4
#endif

struct timespec current_time;
struct timespec start_time;
struct timespec pause_time;
struct timespec lastUpdateTime = {0, 0};

bool nextSongNeedsRebuilding = false;
bool skipFromStopped = false;
bool usingSongDataA = true;

LoadingThreadData loadingdata;

volatile bool loadedNextSong = false;
bool waitingForPlaylist = false;
bool waitingForNext = false;
Node *nextSong = NULL;
Node *tryNextSong = NULL;
Node *songToStartFrom = NULL;
Node *prevSong = NULL;
int lastPlayedId = -1;

bool songHasErrors = false;
bool skipOutOfOrder = false;
bool skipping = false;
bool forceSkip = false;
volatile bool clearingErrors = false;
volatile bool songLoading = false;

GDBusConnection *connection = NULL;
GMainContext *global_main_context = NULL;

typedef struct
{
        char *path;
        UISettings *ui;
} UpdateLibraryThreadArgs;

void reshufflePlaylist(void)
{
        if (isShuffleEnabled())
        {
                if (currentSong != NULL)
                        shufflePlaylistStartingFromSong(&playlist, currentSong);
                else
                        shufflePlaylist(&playlist);

                nextSongNeedsRebuilding = true;
        }
}

void skip(void)
{
        setCurrentImplementationType(NONE);

        setRepeatEnabled(false);
        audioData.endOfListReached = false;

        if (!isPlaying())
        {
                switchAudioImplementation();
                skipFromStopped = true;
        }
        else
        {
                setSkipToNext(true);
        }

        if (!skipOutOfOrder)
                refresh = true;
}

void updateLastSongSwitchTime(void)
{
        clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void updatePlaybackPosition(double elapsedSeconds)
{
        GVariantBuilder changedPropertiesBuilder;
        g_variant_builder_init(&changedPropertiesBuilder, G_VARIANT_TYPE_DICTIONARY);
        g_variant_builder_add(&changedPropertiesBuilder, "{sv}", "Position", g_variant_new_int64(llround(elapsedSeconds * G_USEC_PER_SEC)));

        GVariant *parameters = g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changedPropertiesBuilder, NULL);

        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.freedesktop.DBus.Properties",
                                      "PropertiesChanged",
                                      parameters,
                                      NULL);
}

void emitSeekedSignal(double newPositionSeconds)
{
#ifndef __APPLE__
        gint64 newPositionMicroseconds = llround(newPositionSeconds * G_USEC_PER_SEC);

        GVariant *parameters = g_variant_new("(x)", newPositionMicroseconds);

        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.mpris.MediaPlayer2.Player",
                                      "Seeked",
                                      parameters,
                                      NULL);
#else
        (void)newPositionSeconds;
#endif
}

void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue)
{
#ifndef __APPLE__
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName, g_variant_new_string(newValue));
        g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                      g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);
        g_variant_builder_clear(&changed_properties_builder);
#else
        (void)propertyName;
        (void)newValue;
#endif
}

void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue)
{
#ifndef __APPLE__
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName, g_variant_new_boolean(newValue));
        g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                      g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);
        g_variant_builder_clear(&changed_properties_builder);
#else
        (void)propertyName;
        (void)newValue;
#endif
}

void playbackPause(struct timespec *pause_time)
{
        if (!isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");
                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        pausePlayback();
}

void skipToSong(int id, bool startPlaying)
{
        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        Node *found = NULL;
        findNodeInList(&playlist, id, &found);

        if (found != NULL)
                currentSong = found;
        else
        {
                return;
        }

        skipping = true;
        skipOutOfOrder = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        if (startPlaying)
        {
                playbackPlay(&totalPauseSeconds, &pauseSeconds);
        }

        // cancel starting from top
        if (waitingForPlaylist || audioData.restart)
        {
                waitingForPlaylist = false;
                audioData.restart = false;

                if (isShuffleEnabled())
                        reshufflePlaylist();
        }

        loadingdata.loadA = !usingSongDataA;

        loadingdata.loadingFirstDecoder = true;
        loadSong(currentSong, &loadingdata);

        int maxNumTries = 50;
        int numtries = 0;

        while (!loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
                if (currentSong->next != NULL)
                        skipToSong(currentSong->next->id, true);
        }

        updateLastSongSwitchTime();
        skip();
}

void skipToBegginningOfSong(void)
{
        if (currentSong != NULL)
        {
                bool playSong = false;
                skipToSong(currentSong->id, playSong);
        }
}

void prepareIfSkippedSilent(void)
{
        if (hasSilentlySwitched)
        {
                skipping = true;
                hasSilentlySwitched = false;
                updateLastSongSwitchTime();
                setCurrentImplementationType(NONE);
                setRepeatEnabled(false);
                audioData.endOfListReached = false;

                usingSongDataA = !usingSongDataA;

                skipping = false;
        }
}

void playbackPlay(double *totalPauseSeconds, double *pauseSeconds)
{
        if (isPaused())
        {
                *totalPauseSeconds += *pauseSeconds;
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
        else if (isStopped())
        {
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }

        if (isStopped() && !hasSilentlySwitched)
        {
                skipToBegginningOfSong();
        }

        resumePlayback();

        if (hasSilentlySwitched)
        {
                *totalPauseSeconds = 0;
                prepareIfSkippedSilent();
        }
}

void togglePause(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time)
{
        togglePausePlayback();
        if (isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");

                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        else
        {
                if (hasSilentlySwitched && !skipping)
                {
                        *totalPauseSeconds = 0;
                        prepareIfSkippedSilent();
                }
                else
                {
                        *totalPauseSeconds += *pauseSeconds;
                }
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
}

void toggleRepeat(void)
{

        bool repeatEnabled = !isRepeatEnabled();
        setRepeatEnabled(repeatEnabled);

        if (repeatEnabled)
        {
                emitStringPropertyChanged("LoopStatus", "Track");
        }
        else
        {
                emitStringPropertyChanged("LoopStatus", "None");
        }
        if (appState.currentView != SONG_VIEW)
                refresh = true;
}

void addToSpecialPlaylist(void)
{
        if (currentSong == NULL)
                return;

        int id = currentSong->id;

        Node *node = NULL;

        if (findSelectedEntryById(specialPlaylist, id) != NULL) // song is already in list
                return;

        createNode(&node, currentSong->song.filePath, id);
        addToList(specialPlaylist, node);
}

void toggleShuffle(void)
{
        bool shuffleEnabled = !isShuffleEnabled();
        setShuffleEnabled(shuffleEnabled);

        if (shuffleEnabled)
        {
                pthread_mutex_lock(&(playlist.mutex));

                shufflePlaylistStartingFromSong(&playlist, currentSong);

                pthread_mutex_unlock(&(playlist.mutex));

                emitBooleanPropertyChanged("Shuffle", TRUE);
        }
        else
        {
                char *path = NULL;
                if (currentSong != NULL)
                {
                        path = strdup(currentSong->song.filePath);
                }

                pthread_mutex_lock(&(playlist.mutex));

                deletePlaylist(&playlist); // Doesn't destroy the mutex
                deepCopyPlayListOntoList(originalPlaylist, &playlist);

                if (path != NULL)
                {
                        currentSong = findPathInPlaylist(path, &playlist);
                        free(path);
                }

                pthread_mutex_unlock(&(playlist.mutex));

                emitBooleanPropertyChanged("Shuffle", FALSE);
        }

        loadedNextSong = false;
        nextSong = NULL;

        if (appState.currentView == PLAYLIST_VIEW || appState.currentView == LIBRARY_VIEW)
                refresh = true;
}

void toggleBlocks(AppSettings *settings, UISettings *ui)
{
        ui->coverAnsi = !ui->coverAnsi;
        c_strcpy(settings->coverAnsi, ui->coverAnsi ? "1" : "0", sizeof(settings->coverAnsi));
        if (ui->coverEnabled)
        {
                clearScreen();
                refresh = true;
        }
}

void toggleColors(AppSettings *settings, UISettings *ui)
{
        ui->useConfigColors = !ui->useConfigColors;
        c_strcpy(settings->useConfigColors, ui->useConfigColors ? "1" : "0", sizeof(settings->useConfigColors));
        clearScreen();
        refresh = true;
}

void toggleVisualizer(AppSettings *settings, UISettings *ui)
{
        ui->visualizerEnabled = !ui->visualizerEnabled;
        c_strcpy(settings->visualizerEnabled, ui->visualizerEnabled ? "1" : "0", sizeof(settings->visualizerEnabled));
        restoreCursorPosition();
        refresh = true;
}

void quit(void)
{
        exit(0);
}

bool isCurrentSongDeleted(void)
{
        return (audioData.currentFileIndex == 0) ? userData.songdataADeleted == true : userData.songdataBDeleted == true;
}

bool isValidSong(SongData *songData)
{
        return songData != NULL && songData->hasErrors == false && songData->metadata != NULL;
}

SongData *getCurrentSongData(void)
{
        if (currentSong == NULL)
                return NULL;

        if (isCurrentSongDeleted())
                return NULL;

        SongData *songData = NULL;

        bool isDeleted = determineCurrentSongData(&songData);

        if (isDeleted)
                return NULL;

        if (!isValidSong(songData))
                return NULL;

        return songData;
}

void calcElapsedTime(void)
{
        if (isStopped())
                return;

        clock_gettime(CLOCK_MONOTONIC, &current_time);

        double timeSinceLastUpdate = (double)(current_time.tv_sec - lastUpdateTime.tv_sec) +
                                     (double)(current_time.tv_nsec - lastUpdateTime.tv_nsec) / 1e9;

        if (!isPaused())
        {
                elapsedSeconds = (double)(current_time.tv_sec - start_time.tv_sec) +
                                 (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
                double seekElapsed = getSeekElapsed();
                double diff = elapsedSeconds + (seekElapsed + seekAccumulatedSeconds - totalPauseSeconds);

                if (diff < 0)
                        seekElapsed -= diff;

                elapsedSeconds += seekElapsed + seekAccumulatedSeconds - totalPauseSeconds;
                if (elapsedSeconds > duration)
                        elapsedSeconds = duration;

                setSeekElapsed(seekElapsed);

                if (elapsedSeconds < 0.0)
                {
                        elapsedSeconds = 0.0;
                }

                if (currentSong != NULL && timeSinceLastUpdate >= 1.0)
                {
                        lastUpdateTime = current_time;
                }
        }
        else
        {
                pauseSeconds = (double)(current_time.tv_sec - pause_time.tv_sec) +
                               (double)(current_time.tv_nsec - pause_time.tv_nsec) / 1e9;
        }
}

void flushSeek(void)
{
        if (seekAccumulatedSeconds != 0.0)
        {
                if (currentSong != NULL)
                {
                        if (pathEndsWith(currentSong->song.filePath, "ogg"))
                        {
                                return;
                        }
                }

                setSeekElapsed(getSeekElapsed() + seekAccumulatedSeconds);
                seekAccumulatedSeconds = 0.0;
                calcElapsedTime();
                float percentage = elapsedSeconds / (float)duration * 100.0;

                if (percentage < 0.0)
                {
                        setSeekElapsed(0.0);
                        percentage = 0.0;
                }

                seekPercentage(percentage);

                emitSeekedSignal(elapsedSeconds);
        }
}

bool setPosition(gint64 newPosition)
{
        if (isPaused())
                return false;

        gint64 currentPositionMicroseconds = llround(elapsedSeconds * G_USEC_PER_SEC);

        if (duration != 0.0)
        {
                gint64 step = newPosition - currentPositionMicroseconds;
                step = step / G_USEC_PER_SEC;
                seekAccumulatedSeconds += step;
                return true;
        }
        else
        {
                return false;
        }
}

bool seekPosition(gint64 offset)
{
        if (isPaused())
                return false;

        if (duration != 0.0)
        {
                gint64 step = offset;
                step = step / G_USEC_PER_SEC;
                seekAccumulatedSeconds += step;
                return true;
        }
        else
        {
                return false;
        }
}

void seekForward(UIState *uis)
{
        if (currentSong != NULL)
        {
                if (pathEndsWith(currentSong->song.filePath, "ogg"))
                {
                        return;
                }
        }

        if (isPaused())
                return;

        if (duration != 0.0)
        {
                float step = 100 / uis->numProgressBars;
                seekAccumulatedSeconds += step * duration / 100.0;
        }
        fastForwarding = true;
}

void seekBack(UIState *uis)
{
        if (currentSong != NULL)
        {
                if (pathEndsWith(currentSong->song.filePath, "ogg"))
                {
                        return;
                }
        }

        if (isPaused())
                return;

        if (duration != 0.0)
        {
                float step = 100 / uis->numProgressBars;
                seekAccumulatedSeconds -= step * duration / 100.0;
        }
        rewinding = true;
}

Node *findSelectedEntryById(PlayList *playlist, int id)
{
        Node *node = playlist->head;

        if (node == NULL || id < 0)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++)
        {
                if (node != NULL && node->id == id)
                {
                        found = true;
                        break;
                }
                else if (node == NULL)
                {
                        return NULL;
                }
                node = node->next;
        }

        if (found)
        {
                return node;
        }

        return NULL;
}

Node *findSelectedEntry(PlayList *playlist, int row)
{
        Node *node = playlist->head;

        if (node == NULL)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++)
        {
                if (i == row)
                {
                        found = true;
                        break;
                }
                node = node->next;
        }

        if (found)
        {
                return node;
        }

        return NULL;
}

bool markAsDequeued(FileSystemEntry *root, char *path)
{
        int numChildrenEnqueued = 0;

        if (root == NULL)
                return false;

        if (!root->isDirectory)
        {
                if (strcmp(root->fullPath, path) == 0)
                {
                        root->isEnqueued = false;
                        return true;
                }
        }
        else
        {
                FileSystemEntry *child = root->children;
                bool found = false;
                while (child != NULL)
                {
                        found = markAsDequeued(child, path);
                        child = child->next;

                        if (found)
                                break;
                }

                if (found)
                {
                        child = root->children;

                        while (child != NULL)
                        {
                                if (child->isEnqueued)
                                        numChildrenEnqueued++;

                                child = child->next;
                        }

                        if (numChildrenEnqueued == 0)
                                root->isEnqueued = false;

                        return true;
                }
        }

        return false;
}

Node *getNextSong(void)
{
        if (nextSong != NULL)
                return nextSong;
        else if (currentSong != NULL && currentSong->next != NULL)
        {
                return currentSong->next;
        }
        else
        {
                return NULL;
        }
}

void enqueueSong(FileSystemEntry *child)
{
        int id = nodeIdCounter++;

        Node *node = NULL;
        createNode(&node, child->fullPath, id);
        addToList(originalPlaylist, node);

        Node *node2 = NULL;
        createNode(&node2, child->fullPath, id);
        addToList(&playlist, node2);

        child->isEnqueued = 1;
        child->parent->isEnqueued = 1;
}

void silentSwitchToNext(bool loadSong, AppState *state)
{
        skipping = true;
        nextSong = NULL;
        setCurrentSongToNext();
        activateSwitch(&audioData);

        skipOutOfOrder = true;

        usingSongDataA = (audioData.currentFileIndex == 0);

        if (loadSong)
        {
                loadNextSong();
                finishLoading();
                loadedNextSong = true;
                state->uiState.doNotifyMPRISSwitched = true;
        }

        resetTimeCount();
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        refresh = true;

        skipping = false;

        hasSilentlySwitched = true;

        nextSongNeedsRebuilding = true;
        nextSong = NULL;
}

void removeCurrentlyPlayingSong(void)
{
        stopPlayback();
        emitStringPropertyChanged("PlaybackStatus", "Stopped");

        clearCurrentTrack();

        loadedNextSong = false;
        audioData.restart = true;
        audioData.endOfListReached = true;
        lastPlayedId = currentSong->id;
        songToStartFrom = getListNext(currentSong);
        waitingForNext = true;
        currentSong = NULL;
}

void dequeueSong(FileSystemEntry *child)
{
        Node *node1 = findLastPathInPlaylist(child->fullPath, originalPlaylist);

        if (node1 == NULL)
                return;

        if (currentSong != NULL && currentSong->id == node1->id)
        {
                removeCurrentlyPlayingSong();
        }
        else
        {
                if (songToStartFrom != NULL)
                {
                        songToStartFrom = getListNext(node1);
                }
        }

        int id = node1->id;

        Node *node2 = findSelectedEntryById(&playlist, id);

        if (node1 != NULL)
                deleteFromList(originalPlaylist, node1);

        if (node2 != NULL)
                deleteFromList(&playlist, node2);

        child->isEnqueued = 0;

        // check if parent needs to be dequeued as well
        bool isEnqueued = false;

        FileSystemEntry *ch = child->parent->children;

        while (ch != NULL)
        {
                if (ch->isEnqueued)
                {
                        isEnqueued = true;
                        break;
                }
                ch = ch->next;
        }

        if (!isEnqueued)
        {
                child->parent->isEnqueued = 0;
                if (child->parent->parent != NULL)
                        child->parent->parent->isEnqueued = 0;
        }
}

void dequeueChildren(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        dequeueChildren(child);
                }
                else
                {
                        dequeueSong(child);
                }

                child = child->next;
        }
}

void enqueueChildren(FileSystemEntry *child, FileSystemEntry **firstEnqueuedEntry)
{
        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        child->isEnqueued = 1;
                        enqueueChildren(child->children, firstEnqueuedEntry);
                }
                else if (!child->isEnqueued)
                {
                        if (*firstEnqueuedEntry == NULL)
                                *firstEnqueuedEntry = child;
                        enqueueSong(child);
                }

                child = child->next;
        }
}

bool hasSongChildren(FileSystemEntry *entry)
{
        FileSystemEntry *child = entry->children;
        int numSongs = 0;

        while (child != NULL)
        {
                if (!child->isDirectory)
                        numSongs++;

                child = child->next;
        }

        if (numSongs == 0)
        {
                return false;
        }

        return true;
}

bool hasDequeuedChildren(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        bool isDequeued = false;
        while (child != NULL)
        {
                if (!child->isEnqueued)
                {
                        isDequeued = true;
                }
                child = child->next;
        }

        return isDequeued;
}

void enqueueSongs(FileSystemEntry *entry, UIState *uis)
{
        FileSystemEntry *chosenDir = getChosenDir();
        bool hasEnqueued = false;
        bool shuffle = false;
        FileSystemEntry *firstEnqueuedEntry = NULL;

        if (entry != NULL)
        {
                if (entry->isDirectory)
                {
                        if (!hasSongChildren(entry) || entry->parent == NULL || (chosenDir != NULL && strcmp(entry->fullPath, chosenDir->fullPath) == 0))
                        {
                                if (hasDequeuedChildren(entry))
                                {
                                        if (entry->parent == NULL)      // Shuffle playlist if it's the root
                                                shuffle = true;

                                        entry->isEnqueued = 1;
                                        entry = entry->children;

                                        enqueueChildren(entry, &firstEnqueuedEntry);

                                        nextSongNeedsRebuilding = true;

                                        hasEnqueued = true;
                                }
                                else
                                {
                                        dequeueChildren(entry);

                                        entry->isEnqueued = 0;

                                        nextSongNeedsRebuilding = true;
                                }
                        }
                        if ((chosenDir != NULL && entry->parent != NULL && strcmp(entry->parent->fullPath, chosenDir->fullPath) == 0) && uis->allowChooseSongs == true)
                        {
                                uis->openedSubDir = true;

                                FileSystemEntry *tmpc = entry->parent->children;

                                uis->numSongsAboveSubDir = 0;

                                while (tmpc != NULL)
                                {
                                        if (strcmp(entry->fullPath, tmpc->fullPath) == 0)
                                                break;
                                        tmpc = tmpc->next;
                                        uis->numSongsAboveSubDir++;
                                }
                        }
                        setCurrentAsChosenDir();
                        uis->allowChooseSongs = true;
                }
                else
                {
                        if (!entry->isEnqueued)
                        {
                                nextSong = NULL;
                                nextSongNeedsRebuilding = true;
                                firstEnqueuedEntry = entry;

                                enqueueSong(entry);

                                hasEnqueued = true;
                        }
                        else
                        {
                                nextSong = NULL;
                                nextSongNeedsRebuilding = true;

                                dequeueSong(entry);
                        }
                }
                refresh = true;
        }

        if (hasEnqueued)
        {
                waitingForNext = true;
                audioData.endOfListReached = false;
                if (firstEnqueuedEntry != NULL)
                        songToStartFrom = findPathInPlaylist(firstEnqueuedEntry->fullPath, &playlist);
                lastPlayedId = -1;
        }

        if (shuffle)
        {
                shufflePlaylist(&playlist);
                songToStartFrom = NULL;
        }

        if (nextSongNeedsRebuilding)
        {
                reshufflePlaylist();
        }
}

void handleRemove(void)
{
        if (refresh)
                return;

        if (appState.currentView != PLAYLIST_VIEW)
        {
                return;
        }

        bool rebuild = false;

        Node *node = findSelectedEntry(originalPlaylist, getChosenRow());

        if (node == NULL)
        {
                return;
        }

        Node *song = getNextSong();
        int id = node->id;

        int currentId = (currentSong != NULL) ? currentSong->id : -1;

        if (currentId == node->id)
        {
                removeCurrentlyPlayingSong();
        }
        else
        {
                if (songToStartFrom != NULL)
                {
                        songToStartFrom = getListNext(node);
                }
        }

        pthread_mutex_lock(&(playlist.mutex));

        if (node != NULL && song != NULL && currentSong != NULL)
        {
                if (strcmp(song->song.filePath, node->song.filePath) == 0 || (currentSong != NULL && currentSong->next != NULL && id == currentSong->next->id))
                        rebuild = true;
        }

        if (node != NULL)
                markAsDequeued(getLibrary(), node->song.filePath);

        Node *node2 = findSelectedEntryById(&playlist, id);

        if (node != NULL)
                deleteFromList(originalPlaylist, node);

        if (node2 != NULL)
                deleteFromList(&playlist, node2);

        if (isShuffleEnabled())
                rebuild = true;

        currentSong = findSelectedEntryById(&playlist, currentId);

        if (rebuild && currentSong != NULL)
        {
                node = NULL;
                nextSong = NULL;
                reshufflePlaylist();

                tryNextSong = currentSong->next;
                nextSongNeedsRebuilding = false;
                nextSong = NULL;
                nextSong = getListNext(currentSong);
                rebuildNextSong(nextSong);
                loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist.mutex));

        refresh = true;
}

Node *getSongByNumber(PlayList *playlist, int songNumber)
{
        Node *song = playlist->head;

        if (!song)
                return currentSong;

        if (songNumber <= 0)
        {
                return song;
        }

        int count = 1;

        while (song->next != NULL && count != songNumber)
        {
                song = getListNext(song);
                count++;
        }

        return song;
}

int loadDecoder(SongData *songData, bool *songDataDeleted)
{
        int result = 0;
        if (songData != NULL)
        {
                *songDataDeleted = false;

                // this should only be done for the second song, as switchAudioImplementation() handles the first one
                if (!loadingdata.loadingFirstDecoder)
                {
                        if (hasBuiltinDecoder(songData->filePath))
                                result = prepareNextDecoder(songData->filePath);
                        else if (pathEndsWith(songData->filePath, "opus"))
                                result = prepareNextOpusDecoder(songData->filePath);
                        else if (pathEndsWith(songData->filePath, "ogg"))
                                result = prepareNextVorbisDecoder(songData->filePath);
#ifdef USE_FAAD
                        else if (pathEndsWith(songData->filePath, "m4a") || pathEndsWith(songData->filePath, "aac"))
                                result = prepareNextM4aDecoder(songData->filePath);
#endif
                }
        }
        return result;
}

int assignLoadedData(void)
{
        int result = 0;

        if (loadingdata.loadA)
        {
                userData.songdataA = loadingdata.songdataA;
                result = loadDecoder(loadingdata.songdataA, &userData.songdataADeleted);
        }
        else
        {
                userData.songdataB = loadingdata.songdataB;
                result = loadDecoder(loadingdata.songdataB, &userData.songdataBDeleted);
        }

        return result;
}

void *songDataReaderThread(void *arg)
{
        LoadingThreadData *loadingdata = (LoadingThreadData *)arg;

        // Acquire the mutex lock
        pthread_mutex_lock(&(loadingdata->mutex));

        char filepath[MAXPATHLEN];
        c_strcpy(filepath, loadingdata->filePath, sizeof(filepath));

        SongData *songdata = NULL;

        if (loadingdata->loadA)
        {
                if (!userData.songdataADeleted)
                {
                        userData.songdataADeleted = true;
                        unloadSongData(&loadingdata->songdataA, &appState);
                }
        }
        else
        {
                if (!userData.songdataBDeleted)
                {
                        userData.songdataBDeleted = true;
                        unloadSongData(&loadingdata->songdataB, &appState);
                }
        }

        if (filepath[0] != '\0')
        {
                songdata = loadSongData(filepath, &appState);
        }
        else
                songdata = NULL;

        if (loadingdata->loadA)
        {
                loadingdata->songdataA = songdata;
        }
        else
        {
                loadingdata->songdataB = songdata;
        }

        int result = assignLoadedData();

        if (result < 0)
                songdata->hasErrors = true;

        // Release the mutex lock
        pthread_mutex_unlock(&(loadingdata->mutex));

        if (songdata != NULL && songdata->hasErrors)
        {
                songHasErrors = true;
                clearingErrors = true;
                nextSong = NULL;
        }
        else
        {
                songHasErrors = false;
                clearingErrors = false;
                nextSong = tryNextSong;
                tryNextSong = NULL;
        }

        loadedNextSong = true;
        skipping = false;
        songLoading = false;

        return NULL;
}

void loadSong(Node *song, LoadingThreadData *loadingdata)
{
        if (song == NULL)
        {
                loadedNextSong = true;
                skipping = false;
                songLoading = false;
                return;
        }

        c_strcpy(loadingdata->filePath, song->song.filePath, sizeof(loadingdata->filePath));

        pthread_t loadingThread;
        pthread_create(&loadingThread, NULL, songDataReaderThread, (void *)loadingdata);
}

void loadNext(LoadingThreadData *loadingdata)
{
        nextSong = getListNext(currentSong);

        if (nextSong == NULL)
        {
                c_strcpy(loadingdata->filePath, "", sizeof(loadingdata->filePath));
        }
        else
        {
                c_strcpy(loadingdata->filePath, nextSong->song.filePath, sizeof(loadingdata->filePath));
        }

        pthread_t loadingThread;
        pthread_create(&loadingThread, NULL, songDataReaderThread, (void *)loadingdata);
}

void rebuildNextSong(Node *song)
{
        if (song == NULL)
                return;

        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = false;

        songLoading = true;

        loadSong(song, &loadingdata);

        int maxNumTries = 50;
        int numtries = 0;

        while (songLoading && !loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }
        songLoading = false;
}

void stop(void)
{
        stopPlayback();

        if (isStopped())
        {
                emitStringPropertyChanged("PlaybackStatus", "Stopped");
        }
}

void loadNextSong(void)
{
        songLoading = true;
        nextSongNeedsRebuilding = false;
        skipFromStopped = false;
        loadingdata.loadA = !usingSongDataA;
        tryNextSong = nextSong = getListNext(currentSong);
        loadingdata.loadingFirstDecoder = false;
        loadSong(nextSong, &loadingdata);
}

bool determineCurrentSongData(SongData **currentSongData)
{
        *currentSongData = (audioData.currentFileIndex == 0) ? userData.songdataA : userData.songdataB;
        bool isDeleted = (audioData.currentFileIndex == 0) ? userData.songdataADeleted == true : userData.songdataBDeleted == true;

        if (isDeleted)
        {
                *currentSongData = (audioData.currentFileIndex != 0) ? userData.songdataA : userData.songdataB;
                isDeleted = (audioData.currentFileIndex != 0) ? userData.songdataADeleted == true : userData.songdataBDeleted == true;

                if (!isDeleted)
                {
                        activateSwitch(&audioData);
                        audioData.switchFiles = false;
                }
        }
        return isDeleted;
}

void setCurrentSongToNext(void)
{
        if (currentSong != NULL)
                lastPlayedId = currentSong->id;
        currentSong = getNextSong();
}

void finishLoading(void)
{
        int maxNumTries = 20;
        int numtries = 0;

        while (!loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        loadedNextSong = true;
}

void resetTimeCount(void)
{
        elapsedSeconds = 0.0;
        pauseSeconds = 0.0;
        totalPauseSeconds = 0.0;
}

void skipToNextSong(AppState *state)
{
        // Stop if there is no song or no next song
        if (currentSong == NULL || currentSong->next == NULL)
        {
                if (!isStopped() && !isPaused())
                        stop();
                return;
        }

        if (songLoading || nextSongNeedsRebuilding || skipping || clearingErrors)
                return;

        if (isStopped() || isPaused())
        {
                silentSwitchToNext(true, state);
                return;
        }

        playbackPlay(&totalPauseSeconds, &pauseSeconds);

        skipping = true;
        skipOutOfOrder = false;

        updateLastSongSwitchTime();

        skip();
}

void setCurrentSongToPrev(void)
{
        if (isShuffleEnabled() && currentSong != NULL && currentSong->prev == NULL)
        {
                if (currentSong->prev == NULL && currentSong->next != NULL)
                        currentSong = currentSong->next;
                else
                        return;
        }
        else
                currentSong = currentSong->prev;
}

void silentSwitchToPrev(AppState *state)
{
        skipping = true;

        setCurrentSongToPrev();
        activateSwitch(&audioData);

        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        usingSongDataA = !usingSongDataA;

        loadingdata.loadA = usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(currentSong, &loadingdata);
        state->uiState.doNotifyMPRISSwitched = true;
        finishLoading();

        resetTimeCount();
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        refresh = true;
        skipping = false;
        nextSongNeedsRebuilding = true;
        nextSong = NULL;

        skipOutOfOrder = true;
        hasSilentlySwitched = true;
}

void skipToPrevSong(AppState *state)
{
        // Stop if there is no song or no previous song
        if ((currentSong == NULL || currentSong->prev == NULL) && !isShuffleEnabled())
        {
                if (!isStopped() && !isPaused())
                        stop();
                return;
        }

        if (songLoading || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        if (isStopped() || isPaused())
        {
                silentSwitchToPrev(state);
                return;
        }

        setCurrentSongToPrev();

        playbackPlay(&totalPauseSeconds, &pauseSeconds);

        skipping = true;
        skipOutOfOrder = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(currentSong, &loadingdata);
        int maxNumTries = 50;
        int numtries = 0;

        while (!loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
                skipToPrevSong(state);
        }

        updateLastSongSwitchTime();
        skip();
}

void skipToNumberedSong(int songNumber)
{
        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        playbackPlay(&totalPauseSeconds, &pauseSeconds);

        skipping = true;
        skipOutOfOrder = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        currentSong = getSongByNumber(originalPlaylist, songNumber);

        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(currentSong, &loadingdata);
        int maxNumTries = 50;
        int numtries = 0;

        while (!loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
                if (songNumber < playlist.count)
                        skipToNumberedSong(songNumber + 1);
        }

        updateLastSongSwitchTime();
        skip();
}

void skipToLastSong(void)
{
        Node *song = playlist.head;

        if (!song)
                return;

        int count = 1;
        while (song->next != NULL)
        {
                song = getListNext(song);
                count++;
        }
        skipToNumberedSong(count);
}

void loadFirstSong(Node *song, UISettings *ui)
{
        if (song == NULL)
                return;
        loadingdata.loadingFirstDecoder = true;
        loadSong(song, &loadingdata);

        int i = 0;
        while (!loadedNextSong)
        {
                if (i != 0 && i % 1000 == 0 && ui->uiEnabled)
                        printf(".");
                i++;
                c_sleep(10);
                fflush(stdout);
        }
}

void unloadSongA(AppState *state)
{
        if (userData.songdataADeleted == false)
        {
                userData.songdataADeleted = true;
                unloadSongData(&loadingdata.songdataA, state);
                userData.songdataA = NULL;
        }
}

void unloadSongB(AppState *state)
{
        if (userData.songdataBDeleted == false)
        {
                userData.songdataBDeleted = true;
                unloadSongData(&loadingdata.songdataB, state);
                userData.songdataB = NULL;
        }
}

void unloadPreviousSong(AppState *state)
{
        pthread_mutex_lock(&(loadingdata.mutex));

        if (usingSongDataA &&
            (skipping || (userData.currentSongData == NULL || userData.songdataADeleted == false ||
                          (loadingdata.songdataA != NULL && userData.songdataADeleted == false && userData.currentSongData->hasErrors == 0 && userData.currentSongData->trackId != NULL && strcmp(loadingdata.songdataA->trackId, userData.currentSongData->trackId) != 0))))
        {
                unloadSongA(state);

                if (!audioData.endOfListReached)
                        loadedNextSong = false;

                usingSongDataA = false;
        }
        else if (!usingSongDataA &&
                 (skipping || (userData.currentSongData == NULL || userData.songdataBDeleted == false ||
                               (loadingdata.songdataB != NULL && userData.songdataBDeleted == false && userData.currentSongData->hasErrors == 0 && userData.currentSongData->trackId != NULL && strcmp(loadingdata.songdataB->trackId, userData.currentSongData->trackId) != 0))))
        {
                unloadSongB(state);

                if (!audioData.endOfListReached)
                        loadedNextSong = false;

                usingSongDataA = true;
        }

        pthread_mutex_unlock(&(loadingdata.mutex));
}

int loadFirst(Node *song, AppState *state)
{
        loadFirstSong(song, &state->uiSettings);

        usingSongDataA = true;

        while (songHasErrors && currentSong->next != NULL)
        {
                songHasErrors = false;
                loadedNextSong = false;
                currentSong = currentSong->next;
                loadFirstSong(currentSong, &state->uiSettings);
        }

        if (songHasErrors)
        {
                // couldn't play any of the songs
                unloadPreviousSong(state);
                currentSong = NULL;
                songHasErrors = false;
                return -1;
        }

        userData.currentPCMFrame = 0;
        userData.currentSongData = userData.songdataA;

        return 0;
}

void *updateLibraryThread(void *arg)
{
        char *path = (char *)arg;
        int tmpDirectoryTreeEntries = 0;

        FileSystemEntry *temp = createDirectoryTree(path, &tmpDirectoryTreeEntries);

        pthread_mutex_lock(&switchMutex);

        copyIsEnqueued(library, temp);

        freeTree(library);
        library = temp;
        appState.uiState.numDirectoryTreeEntries = tmpDirectoryTreeEntries;
        resetChosenDir();

        pthread_mutex_unlock(&switchMutex);

        refresh = true;

        return NULL;
}

void updateLibrary(char *path)
{
        pthread_t threadId;

        freeSearchResults();

        if (pthread_create(&threadId, NULL, updateLibraryThread, path) != 0)
        {
                perror("Failed to create thread");
                return;
        }
}

void askIfCacheLibrary(UISettings *ui)
{
        if (ui->cacheLibrary > -1) // Only use this function if cacheLibrary isn't set
                return;

        char input = '\0';

        restoreTerminalMode();
        enableInputBuffering();
        showCursor();

        printf("Would you like to enable a (local) library cache for quicker startup times?\nYou can update the cache at any time by pressing 'u'. (y/n): ");

        fflush(stdout);

        do
        {
                int res = scanf(" %c", &input);

                if (res < 0)
                        break;

        } while (input != 'Y' && input != 'y' && input != 'N' && input != 'n');

        if (input == 'Y' || input == 'y')
        {
                printf("Y\n");
                ui->cacheLibrary = 1;
        }
        else
        {
                printf("N\n");
                ui->cacheLibrary = 0;
        }

        setNonblockingMode();
        disableInputBuffering();
        hideCursor();
}

void createLibrary(AppSettings *settings, AppState *state)
{
        if (state->uiSettings.cacheLibrary > 0)
        {
                char *libFilepath = getLibraryFilePath();
                library = reconstructTreeFromFile(libFilepath, settings->path, &state->uiState.numDirectoryTreeEntries);
                free(libFilepath);
                updateLibraryIfChangedDetected();
        }

        if (library == NULL || library->children == NULL)
        {
                struct timeval start, end;

                gettimeofday(&start, NULL);

                library = createDirectoryTree(settings->path, &state->uiState.numDirectoryTreeEntries);

                gettimeofday(&end, NULL);
                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed = seconds + microseconds * 1e-6;

                // If time to load the library was significant, ask to use cache instead
                if (elapsed > ASK_IF_USE_CACHE_LIMIT_SECONDS)
                {
                        askIfCacheLibrary(&state->uiSettings);
                }
        }

        if (library == NULL || library->children == NULL)
        {
                exit(0);
        }
}

time_t getModificationTime(struct stat *path_stat)
{

        if (path_stat->st_mtime != 0)
        {
                return path_stat->st_mtime;
        }
        else
        {
#ifdef __APPLE__
                return path_stat->st_mtimespec.tv_sec; // macOS-specific member.
#else
                return path_stat->st_mtim.tv_sec; // Linux-specific member.
#endif
        }
}

void *updateIfTopLevelFoldersMtimesChangedThread(void *arg)
{
        UpdateLibraryThreadArgs *args = (UpdateLibraryThreadArgs *)arg; // Cast `arg` back to the structure pointer
        char *path = args->path;
        UISettings *ui = args->ui;

        struct stat path_stat;

        if (stat(path, &path_stat) == -1)
        {
                perror("stat");
                free(args);
                pthread_exit(NULL);
        }

        if (getModificationTime(&path_stat) > ui->lastTimeAppRan && ui->lastTimeAppRan > 0)
        {
                updateLibrary(path);
                free(args);
                pthread_exit(NULL);
        }

        DIR *dir = opendir(path);
        if (!dir)
        {
                perror("opendir");
                free(args);
                pthread_exit(NULL);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {

                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                {
                        continue;
                }

                char fullPath[1024];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

                if (stat(fullPath, &path_stat) == -1)
                {
                        perror("stat");
                        continue;
                }

                if (S_ISDIR(path_stat.st_mode))
                {

                        if (getModificationTime(&path_stat) > ui->lastTimeAppRan && ui->lastTimeAppRan > 0)
                        {
                                updateLibrary(path);
                                break;
                        }
                }
        }

        closedir(dir);

        free(args);

        pthread_exit(NULL);
}

// This only checks the library mtime and toplevel subfolders mtimes
void updateLibraryIfChangedDetected(void)
{
        pthread_t tid;

        UpdateLibraryThreadArgs *args = malloc(sizeof(UpdateLibraryThreadArgs));
        if (args == NULL)
        {
                perror("malloc");
                return;
        }

        args->path = settings.path;
        args->ui = &appState.uiSettings;

        if (pthread_create(&tid, NULL, updateIfTopLevelFoldersMtimesChangedThread, (void *)args) != 0)
        {
                perror("pthread_create");
                free(args);
        }
}
