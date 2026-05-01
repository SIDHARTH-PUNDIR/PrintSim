<div align="center">
  <h1>🖨️ PrintServer Simulator</h1>
  <p><b>Multi-Threaded Print Job Scheduling & Execution System</b></p>

  [![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=black)](https://en.wikipedia.org/wiki/C_(programming_language))
  [![pthreads](https://img.shields.io/badge/pthreads-POSIX-6E4C9F?style=for-the-badge&logo=linux&logoColor=white)](https://man7.org/linux/man-pages/man7/pthreads.7.html)
  [![VS Code](https://img.shields.io/badge/VS_Code-007ACC?style=for-the-badge&logo=visualstudiocode&logoColor=white)](https://code.visualstudio.com/)
  [![Windows](https://img.shields.io/badge/Windows-0078D4?style=for-the-badge&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
  [![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)](https://www.linux.org/)

  *A multi-threaded Print Server Simulator written in C that models how print jobs are queued, scheduled, and executed by virtual printers. This project demonstrates core concepts of **process scheduling**, **synchronization**, **logging**, and **file handling** in Operating Systems.*

</div>

---

## ✨ Key Features

* **Job Queue Management:** Add, schedule, and print jobs using priority and preemptive scheduling algorithms.
* **Multi-threaded Execution:** Simulated concurrent printing using `pthread` for parallel job processing.
* **Mutex Synchronization:** Thread-safe queue access enforced via POSIX Mutex locks.
* **Backup System:** Automatically backs up each submitted print job file to `PrintServerJobs/`.
* **Logging System:** Records job events, completion times, and errors in `job_log.txt`.
* **CLI Interface:** Submit new jobs, view the queue, cancel jobs, reassign priorities, and monitor system status — all from the terminal.
* **Priority Scheduling:** Supports priority-based queuing of jobs without preemption.
* **Preemptive Scheduling:** Supports preemption based on number of pages for fairer execution.
* **Starvation Resolution:** A job can only be preempted a limited number of times to prevent indefinite starvation.

---

## 🧠 Scheduling Algorithms

The simulator implements three scheduling strategies that work together to balance performance and fairness.

---

### 1️⃣ Priority-Based Scheduling (Non-Preemptive)

Each print job is assigned a **priority level** (e.g., 1 = Highest, 5 = Lowest) at the time of submission. The scheduler always picks the highest-priority job from the queue and runs it to completion before picking the next one.

```
Queue (sorted by priority):
┌─────────────────────────────────────────────┐
│  [P1] Job A  →  [P2] Job C  →  [P4] Job B  │
└─────────────────────────────────────────────┘
         ↓ Printer picks Job A first (highest priority)
```

**Key behaviour:**
* Once a job starts printing, it is never interrupted.
* New high-priority jobs submitted during execution wait for the current job to finish.
* Best suited for critical documents that must be printed without interruption.

---

### 2️⃣ Preemptive Scheduling (Page-Based)

When a new job arrives with a **higher priority**, the scheduler can interrupt the currently printing job if the incoming job also has **fewer pages** remaining. The interrupted job is pushed back into the queue and resumes later.

```
Printer running: [P3] Job B  (10 pages left)
New arrival:     [P1] Job D  (3 pages)

→ Job D PREEMPTS Job B
→ Job B re-enters queue with its remaining pages intact
→ Job D starts printing immediately
```

**Key behaviour:**
* Preemption is triggered only when both priority AND page count conditions are met.
* The preempted job retains its progress — it does not restart from page 1.
* Ensures urgent, short jobs are never stuck behind long low-priority prints.

---

### 3️⃣ Starvation Resolution

A side effect of preemptive + priority scheduling is **starvation** — a low-priority job may keep getting pushed back indefinitely as higher-priority jobs keep arriving.

To prevent this, each job tracks a **preemption counter**:

```
Job B preempted → counter: 1
Job B preempted → counter: 2
Job B preempted → counter: MAX_PREEMPTIONS (e.g., 3)
→ Job B is now PREEMPTION-IMMUNE
→ No further jobs can interrupt it, regardless of priority
```

**Key behaviour:**
* Once a job hits the preemption limit, it is guaranteed to complete its run.
* The limit (`MAX_PREEMPTIONS`) is defined as a constant in `print_server.h` and can be tuned.
* Balances responsiveness for high-priority jobs with fairness for long-waiting jobs.

---

### ⚖️ Algorithm Comparison

| Feature | Priority (Non-Preemptive) | Preemptive | With Starvation Guard |
|---|---|---|---|
| Interrupts running jobs | ❌ No | ✅ Yes | ✅ Yes (with limit) |
| Fairness for low-priority | ✅ Fair | ❌ Can starve | ✅ Guaranteed |
| Responsiveness for urgent jobs | ❌ Must wait | ✅ Immediate | ✅ Immediate |
| Job restarts on preemption | — | ❌ Resumes | ❌ Resumes |

---

## 🛠️ Tech Stack

| Domain | Technologies |
|---|---|
| **Language** | C |
| **Threading & Sync** | POSIX Threads (`pthreads`), Mutex |
| **IDE** | Visual Studio Code |
| **OS Compatibility** | Windows / Linux (POSIX compliant) |

---

## 📂 Project Structure

```text
PrintServer/
├── main.c                   # Main scheduler and program entry point
├── producer.c               # Handles job submission and job queue
├── printer.c                # Manages printing and OS-level interactions
├── cli.c                    # Command-line interface for user operations
├── logger.c                 # Handles event logging to job_log.txt
├── utils.c                  # Helper functions (file backup, comparators, etc.)
├── include/
│   └── print_server.h       # Shared constants, structs, and definitions
├── PrintServerJobs/         # Folder where backup of submitted jobs is stored
├── job_log.txt              # Event log file
└── .vscode/                 # VS Code configurations (launch.json, tasks.json)
```

---

## 🚀 Getting Started

### Prerequisites
* GCC compiler (MinGW on Windows or GCC on Linux)
* POSIX-compatible environment (Linux native or WSL on Windows)
* Visual Studio Code (recommended)

### 1️⃣ Clone the Repository
```bash
git clone https://github.com/Astic-x/PrintSim
cd PrintSim
```

### 2️⃣ Compile the Project
```bash
gcc -g *.c -o PrintServer.exe -pthread
```

### 3️⃣ Run the Simulator
```bash
./PrintServer.exe
```

The CLI will launch and allow you to submit jobs, view the queue, and monitor the system in real time.



## 👥 Contributors

<table>
  <tr>
    <td align="center">
      <a href="https://github.com/Astic-x">
        <img src="https://github.com/Astic-x.png" width="100px;" alt="Ankush Malik"/><br />
        <sub><b>Ankush Malik</b></sub>
      </a><br />
      <sub>Job Queue & Producer Logic</sub>
    </td>
    <td align="center">
      <a href="https://github.com/vishalsingh21xyz">
        <img src="https://github.com/vishalsingh21xyz.png" width="100px;" alt="Vishal Vijay Singh"/><br />
        <sub><b>Vishal Vijay Singh</b></sub>
      </a><br />
      <sub>CLI Interface & Logging</sub>
    </td>
    <td align="center">
      <a href="https://github.com/SIDHARTH-PUNDIR">
        <img src="https://github.com/SIDHARTH-PUNDIR.png" width="100px;" alt="Sidharth Pundir"/><br />
        <sub><b>Sidharth Pundir</b></sub>
      </a><br />
      <sub>Scheduler & Synchronization</sub>
    </td>
    <td align="center">
      <a href="https://github.com/akshatbansal13">
        <img src="https://github.com/akshatbansal13.png" width="100px;" alt="Akshat Bansal"/><br />
        <sub><b>Akshat Bansal</b></sub>
      </a><br />
      <sub>Printer Module & Backup System</sub>
    </td>
  </tr>
</table>

<p align="center">
  <br>
  <i>Developed for academic purposes as part of the Operating Systems coursework.</i><br>
  <i>Free to use and modify for educational or demonstration purposes.</i>
</p>
