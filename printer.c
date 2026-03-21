#include "include/print_server.h"


void* print_manager(void* arg) {
    PrintJob* currentJob = NULL;
    char log_msg[256]; // Declare log_msg buffer locally

    while (1) {
        // Lock the queue to get a job
        pthread_mutex_lock(&queueMutex);
        if (jobQueue == NULL) {
            pthread_mutex_unlock(&queueMutex);
            sleep(1);
            continue;
        }


        // At the start of the loop, check if a waiting job should preempt 'currentlyPrintingJob'
        // This is necessary if 'currentlyPrintingJob' was set by a previous iteration
        // and a new job was added *before* the inner printing loop started for a fresh job.
        PrintJob* potentialPreemptor = NULL;
        if (currentJob != NULL) { // If there's an active job from a previous loop iteration (e.g. preempted)
            PrintJob* temp = jobQueue;
            while(temp != NULL) {
                if (should_preempt(currentJob, temp)) {
                    potentialPreemptor = temp;
                    break;
                }
                temp = temp->next;
            }
        }

        // If a preemptor is found at the beginning of the loop (or currentJob is NULL),
        // we need to prioritize dequeuing the highest priority job or the preemptor.
        // For simplicity, let's handle the direct dequeue first for the *initial* job.
        // The more complex preemption check happens within the printing loop.

        // Dequeue the first job from the queue to become currentJob
        currentJob = jobQueue;
        jobQueue = jobQueue->next;
        currentJob->next = NULL; // Detach from the queue

        pthread_mutex_unlock(&queueMutex);

        // --- Critical Section for currentlyPrintingJob ---
        pthread_mutex_lock(&currentlyPrintingJobMutex); // Corrected mutex name
        currentlyPrintingJob = currentJob; // Set the global pointer
        cancelPrintingFlag = 0;           // Reset cancellation flag for this new job
        pthread_mutex_unlock(&currentlyPrintingJobMutex); // Corrected mutex name
        // --- End Critical Section ---

        // Process files if it's the first time printing this job
        if (currentJob->pagesPrinted == 0) {
            if (process_job_files(currentJob) == 0) {
                printf("\nError processing files for Job %d. Aborting job.\n", currentJob->jobId);
                sprintf(log_msg, "Error processing files for Job %d. Aborting job.", currentJob->jobId);
                log_event(log_msg);
                cleanup_temp_pdf_files(currentJob); // Clean up any generated temp files
                free(currentJob);
                currentJob = NULL;
                
                // --- Critical Section for currentlyPrintingJob ---
                pthread_mutex_lock(&currentlyPrintingJobMutex); // Corrected mutex name
                currentlyPrintingJob = NULL; // Clear the global pointer
                pthread_mutex_unlock(&currentlyPrintingJobMutex); // Corrected mutex name
                // --- End Critical Section ---
                continue;
            }
        }

        // Start the printing simulation
        sprintf(log_msg, "Job %d (%s) started printing.", currentJob->jobId, currentJob->filename);
        log_event(log_msg);
        printf("\n--- Printing Job %d: %s (%d pages remaining) ---\n", currentJob->jobId, currentJob->filename, currentJob->totalPages - currentJob->pagesPrinted);

        int wasPreempted = 0;
        int chunk_to_print = (currentJob->pagesPrinted / 4) + 1;
        const int PAGES_PER_CHUNK = 4;
        const char* extension = strrchr(currentJob->filename, '.');

        // Loop through and simulate printing each page
        for (int i = currentJob->pagesPrinted; i < currentJob->totalPages; i++) {
            sleep(2); // Simulate printing time

            // --- Check for cancellation flag ---
            pthread_mutex_lock(&currentlyPrintingJobMutex); // Corrected mutex name
            if (cancelPrintingFlag == 1) {
                printf("\n!!! Job %d CANCELLED while printing !!!\n", currentJob->jobId);
                sprintf(log_msg, "Job %d (%s) was cancelled while printing.", currentJob->jobId, currentJob->filename);
                log_event(log_msg);
                
                // Perform cleanup for the canceled job
                cleanup_temp_pdf_files(currentJob);
                free(currentJob);
                currentJob = NULL; // Mark as null to prevent further processing
                stats.totalJobsSubmitted--; // Decrement submitted count
                
                currentlyPrintingJob = NULL; // Clear the global pointer
                cancelPrintingFlag = 0;      // Reset the flag
                pthread_mutex_unlock(&currentlyPrintingJobMutex); // Corrected mutex name
                break; // Exit the printing loop for this job
            }
            pthread_mutex_unlock(&currentlyPrintingJobMutex); // Corrected mutex name
            // --- End cancellation check ---

            currentJob->pagesPrinted++;
            printf("  Job %d: Printed page %d/%d\n", currentJob->jobId, currentJob->pagesPrinted, currentJob->totalPages);

            // Print a 4-page chunk if ready
            if ((currentJob->pagesPrinted % PAGES_PER_CHUNK == 0) || (currentJob->pagesPrinted == currentJob->totalPages)) {
                char chunk_filename[256];
                char job_id_str[10];
                sprintf(job_id_str, "%d", currentJob->jobId);
                sprintf(chunk_filename, "%s/%s_chunk_%d%s", BACKUP_DIR, job_id_str, chunk_to_print, extension ? extension : ".txt");
                printf("  --> Job %d: Sending pages %d-%d to printer...\n", currentJob->jobId, (chunk_to_print - 1) * PAGES_PER_CHUNK + 1, currentJob->pagesPrinted);
                print_file_os(chunk_filename); 
                chunk_to_print++;
            }

            // Check for preemption
            pthread_mutex_lock(&queueMutex);
            PrintJob* preemptingJob = NULL;
            PrintJob* prev = NULL;
            PrintJob* temp = jobQueue; // Start from the beginning of the waiting queue

            // Iterate through the waiting queue to find a job that should preempt
            while (temp != NULL) {
                if (should_preempt(currentJob, temp)) {
                    preemptingJob = temp;
                    // If found, remove it from the waiting queue
                    if (prev == NULL) { // Preempting job is the head of the waiting queue
                        jobQueue = temp->next;
                    } else {
                        prev->next = temp->next;
                    }
                    preemptingJob->next = NULL; // Detach from the waiting queue
                    break; // Found our preemptor, exit loop
                }
                prev = temp;
                temp = temp->next;
            }

            if (preemptingJob != NULL) {
                sprintf(log_msg, "Job %d preempted by Job %d.", currentJob->jobId, preemptingJob->jobId);
                log_event(log_msg);
                printf("\n!!! Job %d PREEMPTED by Job %d !!!\n", currentJob->jobId, preemptingJob->jobId);
                currentJob->preemptionAttempts++;
                // Put the current (interrupted) job back into the queue.
                insert_job_into_queue(currentJob); // Re-insert it into the queue
                
                // Place the preempting job at the VERY FRONT of the queue to be picked next.
                preemptingJob->next = jobQueue;
                jobQueue = preemptingJob;

                stats.preemptionCount++;
                wasPreempted = 1;
                
                pthread_mutex_unlock(&queueMutex); // Unlock queue before breaking loop
                break; // Exit the for-loop to force print_manager to re-evaluate the queue
            }
            pthread_mutex_unlock(&queueMutex); // Unlock queue if no preemption
            // --- END MODIFIED PREEMPTION CHECK ---
        } // End of for-loop (page printing)

        // Only process if currentJob is not NULL (i.e., not canceled within the loop)
        if (currentJob != NULL) { 
            // If the job finished without being preempted or canceled, clean it up.
            if (!wasPreempted) {
                printf("--- Finished Job %d ---\n", currentJob->jobId);
                sprintf(log_msg, "Job %d finished printing.", currentJob->jobId);
                log_event(log_msg);
                
                // Cleanup temporary files after successful printing
                cleanup_temp_pdf_files(currentJob);
                
                pthread_mutex_lock(&queueMutex);
                stats.totalJobsPrinted++;
                pthread_mutex_unlock(&queueMutex);
                free(currentJob);
            }
            // If it was preempted, `currentJob` was already put back into `jobQueue`
            // and `currentlyPrintingJob` will be reset at the start of the next loop iteration.

            // --- Critical Section for currentlyPrintingJob ---
            pthread_mutex_lock(&currentlyPrintingJobMutex); // Corrected mutex name
            if (currentlyPrintingJob == currentJob) { // Only clear if it's still this job
                currentlyPrintingJob = NULL; // Clear the global pointer
            }
            pthread_mutex_unlock(&currentlyPrintingJobMutex); // Corrected mutex name
            // --- End Critical Section ---
        }
        
        currentJob = NULL; // Ensure currentJob is NULL for next iteration
    }
    return NULL;
}