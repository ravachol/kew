#ifndef SMTC_H
#define SMTC_H

#ifdef __cplusplus
extern "C" {
        #endif

        void smtc_init(void);
        void smtc_shutdown(void);

        void smtc_set_playback_playing(void);
        void smtc_set_playback_paused(void);
        void smtc_set_playback_stopped(void);
        void smtc_set_playback_position(double position);

        void smtc_update_metadata(const char *title,
                                  const char *artist,
                                  const char *album,
                                  const char *cover_art_path,
                                  double duration);

        #ifdef __cplusplus
}
#endif

#endif
