#ifndef SYSTEMINTEGRATION_H
#define SYSTEMINTEGRATION_H

#include "glib.h"
#include "gio/gio.h"

GDBusConnection *getGDBusConnection(void);

void setGDBusConnection(GDBusConnection *val);

void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue);

void updatePlaybackPosition(double elapsedSeconds);

void emitSeekedSignal(double newPositionSeconds);

void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue);

void quit(void);

#endif
