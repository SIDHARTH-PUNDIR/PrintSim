
# Print Server Simulator

A multi-threaded **Print Server Simulator** written in C that models how print jobs are queued, scheduled, and executed by virtual printers.  
This project demonstrates concepts of **process scheduling**, **synchronization**, **logging**, and **file handling** in Operating Systems.

## Project Structure

PrintServer/
├── main.c # Main scheduler and program entry
├── producer.c # Handles job submission and job queue
├── printer.c # Manages printing and OS-level interactions
├── cli.c # Command-line interface for user operations
├── logger.c # Handles event logging to job_log.txt
├── utils.c # Helper functions (file backup, comparators, etc.)
├── include/
│ └── print_server.h # Shared constants, structs, and definitions
├── PrintServerJobs/ # Folder where backup of submitted jobs is stored
├── job_log.txt # Event log file
└── .vscode/ # VS Code configurations (launch.json, tasks.json)

## Features

- **Job Queue Management** — Add, schedule, and print jobs using priority/preemptive scheduling
- **Multi-threaded Execution and Synchronization** — Simulated concurrent printing using `pthread` and synchronize queue acces using Mutex 
- **Backup System** — Automatically backs up each print job file to `PrintServerJobs/`  
- **Logging System** — Records job events, completion time, and errors in `job_log.txt` 
- **CLI Interface** — User can submit new jobs, view queue, cancel jobs, reassign priority to new jobs and monitor system status
- **Priority Scheduling** — Supports Priority based queing of jobs without preemption 
- **Preemptive Scheduling** — Supports preemption based on number of pages
- **Starvation Resolutin** — A job can be preempted only limited number of times 




## Run Locally

Clone the project

```bash
  git clone https://github.com/Astic-x/PrintSim
```

Go to the project directory

```bash
  cd my-project
```

Install dependencies

```bash
  npm install
```

Compile

```bash
 gcc -g *.c -o PrintServer.exe -pthread
```


## Tech Stack

**Language**: C
**Threading**: POSIX Threads (pthreads)
**IDE**: Visual Studio Code
**OS Compatibility**: Windows / Linux (POSIX compliant)

## Authors

- [Ankush Malik](https://www.github.com/Astic-x)
- [Sidharth Pundir](https://www.github.com/SIDHARTH-PUNDIR)
- [Akshat Bansal](https://www.github.com/akshatbansal13)
- [Vishal Singh](https://www.github.com/vishalsingh21xyz)


## License

This project is developed for academic purposes as part of the Operating Systems coursework.
You are free to modify and use it for educational or demonstration purposes.

