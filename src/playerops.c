#include "playerops.h"
/*

playerops.c

 Related to features/actions of the player.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

struct timespec current_time;
struct timespec start_time;
struct timespec pause_time;
struct timespec lastInputTime;
struct timespec lastPlaylistChangeTime;
struct timespec lastUpdateTime = {0, 0};

bool playlistNeedsUpdate = false;
bool nextSongNeedsRebuilding = false;
bool enqueuedNeedsUpdate = false;

bool playingMainPlaylist = false;
bool usingSongDataA = true;

LoadingThreadData loadingdata;

volatile bool loadedNextSong = false;
bool waitingForPlaylist = false;
bool waitingForNext = false;
Node *nextSong = NULL;
Node *tryNextSong = NULL;
Node *prevSong = NULL;
int lastPlayedId = -1;

bool songHasErrors = false;
bool skipPrev = false;
bool skipping = false;
bool loadingFailed = false;
bool forceSkip = false;
volatile bool clearingErrors = false;
volatile bool songLoading = false;
GDBusConnection *connection = NULL;

void updatePlaylist()
{
        playlistNeedsUpdate = false;

        if (isShuffleEnabled())
        {
                if (currentSong != NULL)
                        shufflePlaylistStartingFromSong(&playlist, currentSong);
                else
                        shufflePlaylist(&playlist);

                nextSongNeedsRebuilding = true;
        }
}

void rebuildAndUpdatePlaylist()
{
        if (!playlistNeedsUpdate && !nextSongNeedsRebuilding)
                return;

        pthread_mutex_lock(&(playlist.mutex));

        if (playlistNeedsUpdate)
        {
                updatePlaylist();
        }

        pthread_mutex_unlock(&(playlist.mutex));
}

void skip()
{
        setCurrentImplementationType(NONE);

        setRepeatEnabled(false);
        audioData.endOfListReached = false;

        rebuildAndUpdatePlaylist();

        if (!isPlaying())
        {
                switchAudioImplementation();
        }
        else
        {
                setSkipToNext(true);
        }

        if (!skipPrev)
                refresh = true;
}

void updateLastSongSwitchTime()
{
        clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void updateLastPlaylistChangeTime()
{
        clock_gettime(CLOCK_MONOTONIC, &lastPlaylistChangeTime);
}

void updateLastInputTime()
{
        clock_gettime(CLOCK_MONOTONIC, &lastInputTime);
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
        gint64 newPositionMicroseconds = llround(newPositionSeconds * G_USEC_PER_SEC);

        GVariant *parameters = g_variant_new("(x)", newPositionMicroseconds);

        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.mpris.MediaPlayer2.Player",
                                      "Seeked",
                                      parameters,
                                      NULL);
}

void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue)
{
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName, g_variant_new_string(newValue));
        g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                      g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);
        g_variant_builder_clear(&changed_properties_builder);
}

void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue)
{
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName, g_variant_new_boolean(newValue));
        g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                      g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);
        g_variant_builder_clear(&changed_properties_builder);
}

void playbackPause(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time)
{
        if (!isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");

                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        pausePlayback();
}

void playbackPlay(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time)
{
        if (isPaused())
        {
                *totalPauseSeconds += *pauseSeconds;
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
        resumePlayback();
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
                *totalPauseSeconds += *pauseSeconds;
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
}

void toggleRepeat()
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

void addToPlaylist()
{
        if (!playingMainPlaylist)
        {
                Node *node = NULL;
                createNode(&node, currentSong->song.filePath, nodeIdCounter++);
                addToList(mainPlaylist, node);
        }
}

void toggleShuffle()
{
        bool shuffleEnabled = !isShuffleEnabled();
        setShuffleEnabled(shuffleEnabled);

        if (shuffleEnabled)
        {
                shufflePlaylistStartingFromSong(&playlist, currentSong);
                emitBooleanPropertyChanged("Shuffle", TRUE);
        }
        else
        {
                char *path = NULL;
                if (currentSong != NULL)
                {
                        path = strdup(currentSong->song.filePath);
                }
                deletePlaylist(&playlist);
                playlist = deepCopyPlayList(originalPlaylist);

                if (path != NULL)
                {
                        currentSong = findPathInPlaylist(path, &playlist);
                        free(path);
                }
                emitBooleanPropertyChanged("Shuffle", FALSE);
        }

        loadedNextSong = false;
        nextSong = NULL;

        if (appState.currentView == PLAYLIST_VIEW || appState.currentView == LIBRARY_VIEW)
                refresh = true;
}

void toggleBlocks()
{
        coverAnsi = !coverAnsi;
        c_strcpy(settings.coverAnsi, sizeof(settings.coverAnsi), coverAnsi ? "1" : "0");
        if (coverEnabled)
        {
                clearScreen();
                refresh = true;
        }
}

void toggleColors()
{
        useProfileColors = !useProfileColors;
        c_strcpy(settings.useProfileColors, sizeof(settings.useProfileColors), useProfileColors ? "1" : "0");
        clearScreen();
        refresh = true;
}

void toggleCovers()
{
        coverEnabled = !coverEnabled;
        c_strcpy(settings.coverEnabled, sizeof(settings.coverEnabled), coverEnabled ? "1" : "0");
        clearScreen();
        refresh = true;
}

void toggleVisualizer()
{
        visualizerEnabled = !visualizerEnabled;
        c_strcpy(settings.visualizerEnabled, sizeof(settings.visualizerEnabled), visualizerEnabled ? "1" : "0");
        restoreCursorPosition();
        refresh = true;
}

void quit()
{
        doQuit = true;
}

SongData *getCurrentSongData()
{
        if (currentSong == NULL)
                return NULL;

        if (usingSongDataA)
                return loadingdata.songdataA;
        else
                return loadingdata.songdataB;
}

void calcElapsedTime()
{
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
                        updatePlaybackPosition(elapsedSeconds);
                        // Update the last update time to the current time
                        lastUpdateTime = current_time;
                }
        }
        else
        {
                pauseSeconds = (double)(current_time.tv_sec - pause_time.tv_sec) +
                               (double)(current_time.tv_nsec - pause_time.tv_nsec) / 1e9;
        }
}

void flushSeek()
{
        if (seekAccumulatedSeconds != 0.0)
        {
                if (currentSong != NULL)
                {
                        if (endsWith(currentSong->song.filePath, "ogg"))
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

void seekForward()
{
        if (currentSong != NULL)
        {
                if (endsWith(currentSong->song.filePath, "ogg"))
                {
                        return;
                }
        }

        if (duration != 0.0)
        {
                float step = 100 / numProgressBars;
                seekAccumulatedSeconds += step * duration / 100.0;
        }
        fastForwarding = true;
}

void seekBack()
{
        if (currentSong != NULL)
        {
                if (endsWith(currentSong->song.filePath, "ogg"))
                {
                        return;
                }
        }

        if (duration != 0.0)
        {
                float step = 100 / numProgressBars;
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

Node *getNextSong()
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

void dequeueSong(FileSystemEntry *child)
{
        Node *node1 = findLastPathInPlaylist(child->fullPath, originalPlaylist);

        if (node1 == NULL)
                return;

        if (currentSong != NULL && currentSong->id == node1->id)
                return;

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
                child->parent->isEnqueued = 0;
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

void enqueueChildren(FileSystemEntry *child)
{
        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        child->isEnqueued = 1;
                        enqueueChildren(child->children);
                }
                else if (!child->isEnqueued)
                {
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

void enqueueSongs()
{
        FileSystemEntry *tmp = getCurrentLibEntry();
        FileSystemEntry *chosenDir = getChosenDir();
        bool hasEnqueued = false;

        if (tmp != NULL)
        {
                if (tmp->isDirectory)
                {
                        if (!hasSongChildren(tmp) || (chosenDir != NULL && strcmp(tmp->fullPath, chosenDir->fullPath) == 0))
                        {
                                if (hasDequeuedChildren(tmp))
                                {
                                        tmp->isEnqueued = 1;
                                        tmp = tmp->children;

                                        enqueueChildren(tmp);

                                        nextSongNeedsRebuilding = true;

                                        hasEnqueued = true;
                                }
                                else
                                {
                                        dequeueChildren(tmp);

                                        nextSongNeedsRebuilding = true;
                                }
                        }

                        setCurrentAsChosenDir();
                        allowChooseSongs = true;
                }
                else
                {
                        if (!tmp->isEnqueued)
                        {
                                nextSong = NULL;
                                nextSongNeedsRebuilding = true;

                                enqueueSong(tmp);

                                hasEnqueued = true;
                        }
                        else
                        {
                                nextSong = NULL;
                                nextSongNeedsRebuilding = true;

                                dequeueSong(tmp);
                        }
                }
                refresh = true;
        }

        if (hasEnqueued)
        {
                waitingForNext = true;
                audioData.endOfListReached = false;
        }

        if (nextSongNeedsRebuilding)
        {
                updatePlaylist();
        }
        updateLastPlaylistChangeTime();
}

void handleRemove()
{
        if (refresh)
                return;

        pthread_mutex_lock(&(playlist.mutex));

        bool rebuild = false;

        if (appState.currentView != PLAYLIST_VIEW)
        {
                pthread_mutex_unlock(&(playlist.mutex));
                return;
        }

        Node *node = findSelectedEntry(originalPlaylist, chosenRow);
        Node *song = getNextSong();
        int id = node->id;

        int currentId = (currentSong != NULL) ? currentSong->id : -1;

        if (node != NULL && playlist.head != NULL && playlist.head->id == node->id && playlist.count == 1)
        {
                pthread_mutex_unlock(&(playlist.mutex));
                return;
        }

        if (currentId == node->id)
        {
                pthread_mutex_unlock(&(playlist.mutex));
                return;
        }

        if (node != NULL && song != NULL)
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

        updateLastPlaylistChangeTime();

        if (isShuffleEnabled())
                rebuild = true;

        currentSong = findSelectedEntryById(&playlist, currentId);

        if (rebuild)
        {
                node = NULL;
                nextSong = NULL;
                updatePlaylist();

                songLoading = true;

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

int loadDecoder(SongData *songData, char **filename, bool *songDataDeleted)
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
                        else if (endsWith(songData->filePath, "opus"))
                                result = prepareNextOpusDecoder(songData->filePath);
                        else if (endsWith(songData->filePath, "ogg"))
                                result = prepareNextVorbisDecoder(songData->filePath);
                        else if (endsWith(songData->filePath, "m4a"))
                                result = prepareNextM4aDecoder(songData->filePath);
                }
        }
        return result;
}

int assignLoadedData()
{
        int result = 0;

        if (loadingdata.loadA)
        {
                userData.songdataA = loadingdata.songdataA;
                result = loadDecoder(loadingdata.songdataA, &userData.filenameA, &userData.songdataADeleted);
        }
        else
        {
                userData.songdataB = loadingdata.songdataB;
                result = loadDecoder(loadingdata.songdataB, &userData.filenameB, &userData.songdataBDeleted);
        }

        return result;
}

void *songDataReaderThread(void *arg)
{
        LoadingThreadData *loadingdata = (LoadingThreadData *)arg;

        // Acquire the mutex lock
        pthread_mutex_lock(&(loadingdata->mutex));

        char filepath[MAXPATHLEN];
        c_strcpy(filepath, sizeof(filepath), loadingdata->filePath);

        SongData *songdata = NULL;

        if (loadingdata->loadA)
        {
                if (!userData.songdataADeleted)
                {
                        userData.songdataADeleted = true;
                        unloadSongData(&loadingdata->songdataA);
                }
        }
        else
        {
                if (!userData.songdataBDeleted)
                {
                        userData.songdataBDeleted = true;
                        unloadSongData(&loadingdata->songdataB);
                }
        }

        if (filepath[0] != '\0')
        {
                songdata = loadSongData(filepath);
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

        c_strcpy(loadingdata->filePath, sizeof(loadingdata->filePath), song->song.filePath);

        pthread_t loadingThread;
        pthread_create(&loadingThread, NULL, songDataReaderThread, (void *)loadingdata);
}

void loadNext(LoadingThreadData *loadingdata)
{
        nextSong = getListNext(currentSong);

        if (nextSong == NULL)
        {
                c_strcpy(loadingdata->filePath, sizeof(loadingdata->filePath), "");
        }
        else
        {
                c_strcpy(loadingdata->filePath, sizeof(loadingdata->filePath), nextSong->song.filePath);
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

        pthread_mutex_lock(&(loadingdata.mutex));

        songLoading = true;

        loadSong(song, &loadingdata);

        pthread_mutex_unlock(&(loadingdata.mutex));

        int maxNumTries = 50;
        int numtries = 0;

        while (songLoading && !loadedNextSong && !loadingFailed && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }
}

void skipToNextSong()
{
        if (currentSong == NULL || currentSong->next == NULL)
        {
                return;
        }

        if (songLoading || nextSongNeedsRebuilding || skipping || clearingErrors)
                return;

        playbackPlay(&totalPauseSeconds, &pauseSeconds, &pause_time);

        skipping = true;

        updateLastSongSwitchTime();

        skip();
}

void skipToPrevSong()
{
        if ((currentSong == NULL || currentSong->prev == NULL) && !isShuffleEnabled())
        {
                return;
        }

        if (songLoading || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        if (isShuffleEnabled() && currentSong != NULL && currentSong->prev == NULL)
        {
                if (currentSong->prev == NULL && currentSong->next != NULL)
                        currentSong = currentSong->next;
                else
                        return;
        }
        else
                currentSong = currentSong->prev;

        playbackPlay(&totalPauseSeconds, &pauseSeconds, &pause_time);

        skipping = true;
        skipPrev = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(currentSong, &loadingdata);
        int maxNumTries = 50;
        int numtries = 0;

        while (!loadedNextSong && !loadingFailed && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
                skipToPrevSong();
        }

        updateLastSongSwitchTime();
        skip();
}

void skipToSong(int id)
{
        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        playbackPlay(&totalPauseSeconds, &pauseSeconds, &pause_time);

        skipping = true;
        skipPrev = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        Node *found = NULL;
        findNodeInList(&playlist, id, &found);

        if (found != NULL)
                currentSong = found;
        else
                return;

        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(currentSong, &loadingdata);
        int maxNumTries = 50;
        int numtries = 0;

        while (!loadedNextSong && !loadingFailed && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
                if (currentSong->next != NULL)
                        skipToSong(currentSong->next->id);
        }

        updateLastSongSwitchTime();
        skip();
}

void skipToNumberedSong(int songNumber)
{
        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        playbackPlay(&totalPauseSeconds, &pauseSeconds, &pause_time);

        skipping = true;
        skipPrev = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        currentSong = getSongByNumber(originalPlaylist, songNumber);

        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(currentSong, &loadingdata);
        int maxNumTries = 50;
        int numtries = 0;

        while (!loadedNextSong && !loadingFailed && numtries < maxNumTries)
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

void skipToLastSong()
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

void loadFirstSong(Node *song)
{
        if (song == NULL)
                return;
        loadingdata.loadingFirstDecoder = true;
        loadSong(song, &loadingdata);

        int i = 0;
        while (!loadedNextSong)
        {
                if (i != 0 && i % 1000 == 0 && uiEnabled)
                        printf(".");
                i++;
                c_sleep(10);
                fflush(stdout);
        }
}

void loadFirst(Node *song)
{
        loadFirstSong(song);

        usingSongDataA = true;

        while (songHasErrors && currentSong->next != NULL)
        {
                songHasErrors = false;
                loadedNextSong = false;
                currentSong = currentSong->next;
                loadFirstSong(currentSong);
        }

        if (songHasErrors)
        {
                printf("Couldn't play any of the songs.\n");
                return;
        }

        userData.currentPCMFrame = 0;
        userData.currentSongData = userData.songdataA;
}
