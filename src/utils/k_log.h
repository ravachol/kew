
#ifndef K_LOG_H
#define K_LOG_H

/**
 * @brief Initializes logging to error.log
 *
 */
void k_log_init(void);

/**
 * @brief Shuts down logging to error.log
 *
 */
void k_log_shutdown(void);

/**
 * @brief Logs to error.log
 *
 */
void k_log(const char *fmt, ...);

#endif
