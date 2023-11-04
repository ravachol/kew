#include "playerops.h"

double elapsedSeconds = 0.0;
double pauseSeconds = 0.0;
double totalPauseSeconds = 0.0;

struct timespec current_time;
struct timespec start_time;
struct timespec pause_time;
struct timespec lastInputTime;

bool playingMainPlaylist = false;
bool usingSongDataA = true;
bool doQuit = false;

LoadingThreadData loadingdata;

volatile bool loadedNextSong = false;
Node *nextSong = NULL;
Node *tryNextSong = NULL;
Node *prevSong = NULL;

bool songHasErrors = false;
bool skipPrev = false;
bool skipping = false;
bool loadingFailed = false;
bool forceSkip = false;
volatile bool clearingErrors = false;
volatile bool songLoading = false;
GDBusConnection *connection = NULL;

UserData userData;

void updateLastSongSwitchTime()
{        
        clock_gettime(CLOCK_MONOTONIC, &start_time);
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

void emitMetadataChanged(const gchar *title, const gchar *artist, const gchar *album, const gchar *coverArtPath, const gchar *trackId)
{
        // Convert the coverArtPath to a valid URL format
        gchar *coverArtUrl = g_strdup_printf("file://%s", coverArtPath);

        // Create a GVariantBuilder for the metadata dictionary
        GVariantBuilder metadata_builder;
        g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);
        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title", g_variant_new_string(title));

        // Build list os strings for artist
        const gchar *artistList[2];
        if (artist)
        {
                artistList[0] = artist;
                artistList[1] = NULL;
        }
        else
        {
                artistList[0] = "";
                artistList[1] = NULL;
        }

        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist", g_variant_new_strv(artistList, -1));
        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album", g_variant_new_string(album));
        g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl", g_variant_new_string(coverArtUrl));
        g_variant_builder_add(&metadata_builder, "{sv}", "mpris:trackid", g_variant_new_object_path(trackId));

        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "Metadata", g_variant_builder_end(&metadata_builder));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoPrevious", g_variant_new_boolean(currentSong->prev != NULL));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoNext", g_variant_new_boolean(currentSong->next != NULL));

        g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                      g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);

        g_variant_builder_clear(&metadata_builder);
        g_variant_builder_clear(&changed_properties_builder);
}

void playbackPause(double *totalPauseSeconds, double pauseSeconds, struct timespec *pause_time)
{
        if (!isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");

                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        pausePlayback();
}

void playbackPlay(double *totalPauseSeconds, double pauseSeconds, struct timespec *pause_time)
{
        if (isPaused())
        {
                *totalPauseSeconds += pauseSeconds;

                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
        resumePlayback();
}

void togglePause(double *totalPauseSeconds, double pauseSeconds, struct timespec *pause_time)
{
        togglePausePlayback();
        if (isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");

                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        else
        {
                *totalPauseSeconds += pauseSeconds;

                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
}

void toggleRepeat()
{
        repeatEnabled = !repeatEnabled;
        if (repeatEnabled)
        {
                emitStringPropertyChanged("LoopStatus", "Track");
        }
        else
        {
                emitStringPropertyChanged("LoopStatus", "None");
        }
        if (printInfo)
                refresh = true;
}

void addToPlaylist()
{
        if (!playingMainPlaylist)
        {
                SongInfo song;
                song.filePath = strdup(currentSong->song.filePath);
                song.duration = 0.0;
                addToList(mainPlaylist, song);
        }
}

void toggleShuffle()
{
        shuffleEnabled = !shuffleEnabled;

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
                                 (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9 + seekElapsed;
                elapsedSeconds -= totalPauseSeconds;
        }
        else
        {
                pauseSeconds = (double)(current_time.tv_sec - pause_time.tv_sec) +
                               (double)(current_time.tv_nsec - pause_time.tv_nsec) / 1e9;
        }
}

void seekForward()
{
        float percentage = 0;

        if (duration != 0)
        {
                float step = 100 / numProgressBars;
                percentage = elapsed / (float)duration * 100.0;

                float newElapsed = (percentage + step) * duration / 100.0;

                if (newElapsed < 0)
                {
                        newElapsed = 0;
                }
                else if (newElapsed > duration)
                {
                        newElapsed = duration;
                }

                percentage = newElapsed / duration * 100.0;
                seekElapsed += step * duration / 100.0;
                seekPercentage(percentage);
        }
}

void seekBack()
{
        float percentage = 0;

        if (duration != 0)
        {
                float step = 100 / numProgressBars;
                percentage = elapsed / (float)duration * 100.0;

                float newElapsed = (percentage - step) * duration / 100.0;

                if (newElapsed < 0)
                {
                        newElapsed = 0;
                }
                else if (newElapsed > duration)
                {
                        newElapsed = duration;
                }

                percentage = newElapsed / duration * 100.0;
                seekElapsed -= step * duration / 100.0;
                elapsed = newElapsed;
                seekPercentage(percentage);
        }
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
        if (usingSongDataA)
        {
                if (loadingdata.songdataB != NULL)
                {
                        userData.filenameB = loadingdata.songdataB->pcmFilePath;
                        userData.songdataB = loadingdata.songdataB;
                }
                else
                        userData.filenameB = NULL;
        }
        else
        {
                if (loadingdata.songdataA != NULL)
                {
                        userData.filenameA = loadingdata.songdataA->pcmFilePath;
                        userData.songdataA = loadingdata.songdataA;
                }
                else
                        userData.filenameA = NULL;
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

void skipToNextSong()
{
        if (currentSong->next == NULL)
        {
                return;
        }

        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                return;

        skipping = true;
        updateLastSongSwitchTime();
        skip();
}

void skipToPrevSong()
{
        if (songLoading || !loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        skipping = true;
        skipPrev = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        if (currentSong->prev == NULL)
        {
                currentSong;
        }
        else
                currentSong = currentSong->prev;

        loadingdata.loadA = !usingSongDataA;
        unloadSongData(usingSongDataA ? &loadingdata.songdataB : &loadingdata.songdataA);

        loadSong(currentSong, &loadingdata);
        while (!loadedNextSong && !loadingFailed)
        {
                c_sleep(10);
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

        skipping = true;
        skipPrev = true;
        loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        currentSong = getSongByNumber(songNumber);

        loadingdata.loadA = !usingSongDataA;
        unloadSongData(usingSongDataA ? &loadingdata.songdataB : &loadingdata.songdataA);

        loadSong(currentSong, &loadingdata);
        while (!loadedNextSong && !loadingFailed)
        {
                c_sleep(10);
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
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
}