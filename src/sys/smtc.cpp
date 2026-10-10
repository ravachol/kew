#include "smtc.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <windows.media.h>

#include <systemmediatransportcontrolsinterop.h>
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/base.h>

#include "common/events.h"
#include "update/messages.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

static SystemMediaTransportControls g_smtc{nullptr};
static event_token g_button_token{};

static double g_duration = 0.0;
static double g_position = 0.0;

static bool g_initialized = false;

static bool g_apartment_initialied = false;

static HWND g_hwnd = nullptr;

static LRESULT CALLBACK smtc_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
        return DefWindowProc(hwnd, msg, wp, lp);
}

static HWND create_hidden_window(void)
{
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = smtc_wndproc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"kew_smtc_hidden_window";

        // OK if already registered from a previous init/shutdown cycle.
        RegisterClassExW(&wc);

        return CreateWindowExW(
            0,
            wc.lpszClassName,
            L"",
            WS_POPUP,
            0, 0, 0, 0,
            HWND_MESSAGE, // message-only window: no visible top-level window
            nullptr,
            wc.hInstance,
            nullptr);
}

/*
 * Convert seconds to a Windows TimeSpan.
 *
 * Windows TimeSpan uses 100-nanosecond units.
 */
static TimeSpan seconds_to_timespan(double seconds)
{
        if (seconds < 0.0)
                seconds = 0.0;

        return TimeSpan{
            static_cast<int64_t>(seconds * 10000000.0)};
}

/*
 * Update the SMTC timeline.
 *
 * StartTime    = beginning of track
 * Position     = current playback position
 * MinSeekTime  = earliest seek position
 * MaxSeekTime  = latest seek position
 * EndTime      = end of track
 */
static void update_timeline(void)
{
        if (!g_smtc)
                return;

        double position = g_position;

        if (position < 0.0)
                position = 0.0;

        if (g_duration > 0.0 && position > g_duration)
                position = g_duration;

        auto timeline = SystemMediaTransportControlsTimelineProperties{};

        timeline.StartTime(seconds_to_timespan(0.0));
        timeline.MinSeekTime(seconds_to_timespan(0.0));
        timeline.Position(seconds_to_timespan(position));
        timeline.MaxSeekTime(seconds_to_timespan(g_duration));
        timeline.EndTime(seconds_to_timespan(g_duration));

        g_smtc.UpdateTimelineProperties(timeline);
}

extern "C" void smtc_init(void)
{
        if (g_initialized)
                return;

        try {
                init_apartment();
                g_apartment_initialied = true;
        } catch (const hresult_error &e) {
                return;
        }

        g_hwnd = create_hidden_window();

        if (!g_hwnd) {
                uninit_apartment();
                g_apartment_initialied = false;
                return;
        }

        try {
                auto factory = get_activation_factory<SystemMediaTransportControls>();
                auto interop = factory.as<ISystemMediaTransportControlsInterop>();

                SystemMediaTransportControls smtc{nullptr};
                check_hresult(
                    interop->GetForWindow(g_hwnd, guid_of<SystemMediaTransportControls>(), put_abi(smtc)));

                g_smtc = smtc;

                g_smtc.IsEnabled(true);
                g_smtc.IsPlayEnabled(true);
                g_smtc.IsPauseEnabled(true);
                g_smtc.IsNextEnabled(true);
                g_smtc.IsPreviousEnabled(true);

                g_smtc.DisplayUpdater().Type(MediaPlaybackType::Music);

                // Receive media-button presses from Windows.
                g_button_token = g_smtc.ButtonPressed(
                    [](SystemMediaTransportControls const &,
                       SystemMediaTransportControlsButtonPressedEventArgs const &args) {
                            Msg msg{};

                            switch (args.Button()) {
                            case SystemMediaTransportControlsButton::Play:
                                    msg.type = MSG_PLAY;
                                    dispatch_msg(msg);
                                    break;
                            case SystemMediaTransportControlsButton::Pause:
                                    msg.type = MSG_PAUSE;
                                    dispatch_msg(msg);
                                    break;
                            case SystemMediaTransportControlsButton::Next:
                                    msg.type = MSG_NEXT;
                                    dispatch_msg(msg);
                                    break;
                            case SystemMediaTransportControlsButton::Previous:
                                    msg.type = MSG_PREV;
                                    dispatch_msg(msg);
                                    break;
                            default:
                                    break;
                            }
                    });

                g_position = 0.0;
                g_duration = 0.0;
                g_initialized = true;
        } catch (const hresult_error &e) {
                g_smtc = nullptr;

                DestroyWindow(g_hwnd);
                g_hwnd = nullptr;

                uninit_apartment();
                g_apartment_initialied = false;
                return;
        }
}

extern "C" void smtc_update_metadata(const char *title,
                                     const char *artist,
                                     const char *album,
                                     const char *cover_art_path,
                                     double duration)
{
        if (!g_smtc || !g_initialized)
                return;

        if (duration < 0.0)
                duration = 0.0;

        g_duration = duration;

        try {
                auto updater = g_smtc.DisplayUpdater();
                updater.Type(MediaPlaybackType::Music);

                auto properties = updater.MusicProperties();

                properties.Title(
                    title ? winrt::to_hstring(title) : L"");

                properties.Artist(
                    artist ? winrt::to_hstring(artist) : L"");

                properties.AlbumTitle(
                    album ? winrt::to_hstring(album) : L"");

                // Artwork is optional. Failure to load it should
                // not prevent metadata from being published.
                try {
                        if (cover_art_path && cover_art_path[0] != '\0') {
                                auto path = winrt::to_hstring(cover_art_path);
                                auto file =
                                    winrt::Windows::Storage::StorageFile::
                                        GetFileFromPathAsync(path)
                                            .get();

                                auto thumbnail =
                                    winrt::Windows::Storage::Streams::
                                        RandomAccessStreamReference::
                                            CreateFromFile(file);

                                updater.Thumbnail(thumbnail);
                        } else {
                                updater.Thumbnail(nullptr);
                        }
                } catch (const winrt::hresult_error &) {
                        try {
                                updater.Thumbnail(nullptr);
                        } catch (const winrt::hresult_error &) {
                                // Artwork is optional.
                        }
                }

                updater.Update();
                update_timeline();
        } catch (const winrt::hresult_error &) {
                // SMTC is optional; don't let its failure
                // disrupt normal playback.
        }
}

extern "C" void smtc_set_playback_playing(void)
{
        if (!g_smtc)
                return;

        if (!g_initialized)
                return;

        g_smtc.PlaybackStatus(
            MediaPlaybackStatus::Playing);

        update_timeline();
}

extern "C" void smtc_set_playback_paused(void)
{
        if (!g_smtc)
                return;

        if (!g_initialized)
                return;

        g_smtc.PlaybackStatus(
            MediaPlaybackStatus::Paused);

        update_timeline();
}

extern "C" void smtc_set_playback_stopped(void)
{
        if (!g_smtc)
                return;

        if (!g_initialized)
                return;

        g_position = 0.0;

        g_smtc.PlaybackStatus(
            MediaPlaybackStatus::Stopped);

        update_timeline();
}

extern "C" void smtc_set_playback_position(double position)
{
        if (!g_smtc)
                return;

        if (!g_initialized)
                return;

        if (position < 0.0)
                position = 0.0;

        if (g_duration > 0.0 && position > g_duration)
                position = g_duration;

        g_position = position;

        update_timeline();
}

extern "C" void smtc_shutdown(void)
{
        if (g_smtc) {
                if (g_button_token.value != 0) {
                        g_smtc.ButtonPressed(g_button_token);
                        g_button_token = {};
                }
                g_smtc.IsEnabled(false);
                g_smtc = nullptr;
        }

        if (g_hwnd) {
                DestroyWindow(g_hwnd);
                g_hwnd = nullptr;
        }

        g_position = 0.0;
        g_duration = 0.0;
        g_initialized = false;

        if (g_apartment_initialied) {
                uninit_apartment();
                g_apartment_initialied = false;
        }
}

#else

extern "C" void smtc_init(void)
{
}

extern "C" void smtc_shutdown(void)
{
}

extern "C" void smtc_set_playback_playing(void)
{
}

extern "C" void smtc_set_playback_paused(void)
{
}

extern "C" void smtc_set_playback_stopped(void)
{
}

extern "C" void smtc_set_playback_position(double position)
{
        (void)position;
}

extern "C" void smtc_update_metadata(const char *title,
                                     const char *artist,
                                     const char *album,
                                     const char *cover_art_path,
                                     double duration)
{
        (void)title;
        (void)artist;
        (void)album;
        (void)cover_art_path;
        (void)duration;
}

#endif
