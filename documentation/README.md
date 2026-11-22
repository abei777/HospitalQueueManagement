# Hospital Queue Management System with a Priority Queue

A console application written in **C++17** that organises waiting hospital
patients by the seriousness of their condition instead of simply by arrival
time.

> **Educational simulation - not a clinical tool.** This project is an
> educational queue-management simulation. It is **not** a clinically validated
> triage system. Real hospital triage requires approved medical protocols and
> qualified healthcare professionals. All names in the examples are invented.

---

## 1. Overview

Patients are registered with a name, age, gender, medical condition and a
priority level. The program keeps them in a **priority queue** so that the next
patient to be served is always the most urgent one; patients with the *same*
priority are served in **arrival order (FIFO)**. Records are saved to a text
file and restored when the program starts again.

## 2. Problem statement

An ordinary first-come-first-served queue makes a critically ill patient wait
behind everybody who arrived earlier. The problem is to design and implement a
system that always selects the most urgent waiting patient, treats patients of
equal urgency fairly by arrival order, lets staff search and cancel
registrations, reports statistics, and does not lose data between sessions.

## 3. Objectives

1. Implement a priority queue with a custom comparator (Emergency, then Urgent,
   then Normal) and FIFO order within each level.
2. Apply object-oriented design: classes, constructors, encapsulation,
   separation of responsibilities.
3. Provide registration, serving, display, search by ID, authorized
   cancellation and statistics.
4. Persist records in a file safely (validation on load, no half-written files).
5. Validate all input and handle empty queues and file errors without crashing.
6. Provide a repeatable automated test suite.

## 4. Technologies and requirements

* Language: **C++17** (uses `std::priority_queue`, `std::map`, `std::optional`,
  `std::filesystem`, `enum class`).
* A C++17-capable compiler. Developed and tested with g++ 13.3; also built and tested with clang++ 18.1.3, and with g++ using AddressSanitizer and UndefinedBehaviorSanitizer (all 392 checks pass). `make` is optional but convenient.
* No external libraries. The test suite uses its own tiny helper
  (`tests/test_framework.h`).
* The code uses `<filesystem>`; compilers that need an extra flag for it (for example `-lstdc++fs` on GCC 8) are not covered by the tests.

## 5. Features

Verified in the source code and covered by the automated tests:

| Feature | Where |
|---|---|
| Register a patient with automatic unique ID and arrival sequence | `HospitalQueue::registerPatient` |
| Three priority levels: Emergency (1), Urgent (2), Normal (3) | `PriorityLevel` in `Patient.h` |
| Priority queue with custom comparator; FIFO within a level | `HospitalQueue::PatientComparator` |
| Serve the next patient, with a message when the queue is empty | `HospitalQueue::popNext`, `HospitalManagementSystem::serveNextPatient` |
| Display all waiting patients in serving order (queue not changed) | `HospitalQueue::getWaitingPatients` |
| Search by patient ID: details, status, queue position | `HospitalManagementSystem::searchPatient` |
| Cancel a waiting patient (staff PIN + confirmation); served patients cannot be cancelled | `cancelPatientAuthorized` |
| Statistics: total registered, waiting, served, cancelled, waiting per priority | `HospitalQueue::getStatistics` |
| Save and load records between sessions; automatic save after each change | `PatientStorage`, `autoSave` |
| Damaged data file is never overwritten silently (backup `.bak` offered) | `saveRecordsInteractive` |
| Unsaved-change protection on exit (including end-of-input) | `finishSession`, `confirmExit` |

Menu wording note: the project brief lists option 2 as "View Waiting Patients";
the program displays it as **"2. View All Waiting Patients"**. The other seven
items are as specified.

## 6. Folder structure

```
HospitalQueueManagement/
|-- main.cpp
|-- Patient.h / Patient.cpp
|-- HospitalQueue.h / HospitalQueue.cpp
|-- PatientStorage.h / PatientStorage.cpp         (file persistence; added in Stage 4)
|-- HospitalManagementSystem.h / HospitalManagementSystem.cpp
|-- Makefile                                      (build, run, test, clean)
|-- data/
|   `-- patients.txt                              (created by the program on first save)
|-- documentation/
|   |-- README.md                                 (this file)
|   |-- Technical_Documentation.md                (design, pseudocode, complexity)
|   |-- Project_Report.docx                       (academic report)
|   |-- defence_questions.md                      (lecturer questions and answers)
|   |-- build_report.js                           (script that regenerates Project_Report.docx)
|   `-- diagrams/                                 (Mermaid sources + PNG images)
`-- tests/
    |-- test_framework.h
    |-- test_patient.cpp
    |-- test_priority_queue.cpp
    |-- test_registration.cpp
    |-- test_serving_search_cancel.cpp
    |-- test_statistics.cpp
    |-- test_persistence.cpp
    |-- test_application.cpp
    `-- test_cases.md                             (what each test checks, expected results)
```

The `build/` folder is created by `make` and can be deleted with `make clean`.

## 7. Compile and run

**Always start from the project root** (the folder that contains `Makefile`),
because the data file path `data/patients.txt` is relative to the current folder.

With make:

```bash
make            # builds build/hospital_queue
make run        # builds and starts the program
```

Without make (single command):

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 \
    main.cpp Patient.cpp HospitalQueue.cpp PatientStorage.cpp HospitalManagementSystem.cpp \
    -o hospital_queue
./hospital_queue
```

Exit codes: `0` normal exit with everything saved; `2` the program closed while
some changes could not be saved; `1` unexpected fatal error.

## 8. Run the tests

From the project root:

```bash
make test
```

This builds seven test programs and runs them, printing one result line per
suite and a grand total. A successful run ends with:

```
test_patient                 Result: 10 passed, 0 failed
test_priority_queue          Result: 11 passed, 0 failed
test_registration            Result: 54 passed, 0 failed
test_serving_search_cancel   Result: 77 passed, 0 failed
test_statistics              Result: 8 passed, 0 failed
test_persistence             Result: 123 passed, 0 failed
test_application             Result: 109 passed, 0 failed
----------------------------------------------------------------
TOTAL: 392 passed, 0 failed
```

`make test` exits with a non-zero status if any check fails or any suite does
not compile. The tests use only a **temporary folder** (created in the system
temporary directory and deleted afterwards) and never read or modify the real
`data/patients.txt`. See `tests/test_cases.md` for details of every suite, and
how to run a single suite by hand.

## 9. Sample session

Welcome screen, then the main menu (exactly as printed by the program):

```
==========================================================
            HOSPITAL QUEUE MANAGEMENT SYSTEM
       Priority-based patient queue (C++17 project)
==========================================================
Patients are served by medical priority first, then by
arrival order:  1 = Emergency   2 = Urgent   3 = Normal

NOTE: This is an educational queue-management simulation,
NOT a clinically validated triage system. Real hospital
triage requires approved medical protocols and qualified
healthcare professionals.

---------- STARTUP ----------
No saved data file found at 'data/patients.txt'. Starting with an empty registry.

========== HOSPITAL QUEUE MANAGEMENT SYSTEM ==========

1. Register New Patient
2. View All Waiting Patients
3. Serve Next Patient
4. Search Patient
5. Cancel Patient Registration
6. View Queue Statistics
7. Save Patient Records
8. Exit System

Enter your choice:
```

After registering Ama Mensah (Emergency), Kwame Asante (Normal) and Efua Owusu
(Urgent), in that order, option 2 shows them in the order they will be served,
which is *not* the order of arrival:

```
========== WAITING PATIENTS (in serving order) ==========
Order  ID    Name                   Age  Gender     Priority   Arrival  Condition
--------------------------------------------------------------------------------------
1      1     Ama Mensah             41   Female     Emergency  1        Chest pain
2      3     Efua Owusu             25   Female     Urgent     3        Fracture
3      2     Kwame Asante           30   Male       Normal     2        Fever
Total waiting: 3
```

Option 3 then serves the first row:

```
========== NOW SERVING ==========
Patient ID        : 1
Full Name         : Ama Mensah
Age               : 41
Gender            : Female
Medical Condition : Chest pain
Priority          : Emergency (1)
Arrival Sequence  : 1
Status            : Served
Patients still waiting: 2
=================================
```



## 10. How data is stored

`data/patients.txt` is a plain-text, tab-separated file, written automatically
after every successful registration, served patient or cancellation, and when
you choose option 7 or exit. Example (the file from the session above, after
serving two patients):

```
HQMS-PATIENT-RECORDS<TAB>1
1<TAB>Ama Mensah<TAB>41<TAB>Female<TAB>Chest pain<TAB>1<TAB>1<TAB>Served
2<TAB>Kwame Asante<TAB>30<TAB>Male<TAB>Fever<TAB>3<TAB>2<TAB>Waiting
3<TAB>Efua Owusu<TAB>25<TAB>Female<TAB>Fracture<TAB>2<TAB>3<TAB>Served
END<TAB>3
```

Fields: ID, name, age, gender, condition, priority, arrival sequence, status
(`Waiting`, `Served` or `Cancelled`). The first line identifies the format and
version; the last line records how many patients the file should contain, so a
truncated file is detected. When the program starts, **all** patients are read
back into the registry, but only those with status `Waiting` re-enter the
priority queue.

Safety measures: the file is written to `patients.txt.tmp` and then renamed
over the old file; a file that is corrupt is rejected entirely, left untouched,
and saving is disabled until the problem is dealt with (menu option 7 offers to
replace it, keeping a `patients.txt.bak` copy first). Please do not edit the
file by hand while the program is running.

The data file contains personal-looking information. Do **not** put real patient
data into this educational project.

## 11. Staff PIN for cancellation

Cancelling a registration (menu option 5) asks for a staff PIN (three
attempts). The PIN is set by the constant `DEFAULT_STAFF_PIN` in
`HospitalManagementSystem.h`. This is a **classroom demonstration** of
authorization, not real security: the PIN is stored in the program as plain
text.

## 12. Known limitations

* **Not clinically validated.** Priority is chosen by the person registering the
  patient; the program does not assess symptoms.
* **Staff PIN is not real security.** It is a fixed string in the source code
  and is not encrypted.
* **Single user, single process.** Two programs sharing one data file are not
  coordinated; the last save wins.
* **Whole-file rewrite.** Every save rewrites all records (O(N)). Fine for a
  teaching project, not for a very large hospital.
* **Plain-text storage, no encryption**, and no audit trail or access log.
* **No editing of a patient** after registration (to fix a typo, cancel and
  register again).
* **Priority cannot change** after registration (no escalation of a waiting
  patient).
* **Display and queue position cost O(n log n)**, because the heap is copied
  and emptied to list patients in order.
* **Simple text interface.** There is no graphical interface. Table columns have fixed widths; over-long names, genders and conditions are shortened with "..." in the table (the stored data is not shortened).
* **Timestamps are not recorded.** "Arrival" is a registration sequence number,
  as allowed by the project brief, not a clock time.
* Tested on Linux with g++ 13.3 and clang++. The code uses only standard C++17
  and should also build on Windows with a recent compiler, but this has not
  been tested here.

## 13. Where to read more

* `Technical_Documentation.md` - class design, comparator, lazy deletion,
  file format, pseudocode, complexity, UML and flowcharts.
* `defence_questions.md` - likely examination questions with answers.
* `Project_Report.docx` - the academic project report.
* `../tests/test_cases.md` - the test plan and expected results.
