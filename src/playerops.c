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
Node *lastPlayedSong = NULL;

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
                shufflePlaylistStartingFromSong(&playlist, currentSong);

                nextSongNeedsRebuilding = true;
        }
}

void updateNextSong()
{
        nextSongNeedsRebuilding = false;
        nextSong = NULL;
        Node *song = getNextSong();
        rebuildNextSong(song);
        loadedNextSong = false;
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

        if (nextSongNeedsRebuilding)
        {
                updateNextSong();
        }

        pthread_mutex_unlock(&(playlist.mutex));
}

void skip()
{
        setCurrentImplementationType(NONE);
        setSkipToNext(true);
        setRepeatEnabled(false);
        audioData.endOfListReached = false;

        rebuildAndUpdatePlaylist();

        if (!isPlaying())
        {
                switchAudioImplementation();
        }
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
                SongInfo song;
                song.filePath = strdup(currentSong->song.filePath);
                song.duration = 0.0;
                addToList(mainPlaylist, song, nodeIdCounter++);
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
                playlist = deepCopyPlayList(originalPlaylist);
                currentSong = findSongInPlaylist(currentSong, &playlist);
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
        if (usingSongDataA)
                return loadingdata.songdataA;
        else
                return loadingdata.songdataB;
}

void calcElapsedTime()
{
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        if (!isPaused())
        {
                elapsedSeconds = (double)(current_time.tv_sec - start_time.tv_sec) +
                                 (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
                elapsedSeconds += getSeekElapsed() + seekAccumulatedSeconds;
                elapsedSeconds -= totalPauseSeconds;
                if (elapsedSeconds > duration)
                        elapsedSeconds = duration;
                if (elapsedSeconds < 0.0)
                        elapsedSeconds = 0.0;
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
                if (elapsedSeconds + getSeekElapsed() + seekAccumulatedSeconds < 0.0)
                        return;
                seekAccumulatedSeconds -= step * duration / 100.0;
        }
        rewinding = true;
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

void removeSelectedEntry(PlayList *playlist, Node *node)
{
        if (node != NULL)
                markAsDequeued(getLibrary(), node->song.filePath);

        deleteFromList(playlist, node);
}

void removeSelectedEntryByPath(PlayList *playlist, char *path)
{
        if (path != NULL)
                markAsDequeued(getLibrary(), path);
        Node *node = findLastPathInPlaylist(path, playlist);
        deleteFromList(playlist, node);
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
        SongInfo song;
        song.filePath = strdup(child->fullPath);
        song.duration = 0.0;
        nodeIdCounter++;
        addToList(originalPlaylist, song, nodeIdCounter);
        addToList(&playlist, song, nodeIdCounter);

        child->isEnqueued = 1;
        child->parent->isEnqueued = 1;
}

void dequeueSong(FileSystemEntry *child)
{
        child->isEnqueued = 0;
        Node *node1 = findLastPathInPlaylist(child->fullPath, originalPlaylist);

        if (node1 == NULL)
                return;

        if (currentSong != NULL && currentSong->id == node1->id)
                return;

        if (node1 != NULL)
                deleteFromList(originalPlaylist, node1);
        Node *node2 = findLastPathInPlaylist(child->fullPath, &playlist);
        if (node2 != NULL)
                deleteFromList(&playlist, node2);

        child->isEnqueued = 0;
        child->parent->isEnqueued = 0;
}

void dequeueChildren(FileSystemEntry *child)
{
        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        child->isEnqueued = 0;
                        dequeueChildren(child->children);
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
                else
                {
                        enqueueSong(child);
                }

                child = child->next;
        }
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
                        if (getEntryTreeDepth(tmp) == 1 || (chosenDir != NULL && strcmp(tmp->fullPath, chosenDir->fullPath) == 0))
                        {
                                if (!tmp->isEnqueued)
                                {
                                        tmp->isEnqueued = 1;
                                        tmp = tmp->children;

                                        enqueueChildren(tmp);

                                        nextSongNeedsRebuilding = true;

                                        hasEnqueued = true;
                                }
                                else
                                {
                                        tmp->isEnqueued = 0;
                                        tmp = tmp->children;

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
                                Node *song = getNextSong();
                                if (song == NULL || strcmp(tmp->fullPath, song->song.filePath) == 0)
                                {
                                        nextSong = NULL;
                                        nextSongNeedsRebuilding = true;
                                }
                                enqueueSong(tmp);

                                hasEnqueued = true;
                        }
                        else
                        {
                                Node *song = getNextSong();
                                if (song == NULL || strcmp(tmp->fullPath, song->song.filePath) == 0)
                                {
                                        nextSong = NULL;
                                        nextSongNeedsRebuilding = true;
                                }
                                dequeueSong(tmp);
                        }
                }
                refresh = true;
        }

        if (hasEnqueued)
        {
                waitingForNext = true;

                if (audioData.endOfListReached)
                        loadedNextSong = false;
                audioData.endOfListReached = false;
        }        

        playlistNeedsUpdate = true;
        updateLastPlaylistChangeTime();        
}

void resetList()
{
        audioData.currentFileIndex = 1 - audioData.currentFileIndex;
        setCurrentImplementationType(NONE);
}

void handleRemove()
{
        pthread_mutex_lock(&(playlist.mutex));

        bool rebuild = false;

        if (appState.currentView != PLAYLIST_VIEW)
        {
                pthread_mutex_unlock(&(playlist.mutex));
                return;
        }

        Node *node = findSelectedEntry(originalPlaylist, chosenRow);
        Node *song = getNextSong();

        if (currentSong != NULL && currentSong->id == node->id)
        {
                pthread_mutex_unlock(&(playlist.mutex));
                return;
        }

        if (node != NULL && song != NULL)
        {
                if (strcmp(song->song.filePath, node->song.filePath) == 0)
                        rebuild = true;
        }

        char *path = strdup(node->song.filePath);

        removeSelectedEntry(originalPlaylist, node);
        removeSelectedEntryByPath(&playlist, path);

        playlistNeedsUpdate = true;
        updateLastPlaylistChangeTime();

        if (isShuffleEnabled())
                rebuild = true;

        if (rebuild)
        {
                node = NULL;
                nextSong = NULL;
                nextSongNeedsRebuilding = true;
        }

        free(path);

        pthread_mutex_unlock(&(playlist.mutex));

        refresh = true;
}

Node *getSongByNumber(int songNumber)
{
        Node *song = playlist.head;

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

void assignLoadedData()
{
        if (loadingdata.loadA)
        {
                if (loadingdata.songdataA != NULL)
                {
                        userData.filenameA = loadingdata.songdataA->pcmFilePath;
                        userData.songdataA = loadingdata.songdataA;

                        if (hasBuiltinDecoder(loadingdata.songdataA->filePath))
                                prepareNextDecoder(loadingdata.songdataA->filePath);
                        else if (endsWith(loadingdata.songdataA->filePath, "opus"))
                                prepareNextOpusDecoder(loadingdata.songdataA->filePath);
                        else if (endsWith(loadingdata.songdataA->filePath, "ogg"))
                                prepareNextVorbisDecoder(loadingdata.songdataA->filePath);
                }
                else
                        userData.filenameA = NULL;
        }
        else
        {
                if (loadingdata.songdataB != NULL)
                {
                        userData.filenameB = loadingdata.songdataB->pcmFilePath;
                        userData.songdataB = loadingdata.songdataB;

                        if (hasBuiltinDecoder(loadingdata.songdataB->filePath))
                                prepareNextDecoder(loadingdata.songdataB->filePath);
                        else if (endsWith(loadingdata.songdataB->filePath, "opus"))
                                prepareNextOpusDecoder(loadingdata.songdataB->filePath);
                        else if (endsWith(loadingdata.songdataB->filePath, "ogg"))
                                prepareNextVorbisDecoder(loadingdata.songdataB->filePath);
                }
                else
                        userData.filenameB = NULL;
        }
}

void *songDataReaderThread(void *arg)
{
        LoadingThreadData *loadingdata = (LoadingThreadData *)arg;

        // Acquire the mutex lock
        pthread_mutex_lock(&(loadingdata->mutex));

        char filepath[MAXPATHLEN];
        c_strcpy(filepath, sizeof(filepath), loadingdata->filePath);
        SongData *songdata = NULL;

        if (filepath[0] != '\0')
        {
                songdata = loadSongData(filepath);
        }
        else
                songdata = NULL;

        if (loadingdata->loadA)
        {
                unloadSongData(&loadingdata->songdataA);
                loadingdata->songdataA = songdata;
        }
        else
        {
                unloadSongData(&loadingdata->songdataB);
                loadingdata->songdataB = songdata;
        }

        assignLoadedData();

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
                loadingFailed = true;
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

        pthread_mutex_lock(&(loadingdata.mutex));

        unloadSongData(usingSongDataA ? &loadingdata.songdataB : &loadingdata.songdataA);

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
        if (currentSong->next == NULL)
        {
                return;
        }

        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                return;

        playbackPlay(&totalPauseSeconds, &pauseSeconds, &pause_time);

        skipping = true;
        updateLastSongSwitchTime();
        skip();
}

void skipToPrevSong()
{
        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;


        if (currentSong->prev != NULL)
                currentSong = currentSong->prev;   

        playbackPlay(&totalPauseSeconds, &pauseSeconds, &pause_time);                                    

        skipping = true;
        skipPrev = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        loadingdata.loadA = !usingSongDataA;
        unloadSongData(usingSongDataA ? &loadingdata.songdataB : &loadingdata.songdataA);

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

        currentSong = getSongByNumber(songNumber);

        loadingdata.loadA = !usingSongDataA;
        unloadSongData(usingSongDataA ? &loadingdata.songdataB : &loadingdata.songdataA);

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
        userData.filenameA = loadingdata.songdataA->pcmFilePath;
        userData.songdataA = loadingdata.songdataA;
        userData.currentSongData = userData.songdataA;
        audioData.currentFileIndex = 0;
}
