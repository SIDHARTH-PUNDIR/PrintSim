#include "include/print_server.h"

void insert_job_into_queue(PrintJob* newJob) {
    if (jobQueue == NULL || newJob->priority < jobQueue->priority) {
        newJob->next = jobQueue;
        jobQueue = newJob;
    } else {
        PrintJob* current = jobQueue;
        while (current->next != NULL && current->next->priority <= newJob->priority) {
            current = current->next;
        }
        newJob->next = current->next;
        current->next = newJob;
    }
}

int reassign_job_priority(int jobId, int newPriority) {
    pthread_mutex_lock(&queueMutex);

    PrintJob* current = jobQueue;
    PrintJob* prev = NULL;

    while (current != NULL && current->jobId != jobId) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        pthread_mutex_unlock(&queueMutex);
        return 0; // Job not found
    }

    // Remove the job from its current position
    if (prev == NULL) {
        jobQueue = current->next; // It was the head
    } else {
        prev->next = current->next;
    }

    current->priority = newPriority;
    insert_job_into_queue(current); // Re-insert it sorted by new priority

    pthread_mutex_unlock(&queueMutex);
    return 1; // Job priority successfully reassigned
}

// Corrected: Function to cancel a job - now handles actively printing jobs
int cancel_job(int jobId) {
    char log_msg[256]; // Declare log_msg buffer locally
    int result = 0; // 0 for not found/failed, 1 for success

    // 1. Check if the job is currently printing
    pthread_mutex_lock(&currentlyPrintingJobMutex); // Corrected mutex name
    if (currentlyPrintingJob != NULL && currentlyPrintingJob->jobId == jobId) {
        printf("Attempting to cancel actively printing Job %d.\n", jobId);
        snprintf(log_msg, sizeof(log_msg), "Attempting to cancel actively printing Job %d (%s).", currentlyPrintingJob->jobId, currentlyPrintingJob->filename);
        log_event(log_msg);
        
        cancelPrintingFlag = 1; // Set the flag to signal the print_manager
        result = 1; // Indicate that we've found and signaled the job
    }
    pthread_mutex_unlock(&currentlyPrintingJobMutex); // Corrected mutex name

    if (result == 1) {
        // If it was the actively printing job, the print_manager will clean it up.
        return 1;
    }

    // 2. If not printing, check if it's in the waiting queue
    pthread_mutex_lock(&queueMutex); // Lock the job queue
    PrintJob* current = jobQueue;
    PrintJob* prev = NULL;

    while (current != NULL && current->jobId != jobId) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        // Job not found in queue either
        pthread_mutex_unlock(&queueMutex);
        return 0;
    }

    // Remove the job from the queue
    if (prev == NULL) {
        jobQueue = current->next; // Job was the head of the queue
    } else {
        prev->next = current->next;
    }
    
    // Log the event
    snprintf(log_msg, sizeof(log_msg), "Job %d (%s) cancelled from queue.", current->jobId, current->filename);
    log_event(log_msg);

    // Free the job's memory and associated resources
    cleanup_temp_pdf_files(current); // Clean up any generated chunk files
    free(current);
    stats.totalJobsSubmitted--; 

    pthread_mutex_unlock(&queueMutex); // Unlock the job queue
    return 1; // Job successfully cancelled from queue
}

void submit_job(const char* filename, int priority, int totalPages) {
    char log_msg[256]; // Declare log_msg buffer locally
    char backup_path[256];
    if (backup_file(filename, backup_path) != 0) {
        printf("Error: Could not backup file %s. Job not submitted.\n", filename);
        sprintf(log_msg, "Error: Could not backup file %s. Job not submitted.", filename);
        log_event(log_msg);
        return;
    }
    PrintJob* newJob = (PrintJob*)malloc(sizeof(PrintJob));
    if (!newJob) {
        perror("Failed to allocate memory for new job");
        log_event("Error: Failed to allocate memory for new job.");
        return;
    }
    newJob->jobId = nextJobId++;
    newJob->priority = priority;
    newJob->totalPages = totalPages;
    newJob->pagesPrinted = 0;
    newJob->totalChunks = 0;
    newJob->preemptionAttempts = 0;
    strcpy(newJob->filename, backup_path);
    newJob->next = NULL;
    printf("Job submitted! ID: %d, File: %s, Pages: %d (estimated), Priority: %d\n", newJob->jobId, filename, newJob->totalPages, priority);
    sprintf(log_msg, "Job %d submitted: %s (Priority %d, Pages %d)", newJob->jobId, filename, newJob->totalPages, priority);
    log_event(log_msg);
    pthread_mutex_lock(&queueMutex);
    insert_job_into_queue(newJob);
    stats.totalJobsSubmitted++;
    pthread_mutex_unlock(&queueMutex);
}