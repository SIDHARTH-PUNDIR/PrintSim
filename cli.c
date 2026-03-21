#include "include/print_server.h"
#include <ctype.h> // For isdigit()

void* cli_handler(void* arg) {
    char command[512]; // Increased buffer size to accommodate more arguments
    char temp_filename_buffer[MAX_FILENAME_LEN];
    char log_message_buffer[256]; // Buffer for log messages
    
    while (1) {
        printf("\n> ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0; // Remove trailing newline

        char* token = strtok(command, " ");
        if (token == NULL) continue;

        if (strcmp(token, "submit") == 0) {
            char* current_token;
            int file_count = 0;
            
            // Loop to parse file, priority, pages triplets
            while ((current_token = strtok(NULL, " ")) != NULL) {
                char* filename_str = current_token;

                // Next token should be priority
                current_token = strtok(NULL, " ");
                if (current_token == NULL || !isdigit(current_token[0])) {
                    printf("Error: Missing priority for file '%s'. Usage: submit <file1> <p1> <pg1> [<file2> <p2> <pg2> ...]\n", filename_str);
                    snprintf(log_message_buffer, sizeof(log_message_buffer), "Error: Missing priority for file '%s'.", filename_str);
                    log_event(log_message_buffer);
                    break; // Exit loop, invalid format
                }
                int priority = atoi(current_token);

                // Next token should be pages
                current_token = strtok(NULL, " ");
                if (current_token == NULL || !isdigit(current_token[0])) {
                    printf("Error: Missing page count for file '%s'. Usage: submit <file1> <p1> <pg1> [<file2> <p2> <pg2> ...]\n", filename_str);
                    snprintf(log_message_buffer, sizeof(log_message_buffer), "Error: Missing page count for file '%s'.", filename_str);
                    log_event(log_message_buffer);
                    break; // Exit loop, invalid format
                }
                int pages = atoi(current_token);

                // Now we have a complete triplet: filename_str, priority, pages
                // Validate file and submit
                FILE* f = fopen(filename_str, "r");
                if (f == NULL) {
                    printf("Error: File '%s' not found. Skipping this job.\n", filename_str);
                    snprintf(log_message_buffer, sizeof(log_message_buffer), "Error: File '%s' not found during submission. Skipping.", filename_str);
                    log_event(log_message_buffer);
                } else {
                    fclose(f);
                    strcpy(temp_filename_buffer, filename_str); // Copy to a non-const buffer for submit_job
                    submit_job(temp_filename_buffer, priority, pages);
                    file_count++;
                }
            }
            
            if (file_count == 0) {
                printf("No valid jobs submitted. Usage: submit <file1> <p1> <pg1> [<file2> <p2> <pg2> ...]\n");
            }

        } else if (strcmp(token, "reassign") == 0) {
            char* jobId_token = strtok(NULL, " ");
            char* newPriority_token = strtok(NULL, " ");

            if (jobId_token && newPriority_token && isdigit(jobId_token[0]) && isdigit(newPriority_token[0])) {
                int jobId = atoi(jobId_token);
                int newPriority = atoi(newPriority_token);

                if (reassign_job_priority(jobId, newPriority)) {
                    printf("Job %d priority reassigned to %d.\n", jobId, newPriority);
                    snprintf(log_message_buffer, sizeof(log_message_buffer), "Job %d priority reassigned to %d.", jobId, newPriority);
                    log_event(log_message_buffer);
                } else {
                    printf("Error: Job %d not found or could not be reassigned.\n", jobId);
                    snprintf(log_message_buffer, sizeof(log_message_buffer), "Error: Failed to reassign priority for Job %d (new priority %d). Job not found.", jobId, newPriority);
                    log_event(log_message_buffer);
                }
            } else {
                printf("Usage: reassign <jobId> <newPriority>\n");
            }
        } else if (strcmp(token, "cancel") == 0) { // NEW COMMAND
            char* jobId_token = strtok(NULL, " ");

            if (jobId_token && isdigit(jobId_token[0])) {
                int jobId = atoi(jobId_token);

                if (cancel_job(jobId)) {
                    printf("Job %d successfully cancelled.\n", jobId);
                    // The log_event for cancellation is handled inside cancel_job
                } else {
                    printf("Error: Job %d not found or could not be cancelled.\n", jobId);
                    snprintf(log_message_buffer, sizeof(log_message_buffer), "Error: Failed to cancel Job %d. Job not found.", jobId);
                    log_event(log_message_buffer);
                }
            } else {
                printf("Usage: cancel <jobId>\n");
            }
        } else if (strcmp(token, "status") == 0) {
            display_queue();
        } else if (strcmp(token, "stats") == 0) {
            display_stats();
        } else if (strcmp(token, "logs") == 0) {
            display_logs();
        } else if (strcmp(token, "exit") == 0) {
            printf("Exiting...\n");
            exit(0);
        } else if (strcmp(token, "help") == 0) {
            printf("Available Commands:\n");
            printf("  submit <file1> <p1> <pg1> [<file2> <p2> <pg2> ...] - Submits one or more print jobs, each with its own priority and page count.\n");
            printf("  reassign <jobId> <newPriority>                     - Changes the priority of an existing job.\n");
            printf("  cancel <jobId>                                     - Cancels and removes a print job from the queue.\n"); // NEW HELP
            printf("  status                                             - Shows the current print queue.\n");
            printf("  stats                                              - Shows printing statistics.\n");
            printf("  logs                                               - Shows the event log.\n");
            printf("  exit                                               - Terminates the program.\n");
        } else {
            printf("Unknown command. Type 'help' for a list of commands.\n");
        }
    }
    return NULL;
}