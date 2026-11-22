// tests/test_application.cpp
// Main-application integration tests. Every test drives the COMPLETE
// application loop, HospitalManagementSystem::run(), with scripted keyboard
// input, exactly as main() does with std::cin / std::cout: menu, staff PIN,
// confirmations, autosave, end-of-input handling, failure paths and recovery.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "HospitalManagementSystem.h"
#include "HospitalQueue.h"
#include "Patient.h"
#include "PatientStorage.h"
#include "test_framework.h"

namespace fs = std::filesystem;
using testfw::check;
using testfw::contains;
using testfw::countOf;
using testfw::linesOf;
using testfw::readFile;
using testfw::writeFile;

// Every file this test creates lives inside a temporary folder (deleted on
// exit). The project's real data/patients.txt is never used.
static fs::path workDir;

// Snapshot of the real data file (if any), compared at the end to prove untouched.
static std::string snapshotRealData() {
    const fs::path real = fs::path("data") / "patients.txt";
    return fs::exists(real) ? "present:" + readFile(real) : "absent";
}

// ---- sessions ---------------------------------------------------------
struct Session {
    int         code;
    std::string out;
    bool        unsaved;
};

[[maybe_unused]] static const std::string PIN = HospitalManagementSystem::DEFAULT_STAFF_PIN;

[[maybe_unused]] static Session runSession(const fs::path& dataFile, const std::string& script,
                          const std::string& pin = HospitalManagementSystem::DEFAULT_STAFF_PIN) {
    HospitalManagementSystem sys(dataFile.string(), pin);
    std::istringstream in(script);
    std::ostringstream out;
    const int code = sys.run(in, out);
    return {code, out.str(), sys.hasUnsavedChanges()};
}

[[maybe_unused]] static std::string reg(const std::string& name, int age, const std::string& gender,
                       const std::string& cond, int prio) {
    return "1\n" + name + "\n" + std::to_string(age) + "\n" + gender + "\n" + cond + "\n" +
           std::to_string(prio) + "\n";
}
[[maybe_unused]] static std::string serve() { return "3\n"; }
[[maybe_unused]] static std::string search(int id) { return "4\n" + std::to_string(id) + "\n"; }
[[maybe_unused]] static std::string cancelYes(int id) { return "5\n" + PIN + "\n" + std::to_string(id) + "\nyes\n"; }
[[maybe_unused]] static const std::string EXIT_SCRIPT = "8\nyes\n";

[[maybe_unused]] static std::vector<int> servedIds(const std::string& text) {
    const std::string marker = "========== NOW SERVING ==========\nPatient ID        : ";
    std::vector<int> ids;
    for (std::size_t pos = text.find(marker); pos != std::string::npos; pos = text.find(marker, pos)) {
        pos += marker.size();
        ids.push_back(std::stoi(text.substr(pos)));
    }
    return ids;
}

// The waiting-list table: from the heading to the "Total waiting" line.
[[maybe_unused]] static std::string lastTable(const std::string& text) {
    const std::size_t start = text.rfind("WAITING PATIENTS");
    if (start == std::string::npos) return "";
    const std::size_t end = text.find("Total waiting:", start);
    return end == std::string::npos ? "" : text.substr(start, end - start);
}

[[maybe_unused]] static std::vector<PatientRecord> loadRecords(const fs::path& path) {
    return PatientStorage(path).load().records;
}

[[maybe_unused]] static const std::string HEADER = "HQMS-PATIENT-RECORDS\t1\n";
[[maybe_unused]] static const std::string GOOD1  = "1\tAma Mensah\t41\tFemale\tChest pain\t1\t1\tWaiting\n";


int main() {
    const std::string realDataBefore = snapshotRealData();
    testfw::TempDir temp("application");
    workDir = temp.path();

    // =====================================================================
    std::cout << "--- 1 + 17: first-time startup with no data file; empty queue ---\n";
    {
        const fs::path file = workDir / "s1" / "data" / "patients.txt";   // data/ does not exist
        const Session s = runSession(file, "2\n3\n4\n999\n6\n" + EXIT_SCRIPT);
        check(contains(s.out, "HOSPITAL QUEUE MANAGEMENT SYSTEM") && contains(s.out, "educational queue-management simulation"),
              "Welcome screen shown, with the educational-simulation notice");
        check(contains(s.out, "---------- STARTUP ----------") && contains(s.out, "No saved data file found") &&
              contains(s.out, "Starting with an empty registry"), "Startup message: no data file, starting empty");
        check(contains(s.out, "1. Register New Patient") && contains(s.out, "2. View All Waiting Patients") &&
              contains(s.out, "3. Serve Next Patient") && contains(s.out, "4. Search Patient") &&
              contains(s.out, "5. Cancel Patient Registration") && contains(s.out, "6. View Queue Statistics") &&
              contains(s.out, "7. Save Patient Records") && contains(s.out, "8. Exit System") &&
              contains(s.out, "Enter your choice: "), "All 8 menu items and the prompt are displayed");
        check(contains(s.out, "========== HOSPITAL QUEUE MANAGEMENT SYSTEM ==========\n\n1. Register New Patient"),
              "Menu heading matches the specification");
        check(contains(s.out, "No patients are currently waiting."), "Empty queue: waiting list message");
        check(contains(s.out, "There are no patients waiting. The queue is empty."), "Empty queue: serve message");
        check(contains(s.out, "No patient with ID 999 exists."), "Searching an empty registry is handled");
        check(contains(s.out, "Total patients registered   : 0") && contains(s.out, "Patients currently waiting  : 0"),
              "Empty queue: statistics are zero");
        check(contains(s.out, "Are you sure you want to exit?") && contains(s.out, "Goodbye."), "Exit asks for confirmation, then says goodbye");
        check(s.code == 0 && !s.unsaved, "Clean exit code 0");
        check(countOf(s.out, "Saved ") == 0 && !fs::exists(file) && !fs::exists(file.parent_path()),
              "Nothing changed, so nothing was saved and no data folder was created");
    }

    // =====================================================================
    std::cout << "--- 2-11: register, display, serve, search, cancel, statistics, save ---\n";
    const fs::path stdFile = workDir / "std" / "data" / "patients.txt";
    std::string firstTable;
    {
        std::string script;
        script += reg("Normal-A", 30, "Female", "Cold", 3);           // 1
        script += reg("Urgent-A", 40, "Male", "Fracture", 2);         // 2
        script += reg("Emergency-A", 50, "Female", "Chest pain", 1);  // 3
        script += reg("Normal-B", 20, "Male", "Sprain", 3);           // 4
        script += reg("Urgent-B", 60, "Female", "Burn", 2);           // 5
        script += reg("Emergency-B", 10, "Male", "Bleeding", 1);      // 6
        script += reg("Normal-C", 35, "Female", "Rash", 3);           // 7
        script += "2\n";                                              // display all 7
        script += serve();                                            // serves 3
        script += cancelYes(5);
        script += cancelYes(7);
        script += "2\n";                                              // display the remaining 4
        script += search(1) + search(3) + search(5) + search(99);
        script += "6\n7\n" + EXIT_SCRIPT;
        const Session s = runSession(stdFile, script);

        check(contains(s.out, "Patient ID       : 1") && contains(s.out, "Patient ID       : 7") &&
              contains(s.out, "Arrival Sequence : 1") && contains(s.out, "Arrival Sequence : 7") &&
              !contains(s.out, "Patient ID       : 8"), "Automatic IDs and arrival sequences run 1 to 7");
        check(contains(s.out, "========== REGISTRATION SUCCESSFUL ==========") && contains(s.out, "registered successfully"),
              "Registration confirmations shown");

        // The first display lists all 7 waiting patients in serving order.
        const std::size_t firstDisplay = s.out.find("WAITING PATIENTS");
        const std::string firstList = s.out.substr(firstDisplay, s.out.find("Total waiting:", firstDisplay) - firstDisplay);
        const char* order[] = {"Emergency-A", "Emergency-B", "Urgent-A", "Urgent-B", "Normal-A", "Normal-B", "Normal-C"};
        bool inOrder = true;
        std::size_t last = 0;
        for (const char* name : order) {
            const std::size_t pos = firstList.find(name);
            if (pos == std::string::npos || pos < last) inOrder = false;
            last = pos;
        }
        check(inOrder && contains(s.out, "Total waiting: 7"), "Waiting list displays all 7 patients in actual serving order");
        check(contains(firstList, "Emergency") && contains(firstList, "Urgent") && contains(firstList, "Normal") &&
              contains(firstList, "Chest pain"), "Table shows clear priority labels and conditions");

        check(servedIds(s.out) == std::vector<int>{3}, "Serving picked the Emergency patient (ID 3) first, although it arrived third");
        check(contains(s.out, "Status            : Served"), "Serve shows the patient's details and status Served");

        check(contains(s.out, "Registration cancelled for patient ID 5") && contains(s.out, "Registration cancelled for patient ID 7"),
              "Both cancellations confirmed");

        firstTable = lastTable(s.out);
        check(contains(firstTable, "Emergency-B") && contains(firstTable, "Urgent-A") && contains(firstTable, "Normal-A") &&
              contains(firstTable, "Normal-B") && !contains(firstTable, "Urgent-B") && !contains(firstTable, "Normal-C") &&
              !contains(firstTable, "Emergency-A"), "After serving/cancelling, the list shows only the 4 waiting patients");

        check(contains(s.out, "Queue Position    : 3 of 4") && contains(s.out, "Patient ID        : 1\n"),
              "Search of a waiting patient: details and position 3 of 4");
        check(contains(s.out, "Status            : Waiting"), "Search shows status Waiting");
        check(contains(s.out, "Patient ID        : 3\nFull Name         : Emergency-A") && contains(s.out, "Status            : Served"),
              "Search of a served patient shows status Served");
        check(contains(s.out, "Status            : Cancelled"), "Search of a cancelled patient shows status Cancelled");
        check(contains(s.out, "No patient with ID 99 exists."), "Search of a nonexistent ID reports it");

        check(contains(s.out, "Total patients registered   : 7") && contains(s.out, "Patients currently waiting  : 4") &&
              contains(s.out, "Patients served             : 1") && contains(s.out, "Registrations cancelled     : 2") &&
              contains(s.out, "  Emergency (1)             : 1") && contains(s.out, "  Urgent (2)                : 1") &&
              contains(s.out, "  Normal (3)                : 2"), "Statistics are accurate (7 / 4 / 1 / 2; E1 U1 N2)");

        // 7 registrations + 1 serve + 2 cancels + 1 manual save = 11 saves; views/search/stats never save.
        check(countOf(s.out, "Saved ") == 11, "Exactly 11 saves: one per change plus the manual save, none after views/searches");
        check(contains(s.out, "Saved 7 patient record(s)"), "Manual save (option 7) reports the full registry of 7 records");
        check(s.code == 0 && !s.unsaved, "Clean exit, nothing unsaved");

        const std::vector<std::string> lines = linesOf(readFile(stdFile));
        check(lines.size() == 9 && lines.front() == "HQMS-PATIENT-RECORDS\t1" && lines.back() == "END\t7",
              "Data folder was created and the file holds header + 7 records + END");
        check(lines.size() == 9 && contains(lines[3], "\tServed") && contains(lines[5], "\tCancelled") &&
              contains(lines[7], "\tCancelled") && contains(lines[2], "\tWaiting"), "Statuses saved: waiting, served, cancelled");
    }

    // =====================================================================
    std::cout << "--- 12-15: exit, restart, restored records, history ---\n";
    {
        const std::string bytesBefore = readFile(stdFile);
        std::string script = "2\n" + search(3) + search(5) + search(7) + "6\n";
        for (int i = 0; i < 5; ++i) script += serve();           // 4 real patients + 1 on an empty queue
        script += "2\n" + EXIT_SCRIPT;
        const Session s = runSession(stdFile, script);

        check(contains(s.out, "Loaded 7 patient record(s)") && contains(s.out, "4 waiting, 1 served, 2 cancelled"),
              "Restart: startup message confirms 7 records loaded (4 waiting, 1 served, 2 cancelled)");
        const std::size_t firstDisplay = s.out.find("WAITING PATIENTS");
        const std::string restoredTable = s.out.substr(firstDisplay, s.out.find("Total waiting:", firstDisplay) - firstDisplay);
        check(restoredTable == firstTable, "The restored waiting list is identical to the list before exit");
        check(contains(s.out, "Patient ID        : 3\nFull Name         : Emergency-A") && contains(s.out, "Status            : Served"),
              "Served patient is still in the historical records after restart");
        check(contains(s.out, "Patient ID        : 5\nFull Name         : Urgent-B") && contains(s.out, "Patient ID        : 7\nFull Name         : Normal-C") &&
              countOf(s.out, "Status            : Cancelled") == 2, "Cancelled patients are still recorded, with status Cancelled");
        check(servedIds(s.out) == (std::vector<int>{6, 2, 1, 4}),
              "Restarted system serves exactly 6, 2, 1, 4: cancelled patients 5 and 7 never return, served patient 3 is not served again");
        check(contains(s.out, "There are no patients waiting."), "After the last real patient the queue is empty");
        check(countOf(s.out, "Saved ") == 4, "4 serves auto-saved; the failed serve on the empty queue did not save");
        check(s.code == 0, "Clean exit");

        const std::vector<PatientRecord> records = loadRecords(stdFile);
        std::size_t served = 0, cancelled = 0;
        for (const PatientRecord& r : records) {
            if (r.status == PatientStatus::Served) ++served;
            if (r.status == PatientStatus::Cancelled) ++cancelled;
        }
        check(records.size() == 7 && served == 5 && cancelled == 2, "All 7 historical records kept: 5 served, 2 cancelled");

        // Third start: IDs and sequences continue.
        const Session s3 = runSession(stdFile, "6\n" + reg("After Restart", 44, "Male", "Headache", 1) + search(8) + EXIT_SCRIPT);
        check(contains(s3.out, "Patients served             : 5") && contains(s3.out, "Registrations cancelled     : 2") &&
              contains(s3.out, "Patients currently waiting  : 0"), "Third start: statistics restored");
        check(contains(s3.out, "Patient ID       : 8") && contains(s3.out, "Arrival Sequence : 8"),
              "New patient after restarts gets ID 8 and sequence 8 (no reuse)");
        check(bytesBefore != readFile(stdFile), "File changed only because of real changes");
    }

    // =====================================================================
    std::cout << "--- 8 + 9: authorized and unauthorized cancellation ---\n";
    {
        const fs::path file = workDir / "auth" / "patients.txt";
        runSession(file, reg("Alice", 30, "F", "Cold", 3) + reg("Bob", 40, "M", "Cough", 2) + EXIT_SCRIPT);
        const std::string bytes = readFile(file);

        // Wrong PIN three times.
        Session s = runSession(file, "5\nwrong\n1234\nnope\n6\n" + EXIT_SCRIPT);
        check(countOf(s.out, "Incorrect PIN") == 3 && contains(s.out, "Access denied: no registration was cancelled."),
              "Three wrong PINs: access denied");
        check(!contains(s.out, "Enter patient ID:") && !contains(s.out, "Cancel this patient's registration?"),
              "Unauthorized user is never asked for a patient ID or shown patient details");
        check(contains(s.out, "Patients currently waiting  : 2") && contains(s.out, "Registrations cancelled     : 0"),
              "Back at the menu: nobody was cancelled");
        check(countOf(s.out, "Saved ") == 0 && readFile(file) == bytes, "Unauthorized attempt triggered no save; the file is unchanged");

        // The explanation of the mechanism is shown.
        check(contains(s.out, "Authorization required") && contains(s.out, "Educational mechanism") && contains(s.out, "staff PIN"),
              "The educational authorization mechanism is explained to the user");

        // Wrong PIN once, then right; PIN with stray spaces accepted.
        s = runSession(file, "5\nbad\n  " + PIN + "  \n2\nyes\n" + EXIT_SCRIPT);
        check(countOf(s.out, "Incorrect PIN") == 1 && contains(s.out, "Authorization accepted.") &&
              contains(s.out, "Registration cancelled for patient ID 2 (Bob)"), "Correct PIN on the second attempt authorizes the cancellation");
        check(countOf(s.out, "Saved ") == 1, "The successful cancellation was auto-saved once");

        // Explicit confirmation: "no" keeps the registration; junk is re-asked; EOF aborts.
        const std::string beforeNo = readFile(file);
        s = runSession(file, "5\n" + PIN + "\n1\nmaybe\nno\n" + search(1) + EXIT_SCRIPT);
        check(contains(s.out, "Please answer yes or no.") && contains(s.out, "Cancellation aborted. The patient remains in the queue.") &&
              contains(s.out, "Status            : Waiting"), "Answering 'no' keeps the patient (junk answers are re-asked)");
        check(countOf(s.out, "Saved ") == 0 && readFile(file) == beforeNo, "Aborted cancellation saved nothing");
        s = runSession(file, "5\n" + PIN + "\n1\n");   // input ends at the confirmation question
        check(contains(s.out, "Input ended before confirmation") && s.code == 0 && readFile(file) == beforeNo,
              "Input ending at the confirmation question aborts safely");
        s = runSession(file, "5\nwrong\n");            // input ends inside the PIN prompt
        check(contains(s.out, "Input ended before a PIN was entered") && s.code == 0, "Input ending at the PIN prompt aborts safely");

        // Served / already cancelled / unknown: refused at once, no confirmation asked.
        runSession(file, serve() + EXIT_SCRIPT);       // serves patient 1 (Alice); Bob (2) is cancelled
        s = runSession(file, "5\n" + PIN + "\n1\n5\n" + PIN + "\n2\n5\n" + PIN + "\n77\n" + EXIT_SCRIPT);
        check(contains(s.out, "Cannot cancel: patient ID 1 (Alice) has already been served."), "Served patient cannot be cancelled");
        check(contains(s.out, "the registration of patient ID 2 (Bob) was already cancelled."), "Already-cancelled patient cannot be cancelled again");
        check(contains(s.out, "No patient with ID 77 exists."), "Unknown ID is reported");
        check(!contains(s.out, "Cancel this patient's registration?") && countOf(s.out, "Saved ") == 0,
              "Refused requests ask for no confirmation and trigger no save");

        // A different PIN can be configured; the default no longer works.
        const fs::path file2 = workDir / "auth" / "custom.txt";
        runSession(file2, reg("Carol", 30, "F", "Cold", 3) + EXIT_SCRIPT);
        s = runSession(file2, "5\n" + PIN + "\n" + PIN + "\n" + PIN + "\n" + "5\nmy-own-pin\n1\nyes\n" + EXIT_SCRIPT, "my-own-pin");
        check(countOf(s.out, "Incorrect PIN") == 3 && contains(s.out, "Registration cancelled for patient ID 1 (Carol)"),
              "A custom staff PIN replaces the default one (default rejected 3 times, custom accepted)");
    }

    // =====================================================================
    std::cout << "--- 16: invalid menu input ---\n";
    {
        const fs::path file = workDir / "menu" / "patients.txt";
        const Session s = runSession(file,
            "abc\n0\n9\n-1\n1.5\n\n   \n99999999999\n3x\n!!\n  6  \n" + EXIT_SCRIPT);
        check(countOf(s.out, "Invalid choice. Please enter a whole number from 1 to 8.") == 10,
              "All 10 invalid entries rejected (letters, 0, 9, -1, decimal, blank, spaces, overflow, '3x', symbols)");
        check(countOf(s.out, "QUEUE STATISTICS") == 1, "'  6  ' (padded) is accepted as choice 6");
        check(countOf(s.out, "Enter your choice: ") == 12, "The menu was shown again after every entry");
        check(s.code == 0 && countOf(s.out, "Saved ") == 0, "No crash, no save");
    }

    // =====================================================================
    std::cout << "--- end-of-input handling ---\n";
    {
        Session s = runSession(workDir / "eof" / "a.txt", "");
        check(s.code == 0 && contains(s.out, "End of input reached") && contains(s.out, "Goodbye."), "Immediate end of input: clean exit");

        s = runSession(workDir / "eof" / "b.txt", "1\nOnly A Name\n");
        check(s.code == 0 && contains(s.out, "Registration cancelled; no patient was added.") &&
              !fs::exists(workDir / "eof" / "b.txt"), "End of input in the middle of registration: nothing added, nothing saved");

        s = runSession(workDir / "eof" / "c.txt", reg("Dora", 30, "F", "Cold", 3));   // no exit command
        check(s.code == 0 && !s.unsaved && loadRecords(workDir / "eof" / "c.txt").size() == 1,
              "Input ending after a registration: it was auto-saved, so the exit is clean");

        s = runSession(workDir / "eof" / "d.txt", "8\n");                              // ends at the exit question
        check(s.code == 0 && contains(s.out, "Goodbye."), "Input ending at the exit confirmation: clean exit");
    }

    // =====================================================================
    std::cout << "--- 18: file-saving failures ---\n";
    {
        writeFile(workDir / "blocked" / "iamafile", "x");
        const fs::path badPath = workDir / "blocked" / "iamafile" / "patients.txt";   // parent is a file

        Session s = runSession(badPath, reg("Eve", 30, "F", "Cold", 3) + "6\n" + "8\nno\n" + "6\n" + "8\nyes\nyes\n");
        check(contains(s.out, "Error: Could not save to"), "Failed automatic save: the error is shown");
        check(contains(s.out, "WARNING: The latest change is NOT saved to disk."), "A clear warning says the change is not saved");
        check(!contains(s.out, "Saved 1 patient record(s)") && countOf(s.out, "Saved ") == 0,
              "The program never claims a save that did not happen");
        check(contains(s.out, "Patients currently waiting  : 1"), "The unsaved patient is NOT discarded: still in memory and in the queue");
        check(contains(s.out, "Saving outstanding changes before exit...") && contains(s.out, "could NOT be saved and will be lost"),
              "Exit retries the save and warns that changes will be lost");
        check(countOf(s.out, "Exit anyway and lose the unsaved changes?") == 1 && contains(s.out, "Exiting with UNSAVED changes."),
              "Exiting with unsaved changes needs explicit consent");
        check(s.code == HospitalManagementSystem::EXIT_UNSAVED_CHANGES && s.unsaved, "Exit code 2 signals unsaved changes");

        // Declining "exit anyway" returns to the menu.
        s = runSession(badPath, reg("Eve", 30, "F", "Cold", 3) + "8\nyes\nno\n6\n8\nyes\nyes\n");
        check(contains(s.out, "Returning to the main menu.") && countOf(s.out, "Patients currently waiting  : 1") == 1,
              "Answering 'no' to 'exit anyway' returns to the menu with the data still in memory");

        // End of input with unsaved changes: warned, code 2.
        s = runSession(badPath, reg("Eve", 30, "F", "Cold", 3));
        check(s.code == HospitalManagementSystem::EXIT_UNSAVED_CHANGES && contains(s.out, "Exiting with UNSAVED changes."),
              "Input ending with unsaved changes: warning shown and exit code 2");

        // A failed serve/cancel on the way does not hide earlier unsaved changes; manual save fails honestly too.
        s = runSession(badPath, reg("Eve", 30, "F", "Cold", 3) + "7\n" + "8\nyes\nyes\n");
        check(contains(s.out, "WARNING: The records are NOT saved. They are held in memory only."),
              "Manual save failure is reported honestly");

        // Recovery: the temp-file name is blocked, then the problem is removed.
        const fs::path file = workDir / "blocked" / "recover.txt";
        fs::create_directories(fs::path(file.string() + ".tmp"));
        HospitalManagementSystem sys(file.string());
        {
            std::istringstream in(reg("Frank", 30, "M", "Cold", 3) + reg("Gina", 31, "F", "Cough", 1));
            std::ostringstream out;
            const int code = sys.run(in, out);
            check(code == 2 && sys.hasUnsavedChanges() && !fs::exists(file), "Blocked save: both patients held in memory, no file written");
        }
        fs::remove_all(fs::path(file.string() + ".tmp"));
        {
            std::istringstream in("7\n" + EXIT_SCRIPT);   // run() starts by loading: the file is missing, memory is kept
            std::ostringstream out;
            const int code = sys.run(in, out);
            check(code == 0 && contains(out.str(), "Saved 2 patient record(s)") && !sys.hasUnsavedChanges(),
                  "After the problem is fixed, 'Save Patient Records' saves the held patients");
        }
        check(loadRecords(file).size() == 2, "The saved file contains both patients");
    }

    // =====================================================================
    std::cout << "--- 19: corrupted data files ---\n";
    {
        const std::string corrupt = HEADER + GOOD1 + "this line is damaged\nEND\t2\n";
        const fs::path file = workDir / "corrupt" / "patients.txt";
        writeFile(file, corrupt);

        // Start and leave at once.
        Session s = runSession(file, EXIT_SCRIPT);
        check(contains(s.out, "Error: Invalid data file") && contains(s.out, "saving is disabled until it is repaired") &&
              contains(s.out, "No records were loaded"), "Corrupt file: startup message explains the problem");
        check(s.code == 0 && readFile(file) == corrupt, "Corrupt file left untouched when nothing changed; clean exit");

        // Register while the file is damaged.
        s = runSession(file, reg("Hana", 30, "F", "Cold", 3) + "6\n" + "7\nno\n" + "8\nyes\nyes\n");
        check(contains(s.out, "Save refused") && contains(s.out, "WARNING: The latest change is NOT saved to disk."),
              "Registration works in memory, but the automatic save is refused and the user is warned");
        check(contains(s.out, "Patients currently waiting  : 1"), "The new patient is kept in memory");
        check(contains(s.out, "Replace the damaged data file?") && contains(s.out, "The data file was not changed. The records are NOT saved."),
              "Manual save offers an explicit overwrite; declining keeps the file");
        check(readFile(file) == corrupt && !fs::exists(fs::path(file.string() + ".bak")),
              "The corrupt file is byte-for-byte unchanged, and no backup is made without consent");
        check(s.code == HospitalManagementSystem::EXIT_UNSAVED_CHANGES && contains(s.out, "Exiting with UNSAVED changes."),
              "Exiting with the unsaved patient needs explicit consent and gives exit code 2");

        // Deliberate overwrite after confirmation: backup first.
        s = runSession(file, reg("Hana", 30, "F", "Cold", 3) + "7\nyes\n" + EXIT_SCRIPT);
        check(contains(s.out, "A backup copy of the damaged file was saved as") && contains(s.out, "Saved 1 patient record(s)"),
              "Confirmed overwrite: backup announced, then the records are saved");
        check(readFile(fs::path(file.string() + ".bak")) == corrupt, "The backup holds the original damaged contents exactly");
        check(s.code == 0 && !s.unsaved && loadRecords(file).size() == 1, "The data file is valid again; clean exit");
        s = runSession(file, "6\n" + EXIT_SCRIPT);
        check(contains(s.out, "Loaded 1 patient record(s)") && !contains(s.out, "Error:"), "Next start loads normally");

        // Other kinds of unusable files.
        const fs::path dirPath = workDir / "corrupt" / "is_a_directory";
        fs::create_directories(dirPath);
        s = runSession(dirPath, reg("Ivan", 30, "M", "Cold", 3) + "8\nyes\nyes\n");
        check(contains(s.out, "not a regular file") && s.code == 2, "Data path that is a directory: reported safely, no crash");
        const fs::path zero = workDir / "corrupt" / "zero.txt";
        writeFile(zero, "");
        s = runSession(zero, reg("Jade", 30, "F", "Cold", 3) + EXIT_SCRIPT);
        check(contains(s.out, "is empty") && s.code == 0 && loadRecords(zero).size() == 1, "Empty data file: treated as a fresh start and then saved normally");
    }

    // =====================================================================
    std::cout << "--- backup helper ---\n";
    {
        const fs::path file = workDir / "backup" / "patients.txt";
        const PatientStorage storage(file);
        check(!storage.createBackupCopy().success && !fs::exists(fs::path(file.string() + ".bak")),
              "Backing up a missing file fails and creates nothing");
        writeFile(file, "first contents");
        check(storage.createBackupCopy().success && readFile(fs::path(file.string() + ".bak")) == "first contents",
              "Backup copies the file exactly");
        writeFile(file, "second contents");
        check(storage.createBackupCopy().success && readFile(fs::path(file.string() + ".bak")) == "second contents",
              "A new backup replaces the older one");
    }

    // =====================================================================
    std::cout << "--- 20: repeated operations across restarts (random, checked against a reference model) ---\n";
    {
        struct Ref { int id; int prio; PatientStatus status; };
        std::vector<Ref> ref;
        std::mt19937 rng(20261121);
        const fs::path file = workDir / "random" / "data" / "patients.txt";
        bool savesMatch = true, codesOk = true;
        int totalOps = 0;

        for (int session = 0; session < 5; ++session) {
            std::string script;
            int expectedSaves = 0;
            for (int op = 0; op < 60; ++op) {
                const int r = static_cast<int>(rng() % 12);
                if (r < 5) {                                              // register
                    const int prio = static_cast<int>(rng() % 3) + 1;
                    const int id = static_cast<int>(ref.size()) + 1;
                    script += reg("Patient" + std::to_string(id), 20 + id % 60, id % 2 ? "Female" : "Male", "Condition " + std::to_string(id), prio);
                    ref.push_back({id, prio, PatientStatus::Waiting});
                    ++expectedSaves;
                } else if (r < 7) {                                       // serve
                    script += serve();
                    Ref* best = nullptr;
                    for (Ref& p : ref)
                        if (p.status == PatientStatus::Waiting && (!best || p.prio < best->prio || (p.prio == best->prio && p.id < best->id))) best = &p;
                    if (best) { best->status = PatientStatus::Served; ++expectedSaves; }
                } else if (r < 10) {                                      // cancel (sometimes a bad target)
                    const int id = 1 + static_cast<int>(rng() % (ref.size() + 1));
                    script += "5\n" + PIN + "\n" + std::to_string(id) + "\n";
                    if (id <= static_cast<int>(ref.size()) && ref[static_cast<std::size_t>(id) - 1].status == PatientStatus::Waiting) {
                        script += "yes\n";
                        ref[static_cast<std::size_t>(id) - 1].status = PatientStatus::Cancelled;
                        ++expectedSaves;
                    }
                } else if (r == 10) {
                    script += "2\n";
                } else {
                    script += "6\n" + search(1 + static_cast<int>(rng() % (ref.size() + 1)));
                }
                ++totalOps;
            }
            script += EXIT_SCRIPT;
            const Session s = runSession(file, script);
            if (countOf(s.out, "Saved ") != expectedSaves) savesMatch = false;
            if (s.code != 0 || s.unsaved) codesOk = false;
        }

        check(savesMatch, "In every session the number of saves equals the number of successful changes (none after failed or read-only operations)");
        check(codesOk, "All 5 sessions ended cleanly with nothing unsaved");

        const std::vector<PatientRecord> records = loadRecords(file);
        bool allMatch = records.size() == ref.size();
        std::set<int> ids;
        for (std::size_t i = 0; allMatch && i < records.size(); ++i) {
            ids.insert(records[i].patient.getId());
            if (records[i].patient.getId() != ref[i].id || records[i].status != ref[i].status ||
                static_cast<int>(records[i].patient.getPriority()) != ref[i].prio) allMatch = false;
        }
        std::cout << "  " << totalOps << " operations over 5 sessions, " << ref.size() << " patients registered\n";
        check(allMatch && ids.size() == ref.size(), "The final file matches the reference model exactly, with no duplicate or lost records");

        // The restored queue serves in the reference order and never serves cancelled patients.
        std::vector<int> expectedOrder;
        std::vector<Ref> waiting;
        for (const Ref& p : ref) if (p.status == PatientStatus::Waiting) waiting.push_back(p);
        std::sort(waiting.begin(), waiting.end(), [](const Ref& a, const Ref& b) { return a.prio != b.prio ? a.prio < b.prio : a.id < b.id; });
        for (const Ref& p : waiting) expectedOrder.push_back(p.id);
        std::string drain;
        for (std::size_t i = 0; i < expectedOrder.size() + 1; ++i) drain += serve();
        const Session s = runSession(file, drain + EXIT_SCRIPT);
        check(servedIds(s.out) == expectedOrder, "After the last restart the remaining patients are served in exactly the reference order");
        const std::vector<PatientRecord> finalRecords = loadRecords(file);
        std::size_t cancelledStill = 0, waitingLeft = 0;
        for (const PatientRecord& r : finalRecords) {
            if (r.status == PatientStatus::Cancelled) ++cancelledStill;
            if (r.status == PatientStatus::Waiting) ++waitingLeft;
        }
        std::size_t cancelledExpected = 0;
        for (const Ref& p : ref) if (p.status == PatientStatus::Cancelled) ++cancelledExpected;
        check(waitingLeft == 0 && cancelledStill == cancelledExpected, "No cancelled patient was ever served; the history is intact");
    }


    // =====================================================================
    // NEW in Stage 6: end of input must never authorize discarding data
    // =====================================================================
    std::cout << "--- EOF at the exit confirmation / 'exit anyway' prompts ---\n";
    {
        writeFile(workDir / "eofx" / "iamafile", "x");
        const fs::path badPath = workDir / "eofx" / "iamafile" / "patients.txt";   // saving always fails

        // (a) Input ends at "Are you sure you want to exit?" while changes are unsaved.
        Session s = runSession(badPath, reg("Eve", 30, "F", "Cold", 3) + "8\n");
        check(contains(s.out, "Input ended before exit was confirmed. Exit is not assumed."),
              "EOF at the exit confirmation is not treated as 'yes'");
        check(s.code == HospitalManagementSystem::EXIT_UNSAVED_CHANGES && s.unsaved &&
                  contains(s.out, "Exiting with UNSAVED changes."),
              "EOF at the exit confirmation with unsaved changes: warning and exit code 2");

        // (b) Input ends at "Exit anyway and lose the unsaved changes?"
        s = runSession(badPath, reg("Eve", 30, "F", "Cold", 3) + "8\nyes\n");
        check(contains(s.out, "Exit anyway and lose the unsaved changes?") &&
                  contains(s.out, "Exiting is NOT assumed"),
              "EOF at 'exit anyway' is not treated as consent to lose changes");
        check(s.code == HospitalManagementSystem::EXIT_UNSAVED_CHANGES && s.unsaved && !fs::exists(badPath),
              "EOF at 'exit anyway': changes still reported as unsaved (exit code 2), nothing written");
        check(countOf(s.out, "Thank you for using") == 1,
              "The program ends exactly once, only because no input remains");

        // (c) The same, but the save problem is fixed before the next session: nothing is lost in memory.
        const fs::path file = workDir / "eofx" / "recover.txt";
        fs::create_directories(fs::path(file.string() + ".tmp"));      // blocks the save
        HospitalManagementSystem sys(file.string());
        {
            std::istringstream in(reg("Frank", 30, "M", "Cold", 3) + "8\nyes\n");
            std::ostringstream out;
            const int code = sys.run(in, out);
            check(code == 2 && sys.hasUnsavedChanges() && !fs::exists(file),
                  "EOF at 'exit anyway' keeps the patient in memory, unsaved");
        }
        fs::remove_all(fs::path(file.string() + ".tmp"));
        {
            std::istringstream in("");                                   // immediate end of input
            std::ostringstream out;
            const int code = sys.run(in, out);
            check(code == 0 && !sys.hasUnsavedChanges() && loadRecords(file).size() == 1,
                  "Once the problem is fixed, the end-of-input shutdown saves the held patient");
        }

        // (d) EOF after answering 'no' to exit still returns to the menu and ends safely.
        s = runSession(badPath, reg("Eve", 30, "F", "Cold", 3) + "8\nno\n");
        check(contains(s.out, "Returning to the main menu.") && s.code == 2 && s.unsaved,
              "'no' returns to the menu; then EOF ends with warning and code 2");
    }

    std::cout << "--- the project's real data/patients.txt is never touched by tests ---\n";
    {
        const std::string realDataAfter = snapshotRealData();
        check(realDataAfter == realDataBefore,
              "data/patients.txt (relative to the current folder) is unchanged by this whole test program");
    }


    return testfw::finish();
}
