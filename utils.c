#include "include/print_server.h"
#ifdef _WIN32
#include <windows.h>
#include <process.h> // For _popen and _pclose on some compilers
#endif
#include <errno.h> // For error checking with remove()
#define MAX_PREEMPTIONS 2 // Maximum preemption attempts before forcing completion

void quote_path_for_windows(char* dest, const char* src, size_t dest_len) {
    char temp_path[dest_len];
    strncpy(temp_path, src, dest_len - 1);
    temp_path[dest_len - 1] = '\0';

    // Convert all backslashes to forward slashes for cross-platform compatibility
    // and for tools like Ghostscript that generally prefer them.
    for (char *p = temp_path; *p; ++p) {
        if (*p == '\\') {
            *p = '/';
        }
    }

    // Only quote if absolutely necessary for the shell
    if (strchr(temp_path, ' ') || strchr(temp_path, '&') || strchr(temp_path, '(') || strchr(temp_path, ')') || strchr(temp_path, '\'')) {
        snprintf(dest, dest_len, "\"%s\"", temp_path);
    } else {
        strncpy(dest, temp_path, dest_len - 1);
        dest[dest_len - 1] = '\0';
    }
}

int should_preempt(PrintJob* currentJob, PrintJob* newJob) {
    if (currentJob == NULL || newJob == NULL) return 0;

    // A job cannot preempt itself
    if (currentJob->jobId == newJob->jobId) return 0;

    if (currentJob->preemptionAttempts >= MAX_PREEMPTIONS) {
        // This job cannot be preempted any further to prevent starvation.
        return 0;
    }

    int remainingPages = currentJob->totalPages - currentJob->pagesPrinted;

    // Ensure there are pages left to print for current job, and new job has pages
    if (newJob->totalPages > 0 && remainingPages > 0) {
        if (newJob->totalPages < (remainingPages / 5)) {
            return 1;
        }
    }
    return 0;
}

void create_backup_directory() {
    #ifdef _WIN32
        CreateDirectory(BACKUP_DIR, NULL);
    #else
        system("mkdir -p " BACKUP_DIR);
    #endif
}

int backup_file(const char* sourcePath, char* newPath) {
    FILE *source, *destination;
    char buffer[1024];
    size_t bytes;
    const char* filename = strrchr(sourcePath, '/');
    if (filename == NULL) filename = strrchr(sourcePath, '\\');
    if (filename == NULL) filename = sourcePath;
    else filename++;
    sprintf(newPath, "%s/%s", BACKUP_DIR, filename);
    source = fopen(sourcePath, "rb");
    if (source == NULL) {
        printf("Error backing up file %s\n", sourcePath); // Keep this for actual errors
        return 1;
    }
    destination = fopen(newPath, "wb");
    if (destination == NULL) { fclose(source); return 1; } // Keep this for actual errors
    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes, destination);
    }
    fclose(source);
    fclose(destination);
    return 0;
}


// Detect default printer using PowerShell
void get_default_printer(char* buffer, size_t size) {
    FILE* fp = popen(
        "powershell -Command \"(Get-CimInstance Win32_Printer | Where-Object {$_.Default}).Name\"",
        "r"
    );
    if (!fp) {
        snprintf(buffer, size, ""); // Empty = no printer
        return;
    }

    // Read first line of output
    if (fgets(buffer, (int)size, fp) != NULL) {
        // Remove newline at the end
        buffer[strcspn(buffer, "\r\n")] = 0;
    } else {
        snprintf(buffer, size, "");
    }

    pclose(fp);
}


void print_file_os(const char* filepath) {
    char command[1024];
    const char* extension = strrchr(filepath, '.');
    char quoted_filepath[MAX_FILENAME_LEN + 3];
    char gs_filepath[MAX_FILENAME_LEN + 1];
    strncpy(gs_filepath, filepath, MAX_FILENAME_LEN);
    gs_filepath[MAX_FILENAME_LEN] = '\0';
    for (char *p = gs_filepath; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
    quote_path_for_windows(quoted_filepath, gs_filepath, sizeof(quoted_filepath));

    #ifdef _WIN32
        if (extension != NULL && strcmp(extension, ".pdf") == 0) {
            char printerName[256] = {0};
            get_default_printer(printerName, sizeof(printerName));

            // Added redirection of stdout to NUL (2>NUL for stderr, not needed here usually)
            // Also removed -dNOPROMPT and -dPrinted=false as they didn't stop the dialog,
            // focusing on the output redirection and potential dialog description.
            sprintf(command,
                "%s -dBATCH -dNOPAUSE -sDEVICE=mswinpr2 -sOutputFile=\"%%printer%%%s\" %s > NUL", // > NUL added here
                GS_EXE_PATH,
                printerName,
                quoted_filepath
                );
            // printf("Attempting to print PDF using Ghostscript directly (device: mswinpr2, headless): %s\n", command); // REMOVED debug print
        } else {
            // For other file types (like .txt), stick to PowerShell's Out-Printer
            // If you want to silence PowerShell's output, you might need to add `> $null` to it.
            sprintf(command, "powershell -Command \"Get-Content %s | Out-Printer\"", quoted_filepath);
            // printf("Attempting to print TEXT using PowerShell: %s\n", command); // REMOVED debug print
        }
    #else
        // Linux/macOS path
        // For Linux, you'd use `> /dev/null`
        sprintf(command, "lp %s > /dev/null", quoted_filepath);
        // printf("Attempting to print using lp: %s\n", command); // REMOVED debug print
    #endif
    
    int ret = system(command);
    if (ret != 0) {
        fprintf(stderr, "Error sending file to printer via OS command. Command returned %d.\n", ret); // Keep this for errors
        fprintf(stderr, "Command: %s\n", command); // Keep this for errors
    }
}

void display_queue() {
    pthread_mutex_lock(&queueMutex);
    printf("\n--- CURRENT PRINT QUEUE ---\n");
    if (jobQueue == NULL) {
        printf("Queue is empty.\n");
    } else {
        PrintJob* current = jobQueue;
        int i = 1;
        while (current != NULL) {
            printf("%d. JobID: %d, File: %s, Pages: %d, Priority: %d\n",
                    i++, current->jobId, current->filename, current->totalPages, current->priority);
            current = current->next;
        }
    }
    printf("---------------------------\n");
    pthread_mutex_unlock(&queueMutex);
}

void display_stats() {
    pthread_mutex_lock(&queueMutex);
    printf("\n--- PRINT SERVER STATS ---\n");
    printf("Total Jobs Submitted: %d\n", stats.totalJobsSubmitted);
    printf("Total Jobs Printed:   %d\n", stats.totalJobsPrinted);
    printf("Number of Preemptions:%d\n", stats.preemptionCount);
    printf("---------------------------\n");
    pthread_mutex_unlock(&queueMutex);
}

void display_logs() {
    FILE* logfile = fopen(LOG_FILE, "r");
    if (logfile == NULL) {
        printf("No logs available.\n");
        return;
    }
    printf("\n--- EVENT LOGS ---\n");
    char line[512];
    while (fgets(line, sizeof(line), logfile)) {
        printf("%s", line);
    }
    printf("--------------------\n");
    fclose(logfile);
}

int split_text_file(const char* sourcePath, const char* job_id_str, int* total_pages) {
    FILE* source = fopen(sourcePath, "r");
    if (source == NULL) return 0;
    char line[1024];
    int chunk_count = 1, line_count = 0, page_count = 0;
    const int LINES_PER_PAGE = 50, PAGES_PER_CHUNK = 4;
    const int LINES_PER_CHUNK = LINES_PER_PAGE * PAGES_PER_CHUNK;
    char chunk_filename[256];
    sprintf(chunk_filename, "%s/%s_chunk_%d.txt", BACKUP_DIR, job_id_str, chunk_count);
    FILE* chunk_file = fopen(chunk_filename, "w");
    if (chunk_file == NULL) { fclose(source); return 0; }
    while (fgets(line, sizeof(line), source) != NULL) {
        if (line_count > 0 && line_count % LINES_PER_PAGE == 0) page_count++;
        if (line_count > 0 && line_count % LINES_PER_CHUNK == 0) {
            fclose(chunk_file);
            chunk_count++;
            sprintf(chunk_filename, "%s/%s_chunk_%d.txt", BACKUP_DIR, job_id_str, chunk_count);
            chunk_file = fopen(chunk_filename, "w");
            if (chunk_file == NULL) { fclose(source); return 0; }
        }
        fputs(line, chunk_file);
        line_count++;
    }
    if (line_count > 0) page_count++;
    fclose(chunk_file);
    fclose(source);
    *total_pages = page_count;
    return chunk_count;
}

void cleanup_temp_pdf_files(PrintJob* job) {
    char command[512];
    char job_id_str[10];
    sprintf(job_id_str, "%d", job->jobId);

#ifdef _WIN32
    char quoted_backup_dir[MAX_FILENAME_LEN + 3];
    quote_path_for_windows(quoted_backup_dir, BACKUP_DIR, sizeof(quoted_backup_dir));
    // Now also clean up .txt chunks
    sprintf(command, "powershell -Command \"Remove-Item -Path '%s\\%s_chunk_*.pdf', '%s\\%s_chunk_*.txt', '%s\\%s_temp_*.pdf', '%s\\gs_*.pdf' -ErrorAction SilentlyContinue\"",
            quoted_backup_dir, job_id_str, quoted_backup_dir, job_id_str, quoted_backup_dir, job_id_str, quoted_backup_dir);
#else
    // Now also clean up .txt chunks
    sprintf(command, "rm %s/%s_chunk_*.pdf %s/%s_chunk_*.txt %s/%s_temp_*.pdf %s/gs_*.pdf 2>/dev/null", 
            BACKUP_DIR, job_id_str, BACKUP_DIR, job_id_str, BACKUP_DIR, job_id_str, BACKUP_DIR);
#endif
    // printf("Cleaning up temp files for Job %d with command: %s\n", job->jobId, command); // Uncomment for debug
    if (system(command) != 0) {
        // fprintf(stderr, "Error cleaning up temp files for Job %d. Command: %s\n", job->jobId, command); // Uncomment for debug
    }
}


// Rewritten process_job_files using Ghostscript
int process_job_files(PrintJob* job) {
    char job_id_str[10];
    sprintf(job_id_str, "%d", job->jobId);
    int num_pages = 0;
    const char* extension = strrchr(job->filename, '.');

    if (extension != NULL && strcmp(extension, ".pdf") == 0) {
        char command[2048];
        char raw_job_filename_path[MAX_FILENAME_LEN + 1];
        strncpy(raw_job_filename_path, job->filename, MAX_FILENAME_LEN);
        raw_job_filename_path[MAX_FILENAME_LEN] = '\0';
        for (char *p = raw_job_filename_path; *p; ++p) {
            if (*p == '\\') *p = '/';
        }
        char quoted_job_filename[MAX_FILENAME_LEN + 3];
        quote_path_for_windows(quoted_job_filename, raw_job_filename_path, sizeof(quoted_job_filename));

        char quoted_backup_dir[MAX_FILENAME_LEN + 3];
        quote_path_for_windows(quoted_backup_dir, BACKUP_DIR, sizeof(quoted_backup_dir));

        // 1. Get total pages using Ghostscript's pdfinfo equivalent
        char temp_page_output_pattern[256];
        sprintf(temp_page_output_pattern, "%s/%s_temp_page_%%d.pdf", BACKUP_DIR, job_id_str);
        char quoted_temp_page_output_pattern[sizeof(temp_page_output_pattern) + 3];
        quote_path_for_windows(quoted_temp_page_output_pattern, temp_page_output_pattern, sizeof(quoted_temp_page_output_pattern));

        // Redirect Ghostscript's stdout to NUL here too
        sprintf(command, "%s -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -dFirstPage=1 -dLastPage=9999 -o %s %s > NUL", // > NUL added
                GS_EXE_PATH, quoted_temp_page_output_pattern, quoted_job_filename);
        if (system(command) != 0) {
            perror("Error generating temp pages for count with Ghostscript");
            cleanup_temp_pdf_files(job);
            return 0;
        }

        // ... (rest of page counting logic, no changes needed) ...

        // 2. Extract 4-page chunks using Ghostscript
        int num_chunks = 0;
        const int PAGES_PER_CHUNK = 4;

        for (int i = 1; i <= job->totalPages; i += PAGES_PER_CHUNK) {
            num_chunks++;
            char chunk_output_path[256];
            char quoted_chunk_output_path[sizeof(chunk_output_path) + 3];

            int start_page = i;
            int end_page = i + PAGES_PER_CHUNK - 1;
            if (end_page > job->totalPages) end_page = job->totalPages;

            sprintf(chunk_output_path, "%s/%s_chunk_%d.pdf", BACKUP_DIR, job_id_str, num_chunks);
            quote_path_for_windows(quoted_chunk_output_path, chunk_output_path, sizeof(quoted_chunk_output_path));

            // Ghostscript command to extract pages: Redirect Ghostscript's stdout to NUL here too
            sprintf(command, "%s -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -dFirstPage=%d -dLastPage=%d -sOutputFile=%s %s > NUL", // > NUL added
                    GS_EXE_PATH, start_page, end_page, quoted_chunk_output_path, quoted_job_filename);

            if (system(command) != 0) {
                perror("Error extracting PDF chunk with Ghostscript");
                return 0; // Indicate failure
            }
        }
        job->totalChunks = num_chunks;
        
        cleanup_temp_pdf_files(job);

    } else { // Text file processing
        job->totalChunks = split_text_file(job->filename, job_id_str, &num_pages);
        if (num_pages <= 0) {
            printf("Error: Could not determine page count for TXT '%s' or file is empty.\n", job->filename);
            return 0;
        }
    }

    if (num_pages > 0) {
        job->totalPages = num_pages;
    } else if (job->totalPages <= 0) {
        job->totalPages = 1; // Fallback to at least one page if still 0
    }
    
    return job->totalPages > 0;
}