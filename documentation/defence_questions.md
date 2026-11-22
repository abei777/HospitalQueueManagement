# Project Defence: Likely Questions and Model Answers

Hospital Queue Management System with a Priority Queue (C++17)

> This project is an **educational queue-management simulation**, not a
> clinically validated triage system. Say this early in your presentation.

Answers refer to the real code. File and function names are given so you can
point at them on screen. Where an answer includes a number (for example the
test totals) it is the result of `make test` at the time of writing; re-run it
before your presentation.

---

## A. Choice of data structure

### 1. Why did you choose a priority queue instead of a normal queue?
A normal queue is first-in-first-out, so a critical patient who arrives last
waits behind everyone. The requirement is that the *most serious* patient is
served first regardless of arrival time. A priority queue always keeps the most
urgent waiting patient at its top: reading it is O(1), inserting is O(log n),
removing is O(log n). A FIFO queue cannot do this without searching through
everyone.

### 2. Why not just keep a sorted vector or list?
Keeping a sorted `std::vector` means every insertion shifts elements: O(n). A
sorted `std::list` needs O(n) to find the position. A heap gives O(log n)
insert and remove, and we only ever need the *top* element, so a full ordering
of all patients is more work than the problem needs. (The only time we need a
full ordering is when displaying the list; that costs O(n log n), see Q27.)

### 3. What is the underlying structure of `std::priority_queue`?
By default it is an adapter over a `std::vector` that maintains a **binary
max-heap** using `std::push_heap` and `std::pop_heap`. The element that the
comparator ranks highest is at the top.

### 4. Why did you use `std::priority_queue` instead of writing your own heap?
The project allowed either. The standard container is well tested and lets the
presentation focus on the *comparator and the surrounding design* (registry,
lazy deletion, persistence) rather than on re-implementing sift-up/sift-down.
I can still explain the heap: push appends at the end and sifts up; pop swaps
the root with the last element, removes it and sifts down.

---

## B. The comparator and ordering

### 5. How does your comparator work?
`HospitalQueue::PatientComparator::operator()(a, b)` returns `true` when `a`
should come out **after** `b`:

```cpp
if (a.getPriority() != b.getPriority())
    return int(a.getPriority()) > int(b.getPriority());   // bigger number = less urgent = later
return a.getArrivalSequence() > b.getArrivalSequence();    // arrived later = later
```

`std::priority_queue` places the element for which the comparator says "comes
later" lower in the heap, so the top is the patient with the smallest priority
number and, among equals, the smallest arrival sequence.

### 6. Why does a *lower* number mean a *higher* urgency?
The project specification defines Emergency as Priority 1, Urgent as 2, Normal
as 3. `std::priority_queue` is a *max*-heap (largest at the top), so the
comparator is written "backwards": a larger number compares as "less important".
The enum is `enum class PriorityLevel : int { Emergency = 1, Urgent = 2, Normal = 3 }`.
An alternative would have been `std::greater` ordering or a min-heap; the effect
is the same.

### 7. How do you maintain FIFO for patients with equal priority?
Each patient gets an **arrival sequence** number (1, 2, 3, ...) when registered.
When two patients have the same priority the comparator compares their sequence
numbers, and the smaller one comes out first. Because sequence numbers are
unique, there are never real ties, so the heap's lack of stability does not
matter. Tests: `test_priority_queue.cpp` ("FIFO within the same priority"),
`test_serving_search_cancel.cpp` A2.

### 8. A heap is not a stable structure. Doesn't that break FIFO?
Stability only matters when elements compare as equal. Here two different
patients never compare as equal because the arrival sequence is a unique
tiebreaker; the order `(priority, sequence)` is a *strict total order*. So the
pop order is fully determined by the data, not by the heap's internal layout.

### 9. How does your system handle patients with different emergency levels?
Three levels: Emergency (1), Urgent (2), Normal (3). Every Emergency patient is
served before any Urgent patient, and every Urgent before any Normal. Within a
level, arrival order decides. Example from the tests: arrivals `N U E N U E N`
are served `E E U U N N N`, with each level in arrival order.

### 10. Can a Normal patient wait forever (starvation)?
In theory yes: if Emergency patients keep arriving, a Normal patient is never
reached. The project does not implement ageing (raising the priority of long
waiters). That is a real limitation of strict priority scheduling and a
possible improvement; it is listed under future work in the report. In the
simulation staff see the whole list.

---

## C. Design and OOP

### 11. Why is there a `std::map` registry alongside the priority queue?
A priority queue only gives access to its top. It cannot find a patient by ID,
and served or cancelled patients must still be searchable and countable. So
`patientRecords_` (a `std::map<int, PatientRecord>`) stores *every* patient
with their status. It gives search by ID in O(log N), rejects duplicate IDs, and
is what gets saved to the file. The heap decides *order*; the map stores
*facts*.

### 12. Why are Patient objects immutable?
`Patient` has only getters. This means a patient's priority or arrival sequence
cannot change while a copy of the patient sits inside the heap, which would
silently break the heap ordering (the heap would not re-sort). The patient's
*status* changes, so it lives in the registry (`PatientRecord::status`), not in
the `Patient`.

### 13. What OOP principles did you use?
Encapsulation (all data members private), constructors that validate (an invalid
`Patient` cannot be built), separation of responsibilities (queue logic /
file format / user interface in three classes), composition
(`HospitalManagementSystem` owns a `HospitalQueue`), const-correctness, and
enumerations for named constants. I did **not** use inheritance or virtual
functions because the problem does not need them.

### 14. Why is `HospitalQueue` separate from `HospitalManagementSystem`?
So the queue rules can be tested without a console or a file. `HospitalQueue`
has no `cout`/`cin`/file code. `HospitalManagementSystem` handles the user and
takes `std::istream&`/`std::ostream&` parameters so that tests can feed it
scripted input.

### 15. How are unique patient IDs generated, and how do you prevent duplicates?
`HospitalQueue` has `nextPatientId_` starting at 1. The ID is used for the new
patient and the counter advances only after the patient is inserted
successfully. `insertPatient` calls `map::emplace`, which fails if the ID
already exists, and throws `std::invalid_argument`. After loading a file, the
counter is set past the highest ID in the file, so IDs of served and cancelled
patients are never reused.

### 16. What happens to the ID counter when a registration fails?
Nothing is consumed. Registration is *validate-then-commit*: all fields are
checked first; only a valid patient is created and inserted. A test registers
twelve invalid patients and checks the next valid one still receives ID 3.

---

## D. Serving, cancellation, lazy deletion

### 17. `std::priority_queue` can't remove an arbitrary element. How do you cancel a patient?
By **lazy deletion**. Cancelling changes the patient's status in the registry to
`Cancelled` and updates the counters; the patient's copy stays in the heap as a
"ghost". The class invariant is that the heap's **top is always a genuine
Waiting patient**. After every `popNext` and `cancelPatient`,
`discardCancelledFromTop()` pops any ghosts from the top. Ghosts deeper in the
heap are ignored when listing (the registry is checked) and removed when they
reach the top.

### 18. Isn't leaving ghosts wasteful, or could a cancelled patient be served?
Memory is bounded by the number of cancellations. A cancelled patient cannot be
served because of the invariant: `popNext` only ever takes the top, and the top
is never a ghost. Tests cancel the front, a middle, the last, and everybody, and
a randomized test compares the queue with a reference model after every one of
4000 operations.

### 19. Why do you keep `waitingCount_` instead of using the heap's size?
The heap size includes ghosts, so it overstates the number of waiting patients.
The counter is exact and gives `size()`/`isEmpty()` in O(1).

### 20. What if the queue is empty when serving?
`HospitalQueue::peekNext`/`popNext` throw `std::out_of_range`. The menu handler
checks `isEmpty()` first and prints "There are no patients waiting. The queue
is empty." so the user never sees an exception. Test: `test_serving_search_cancel`
A3 and B1.

### 21. Why does cancellation need a staff PIN?
The requirement says "an authorized user". The PIN is a classroom demonstration
of access control: three attempts, asked *before* the ID so an unauthorized user
cannot probe which IDs exist. It is **not** real security (a fixed string in the
source). A real system would use accounts, hashed credentials and audit logs.

### 22. Can a served patient be cancelled?
No. `cancelPatient` returns `CancelResult::AlreadyServed` and changes nothing,
and the menu says so. Likewise a cancelled patient cannot be cancelled again.

---

## E. Persistence

### 23. How does your file persistence work?
`PatientStorage` writes a versioned text file: a header line, one tab-separated
line per patient (ID, name, age, gender, condition, priority, arrival sequence,
status), and an `END` line with the record count. Text fields escape backslash,
tab and newline. All patients are saved, including served and cancelled ones.
On startup all are loaded into the registry and only `Waiting` ones are pushed
into the heap.

### 24. How do you make sure a crash while saving doesn't destroy the data file?
The file is written to `patients.txt.tmp` first and then `rename`d over the real
file. The real file is only replaced once the new one is complete. If writing
fails the temporary file is removed and the old file is untouched.

### 25. What happens if the data file is corrupted?
`load()` validates strictly (header and version, 8 fields per line, whole-number
parsing, escapes, status, the `Patient` constructor's rules, duplicate ID and
duplicate arrival sequence, END count). One error rejects the *whole* file;
nothing partial is loaded. The program then blocks saving so the damaged file is
not overwritten by accident. The user can choose menu option 7 and agree to
replace it, in which case a `.bak` copy is made first.

### 26. How do you restore FIFO order after a restart?
The arrival sequence is saved with each patient, so the comparator gives the
same order after loading. A test writes a file by hand with patients in a
scrambled order and checks they are served in sequence order.

---

## F. Complexity

### 27. What is the time complexity of insertion and deletion?
For the heap: insertion (`push`) O(log n), view next (`top`) O(1), removal of
the top (`pop`) O(log n). For the whole operations: registering a patient is
O(log N) (registry insert dominates, plus O(log n) for the heap); serving is
O(log n) amortised (ghost cleanup is amortised against cancellations);
search by ID O(log N); cancel O(log N) plus amortised cleanup; display of the
waiting list O(n log n) because the heap is copied and emptied to list it in
order; statistics O(N); save O(N); load O(N log N). n = waiting patients,
N = all registered. Full table: `Technical_Documentation.md`, section 12.

### 28. "Amortised" - what does it mean here?
Each ghost entry is removed from the heap at most once, and each removal costs
O(log n). The number of ghosts is at most the number of cancellations, so the
total cleanup cost over many operations is bounded by (number of cancellations)
x O(log n); averaged over operations it adds only O(log n) per cancellation.

### 29. Why is displaying the waiting list O(n log n)?
`std::priority_queue` cannot be iterated. To print patients in serving order,
`getWaitingPatients` **copies the heap** and pops the copy n times. The real
queue is untouched. A sorted copy of a vector would cost the same.

---

## G. Testing and quality

### 30. How did you test the system?
Seven C++ test programs under `tests/`, run by `make test`: Patient, priority
queue, registration, serving/search/cancellation, statistics, persistence, and
whole-application integration (driving `run()` with scripted input). Total: 392
checks at the time of writing (384 from earlier stages plus 8 added in Stage 6),
all passing. They include the ten scenarios in the project brief, malformed-file
tests, failure injection (unwritable paths), and randomized tests against a
simple reference model.

### 31. Do the tests overwrite real patient data?
No. Tests that use files create a uniquely named folder in the system temporary
directory and delete it afterwards. The application suite also compares the real
`data/patients.txt` before and after and checks it is unchanged.

### 32. What is a "reference model" test?
A deliberately simple, obviously correct implementation (a plain list that is
sorted by priority then arrival sequence each time) is run side by side with the
real queue through thousands of random register/serve/cancel operations. After
each step the results must agree. It catches combinations of operations that a
human would not think of.

### 33. What does the program do when input ends unexpectedly (Ctrl-D)?
Each prompt handles end-of-input by cancelling that action safely. Importantly,
**end of input is never treated as consent**: at "Are you sure you want to exit?"
or "Exit anyway and lose the unsaved changes?" an end of input is not taken as
"yes". The program then runs its end-of-input shutdown, which tries to save and
exits with code 2 if changes could not be saved. This is tested.

### 34. What is the exit code 2?
It tells a calling script that the program closed while some changes were
**not** saved (for example a save failure followed by consent to exit, or end of
input). 0 means everything was saved.

---

## H. Limitations, ethics and improvements

### 35. Is this a real triage system?
No. It is an educational simulation. Priority is entered by the person
registering the patient; the program does no clinical assessment. Real triage
uses approved protocols and qualified clinicians.

### 36. What are the main limitations?
Not clinically validated; no ageing (possible starvation of Normal patients); PIN
is not real security; single user (no file locking); whole file rewritten on each
save; plain-text unencrypted storage; no editing of a patient; no timestamps
(arrival is a sequence number); console interface only. See the README.

### 37. How would you improve it?
Add priority ageing or an escalation operation (which needs a heap that supports
decrease-key, or removing and re-inserting); use a database or a binary format
with locking; real authentication and an audit log; timestamps and waiting-time
statistics; a GUI or web front end; a clinically validated triage scale with
input from qualified professionals.

### 38. Why store the sequence number rather than the arrival time?
The project allowed either. A sequence number is exact, cannot repeat and does
not depend on the clock (two patients registered in the same second still have
a strict order, and a changed system clock cannot reorder them). A timestamp
could be stored in addition for reporting.

### 39. What C++17 features did you use and why?
`std::optional` (`findPatient` returns "a patient or nothing"), `std::filesystem`
(folder creation, atomic rename, backup copy), `std::from_chars`-style strict
number parsing, `enum class`, structured use of `constexpr` constants and
`const` correctness. The code compiles with `-Wall -Wextra -Wpedantic -Werror`.

### 40. If your lecturer asks you to change the rule so that equal-priority patients are served newest-first, what changes?
Only the last line of the comparator: `return a.getArrivalSequence() < b.getArrivalSequence();`.
(Then the FIFO tests would, correctly, fail, which shows the tests really check
the rule.)
