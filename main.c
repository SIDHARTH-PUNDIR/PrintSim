#include "include/print_server.h"

PrintJob* jobQueue = NULL;
pthread_mutex_t queueMutex;
Statistics stats = {0, 0, 0};
int nextJobId = 1;

// NEW: Global variables for currently printing job and cancellation flag
PrintJob* currentlyPrintingJob = NULL;
int cancelPrintingFlag = 0;
pthread_mutex_t currentlyPrintingJobMutex; // Initialize this mutex

int main() {
    pthread_t producer_thread, consumer_thread;
    printf("Print Server Simulator starting...\n");
    if (pthread_mutex_init(&queueMutex, NULL) != 0) {
        perror("Mutex init failed");
        return 1;
    }
    if (pthread_mutex_init(&currentlyPrintingJobMutex, NULL) != 0) {
        perror("Currently Printing Job Mutex init failed");
        pthread_mutex_destroy(&queueMutex); // Clean up already initialized mutex
        return 1;
    }
    create_backup_directory();
    log_event("--- Print Server Started ---");
    if (pthread_create(&producer_thread, NULL, cli_handler, NULL) != 0) {
        perror("Failed to create producer thread");
        pthread_mutex_destroy(&currentlyPrintingJobMutex);
        pthread_mutex_destroy(&queueMutex);
        return 1;
    }
    if (pthread_create(&consumer_thread, NULL, print_manager, NULL) != 0) {
        perror("Failed to create consumer thread");
        pthread_mutex_destroy(&currentlyPrintingJobMutex);
        pthread_mutex_destroy(&queueMutex);
        pthread_join(producer_thread, NULL); // Join producer if consumer fails
        return 1;
    }
    printf("Threads created successfully. Type 'help' for commands.\n");
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);
    pthread_mutex_destroy(&queueMutex);
    pthread_mutex_destroy(&currentlyPrintingJobMutex);
    log_event("--- Print Server Shutting Down ---");
    printf("Print Server has shut down.\n");
    return 0;
}