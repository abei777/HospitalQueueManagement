// Builds documentation/Project_Report.docx  (run: node documentation/build_report.js from the project root)
const fs = require('fs');
const path = require('path');
const D = require('docx');
const { Document, Packer, Paragraph, TextRun, HeadingLevel, AlignmentType, ImageRun, Table, TableRow, TableCell,
        WidthType, ShadingType, BorderStyle, LevelFormat, PageBreak, Footer, PageNumber, PageOrientation } = D;

const FONT = 'Times New Roman';
const MONO = 'Courier New';
const dia = (f) => fs.readFileSync(path.join(__dirname, 'diagrams', f));

// ---------- helpers ----------
const p = (text, opts = {}) => new Paragraph({
  spacing: { after: 120, line: 300 }, alignment: opts.align || AlignmentType.JUSTIFIED,
  children: runs(text), ...opts.para });
function runs(text) {           // **bold** and `code` inline
  const out = [];
  text.split(/(\*\*[^*]+\*\*|`[^`]+`)/).forEach(part => {
    if (!part) return;
    if (part.startsWith('**')) out.push(new TextRun({ text: part.slice(2, -2), bold: true, font: FONT, size: 24 }));
    else if (part.startsWith('`')) out.push(new TextRun({ text: part.slice(1, -1), font: MONO, size: 21 }));
    else out.push(new TextRun({ text: part, font: FONT, size: 24 }));
  });
  return out;
}
const h1 = (t) => new Paragraph({ heading: HeadingLevel.HEADING_1, children: [new TextRun(t)], pageBreakBefore: true });
const h1nb = (t) => new Paragraph({ heading: HeadingLevel.HEADING_1, children: [new TextRun(t)] });
const h2 = (t) => new Paragraph({ heading: HeadingLevel.HEADING_2, children: [new TextRun(t)] });
const bullets = (items) => items.map(t => new Paragraph({
  numbering: { reference: 'bul', level: 0 }, spacing: { after: 80, line: 290 }, children: runs(t) }));
const numbered = (items, ref) => items.map(t => new Paragraph({
  numbering: { reference: ref, level: 0 }, spacing: { after: 80, line: 290 }, children: runs(t) }));
const code = (text) => text.split('\n').map((line, i, arr) => new Paragraph({
  keepNext: i < arr.length - 1, keepLines: true, spacing: { after: 0, line: 240 }, shading: { type: ShadingType.CLEAR, fill: 'F2F2F2' },
  children: [new TextRun({ text: line === '' ? ' ' : line, font: MONO, size: 18 })] }));
const gap = () => new Paragraph({ spacing: { after: 120 }, children: [] });
const caption = (t) => new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 200 },
  children: [new TextRun({ text: t, italics: true, font: FONT, size: 22 })] });
const img = (file, w, h) => new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 80 },
  children: [new ImageRun({ type: 'png', data: dia(file), transformation: { width: w, height: h },
    altText: { title: file, description: file, name: file } })] });

const border = { style: BorderStyle.SINGLE, size: 4, color: '808080' };
const borders = { top: border, bottom: border, left: border, right: border };
function table(widths, header, rows) {
  const total = widths.reduce((a, b) => a + b, 0);
  const cell = (t, i, head) => new TableCell({
    width: { size: widths[i], type: WidthType.DXA }, borders,
    shading: head ? { type: ShadingType.CLEAR, fill: 'D9E2F3' } : undefined,
    margins: { top: 60, bottom: 60, left: 100, right: 100 },
    children: [new Paragraph({ spacing: { after: 0 }, children: head
      ? [new TextRun({ text: t, bold: true, font: FONT, size: 21 })]
      : t.split(/(`[^`]+`)/).filter(Boolean).map(s => s.startsWith('`')
          ? new TextRun({ text: s.slice(1, -1), font: MONO, size: 19 })
          : new TextRun({ text: s, font: FONT, size: 21 })) })] });
  return new Table({ width: { size: total, type: WidthType.DXA }, columnWidths: widths,
    rows: [new TableRow({ tableHeader: true, children: header.map((t, i) => cell(t, i, true)) }),
           ...rows.map(r => new TableRow({ children: r.map((t, i) => cell(t, i, false)) }))] });
}

// ---------- content ----------
const title = [
  new Paragraph({ spacing: { before: 1400, after: 300 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: 'HOSPITAL QUEUE MANAGEMENT SYSTEM', bold: true, font: FONT, size: 40 })] }),
  new Paragraph({ spacing: { after: 300 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: 'WITH A PRIORITY QUEUE', bold: true, font: FONT, size: 40 })] }),
  new Paragraph({ spacing: { after: 600 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: 'An Object-Oriented C++17 Implementation', italics: true, font: FONT, size: 28 })] }),
  new Paragraph({ spacing: { after: 100 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: 'A Project Report', font: FONT, size: 26 })] }),
  new Paragraph({ spacing: { after: 800 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: 'Submitted in partial fulfilment of the requirements of [COURSE NAME AND CODE]', font: FONT, size: 24 })] }),
  ...[['Student name', '[STUDENT NAME]'], ['Registration / index number', '[REGISTRATION NUMBER]'],
      ['Programme / level', '[PROGRAMME], Level 400'], ['Department', '[DEPARTMENT]'],
      ['Institution', '[INSTITUTION]'], ['Supervisor / lecturer', '[SUPERVISOR NAME]'],
      ['Date of submission', '[DATE]']].map(([k, v]) => new Paragraph({ spacing: { after: 120 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: k + ': ', bold: true, font: FONT, size: 24 }), new TextRun({ text: v, font: FONT, size: 24 })] })),
  new Paragraph({ spacing: { before: 700 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: 'The bracketed items above are placeholders to be completed by the student. They have not been filled in by the author of this document.', italics: true, font: FONT, size: 20 })] }),
  new Paragraph({ spacing: { before: 400 }, alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: 'Educational simulation: this project is not a clinically validated triage system.', bold: true, font: FONT, size: 22 })] }),
];

const contentsList = ['Abstract', '1. Introduction', '2. Background', '3. Problem Statement', '4. Aim and Objectives', '5. Scope',
  '6. Significance of the Project', '7. Literature and Technical Background', '8. System Analysis and Requirements',
  '9. System Design', '10. Implementation', '11. Testing and Results', '12. Complexity Analysis', '13. Limitations',
  '14. Conclusion', '15. Recommendations', '16. References'];

const front = [
  h1('Table of Contents'),
  ...contentsList.map(t => new Paragraph({ spacing: { after: 80 }, children: [new TextRun({ text: t, font: FONT, size: 24 })] })),
  gap(),
  p('Section numbering follows the required report outline; the Title Page precedes this list and the References close the report.', { para: {} }),

  h1('Abstract'),
  p('This report describes the design, implementation and testing of a Hospital Queue Management System written in C++17. The system manages patients waiting for medical attention and serves them according to the seriousness of their condition rather than their arrival time. Three priority levels are used: Emergency (1), Urgent (2) and Normal (3). A `std::priority_queue` with a custom comparator orders patients by priority and, within a priority level, by arrival sequence, so that equal-priority patients are served first-in, first-out.'),
  p('Around the queue the system provides patient registration with automatically generated identifiers, serving the next patient, listing waiting patients in serving order, search by identifier, authorized cancellation using lazy deletion, statistics, and persistent storage in a validated, versioned text file that is written atomically. The program is organised into four classes with separate responsibilities (Patient, HospitalQueue, PatientStorage and HospitalManagementSystem) and a menu-driven console interface.'),
  p('The implementation was verified by an automated suite of 392 checks in seven test programs, all of which passed at the time of writing, including randomized comparisons against a reference model, failure-injection tests for file errors, and integration tests that drive the whole menu loop. The work is an educational simulation; it is not a clinically validated triage system.'),
  p('**Keywords:** priority queue, binary heap, custom comparator, lazy deletion, object-oriented programming, C++17, file persistence.', { align: AlignmentType.LEFT }),

  h1('1. Introduction'),
  p('Queues are among the first data structures a computer science student meets. A plain queue serves items strictly in the order in which they arrive. Many real situations need a different rule: the next item to be served should be the most important one, whatever its arrival time. Hospital waiting areas are a familiar example. A patient with a life-threatening condition should not wait behind a patient with a minor complaint merely because the second patient arrived earlier.'),
  p('This project applies the priority queue abstract data type to a simplified hospital waiting scenario. It combines a data structure (a binary heap accessed through `std::priority_queue`), an algorithmic rule (priority first, arrival order second), object-oriented design, input validation, error handling and file handling in one working program.'),
  p('**Important notice.** The system is an educational queue-management simulation. It is not a clinically validated triage system. Real hospital triage requires approved medical protocols and qualified healthcare professionals. In the program the priority is chosen by the person who registers the patient; the software performs no medical assessment.'),

  h1('2. Background'),
  p('In emergency care, triage is the process of deciding the order in which patients receive attention, based on how urgent their condition is. Structured triage scales exist in clinical practice and are applied by trained staff. This project does not implement, copy or approximate any clinical triage scale. It uses three abstract levels (Emergency, Urgent, Normal) defined by the project specification only so that the queue behaviour can be studied.'),
  p('From the computing side, scheduling items by priority is a standard problem. Operating systems order processes by priority, networks order packets, and event-driven simulations order events by time. The usual data structure for repeatedly extracting the highest-priority item is the heap, which gives constant-time access to the top item and logarithmic-time insertion and removal.'),

  h1('3. Problem Statement'),
  p('A first-come, first-served queue treats all patients alike. When patients differ in urgency this is unsafe: serious cases may wait behind minor ones. A solution is needed that (a) always selects the most urgent waiting patient, (b) treats patients of equal urgency fairly by arrival order, (c) lets authorized staff remove a patient who is still waiting without disturbing the ordering of the others, (d) can find any patient by identifier and report overall statistics, and (e) does not lose its records when the program is closed. The solution must validate input, handle an empty queue and file errors safely, and be testable.'),

  h1('4. Aim and Objectives'),
  p('**Aim.** To design and implement a console-based Hospital Queue Management System in C++17 that serves patients by medical priority, using a priority queue, and to demonstrate sound object-oriented design, algorithmic reasoning and verification.'),
  p('**Objectives.** The project set out to:', { para: {} }),
  ...numbered([
    'implement a priority queue with a custom comparator ranking Emergency before Urgent before Normal;',
    'preserve arrival (FIFO) order among patients of the same priority;',
    'register patients with automatically generated unique identifiers and arrival sequence numbers;',
    'serve, list, search for, and cancel patients, and report statistics, with status tracking (Waiting, Served, Cancelled);',
    'save patient records to a file and restore them on the next start-up, protecting existing data from accidental loss;',
    'validate all input and handle errors, including an empty queue, invalid menu choices and file problems, without crashing;',
    'organise the code in classes with clear responsibilities, using encapsulation and constructors that enforce validity;',
    'build a repeatable automated test suite and document the system, its algorithms and their complexity.'], 'obj'),

  h1('5. Scope'),
  p('**Within scope:** a single-user console program; one waiting queue; three priority levels; registration, serving, listing, search by identifier, cancellation by authorized staff, statistics; text-file persistence; automated tests; documentation.'),
  p('**Outside scope:** clinical decision support or any medical assessment; multiple users or concurrent access to the data file; a graphical or web interface; databases; real authentication, encryption or audit logging; editing a patient\'s details or escalating a waiting patient\'s priority; recording clock times; integration with real hospital systems. No real patient data should be entered into the system.'),

  h1('6. Significance of the Project'),
  p('The project shows, in a form small enough to understand completely, how an abstract data type is turned into a dependable program. It makes the following points concrete: why a priority queue is preferable to an ordinary queue when items differ in importance; how a comparator defines an ordering; how a unique tiebreaker (the arrival sequence) turns a heap, which is not stable, into a structure with exact FIFO behaviour inside each level; how an operation that the standard container lacks (removing an arbitrary element) can be provided by lazy deletion; and how data can be stored safely in a file. It also illustrates software-engineering practice: separation of responsibilities, validation, failure handling and automated testing.'),

  h1('7. Literature and Technical Background'),
  h2('7.1 Priority queues and heaps'),
  p('A priority queue is an abstract data type supporting insertion of an element and extraction of the element with the highest priority. A binary heap implements it with O(log n) insertion and removal and O(1) access to the top element (Cormen et al., 2022). The C++ standard library provides `std::priority_queue`, a container adaptor, by default over `std::vector`, which keeps the vector arranged as a binary max-heap with respect to a comparison function (ISO/IEC 14882:2017; cppreference.com).'),
  h2('7.2 Comparators and strict weak ordering'),
  p('`std::priority_queue` takes a comparison type `Compare`. If `Compare(a, b)` returns true, `a` is considered to have lower priority than `b` and will be extracted later. The comparison must be a strict weak ordering. In this project the comparator orders by `(priority number, arrival sequence)`, which is a strict total order because arrival sequences are unique.'),
  h2('7.3 Lazy deletion'),
  p('A binary heap supports efficient removal only of its top element. When arbitrary removal is required and the container does not offer it, a standard technique is lazy deletion: the element is marked as deleted in a separate structure and is discarded when it reaches the top. The project uses this technique together with a class invariant that the top is never a deleted element.'),
  h2('7.4 Object-oriented design and C++17 facilities'),
  p('The program uses classes with private data and validating constructors, composition, enumeration classes, and exceptions for violations of a class\'s rules. C++17 facilities used include `std::optional`, `std::filesystem` (directory creation, atomic rename and file copy) and `std::from_chars` for strict number parsing (Stroustrup, 2022; ISO/IEC 14882:2017).'),

  h1('8. System Analysis and Requirements'),
  h2('8.1 Functional requirements'),
  table([900, 8460], ['ID', 'Requirement'], [
    ['F1', 'Register a patient: name, age, gender, condition, priority; generate a unique ID; record arrival sequence.'],
    ['F2', 'Insert each registered patient automatically into the priority queue.'],
    ['F3', 'Serve the next patient: highest priority, FIFO within a level; remove from the queue; show details; message if the queue is empty.'],
    ['F4', 'Display all waiting patients in the order they will be served, showing ID, name, age, condition, priority and arrival order.'],
    ['F5', 'Search by patient ID and show details and queue status.'],
    ['F6', 'Cancel a waiting patient\'s registration (authorized staff only); never cancel a served patient.'],
    ['F7', 'Show statistics: total registered, served, waiting, and waiting per priority level.'],
    ['F8', 'Save records to a file and load them at start-up so data survives between sessions.'],
    ['F9', 'Provide the eight-item menu and repeat until the user chooses Exit.'],
  ]),
  gap(),
  h2('8.2 Non-functional requirements'),
  ...bullets([
    '**Correctness:** the serving order must always follow the priority and FIFO rules.',
    '**Robustness:** invalid input, an empty queue, end of input and file errors must never crash the program or silently destroy data.',
    '**Maintainability:** clear class responsibilities, meaningful names, comments, no unnecessary global variables.',
    '**Portability:** standard C++17 only, compiled with `-Wall -Wextra -Wpedantic -Werror`.',
    '**Testability:** the user-interface layer reads and writes through stream parameters so that scripted input can be used.']),
  h2('8.3 Priority levels'),
  table([1700, 1500, 6160], ['Level', 'Number', 'Meaning (as defined by the project)'], [
    ['Emergency', '1', 'Critical or life-threatening conditions; served first.'],
    ['Urgent', '2', 'Serious conditions needing prompt attention.'],
    ['Normal', '3', 'Stable conditions that can safely wait.'],
  ]),
  gap(),
  p('These definitions come from the project brief; they are labels for the simulation, not clinical criteria.'),

  h1('9. System Design'),
  h2('9.1 Architecture and class responsibilities'),
  p('The program has four classes in a layered arrangement. `main` creates one `HospitalManagementSystem`, which owns a `HospitalQueue`; the system uses a `PatientStorage` object whenever it saves or loads. `HospitalQueue` contains no console or file code, so it can be tested alone.'),
  table([3200, 6160], ['Class', 'Responsibility'], [
    ['`Patient`', 'Immutable record of one patient (ID, name, age, gender, condition, priority, arrival sequence). The constructor validates and throws `std::invalid_argument` on invalid data. Only getters are provided.'],
    ['`HospitalQueue`', 'Owns the `std::priority_queue` (with the private `PatientComparator`) and the `std::map` registry of `PatientRecord` (patient plus status). Registration, serving, cancellation, search, listing, statistics, export and import of records.'],
    ['`PatientStorage`', 'Reads and writes the data file: versioned format, escaping, atomic save, strict validation on load, backup copy. Returns result objects with messages instead of throwing for expected problems.'],
    ['`HospitalManagementSystem`', 'The application layer: welcome screen, menu loop, prompts with validation, staff PIN, confirmations, automatic saving, shutdown rules and exit codes.'],
  ]),
  gap(),
  h2('9.2 Key design decisions'),
  ...bullets([
    '**Registry beside the heap.** A heap cannot look up a patient by ID. A `std::map<int, PatientRecord>` stores every patient with a status (Waiting, Served, Cancelled), supports search in O(log N), detects duplicate IDs, and is what is saved to the file.',
    '**Immutable patients.** If a patient could be modified while a copy sits in the heap, the heap order could silently become wrong. Status therefore lives in the registry, not in the `Patient`.',
    '**Lower number, higher urgency.** The enum values 1, 2, 3 follow the specification. Because `std::priority_queue` is a max-heap, the comparator returns true when the first patient should be served after the second.',
    '**Unique arrival sequence.** The sequence is the tiebreaker that gives exact FIFO inside a priority level.',
    '**Validate, then commit.** A failed registration consumes no ID or sequence number and leaves the queue unchanged.',
    '**Safe persistence.** Atomic write, strict all-or-nothing load, protection of a damaged file from being overwritten, and a backup copy before deliberate replacement.']),
  h2('9.3 The comparator'),
  ...code(`struct PatientComparator {
    bool operator()(const Patient& a, const Patient& b) const {
        if (a.getPriority() != b.getPriority()) {
            return static_cast<int>(a.getPriority()) >
                   static_cast<int>(b.getPriority());
        }
        return a.getArrivalSequence() > b.getArrivalSequence();
    }
};`),
  gap(),
  p('The function answers the question "should `a` be served after `b`?". A larger priority number (less urgent) is served later; for equal priorities the later arrival is served later. The top of the heap is therefore the most urgent patient who arrived earliest among patients of that urgency.'),
  h2('9.4 Patient status and lazy deletion'),
  p('A patient is Waiting after registration. Serving makes the patient Served. Cancelling makes the patient Cancelled. Served and Cancelled are final; the program refuses to change them. Because the standard priority queue cannot remove an arbitrary element, cancelling only changes the status in the registry and leaves a "ghost" copy in the heap. The class invariant is that the top of the heap is always a genuine Waiting patient. The private function `discardCancelledFromTop()` restores the invariant after every serve and every cancellation by popping ghosts from the top. Ghosts deeper in the heap are skipped when the waiting list is built and are removed when they reach the top.'),
  h2('9.5 Data file format'),
  ...code(`HQMS-PATIENT-RECORDS<TAB>1
<id><TAB><name><TAB><age><TAB><gender><TAB><condition><TAB><priority><TAB><arrival sequence><TAB><status>
...
END<TAB><number of patient lines>`),
  gap(),
  p('Backslash, tab, newline and carriage return in text fields are escaped. The header carries a format name and version and the footer a record count, so that wrong or truncated files are detected.'),
  h2('9.6 UML class diagram'),
  p('Figure 1 (next page) shows the classes, structures and enumerations that exist in the source code and their relationships: `HospitalManagementSystem` owns a `HospitalQueue` (composition); `HospitalQueue` contains `PatientRecord` entries and holds copies of `Patient` objects in its heap; `HospitalManagementSystem` depends on `PatientStorage`, which reads and writes `PatientRecord` values. There is no inheritance.'),
];

const landscape = [
  img('class_diagram.png', 880, 430),
  caption('Figure 1: UML class diagram (generated from documentation/diagrams/class_diagram.mmd).'),
  new Paragraph({ children: [new PageBreak()] }),
  img('system_flowchart.png', 600, 583),
  caption('Figure 2: System flowchart: start-up, menu loop, automatic saving and shutdown (documentation/diagrams/system_flowchart.mmd).'),
];

const rest = [
  h2('9.7 Flowcharts for saving and loading'),
  p('Figure 2 (previous page) shows the start-up, menu, saving and shutdown flow. Figures 3 and 4 show the file operations.'),
  img('save_flowchart.png', 250, 410),
  caption('Figure 3: Saving: write to a temporary file, then rename.'),
  img('load_flowchart.png', 190, 355),
  caption('Figure 4: Loading: strict, all-or-nothing validation.'),

  h1('10. Implementation'),
  h2('10.1 Development method'),
  p('The system was built in six stages, each compiled and tested before the next: (1) class structure and priority queue; (2) registration and insertion; (3) serving, searching, cancellation and display; (4) statistics and file persistence; (5) the menu and integration; (6) documentation and the permanent test suite. Source code is compiled with `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror`.'),
  h2('10.2 Source files'),
  table([3700, 5660], ['File', 'Contents'], [
    ['`Patient.h`, `Patient.cpp`', '`PriorityLevel`, helper conversions, the `Patient` class'],
    ['`HospitalQueue.h`, `HospitalQueue.cpp`', '`PatientStatus`, `CancelResult`, `PatientRecord`, `QueueStatistics`, the `HospitalQueue` class and its comparator'],
    ['`PatientStorage.h`, `PatientStorage.cpp`', 'File format, save, load, backup copy'],
    ['`HospitalManagementSystem.h`, `.cpp`', 'Menu, prompts, authorization, auto-save, shutdown logic'],
    ['`main.cpp`', 'Creates the system and runs it with `std::cin` and `std::cout`'],
    ['`Makefile`', 'Targets: all, run, test, clean'],
  ]),
  gap(),
  h2('10.3 Registration'),
  p('The user interface collects and validates all five fields (re-prompting on blank text, non-numeric or out-of-range age or priority) before it calls `HospitalQueue::registerPatient`. That function trims the text, validates it, builds a `Patient` with the next ID and the next arrival sequence, and calls `insertPatient`. `insertPatient` adds the patient to the registry (failing on a duplicate ID), pushes the patient onto the heap, increments the waiting counter and advances the generators. Generators advance only after success, so failures consume nothing.'),
  h2('10.4 Serving and listing'),
  p('`popNext` reads the heap top, removes it, marks the registry entry Served, updates the counters and calls `discardCancelledFromTop`. The interface shows the patient\'s details and the number still waiting. To list the waiting patients in serving order without changing the queue, `getWaitingPatients` copies the heap, pops the copy until empty and keeps entries whose registry status is Waiting.'),
  h2('10.5 Search'),
  p('Search looks the identifier up in the registry. A found patient is shown with details and status; a waiting patient is also shown with the queue position ("k of n"), calculated from the ordered waiting list. A missing identifier produces a clear message.'),
  h2('10.6 Cancellation'),
  p('The menu asks for a staff PIN (three attempts) before it asks which patient to cancel. Served and already cancelled patients are refused immediately. For a waiting patient the details are shown and an explicit "yes" is required. The PIN is a classroom demonstration of authorization, not real security. End of input is never accepted as confirmation.'),
  h2('10.7 Statistics'),
  p('The totals of registered, served, cancelled and waiting patients come from counters; the numbers of waiting Emergency, Urgent and Normal patients are obtained by scanning the registry. `QueueStatistics::isConsistent()` checks that the parts add up to the totals.'),
  h2('10.8 Persistence and error handling'),
  p('Records are saved after each successful registration, serve or cancellation, on request (menu option 7) and on exit. Saving writes a temporary file and renames it. Loading validates the whole file and rejects it as a whole if any problem is found. If the data file is damaged the program starts with an empty registry, leaves the file untouched and blocks saving; menu option 7 can replace it after making a `.bak` copy, but only on an explicit "yes". If a save fails, the data stay in memory, a warning is shown, and the program never claims a save that did not happen. When the program must close with unsaved changes it exits with code 2.'),
  p('Error handling follows a consistent pattern: classes report violations of their rules with exceptions (`std::invalid_argument`, `std::out_of_range`); the interface validates input before calling them and turns exceptions into messages; the storage class returns result objects for expected file problems.'),
  h2('10.9 Menu'),
  ...code(`========== HOSPITAL QUEUE MANAGEMENT SYSTEM ==========

1. Register New Patient
2. View All Waiting Patients
3. Serve Next Patient
4. Search Patient
5. Cancel Patient Registration
6. View Queue Statistics
7. Save Patient Records
8. Exit System

Enter your choice:`),
  gap(),
  p('The brief names option 2 "View Waiting Patients"; the program shows "View All Waiting Patients". The other items are as specified.'),
  h2('10.10 Sample output'),
  p('Three patients were registered in the order Ama Mensah (Emergency), Kwame Asante (Normal), Efua Owusu (Urgent); the names are invented. Option 2 displayed:'),
  ...code(`========== WAITING PATIENTS (in serving order) ==========
Order  ID    Name                   Age  Gender     Priority   Arrival  Condition
--------------------------------------------------------------------------------------
1      1     Ama Mensah             41   Female     Emergency  1        Chest pain
2      3     Efua Owusu             25   Female     Urgent     3        Fracture
3      2     Kwame Asante           30   Male       Normal     2        Fever
Total waiting: 3`),
  gap(),

  h1('11. Testing and Results'),
  h2('11.1 Approach'),
  p('Testing is automated and repeatable. From the project root the single command `make test` builds seven test programs and runs them. Each prints `[PASS]` or `[FAIL]` for every check and a summary line; the Makefile adds a grand total and returns a failure status if any check fails. The tests use only temporary folders created in the operating system\'s temporary directory and never touch the real `data/patients.txt`; one test confirms this by comparing the real file before and after. The user-interface tests drive the complete `run()` menu loop with scripted input.'),
  h2('11.2 Results'),
  p('The table gives the result of `make test` at the time of writing (g++ 13.3, Linux).'),
  table([3300, 4560, 1500], ['Test suite', 'Area', 'Result'], [
    ['`test_patient`', 'Patient construction, validation, priority helpers', '10 passed'],
    ['`test_priority_queue`', 'Comparator, priority order, FIFO, empty queue', '11 passed'],
    ['`test_registration`', 'IDs, sequences, validation, failure safety, duplicates, dialogue', '54 passed'],
    ['`test_serving_search_cancel`', 'Serving, display, search, cancellation, lazy deletion, 4000-operation model test', '77 passed'],
    ['`test_statistics`', 'Counts per status and priority, 3000-operation model test', '8 passed'],
    ['`test_persistence`', 'File format, round trips, escaping, malformed files, failure injection', '123 passed'],
    ['`test_application`', 'Whole menu loop, PIN, EOF handling, failures, corrupted files, restarts', '109 passed'],
    ['TOTAL', '', '392 passed, 0 failed'],
  ]),
  gap(),
  p('The earlier development stages produced 384 passing checks. All were retained when the tests were reorganised by topic. Eight checks were added in the final stage: end of input at the exit confirmation and at the "exit anyway" prompt must not authorize discarding unsaved changes, and the real data file must not be touched.'),
  h2('11.3 The ten required scenarios'),
  table([500, 4400, 4460], ['#', 'Scenario', 'Verified result'], [
    ['1', 'Register three patients with different priorities', 'IDs 1-3 and sequences 1-3 assigned automatically; all queued.'],
    ['2', 'Several patients with the same priority', 'Unique IDs; increasing arrival sequences.'],
    ['3', 'Emergency served first', 'An Emergency patient who arrived last is served first.'],
    ['4', 'Urgent before Normal', 'Order is Emergency, Urgent, Normal.'],
    ['5', 'Equal priority in arrival order', 'Within each level patients are served in arrival order.'],
    ['6', 'Serve when the queue is empty', 'Message "There are no patients waiting"; no change.'],
    ['7', 'Search existing and nonexistent patient', 'Details and status for an existing ID; "No patient with ID ... exists" otherwise.'],
    ['8', 'Cancel a waiting patient', 'Status Cancelled; not listed; never served; served patients cannot be cancelled.'],
    ['9', 'Save, restart, persistence', 'Identical records and serving order after restart; ID and sequence continue.'],
    ['10', 'Invalid menu selections', 'Message and menu shown again; no state change.'],
  ]),
  gap(),
  h2('11.4 Evidence that the tests detect faults'),
  p('As a check on the tests themselves, the last line of the comparator was temporarily changed so that later arrivals are served first within a priority level. Several suites then reported failures (for example 15 failures in the serving suite and two in the priority-queue suite), and the original code passes all tests. This shows that the FIFO rule is actually being tested.'),
  h2('11.5 Limits of the testing'),
  p('The tests show that the program behaves as described for the inputs tested, on Linux with the compilers used. They do not prove correctness for every possible input, do not measure running time, and do not cover Windows console behaviour.'),

  h1('12. Complexity Analysis'),
  p('Let n be the number of entries in the heap (waiting patients plus ghosts of cancelled patients) and N the number of patients ever registered, which is the size of the registry.'),
  table([3200, 1700, 4460], ['Operation', 'Time', 'Reason'], [
    ['Insert into heap (`push`)', 'O(log n)', 'Sift-up in a binary heap.'],
    ['Register a patient', 'O(log N)', 'Registry insertion O(log N) plus heap insertion O(log n).'],
    ['View next (`peekNext`)', 'O(1)', 'Reads the top; invariant guarantees it is a Waiting patient.'],
    ['Serve next (`popNext`)', 'O(log n) amortised', 'Heap pop O(log n), registry update O(log N); each ghost is popped at most once.'],
    ['Search by ID', 'O(log N)', '`std::map::find`.'],
    ['Search with queue position', 'O(n log n)', 'Builds the ordered waiting list.'],
    ['Cancel', 'O(log N) plus amortised cleanup', 'Registry lookup and status change; ghost removal at the top.'],
    ['Display waiting patients', 'O(n log n), O(n) space', 'Copy the heap, pop n times.'],
    ['Statistics', 'O(N)', 'Totals are counters; per-priority waiting counts need one registry scan.'],
    ['Save', 'O(N)', 'One line per registered patient.'],
    ['Load', 'O(N log N)', 'Parsing is linear; each record goes into the map and, if Waiting, the heap.'],
  ]),
  gap(),
  p('The costs of the priority queue itself (insert O(log n), view O(1), remove O(log n)) are separate from the cost of the registry and of file operations, which are why registering a patient is quoted as O(log N) overall. Space use is O(N).'),

  h1('13. Limitations'),
  ...bullets([
    'Not clinically validated. Priority is chosen by the user; the program makes no assessment.',
    'Strict priority can starve low-priority patients if higher-priority patients keep arriving; there is no ageing.',
    'The staff PIN is a fixed string in the source code, not real security. There is no encryption or audit log.',
    'Single user. Two programs sharing one data file are not coordinated; the last save wins.',
    'Every save rewrites the whole file; listing the queue copies the heap.',
    'A registered patient cannot be edited and a waiting patient\'s priority cannot be changed.',
    'Arrival is a sequence number, not a clock time.',
    'Console interface only; tested on Linux with g++ and clang++ and not on other platforms.']),

  h1('14. Conclusion'),
  p('The project met its objectives. A priority queue with a custom comparator selects the most urgent patient, and a unique arrival sequence gives exact first-in, first-out order within each priority level. The surrounding system provides registration, serving, listing, search, authorized cancellation, statistics and safe persistence, with consistent validation and error handling. The design choices that the project set out to demonstrate (a registry beside the heap, immutable patients, lazy deletion with an explicit invariant, validate-then-commit registration and atomic file writes) are implemented and are covered by 392 automated checks that pass. The work is an educational simulation and is not suitable for clinical use.'),

  h1('15. Recommendations'),
  ...bullets([
    'Add priority ageing or an explicit escalation operation to reduce the risk of starvation.',
    'Replace the text file by a database or a locked file format to allow several users.',
    'Introduce real authentication, role-based access and an audit trail.',
    'Record timestamps and report waiting times.',
    'Add editing of patient details and a controlled way to change priority.',
    'Provide a graphical or web interface.',
    'If the idea were ever to be used in practice, base the priority rules on an approved clinical triage protocol designed and validated by qualified healthcare professionals.']),

  h1('16. References'),
  p('The sources below are standard references for the technical content. The student should check edition, publisher and page details against the copies used and add any further references required by the institution\'s referencing style.', { align: AlignmentType.LEFT }),
  ...[
    'Cormen, T. H., Leiserson, C. E., Rivest, R. L. and Stein, C. (2022) Introduction to Algorithms. 4th edn. Cambridge, MA: MIT Press. (Chapter on heapsort and priority queues.)',
    'ISO/IEC (2017) ISO/IEC 14882:2017 Programming languages - C++. Geneva: International Organization for Standardization.',
    'Stroustrup, B. (2022) A Tour of C++. 3rd edn. Boston: Addison-Wesley.',
    'cppreference.com. std::priority_queue, std::map, std::optional and std::filesystem reference pages. Available at: https://en.cppreference.com (Accessed: [DATE]).',
  ].map(t => new Paragraph({ spacing: { after: 120 }, alignment: AlignmentType.LEFT,
      indent: { left: 540, hanging: 540 }, children: [new TextRun({ text: t, font: FONT, size: 24 })] })),
];

const footer = new Footer({ children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [
  new TextRun({ text: 'Hospital Queue Management System - educational simulation, not a clinical tool   |   Page ', font: FONT, size: 18 }),
  new TextRun({ children: [PageNumber.CURRENT], font: FONT, size: 18 })] })] });

const portraitPage = { size: { width: 12240, height: 15840 }, margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 } };
const landscapePage = { size: { width: 12240, height: 15840, orientation: PageOrientation.LANDSCAPE },
                        margin: { top: 900, right: 900, bottom: 900, left: 900 } };

const doc = new Document({
  creator: '[STUDENT NAME]', title: 'Hospital Queue Management System with a Priority Queue - Project Report',
  styles: {
    default: { document: { run: { font: FONT, size: 24 } } },
    paragraphStyles: [
      { id: 'Heading1', name: 'Heading 1', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: FONT, size: 32, bold: true }, paragraph: { spacing: { before: 240, after: 200 }, keepNext: true, outlineLevel: 0 } },
      { id: 'Heading2', name: 'Heading 2', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: FONT, size: 26, bold: true }, paragraph: { spacing: { before: 240, after: 120 }, keepNext: true, outlineLevel: 1 } },
    ] },
  numbering: { config: [
    { reference: 'bul', levels: [{ level: 0, format: LevelFormat.BULLET, text: '•', alignment: AlignmentType.LEFT,
        style: { paragraph: { indent: { left: 720, hanging: 360 } } } }] },
    { reference: 'obj', levels: [{ level: 0, format: LevelFormat.DECIMAL, text: '%1.', alignment: AlignmentType.LEFT,
        style: { paragraph: { indent: { left: 720, hanging: 360 } } } }] },
  ] },
  sections: [
    { properties: { page: portraitPage }, footers: { default: footer }, children: [...title, ...front] },
    { properties: { page: landscapePage }, footers: { default: footer }, children: landscape },
    { properties: { page: portraitPage }, footers: { default: footer }, children: rest },
  ],
});

Packer.toBuffer(doc).then(buf => {
  const out = path.join(__dirname, 'Project_Report.docx');
  fs.writeFileSync(out, buf);
  console.log('wrote', out, buf.length, 'bytes');
});
