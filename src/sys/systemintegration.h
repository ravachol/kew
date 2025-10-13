/**
 * @file systemintegration.h
 * @brief System-level integrations and OS interop.
 *
 * Provides hooks for platform-specific features such as
 * power management, clipboard access, or system event handling.
 */

#ifndef SYSTEMINTEGRATION_H
#define SYSTEMINTEGRATION_H

#include "common/appstate.h"

#include "glib.h"
#include "gio/gio.h"

void setGMainContext(GMainContext *val);
void *getGMainContext(void);
void setGDBusConnection(GDBusConnection *val);
void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue);
void updatePlaybackPosition(double elapsedSeconds);
void emitSeekedSignal(double newPositionSeconds);
void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue);
void quit(void);
void notifyMPRISSwitch(SongData *currentSongData);
void notifySongSwitch(SongData *currentSongData);
void processDBusEvents(void);
void resize(UIState *uis);
void exitIfAlreadyRunning();
GDBusConnection *getGDBusConnection(void);

#endif
