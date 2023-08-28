/* cue - a command-line music player
Copyright (C) 2022 Ravachol

http://github.com/ravachol/cue

 $$$$$$$\ $$\   $$\  $$$$$$\
$$  _____|$$ |  $$ |$$  __$$\
$$ /      $$ |  $$ |$$$$$$$$ |
$$ |      $$ |  $$ |$$   ____|
\$$$$$$$\ \$$$$$$  |\$$$$$$$\
 \_______| \______/  \_______|

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdio.h>
#include <pwd.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include <poll.h>
#include <dirent.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <unistd.h>
#include <glib.h>
#include <gio/gio.h>

#include "soundgapless.h"
#include "stringfunc.h"
#include "settings.h"
#include "printfunc.h"
#include "playlist.h"
#include "events.h"
#include "file.h"
#include "visuals.h"
#include "albumart.h"
#include "player.h"
#include "cache.h"
#include "songloader.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

typedef struct
{
    char filePath[MAXPATHLEN];
    SongData *songdataA;
    SongData *songdataB;
    bool loadA;
    pthread_mutex_t mutex;
} LoadingThreadData;

bool playingMainPlaylist = false;
bool doQuit = false;
bool usingSongDataA = true;
bool loadingFailed = false;
bool skipPrev = false;
bool skipping = false;
enum modes {normal,vim};
enum modes selectedMode = normal;

struct timespec current_time;
struct timespec start_time;
struct timespec pause_time;

double elapsedSeconds = 0.0;
double pauseSeconds = 0.0;
double totalPauseSeconds = 0.0;

volatile bool loadedNextSong = false;
volatile bool songLoading = false;

UserData userData;
LoadingThreadData loadingdata;

Node *nextSong = NULL;
Node *prevSong = NULL;

#define COOLDOWN_DURATION 1000

static struct timespec lastInputTime;
static bool eventProcessed = false;

// Mpris variables:

GDBusConnection* connection = NULL;
GMainContext* global_main_context = NULL;
GMainLoop* main_loop;

void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue) {  

    // Create a GVariantBuilder for changed properties
    GVariantBuilder changed_properties_builder;
    g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName, g_variant_new_string(newValue));

    // Emit the PropertiesChanged signal
    g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                  g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);

    // Clean up
    g_variant_builder_clear(&changed_properties_builder);
}

bool isCooldownElapsed()
{
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    double elapsedMilliseconds = (currentTime.tv_sec - lastInputTime.tv_sec) * 1000.0 +
                                 (currentTime.tv_nsec - lastInputTime.tv_nsec) / 1000000.0;
    return elapsedMilliseconds >= COOLDOWN_DURATION;
}

void updateLastInputTime()
{
    clock_gettime(CLOCK_MONOTONIC, &lastInputTime);
}

struct Event processInput()
{
    struct Event event;
    event.type = EVENT_NONE;
    event.key = '\0';
    bool cooldownElapsed = false;

    if (songLoading || !loadedNextSong)
        return event;

    if (!isInputAvailable())
        return event;

    if (isCooldownElapsed() && !eventProcessed)
        cooldownElapsed = true;

    char seq[4];
    int seqLength = 0;
    while (isInputAvailable())
    {
        seqLength = readInputSequence(seq, sizeof(seq));
        usleep(10000);
    }

    eventProcessed = true;

    event.type = EVENT_NONE;
    event.key = seq[0];

    if (seqLength > 1) { //0x1B +

            if (strcmp(seq, "[A") == 0) {
                // Arrow Up
                    event.type = EVENT_VOLUME_UP;
            } else if (strcmp(seq, "[B") == 0) {
                // Arrow Down
                    event.type = EVENT_VOLUME_DOWN;
            } else if (strcmp(seq, "[C") == 0) {
                // Arrow Left
                    event.type = EVENT_NEXT;
            } else if (strcmp(seq, "[D") == 0) {
                // Arrow Right
                    event.type = EVENT_PREV;
            } else if (strcmp(seq, "OP") == 0 || strcmp(seq, "[[A") == 0) {
                // F1 key
                event.type = EVENT_SHOWINFO;
            } else if (strcmp(seq, "OQ") == 0 || strcmp(seq, "[[B") == 0) {
                // F2 key
                event.type = EVENT_SHOWKEYBIDINGS;    
            }
    }
    else
    {
        switch (event.key)
        {
            case 'k' :   // Next song
                event.type = EVENT_NEXT;
                break;
            case 'j':   // Prev song
                event.type = EVENT_PREV;
                break;
            case 'l':   // Volume UP
                event.type = EVENT_VOLUME_UP;
                break;
            case 'h':   //Volume DOWN
                event.type = EVENT_VOLUME_DOWN;
                break;
            case 'p':   // Play/Pau se
                event.type = EVENT_PLAY_PAUSE;
                break;      
            case 'q':
                event.type = EVENT_QUIT;
                break;
            case 's':
                event.type = EVENT_SHUFFLE;
                break;
            case 'c':
                event.type = EVENT_TOGGLECOVERS;
                break;
            case 'v':
                event.type = EVENT_TOGGLEVISUALIZER;
                break;
            case 'b':
                event.type = EVENT_TOGGLEBLOCKS;
                break;
            case 'a':
                event.type = EVENT_ADDTOMAINPLAYLIST;
                break;
            case 'd':
                event.type = EVENT_DELETEFROMMAINPLAYLIST;
                break;
            case 'r':
                event.type = EVENT_TOGGLEREPEAT;
                break;
            case 'x':
                event.type = EVENT_EXPORTPLAYLIST;
                break;              
            case ' ':
                event.type = EVENT_PLAY_PAUSE;
                break;            
            default:
                break;
        }
    }

    // cooldown is for next and previous only
    if (!cooldownElapsed && (event.type == EVENT_NEXT || event.type == EVENT_PREV))
        event.type = EVENT_NONE;
    else
        updateLastInputTime();

    return event;
}

void cleanup()
{
    cleanupPlaybackDevice();
    unloadSongData(&loadingdata.songdataA);
    unloadSongData(&loadingdata.songdataB);
    clearRestOfScreen();
}

void doShuffle()
{
    stopPlayListDurationCount();
    usleep(100000);
    playlist.totalDuration = 0.0;
    shufflePlaylistStartingFromSong(&playlist, currentSong);
    calculatePlayListDuration(&playlist);
    usleep(100000);
    loadedNextSong = false;
    refresh = true;
    nextSong = NULL;
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

void toggleBlocks()
{
    coverAnsi = !coverAnsi;
    strcpy(settings.coverAnsi, coverAnsi ? "1" : "0");
    if (coverEnabled)
    {
        clearScreen();
        refresh = true;
    }
}

void toggleCovers()
{
    coverEnabled = !coverEnabled;
    strcpy(settings.coverEnabled, coverEnabled ? "1" : "0");
    clearScreen();
    refresh = true;
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
}

void toggleVisualizer()
{
    visualizerEnabled = !visualizerEnabled;
    strcpy(settings.visualizerEnabled, visualizerEnabled ? "1" : "0");
    restoreCursorPosition();
    refresh = true;
}

void playbackPlay(double *totalPauseSeconds, double pauseSeconds, struct timespec *pause_time)
{
    if (isPaused())
    {
        *totalPauseSeconds += pauseSeconds;

        // Emit PropertiesChanged signal
        emitStringPropertyChanged("PlaybackStatus", "Playing");     
    }    
    resumePlayback();
}

void playbackPause(double *totalPauseSeconds, double pauseSeconds, struct timespec *pause_time)
{
    if (!isPaused())
    {
        // Emit PropertiesChanged signal
        emitStringPropertyChanged("PlaybackStatus", "Paused");
                                        
        clock_gettime(CLOCK_MONOTONIC, pause_time);
    }    
    pausePlayback();    
}

void togglePause(double *totalPauseSeconds, double pauseSeconds, struct timespec *pause_time)
{
    togglePausePlayback();
    if (isPaused())
    {
        // Emit PlaybackStatusChanged signal
        // Emit PropertiesChanged signal
        emitStringPropertyChanged("PlaybackStatus", "Paused");
                                        
        clock_gettime(CLOCK_MONOTONIC, pause_time);
    }
    else
    {
        *totalPauseSeconds += pauseSeconds;

        // Emit PropertiesChanged signal
        emitStringPropertyChanged("PlaybackStatus", "Playing");      
    }
    
}

void quit()
{
    doQuit = true;
}

void freeAudioBuffer()
{
    if (g_audioBuffer != NULL)
    {
        free(g_audioBuffer);
        g_audioBuffer = NULL;
    }
}

void assignLoadedData()
{
    if (usingSongDataA)
    {
        if (loadingdata.songdataB != NULL)
            userData.pcmFileB.filename = loadingdata.songdataB->pcmFilePath;
        else
            userData.pcmFileB.filename = NULL;
        userData.pcmFileB.file = NULL;
    }
    else
    {
        if (loadingdata.songdataA != NULL)
            userData.pcmFileA.filename = loadingdata.songdataA->pcmFilePath;
        else
            userData.pcmFileA.filename = NULL;
        userData.pcmFileA.file = NULL;
    }
}

// After updating the playback state (play, pause, stop)
void updatePlaybackStatus(const gchar* status) {
    // Update your playback status here
    
    // Emit the PlaybackStatus signal
    GVariant* status_variant = g_variant_new_string(status);
    g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2/cueMusic", "org.mpris.MediaPlayer2.Player", "PlaybackStatus", g_variant_new("(s)", status_variant), NULL);

    // Clean up
    g_variant_unref(status_variant);
}

void emitMetadataChanged(const gchar* title, const gchar* artist, const gchar* album, const gchar* coverArtPath, const gchar* trackId) {
    // Convert the coverArtPath to a valid URL format
    gchar *coverArtUrl = g_strdup_printf("file://%s", coverArtPath);

    // Create a GVariantBuilder for the metadata dictionary
    GVariantBuilder metadata_builder;
    g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);
    g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title", g_variant_new_string(title));
    g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist", g_variant_new_string(artist));
    g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album", g_variant_new_string(album));
    g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl", g_variant_new_string(coverArtUrl));
    g_variant_builder_add(&metadata_builder, "{sv}", "mpris:trackid", g_variant_new_object_path(trackId));

    // Create a GVariantBuilder for changed properties
    GVariantBuilder changed_properties_builder;
    g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&changed_properties_builder, "{sv}", "Metadata", g_variant_builder_end(&metadata_builder));
    g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoPrevious", g_variant_new_boolean(currentSong->prev != NULL));
    g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoNext", g_variant_new_boolean(currentSong->next != NULL));

    // Emit the PropertiesChanged signal
    g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                  g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);

    // Clean up
    g_variant_builder_clear(&metadata_builder);
    g_variant_builder_clear(&changed_properties_builder);
}

void *songDataReaderThread(void *arg)
{
    LoadingThreadData *loadingdata = (LoadingThreadData *)arg;

    // Acquire the mutex lock
    pthread_mutex_lock(&(loadingdata->mutex));

    char filepath[MAXPATHLEN];
    strcpy(filepath, loadingdata->filePath);
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

    loadedNextSong = true;
    skipping = false;
    songLoading = false;

    // Release the mutex lock
    pthread_mutex_unlock(&(loadingdata->mutex));

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

    strcpy(loadingdata->filePath, song->song.filePath);

    pthread_t loadingThread;
    pthread_create(&loadingThread, NULL, songDataReaderThread, (void *)loadingdata);
}

void loadNext(LoadingThreadData *loadingdata)
{
    nextSong = getListNext(currentSong);

    if (nextSong == NULL)
    {
        strcpy(loadingdata->filePath, "");
    }
    else
    {
        strcpy(loadingdata->filePath, nextSong->song.filePath);
    }

    pthread_t loadingThread;
    pthread_create(&loadingThread, NULL, songDataReaderThread, (void *)loadingdata);
}

bool isPlaybackOfListDone()
{
    return (userData.endOfListReached == 1);
}

void prepareNextSong()
{
    while (!loadedNextSong && !loadingFailed)
    {
        usleep(10000);
    }

    if (loadingFailed)
        return;

    if (!skipPrev && !repeatEnabled)
        currentSong = currentSong->next;
    else
        skipPrev = false;

    if (currentSong == NULL)
    {
        quit();
        return;
    }

    elapsedSeconds = 0.0;
    pauseSeconds = 0.0;
    totalPauseSeconds = 0.0;

    loadedNextSong = false;
    nextSong = NULL;

    refresh = true;

    if (!repeatEnabled)
    {
        if (usingSongDataA)
        {
            unloadSongData(&loadingdata.songdataA);
            userData.pcmFileA.file = NULL;
            userData.pcmFileA.filename = NULL;
        }
        else
        {
            unloadSongData(&loadingdata.songdataB);
            userData.pcmFileB.file = NULL;
            userData.pcmFileB.filename = NULL;
        }
        usingSongDataA = !usingSongDataA;
    }
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void skipToNextSong()
{
    if (currentSong->next == NULL)
    {
        return;
    }
        
    if (songLoading || !loadedNextSong || skipping)
        return;

    skipping = true;
    skip();
}

void skipToPrevSong()
{
    if (currentSong->prev == NULL)
    {
        return;
    }

    if (songLoading || !loadedNextSong || skipping)
    return;

    skipping = true;
    skipPrev = true;
    loadedNextSong = false;
    songLoading = true;

    currentSong = currentSong->prev;
    if (usingSongDataA)
    {
        loadingdata.loadA = false;
        unloadSongData(&loadingdata.songdataB);
    }
    else
    {
        loadingdata.loadA = true;
        unloadSongData(&loadingdata.songdataA);
    }

    loadSong(currentSong, &loadingdata);
    while (!loadedNextSong && !loadingFailed)
    {
        usleep(10000);
    }
    clock_gettime(CLOCK_MONOTONIC, &start_time);   
    skip();
}

void calcElapsedTime()
{
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    if (!isPaused())
    {
        elapsedSeconds = (double)(current_time.tv_sec - start_time.tv_sec) +
                         (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
        elapsedSeconds -= totalPauseSeconds;
    }
    else
    {
        pauseSeconds = (double)(current_time.tv_sec - pause_time.tv_sec) +
                       (double)(current_time.tv_nsec - pause_time.tv_nsec) / 1e9;
    }
}

void refreshPlayer()
{
    if (usingSongDataA)
        printPlayer(loadingdata.songdataA, elapsedSeconds, &playlist);
    else
        printPlayer(loadingdata.songdataB, elapsedSeconds, &playlist);
}

void loadAudioData()
{
    if (nextSong == NULL && !songLoading)
    {
        songLoading = true;
        loadingdata.loadA = !usingSongDataA;
        loadNext(&loadingdata);
    }
}

void handleInput()
{
    struct Event event = processInput();

    switch (event.type)
    {
    case EVENT_PLAY_PAUSE:
        togglePause(&totalPauseSeconds, pauseSeconds, &pause_time);
        break;
    case EVENT_TOGGLEVISUALIZER:
        toggleVisualizer();
        break;
    case EVENT_TOGGLEREPEAT:
        toggleRepeat();
        break;
    case EVENT_TOGGLECOVERS:
        toggleCovers();
        break;
    case EVENT_TOGGLEBLOCKS:
        toggleBlocks();
        break;
    case EVENT_SHUFFLE:
        doShuffle();
        break;
    case EVENT_QUIT:
        quit();
        break;
    case EVENT_VOLUME_UP:
        adjustVolumePercent(5);
        break;
    case EVENT_VOLUME_DOWN:
        adjustVolumePercent(-5);
        break;
    case EVENT_NEXT:
        skipToNextSong();
        break;
    case EVENT_PREV:
        skipToPrevSong();
        break;
    case EVENT_ADDTOMAINPLAYLIST:
        addToPlaylist();
        break;
    case EVENT_DELETEFROMMAINPLAYLIST:
        // FIXME implement this
        break;
    case EVENT_EXPORTPLAYLIST:
        savePlaylist();
        break;
    case EVENT_SHOWKEYBIDINGS:
        refresh = true;
        printKeyBindings = !printKeyBindings;
        printInfo = false;
        break;
    case EVENT_SHOWINFO:
        refresh = true;
        printInfo = !printInfo;
        printKeyBindings = false;
        break;
    default:
        break;
    }
    eventProcessed = false;
}

void resize()
{
    alarm(1); // Timer
    setCursorPosition(1, 1);
    clearRestOfScreen();
    while (resizeFlag)
    {
        usleep(100000);
    }
    alarm(0); // Cancel timer
    refresh = true;
    printf("\033[1;1H");
    clearRestOfScreen();
}

void updatePlayer()
{
    if (resizeFlag)
        resize();
    else
    {
        if (refresh)
        {
            SongData *currentSongData = NULL;

            if (usingSongDataA)
                currentSongData = loadingdata.songdataA;
            else
                currentSongData = loadingdata.songdataB;
                
            // update mpris
            emitMetadataChanged(
                currentSongData->metadata->title ? currentSongData->metadata->title : "",
                currentSongData->metadata->artist ? currentSongData->metadata->artist : "",
                currentSongData->metadata->album ? currentSongData->metadata->album : "",
                currentSongData->coverArtPath ? currentSongData->coverArtPath : "",
                currentSongData->trackId ? currentSongData->trackId : ""
            );           
        }
        refreshPlayer();
    }        
}

// MPRIS Functions:
const gchar* introspection_xml =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"org.mpris.MediaPlayer2\">\n"
    "    <method name=\"Raise\"/>\n"
    "    <method name=\"Quit\"/>\n"
    "    <property name=\"CanQuit\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"Fullscreen\" type=\"b\" access=\"readwrite\"/>\n"
    "    <property name=\"CanSetFullscreen\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanRaise\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"HasTrackList\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"Identity\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"DesktopIconName\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>\n"
    "    <property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>\n"
    "  </interface>\n"
    "  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
    "    <method name=\"Next\"/>\n"
    "    <method name=\"Previous\"/>\n"
    "    <method name=\"Pause\"/>\n"
    "    <method name=\"PlayPause\"/>\n"
    "    <method name=\"Stop\"/>\n"
    "    <method name=\"Play\"/>\n"
    "    <method name=\"Seek\">\n"
    "      <arg name=\"x\" type=\"x\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"SetPosition\">\n"
    "      <arg name=\"o\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"x\" type=\"x\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"OpenUri\">\n"
    "      <arg name=\"s\" type=\"s\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"Seeked\">\n"
    "      <arg name=\"x\" type=\"x\"/>\n"
    "    </signal>\n"
    "    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"LoopStatus\" type=\"s\" access=\"readwrite\"/>\n"
    "    <property name=\"Rate\" type=\"d\" access=\"readwrite\"/>\n"
    "    <property name=\"Shuffle\" type=\"b\" access=\"readwrite\"/>\n"
    "    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>\n"
    "    <property name=\"Volume\" type=\"d\" access=\"readwrite\"/>\n"
    "    <property name=\"Position\" type=\"x\" access=\"read\"/>\n"
    "    <property name=\"MinimumRate\" type=\"d\" access=\"read\"/>\n"
    "    <property name=\"MaximumRate\" type=\"d\" access=\"read\"/>\n"
    "    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPause\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanSeek\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanControl\" type=\"b\" access=\"read\"/>\n"
    "  </interface>\n"
    "</node>\n";

static gboolean canRaise = FALSE;
static gboolean canQuit = TRUE;
static gboolean hasTrackList = FALSE;
static const gchar *identity = "cueMusic";
static const gchar *supportedUriSchemes[] = {"file", "http", NULL};
static const gchar *supportedMimeTypes[] = {"audio/mpeg", "audio/ogg", NULL};
static const gchar *desktopIconName = ""; // Without file extension
static const gchar *desktopEntry = ""; // The name of your .desktop file

// Implement Raise method
static void handle_raise(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *method_name, GVariant *parameters,
                         GDBusMethodInvocation *invocation, gpointer user_data) {    
}

// Property getter for CanRaise
static gboolean get_can_raise(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(canRaise);
    return TRUE;
}

// Implement Quit method
static void handle_quit(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data) {
    quit();
}

// Property getter for CanQuit
static gboolean get_can_quit(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(canQuit);
    return TRUE;
}

// Property getter for HasTrackList
static gboolean get_has_track_list(GDBusConnection *connection, const gchar *sender,
                                  const gchar *object_path, const gchar *interface_name,
                                  const gchar *property_name, GVariant **value,
                                  GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(hasTrackList);
    return TRUE;
}

// Property getter for Identity
static gboolean get_identity(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *property_name, GVariant **value,
                            GError **error, gpointer user_data) {
    *value = g_variant_new_string(identity);
    return TRUE;
}

// Property getter for DesktopEntry
static gboolean get_desktop_entry(GDBusConnection *connection, const gchar *sender,
                                  const gchar *object_path, const gchar *interface_name,
                                  const gchar *property_name, GVariant **value,
                                  GError **error, gpointer user_data) {
    *value = g_variant_new_string(desktopEntry);
    return TRUE;
}

// Property getter for DesktopIconName
static gboolean get_desktop_icon_name(GDBusConnection *connection, const gchar *sender,
                                      const gchar *object_path, const gchar *interface_name,
                                      const gchar *property_name, GVariant **value,
                                      GError **error, gpointer user_data) {
    *value = g_variant_new_string(desktopIconName);
    return TRUE;
}

// Property getter for SupportedUriSchemes
static gboolean get_supported_uri_schemes(GDBusConnection *connection, const gchar *sender,
                                         const gchar *object_path, const gchar *interface_name,
                                         const gchar *property_name, GVariant **value,
                                         GError **error, gpointer user_data) {
    *value = g_variant_new_strv(supportedUriSchemes, -1);
    return TRUE;
}

// Property getter for SupportedMimeTypes
static gboolean get_supported_mime_types(GDBusConnection *connection, const gchar *sender,
                                        const gchar *object_path, const gchar *interface_name,
                                        const gchar *property_name, GVariant **value,
                                        GError **error, gpointer user_data) {
    *value = g_variant_new_strv(supportedMimeTypes, -1);
    return TRUE;
}

// Method handlers
static void handle_next(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data) {
    skipToNextSong();
    g_dbus_method_invocation_return_value(invocation, NULL);                            

}

static void handle_previous(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *method_name, GVariant *parameters,
                            GDBusMethodInvocation *invocation, gpointer user_data) {
    skipToPrevSong();    
    g_dbus_method_invocation_return_value(invocation, NULL);                                

}

static void handle_pause(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *method_name, GVariant *parameters,
                         GDBusMethodInvocation *invocation, gpointer user_data) {
    playbackPause(&totalPauseSeconds, pauseSeconds, &pause_time);                             
}

static void handle_play_pause(GDBusConnection *connection, const gchar *sender,
                              const gchar *object_path, const gchar *interface_name,
                              const gchar *method_name, GVariant *parameters,
                              GDBusMethodInvocation *invocation, gpointer user_data) {
    togglePause(&totalPauseSeconds, pauseSeconds, &pause_time);
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_stop(GDBusConnection *connection, const gchar *sender,
                       const gchar *object_path, const gchar *interface_name,
                       const gchar *method_name, GVariant *parameters,
                       GDBusMethodInvocation *invocation, gpointer user_data) {
    quit();
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_play(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data) {
    playbackPlay(&totalPauseSeconds, pauseSeconds, &pause_time);                        

}

static void handle_seek(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data) {

}

static void handle_set_position(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *method_name, GVariant *parameters,
                                GDBusMethodInvocation *invocation, gpointer user_data) {

}

static void handle_open_uri(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *method_name, GVariant *parameters,
                            GDBusMethodInvocation *invocation, gpointer user_data) {

}

static void handle_method_call(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path, const gchar *interface_name,
                               const gchar *method_name, GVariant *parameters,
                               GDBusMethodInvocation *invocation, gpointer user_data) {
    if (g_strcmp0(method_name, "PlayPause") == 0) {
        handle_play_pause(connection, sender, object_path, interface_name,
                          method_name, parameters, invocation, user_data);
    } else if (g_strcmp0(method_name, "Next") == 0) {
        handle_next(connection, sender, object_path, interface_name,
                    method_name, parameters, invocation, user_data);
    } else if (g_strcmp0(method_name, "Previous") == 0) {
        handle_previous(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data);
    } else if (g_strcmp0(method_name, "Pause") == 0) {
        handle_pause(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data);
    } else if (g_strcmp0(method_name, "Stop") == 0) {
        handle_stop(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data);
    } else if (g_strcmp0(method_name, "Play") == 0) {
        handle_play(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data);                                                                    
    } else if (g_strcmp0(method_name, "Seek") == 0) {
        handle_seek(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data);   
 } else if (g_strcmp0(method_name, "SetPosition") == 0) {
        handle_set_position(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data); 
 } else if (g_strcmp0(method_name, "Raise") == 0) {
        handle_raise(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data); 
 } else if (g_strcmp0(method_name, "Quit") == 0) {
        handle_quit(connection, sender, object_path, interface_name,
                        method_name, parameters, invocation, user_data);                                                                                                                                                                   
    } else {
        g_dbus_method_invocation_return_dbus_error(invocation,
                                                   "org.freedesktop.DBus.Error.UnknownMethod",
                                                   "No such method");
    }
}

static void on_bus_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    //g_print("Acquired bus name: %s\n", name);
}

static void on_bus_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    //g_print("Lost bus name: %s\n", name);
}


static const gchar *PlaybackStatus = "Playing"; 
static const gchar *LoopStatus = "None";
static gdouble Rate = 1.0;
static gboolean Shuffle = FALSE;
static gdouble Volume = 0.5;
static gint64 Position = 0;
static gdouble MinimumRate = 1.0;
static gdouble MaximumRate = 1.0;
static gboolean CanGoNext = TRUE;
static gboolean CanGoPrevious = TRUE;
static gboolean CanPlay = TRUE;
static gboolean CanPause = TRUE;
static gboolean CanSeek = FALSE;
static gboolean CanControl = TRUE;

// Property getter for PlaybackStatus
static gboolean get_playback_status(GDBusConnection *connection, const gchar *sender,
                                    const gchar *object_path, const gchar *interface_name,
                                    const gchar *property_name, GVariant **value,
                                    GError **error, gpointer user_data) {

    if (isPaused())
        PlaybackStatus = "Paused";
    else
        PlaybackStatus = "Playing";

    *value = g_variant_new_string(PlaybackStatus);
    return TRUE;
}

// Property getter for LoopStatus
static gboolean get_loop_status(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data) {
    *value = g_variant_new_string(LoopStatus);
    return TRUE;
}

// Property getter for Rate
static gboolean get_rate(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *property_name, GVariant **value,
                         GError **error, gpointer user_data) {
    *value = g_variant_new_double(Rate);
    return TRUE;
}

// Property getter for Shuffle
static gboolean get_shuffle(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *property_name, GVariant **value,
                            GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(Shuffle);
    return TRUE;
}

// Property getter for Metadata
static gboolean get_metadata(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data) {

    SongData *currentSongData = NULL;

   if (usingSongDataA)
        currentSongData = loadingdata.songdataA;
    else
        currentSongData = loadingdata.songdataB;
    
    GVariantBuilder metadata_builder;
    g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);
    g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title", g_variant_new_string(currentSongData->metadata->title));
    g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist", g_variant_new_string(currentSongData->metadata->artist));
    g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album", g_variant_new_string(currentSongData->metadata->album));
    g_variant_builder_add(&metadata_builder, "{sv}", "xesam:contentCreated", g_variant_new_string(currentSongData->metadata->date));
    g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl", g_variant_new_string(currentSongData->coverArtPath));
    g_variant_builder_add(&metadata_builder, "{sv}", "mpris:trackid", g_variant_new_object_path(currentSongData->trackId));
    // Add other metadata fields to the builder
    
    GVariant *metadata_variant = g_variant_builder_end(&metadata_builder);

    *value = g_variant_ref_sink(metadata_variant);
    return TRUE;
}

// Property getter for Volume
static gboolean get_volume(GDBusConnection *connection, const gchar *sender,
                           const gchar *object_path, const gchar *interface_name,
                           const gchar *property_name, GVariant **value,
                           GError **error, gpointer user_data) {

    Volume = getCurrentVolume();

    *value = g_variant_new_double(Volume);
    return TRUE;
}

// Property getter for Position
static gboolean get_position(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data) {
    *value = g_variant_new_int64(Position);
    return TRUE;
}

// Property getter for MinimumRate
static gboolean get_minimum_rate(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data) {
    *value = g_variant_new_double(MinimumRate);
    return TRUE;
}

// Property getter for MaximumRate
static gboolean get_maximum_rate(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data) {
    *value = g_variant_new_double(MaximumRate);
    return TRUE;
}

// Property getter for CanGoNext
static gboolean get_can_go_next(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path, const gchar *interface_name,
                               const gchar *property_name, GVariant **value,
                               GError **error, gpointer user_data) {

    CanGoNext = (currentSong->next != NULL);

    *value = g_variant_new_boolean(CanGoNext);
    return TRUE;
}

// Property getter for CanGoPrevious
static gboolean get_can_go_previous(GDBusConnection *connection, const gchar *sender,
                                   const gchar *object_path, const gchar *interface_name,
                                   const gchar *property_name, GVariant **value,
                                   GError **error, gpointer user_data) {

    CanGoPrevious = (currentSong->prev != NULL);

    *value = g_variant_new_boolean(CanGoPrevious);
    return TRUE;
}

// Property getter for CanPlay
static gboolean get_can_play(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *property_name, GVariant **value,
                            GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(CanPlay);
    return TRUE;
}

// Property getter for CanPause
static gboolean get_can_pause(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(CanPause);
    return TRUE;
}

// Property getter for CanSeek
static gboolean get_can_seek(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *property_name, GVariant **value,
                            GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(CanSeek);
    return TRUE;
}

// Property getter for CanControl
static gboolean get_can_control(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path, const gchar *interface_name,
                               const gchar *property_name, GVariant **value,
                               GError **error, gpointer user_data) {
    *value = g_variant_new_boolean(CanControl);
    return TRUE;
}

static GVariant* get_property_callback(GDBusConnection *connection, const gchar *sender,
                                  const gchar *object_path, const gchar *interface_name,
                                  const gchar *property_name, GError **error, gpointer user_data) {

    GVariant *value = NULL;

    if (g_strcmp0(interface_name, "org.mpris.MediaPlayer2.Player") == 0 || g_strcmp0(interface_name, "org.mpris.MediaPlayer2") == 0) {
        if (g_strcmp0(property_name, "PlaybackStatus") == 0) {
            get_playback_status(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "LoopStatus") == 0) {
            get_loop_status(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Rate") == 0) {
            get_rate(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Shuffle") == 0) {
            get_shuffle(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Metadata") == 0) {
            get_metadata(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Volume") == 0) {
            get_volume(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Position") == 0) {
            get_position(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "MinimumRate") == 0) {
            get_minimum_rate(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "MaximumRate") == 0) {
            get_maximum_rate(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanGoNext") == 0) {
            get_can_go_next(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanGoPrevious") == 0) {
            get_can_go_previous(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanPlay") == 0) {
            get_can_play(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanPause") == 0) {
            get_can_pause(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanSeek") == 0) {
            get_can_seek(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanControl") == 0) {
            get_can_control(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "DesktopIconName") == 0) {
            get_desktop_icon_name(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "DesktopEntry") == 0) {
            get_desktop_entry(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Identity") == 0) {
            get_identity(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        } else {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown property");
        }
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown interface");
    }
    return value;
}

static gboolean set_property_callback(GDBusConnection *connection, const gchar *sender,
                                      const gchar *object_path, const gchar *interface_name,
                                      const gchar *property_name, GVariant *value,
                                      GError **error, gpointer user_data) {

    if (g_strcmp0(interface_name, "org.mpris.MediaPlayer2.Player") == 0) {
        if (g_strcmp0(property_name, "PlaybackStatus") == 0) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Setting PlaybackStatus property not supported");
            return FALSE;
        } else if (g_strcmp0(property_name, "Volume") == 0) {
            double new_volume;
            g_variant_get(value, "d", &new_volume);
            setVolume((int)new_volume);
            return TRUE;
        } else if (g_strcmp0(property_name, "LoopStatus") == 0) {             
             toggleRepeat();
             return TRUE;
        } else if (g_strcmp0(property_name, "Position") == 0) {
            //gint64 new_position;
            //g_variant_get(value, "x", &new_position);
            //set_position(new_position); 
            //return TRUE;
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Setting Position property not supported");
            return FALSE;
        } else {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Setting property not supported");
            return FALSE;
        }
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown interface");
        return FALSE;
    }
}

// MPRIS MediaPlayer2 interface vtable
static const GDBusInterfaceVTable media_player_interface_vtable = {
    .method_call = handle_method_call, // We're using individual method handlers
    .get_property = get_property_callback, // Handle the property getters individually
    .set_property = set_property_callback,
    .padding = {
        handle_raise,
        handle_quit
    }
};

// MPRIS Player interface vtable
static const GDBusInterfaceVTable player_interface_vtable = {
    .method_call = handle_method_call, // We're using individual method handlers
    .get_property = get_property_callback, // Handle the property getters individually
    .set_property = set_property_callback,
    .padding = {
        handle_next,
        handle_previous,
        handle_pause,
        handle_play_pause,
        handle_stop,
        handle_play,
        handle_seek,
        handle_set_position
    }    
};

// Emit a signal for playback status change
void emit_playback_status_changed_signal(GDBusConnection *connection, const gchar *object_path, const gchar *new_status) {
    GVariant *signal_params = g_variant_new("(s)", new_status);  // Create a GVariant containing the new status

    // Emit the PlaybackStatusChanged signal
    g_dbus_connection_emit_signal(connection,
                                  NULL,  // Sender is usually NULL in this context
                                  "/org/mpris/MediaPlayer2/cueMusic",
                                  "org.mpris.MediaPlayer2.Player",
                                  "PlaybackStatusChanged",
                                  g_variant_new("(s)", new_status),
                                  NULL); // GError, if any
}

// End MPRIS Functions

gboolean mainloop_callback(gpointer data) {
    calcElapsedTime();
    handleInput();
    
    // Process GDBus events in the global_main_context
    while (g_main_context_pending(global_main_context)) {
        g_main_context_iteration(global_main_context, FALSE);
    }

    updatePlayer();

    if (!loadedNextSong)
        loadAudioData();

    if (isPlaybackDone())
    {
        prepareNextSong();
    }

    if (doQuit || isPlaybackOfListDone() || loadingFailed)
    {
        g_main_loop_quit(main_loop);
        return FALSE;
    }

    return TRUE;
}

void play(Node *song)
{
    updateLastInputTime();    
    pthread_mutex_init(&(loadingdata.mutex), NULL);
    loadSong(song, &loadingdata);
    int i = 0;
    while (!loadedNextSong)
    {        
        if (i != 0 && i % 1000 == 0 && uiEnabled)
            printf(".");
        i++;
        usleep(10000);        
        fflush(stdout);
    }
    userData.currentFileIndex = 0;
    userData.currentPCMFrame = 0;
    userData.pcmFileA.filename = loadingdata.songdataA->pcmFilePath;

    createAudioDevice(&userData);

    loadedNextSong = false;
    nextSong = NULL;
    refresh = true;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    calculatePlayListDuration(&playlist);

    // mpris stuff:
    
    // initialize global main context
    if (global_main_context == NULL) {
        global_main_context = g_main_context_new();
    }    

    GDBusNodeInfo* introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);

    // mpris: Initialize D-Bus
    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);  

    if (!connection) {
        g_printerr("Failed to connect to D-Bus\n");
        return;
    }        

    GError *error = NULL;
    guint bus_name_id = g_bus_own_name_on_connection(connection,
                                                     "org.mpris.MediaPlayer2.cueMusic",
                                                     G_BUS_NAME_OWNER_FLAGS_NONE,
                                                     on_bus_name_acquired,
                                                     on_bus_name_lost,
                                                     NULL,
                                                     NULL);

    if (!bus_name_id) {
        g_printerr("Failed to own bus name: %s\n", error->message);
        g_error_free(error);
        return;
    }

    // Register D-Bus object with the global_main_context
    guint registration_id = g_dbus_connection_register_object(
        connection,
        "/org/mpris/MediaPlayer2",
        introspection_data->interfaces[0],
        &media_player_interface_vtable,
        NULL,
        NULL,
        &error        
    );    

    if (!registration_id) {
        g_printerr("Failed to register media player object: %s\n", error->message);
        g_error_free(error);
        return;
    }    

    guint player_registration_id = g_dbus_connection_register_object(
        connection,
        "/org/mpris/MediaPlayer2", 
        introspection_data->interfaces[1],
        &player_interface_vtable,
        NULL,
        NULL,
        &error
    );

    if (!player_registration_id) {
        g_printerr("Failed to register media player object: %s\n", error->message);
        g_error_free(error);
        return;
    }        

    main_loop = g_main_loop_new(NULL, FALSE);    

    // Emit PlaybackStatusChanged signal
    GVariant *parameters = g_variant_new("(s)", "Playing"); // Replace with actual value
    g_dbus_connection_emit_signal(connection,
                               NULL,                        // Destination object path (or NULL)
                               "/org/mpris/MediaPlayer2/cueMusic",   // Object path of the media player
                               "org.mpris.MediaPlayer2.Player", // Interface name
                               "PlaybackStatusChanged",     // Signal name
                               parameters,
                               NULL);    
      
    g_timeout_add(100, mainloop_callback, NULL);

    g_main_loop_run(main_loop);    

    g_bus_unown_name(bus_name_id);
    g_dbus_connection_unregister_object(connection, registration_id);
    g_dbus_connection_unregister_object(connection, player_registration_id);
    g_object_unref(connection);
    g_main_loop_unref(main_loop);

    // end mpris stuff

    return;
}

int run()
{
    if (playlist.head == NULL)
    {
        showCursor();
        restoreTerminalMode();
        enableInputBuffering();
        return -1;
    }
    currentSong = playlist.head;
    play(currentSong);
    cleanup();
    restoreTerminalMode();
    enableInputBuffering();
    setConfig();
    saveMainPlaylist(settings.path, playingMainPlaylist);
    freeAudioBuffer();
    deleteCache(tempCache);
    deleteTempDir();
    deletePlaylist(&playlist);
    deletePlaylist(mainPlaylist);
    free(mainPlaylist);
    showCursor();
    printf("\n");

    return 0;
}

void init()
{
    disableInputBuffering();
    srand(time(NULL));
    FILE* nullStream = freopen("/dev/null", "w", stderr);
    (void)nullStream;
    initResize();
    enableScrolling();
    setNonblockingMode();
    srand(time(NULL));
    tempCache = createCache();
    strcpy(loadingdata.filePath, "");
    loadingdata.songdataA = NULL;
    loadingdata.songdataB = NULL;
    loadingdata.loadA = true;
}

void playMainPlaylist()
{
    if (mainPlaylist->count == 0)
    {
        printf("Couldn't find any songs in the main playlist. Add a song by pressing 'a' while it's playing. \n");
        exit(0);
    }
    init();
    playingMainPlaylist = true;
    playlist = deepCopyPlayList(mainPlaylist);
    shufflePlaylist(&playlist);
    run();
}

void playAll(int argc, char **argv)
{
    init();
    makePlaylist(argc, argv);
    if (playlist.count == 0)
    {
        printf("Please make sure the path is set correctly. \n");
        printf("To set it type: cue path \"/path/to/Music\". \n");
    }
    run();
}

void removeArgElement(char* argv[], int index, int* argc) {
    if (index < 0 || index >= *argc) {
        // Invalid index
        return;
    }
    
    // Shift elements after the index
    for (int i = index; i < *argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    
    // Update the argument count
    (*argc)--;
}

void handleOptions(int *argc, char *argv[])
{
  const char* noUiOption = "--noui";
  const char* noCoverOption = "--nocover";
  int idx = -1;
  for (int i = 0; i < *argc; i++) {
    if (strcasestr(argv[i], noUiOption))
    {
      uiEnabled = false;
      idx = i;
    }
  }
  if (idx >= 0)
    removeArgElement(argv, idx, argc);
  idx = -1;    
  for (int i = 0; i < *argc; i++) {
    if (strcasestr(argv[i], noCoverOption))
    {
      coverEnabled = false;
      idx = i;
    }
  }
  if (idx >= 0)
    removeArgElement(argv, idx, argc);    
}

int main(int argc, char *argv[])
{
    getConfig();
    loadMainPlaylist(settings.path);

    handleOptions(&argc, argv);

    if (argc == 1)
    {
        playAll(argc, argv);
    }
    else if (strcmp(argv[1], ".") == 0)
    {
        playMainPlaylist();
    }
    else if ((argc == 2 && ((strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0))))
    {
        showHelp();
    }
    else if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0))
    {
        printAbout();
    }
    else if (argc == 3 && (strcmp(argv[1], "path") == 0))
    {
        strcpy(settings.path, argv[2]);
        setConfig();
    }
    else if (argc >= 2)
    {
        init();
        makePlaylist(argc, argv);
        run();
    }
    return 0;
}