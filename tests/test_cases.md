# Test Cases and Expected Results

Hospital Queue Management System - educational queue-management simulation
(not a clinically validated triage system).

## 1. How to run

From the **project root** (the folder containing `Makefile`):

```bash
make test
```

One command builds all seven test programs (with `-std=c++17 -Wall -Wextra
-Wpedantic -Werror`) and runs them. Expected final lines:

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

`make test` returns a non-zero exit status if any check fails. Any failing
check is printed as a line beginning `[FAIL]`.

Run one suite by hand (after `make test` has built it), for example:

```bash
./build/tests/test_priority_queue          # prints every [PASS]/[FAIL] line
```

Without make:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. -Itests \
    tests/test_priority_queue.cpp Patient.cpp HospitalQueue.cpp PatientStorage.cpp \
    HospitalManagementSystem.cpp -o test_priority_queue && ./test_priority_queue
```

## 2. Test design

* **Repeatable and independent.** Every suite is a normal C++ program with its
  own `main`. It prints `[PASS]`/`[FAIL]` per check and a final
  `Result: N passed, M failed` line. No suite depends on another or on files
  left behind by an earlier run.
* **Temporary data only.** Any suite that touches the file system creates a
  uniquely named folder in the operating system's temporary directory
  (`TempDir` in `test_framework.h`) and deletes it on exit. No suite reads or
  writes the project's real `data/patients.txt`; the application suite also
  compares the real file's state (present or absent, and its content) before
  and after and checks that it is unchanged.
* **Real user path.** `test_application.cpp` drives the *complete* menu loop
  `HospitalManagementSystem::run()` with scripted keyboard input, exactly as
  `main()` does with `std::cin`/`std::cout`.
* **Reference-model checks.** Three suites run randomized operations (300 to 4000 each) and
  compare the results with a simple independent model (after every step in the queue and statistics suites, after every session in the application suite) (a sorted
  list by priority then arrival sequence).
* **Counts.** The earlier development stages had 384 passing checks. All 384
  were retained (the original per-stage test programs were split by topic into
  these seven suites). Eight new checks were added in Stage 6 (end-of-input at
  the exit prompts, and the real-data-untouched check), so the total is 392.

## 3. The ten required scenarios and where they are tested

| # | Required scenario | Test(s) | Expected result |
|---|---|---|---|
| 1 | Register three patients with different priorities | `test_registration` A1; `test_priority_queue` "Priority ordering" | IDs 1..3 and sequences 1..3 assigned automatically; all three queued. |
| 2 | Register several patients with the same priority | `test_registration` A1; `test_priority_queue` "FIFO within the same priority" | Unique IDs, increasing arrival sequences. |
| 3 | Emergency patients are served first | `test_serving_search_cancel` A1; `test_priority_queue` | Emergency patient who arrived **last** is served first. |
| 4 | Urgent served before Normal | `test_serving_search_cancel` A1, A2 | Order Emergency, Urgent, Normal. |
| 5 | Equal priority served in arrival order | `test_priority_queue`; `test_serving_search_cancel` A2; `test_persistence` B5 | 12 patients (4 per level) are served 1,2,...,12 in the order E,E,E,E,U,U,U,U,N,N,N,N because they arrived in that order within each level. |
| 6 | Serve when the queue is empty | `test_serving_search_cancel` A3, B1; `test_application` "1 + 17" | `popNext` throws `out_of_range`; the menu prints "There are no patients waiting. The queue is empty." and nothing changes. |
| 7 | Search an existing and a nonexistent patient | `test_serving_search_cancel` A5, B6; `test_application` "2-11" | Existing: full details, status and (if waiting) "Queue Position : k of n". Nonexistent: "No patient with ID 999 exists." |
| 8 | Cancel a waiting patient's registration | `test_serving_search_cancel` A6, A7, B4; `test_application` "8 + 9" | Status becomes Cancelled, patient disappears from the waiting list and is never served; a served patient cannot be cancelled; wrong PIN refuses cancellation. |
| 9 | Save records and restart to test persistence | `test_persistence` B1-B6, B12, B14; `test_application` "12-15" | A new system loading the file has identical records, identical serving order, IDs and sequences continue (next ID is highest + 1). |
| 10 | Invalid menu selections | `test_application` "16" | Text like `abc`, `0`, `9`, `-1`, `1.5`, empty input prints "Invalid choice. Please enter a whole number from 1 to 8." and the menu is shown again; no state changes. |

## 4. Suite-by-suite description

### 4.1 `test_patient.cpp` (10 checks) - Patient class

Constructor and getters; rejects ID 0, blank name, negative age, age 121,
non-positive arrival sequence; `priorityFromInt` accepts 1/2/3 and rejects 0 and
4; `priorityToString`.

### 4.2 `test_priority_queue.cpp` (11 checks) - ordering

Empty-queue safety (`peekNext`/`popNext` throw `out_of_range`); inserting
Normal, Urgent, Emergency in that arrival order gives Emergency, Urgent, Normal;
five Urgent patients are served 1-5 (FIFO); a mixed insertion of seven patients
(`N U N E U E N`) is served `E1,E2,U1,U2,N1,N2,N3`.

### 4.3 `test_registration.cpp` (54 checks) - registration

* A1/A2: seven patients with mixed and repeated priorities; automatic unique
  IDs 1..7 and sequences 1..7; serving order `3,6,2,5,1,4,7`.
* A3: whitespace trimming; age boundaries 0 and 120 accepted.
* A4: twelve invalid registrations (empty/blank text, ages -1, -50, 121, 500,
  priorities 0, 4, -1, 99) are each rejected, change nothing and **consume no
  ID or sequence** (the next valid patient is ID 3).
* A5: duplicate ID rejected; ID/sequence generators move past externally
  inserted records.
* B1-B4: interactive dialogue: confirmation text, re-prompting after each kind
  of invalid input (blank, `abc`, `-5`, `121`, `3.5`, `30x`, overflow), input
  ending mid-registration adds nothing, CRLF line endings and padded numbers.

### 4.4 `test_serving_search_cancel.cpp` (77 checks)

Serving order, FIFO, empty queue, a non-destructive and correctly ordered
waiting list, `findPatient`, cancellation results (`Cancelled`, `NotFound`,
`AlreadyServed`, `AlreadyCancelled`), **lazy deletion** (cancelling the front,
the middle, the last, and everyone), duplicate protection, a 4000-operation
random comparison with a reference model, and the user-facing display, search,
cancel and serve functions including table truncation of very long text.

### 4.5 `test_statistics.cpp` (8 checks)

Zero statistics when empty; correct totals and per-priority waiting counts after
registration, serving and cancelling; refused cancellations change nothing; the
displayed statistics text; a 3000-operation random comparison with a reference.

### 4.6 `test_persistence.cpp` (123 checks)

* File content and format (header, 8 tab-separated fields, `END` count); folder
  creation on first save.
* Round trips: identical records and serving order after a "restart"; served and
  cancelled patients stay served/cancelled; new IDs/sequences continue.
* Special characters: names with spaces, tabs, backslashes, quotes, newlines.
* Missing file, zero-byte file.
* **Malformed files** are rejected completely and leave memory and file
  untouched: bad header, wrong version, wrong field count, non-numeric or
  overflowing numbers, bad escape, unknown status, invalid age or priority,
  duplicate ID, duplicate arrival sequence, missing or wrong `END`, truncated
  file, data after `END`.
* Valid variations load (CRLF endings, empty registry, zero records).
* Unreadable file: saving is blocked so the file is not overwritten.
* Save failures (parent path is a file, blocked temp file) are reported and the
  old file is not damaged; repeated save/load never creates duplicates; loading
  replaces rather than merges; default data path is `data/patients.txt`.

### 4.7 `test_application.cpp` (109 checks) - complete application loop

Startup with no data file; welcome screen with the educational notice; all
eight menu items and the prompt; register/view/serve/search/statistics/save/exit
through the menu; restart and restored data; authorized and unauthorized
cancellation (wrong PIN three times, custom PIN); ten kinds of invalid menu
input; end-of-input at every stage; file-saving failures (warning shown, data
kept in memory, exit needs consent, exit code 2, recovery after fixing the
problem); corrupted data files (not overwritten, backup offered and made);
a randomized workload of 5 sessions x 60 operations with a restart between sessions, checked against a reference model.

**End of input must not authorize discarding data** (added in Stage 6):

| Scenario | Expected result |
|---|---|
| Unsaved changes; input ends at "Are you sure you want to exit?" | "Input ended before exit was confirmed. Exit is not assumed." Then the end-of-input shutdown tries to save; if it fails, "Exiting with UNSAVED changes." and exit code 2. |
| Unsaved changes; the save fails; input ends at "Exit anyway and lose the unsaved changes?" | "Exiting is NOT assumed ..." The patient stays in memory, nothing is written, exit code 2. |
| The same, then the save problem is fixed and the program is started again with immediate end of input | The held patient is saved; exit code 0. |
| Answer `no` at "exit anyway", then end of input | Back at the menu, then warned shutdown with exit code 2. |
| Whole test program | The real `data/patients.txt` is unchanged. |

## 5. Sample manual test script and expected output

To run the ten scenarios by hand, start in a scratch copy of the project (so
`data/patients.txt` of your real project is not used) and run `./build/hospital_queue`.

| Step | Input | Expected output (key lines) |
|---|---|---|
| 1 | `3` | `There are no patients waiting. The queue is empty.` |
| 2 | `1`, `Ama Mensah`, `41`, `Female`, `Chest pain`, `3` | `REGISTRATION SUCCESSFUL`, `Patient ID : 1`, `Priority : Normal (3)`, `Arrival Sequence : 1` |
| 3 | register `Kwame Asante`, 30, Male, Fever, `2` | Patient ID 2, `Urgent (2)` |
| 4 | register `Efua Owusu`, 25, Female, Fracture, `1` | Patient ID 3, `Emergency (1)` |
| 5 | register `Yaw Darko`, 52, Male, Chest pain, `1` | Patient ID 4, `Emergency (1)` |
| 6 | `2` | Table in order: ID 3 Efua (Emergency), ID 4 Yaw (Emergency), ID 2 Kwame (Urgent), ID 1 Ama (Normal); `Total waiting: 4` |
| 7 | `3` | `NOW SERVING` ... Efua Owusu (ID 3); `Patients still waiting: 3` |
| 8 | `4`, `4` | Yaw Darko, `Status : Waiting`, `Queue Position : 1 of 3` |
| 9 | `4`, `999` | `No patient with ID 999 exists.` |
| 10 | `5`, `wrong`, `wrong`, `wrong` | three `Incorrect PIN` lines, then `Access denied: no registration was cancelled.` |
| 11 | `5`, the staff PIN, `2`, `yes` | `Registration cancelled for patient ID 2 (Kwame Asante).` |
| 12 | `6` | registered 4, waiting 2, served 1, cancelled 1; Emergency 1, Urgent 0, Normal 1 |
| 13 | `9`, `abc`, (empty line) | `Invalid choice. Please enter a whole number from 1 to 8.` each time |
| 14 | `8`, `yes`, then start the program again | After restart: `Loaded 4 patient record(s)`, `2 waiting, 1 served, 1 cancelled.`; option 2 lists Yaw Darko then Ama Mensah |

(Step 14's counts follow from steps 2-11. The automated tests perform the same
kind of sequence and check the results.)

## 6. Limits of the testing

* The tests prove the behaviour described above on Linux with the compilers
  used; they are not a proof of correctness for every possible input.
* Time performance is not measured; complexity is analysed in
  `documentation/Technical_Documentation.md`.
* Terminal-specific behaviour (Windows console, non-ASCII alignment) is not
  tested.
