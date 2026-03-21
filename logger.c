#include "include/print_server.h"

void log_event(const char* message) {
    FILE* logfile = fopen(LOG_FILE, "a");
    if (logfile == NULL) {
        perror("Could not open log file");
        return;
    }
    time_t now = time(NULL);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(logfile, "[%s] %s\n", time_str, message);
    fclose(logfile);
}