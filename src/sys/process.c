#include "sys/process.h"

#include "utils/file.h"

#include "errno.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Returns 1 if the process is running

int isProcessRunning(pid_t pid)
{
        if (pid <= 0)
        {
                return 0; // Invalid PID
        }

        // Send signal 0 to check if the process exists
        if (kill(pid, 0) == 0)
        {
                return 1; // Process exists
        }

        // Check errno for detailed status
        if (errno == ESRCH)
        {
                return 0; // No such process
        }
        else if (errno == EPERM)
        {
                return 1; // Process exists but we don't have permission
        }

        return 0; // Other errors
}

int isKewProcess(pid_t pid)
{
        char comm_path[64];
        char process_name[256];
        FILE *file;

        // First check /proc/[pid]/comm for the process name
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
        file = fopen(comm_path, "r");
        if (file != NULL)
        {
                if (fgets(process_name, sizeof(process_name), file))
                {
                        fclose(file);
                        // Remove trailing newline
                        process_name[strcspn(process_name, "\n")] = 0;

                        // Check if it's kew (process name might be truncated to
                        // 15 chars)
                        if (strstr(process_name, "kew") != NULL)
                        {
                                return 1; // It's likely kew
                        }
                }
                else
                {
                        fclose(file);
                }
        }

        return 0; // Not kew or couldn't determine
}

// Ensures only a single instance of kew can run at a time for the current user.
void exitIfAlreadyRunning()
{
        char pidfile_path[512]; // Increased size for longer paths
        const char *temp_dir = getTempDir();

        snprintf(pidfile_path, sizeof(pidfile_path), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile;
        pid_t pid;

        pidfile = fopen(pidfile_path, "r");
        if (pidfile != NULL)
        {
                if (fscanf(pidfile, "%d", &pid) == 1)
                {
                        fclose(pidfile);

#ifdef __ANDROID__
                        if (isProcessRunning(pid) && isKewProcess(pid))
#else
                        if (isProcessRunning(pid))
#endif
                        {
                                fprintf(
                                    stderr,
                                    "An instance of kew is already running. "
                                    "Pid: %d. Type 'kill %d' to remove it.\n",
                                    pid, pid);
                                exit(1);
                        }
                        else
                        {
                                unlink(pidfile_path);
                        }
                }
                else
                {
                        fclose(pidfile);
                        unlink(pidfile_path);
                }
        }

        // Create a new PID file
        pidfile = fopen(pidfile_path, "w");
        if (pidfile == NULL)
        {
                perror("Unable to create PID file");
                exit(1);
        }
        fprintf(pidfile, "%d\n", getpid());
        fclose(pidfile);
}
