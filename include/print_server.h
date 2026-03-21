#ifndef PRINT_SERVER_H
// Libraries
#define PRINT_SERVER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// macros
#define MAX_FILENAME_LEN 100
#define LOG_FILE "job_log.txt"
#define BACKUP_DIR "PrintServerJobs"
#define GS_EXE_PATH "D:/gs10.06.0/bin/gswin64c.exe" 

// Data Structures
typedef struct PrintJob {
    int jobId;
    int priority;
    int totalPages;
    int pagesPrinted;
    char filename[MAX_FILENAME_LEN];
    int totalChunks;
    int preemptionAttempts;
    struct PrintJob* next;
} PrintJob;

typedef struct Statistics {
    int totalJobsSubmitted;
    int totalJobsPrinted;
    int preemptionCount;
} Statistics;

extern PrintJob* jobQueue;
extern pthread_mutex_t queueMutex;
extern PrintJob* currentlyPrintingJob; // To hold the job actively being printed
extern pthread_mutex_t currentlyPrintingJobMutex; // Protect access to currentlyPrintingJob
extern int cancelPrintingFlag;// Flag to signal cancellation of the currently printing job
extern Statistics stats;
extern int nextJobId;

// Function Prototypes
void* cli_handler(void* arg);
void submit_job(const char* filename, int priority, int totalPages);
void insert_job_into_queue(PrintJob* newJob);
int cancel_job(int jobId);
int reassign_job_priority(int jobId, int newPriority);
void* print_manager(void* arg);
void log_event(const char* message);
void create_backup_directory();
int backup_file(const char* sourcePath, char* newPath);
void print_file_os(const char* filepath);
void display_queue();
void display_stats();
void display_logs();
int should_preempt(PrintJob* currentJob, PrintJob* newJob);
int split_text_file(const char* sourcePath, const char* job_id_str, int* total_pages);
void cleanup_temp_pdf_files(PrintJob* job);
int process_job_files(PrintJob* job);

#endif // PRINT_SERVER_H
