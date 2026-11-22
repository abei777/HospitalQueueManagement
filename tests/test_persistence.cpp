// tests/test_persistence.cpp
// Persistence tests: file format, save/load round trips, escaping, atomic save, corrupted or
// unreadable files, overwrite protection and backup, and generator continuity after a load.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
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
using testfw::linesOf;
using testfw::readFile;
using testfw::writeFile;

// Every file this test creates lives inside this temporary folder (deleted on
// exit). The project's real data/patients.txt is never touched.
static fs::path workDir;

// ---- queue helpers ----------------------------------------------------
[[maybe_unused]] static bool recordsEqual(const PatientRecord& a, const PatientRecord& b) {
    return a.status == b.status && a.patient.getId() == b.patient.getId() &&
           a.patient.getFullName() == b.patient.getFullName() && a.patient.getAge() == b.patient.getAge() &&
           a.patient.getGender() == b.patient.getGender() &&
           a.patient.getMedicalCondition() == b.patient.getMedicalCondition() &&
           a.patient.getPriority() == b.patient.getPriority() &&
           a.patient.getArrivalSequence() == b.patient.getArrivalSequence();
}

[[maybe_unused]] static bool allRecordsEqual(const std::vector<PatientRecord>& a, const std::vector<PatientRecord>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!recordsEqual(a[i], b[i])) return false;
    return true;
}

[[maybe_unused]] static std::vector<int> waitingIds(const HospitalQueue& q) {
    std::vector<int> result;
    for (const Patient& p : q.getWaitingPatients()) result.push_back(p.getId());
    return result;
}

[[maybe_unused]] static bool statsEqual(const QueueStatistics& a, const QueueStatistics& b) {
    return a.totalRegistered == b.totalRegistered && a.waiting == b.waiting && a.served == b.served &&
           a.cancelled == b.cancelled && a.waitingEmergency == b.waitingEmergency &&
           a.waitingUrgent == b.waitingUrgent && a.waitingNormal == b.waitingNormal;
}

// ---- UI helpers -------------------------------------------------------
[[maybe_unused]] static void registerVia(HospitalManagementSystem& s, const std::string& name, int age,
                        const std::string& gender, const std::string& cond, int prio) {
    std::istringstream in(name + "\n" + std::to_string(age) + "\n" + gender + "\n" + cond + "\n" +
                          std::to_string(prio) + "\n");
    std::ostringstream out;
    s.registerPatientInteractive(in, out);
}

[[maybe_unused]] static bool serveQuiet(HospitalManagementSystem& s) {
    std::ostringstream out;
    return s.serveNextPatient(out);
}

[[maybe_unused]] static std::string cancelQuiet(HospitalManagementSystem& s, int id) {
    std::istringstream in(std::to_string(id) + "\n");
    std::ostringstream out;
    s.cancelPatient(in, out);
    return out.str();
}

[[maybe_unused]] static std::string searchText(const HospitalManagementSystem& s, int id) {
    std::istringstream in(std::to_string(id) + "\n");
    std::ostringstream out;
    s.searchPatient(in, out);
    return out.str();
}

[[maybe_unused]] static std::string displayText(const HospitalManagementSystem& s) {
    std::ostringstream out;
    s.displayWaitingPatients(out);
    return out.str();
}

[[maybe_unused]] static std::string statsText(const HospitalManagementSystem& s) {
    std::ostringstream out;
    s.displayStatistics(out);
    return out.str();
}

struct Outcome { bool ok; std::string text; };

[[maybe_unused]] static Outcome saveVia(HospitalManagementSystem& s, bool force = false) {
    std::ostringstream out;
    const bool ok = s.saveRecords(out, force);
    return {ok, out.str()};
}

[[maybe_unused]] static Outcome loadVia(HospitalManagementSystem& s) {
    std::ostringstream out;
    const bool ok = s.loadSavedRecords(out);
    return {ok, out.str()};
}

// Standard scenario (7 patients registered, then 1 served, 2 cancelled):
//   1 Normal-A(N)  2 Urgent-A(U)  3 Emergency-A(E)  4 Normal-B(N)
//   5 Urgent-B(U)  6 Emergency-B(E)  7 Normal-C(N)
//   serve -> patient 3 (Emergency-A); cancel 5 and 7.
//   Result: waiting 6,2,1,4 ; served 3 ; cancelled 5,7
[[maybe_unused]] static void buildScenario(HospitalManagementSystem& s) {
    registerVia(s, "Normal-A", 30, "Female", "Cold", 3);
    registerVia(s, "Urgent-A", 40, "Male", "Fracture", 2);
    registerVia(s, "Emergency-A", 50, "Female", "Chest pain", 1);
    registerVia(s, "Normal-B", 20, "Male", "Sprain", 3);
    registerVia(s, "Urgent-B", 60, "Female", "Burn", 2);
    registerVia(s, "Emergency-B", 10, "Male", "Bleeding", 1);
    registerVia(s, "Normal-C", 35, "Female", "Rash", 3);
    serveQuiet(s);
    cancelQuiet(s, 5);
    cancelQuiet(s, 7);
}

[[maybe_unused]] static const std::string HEADER = "HQMS-PATIENT-RECORDS\t1\n";
[[maybe_unused]] static const std::string GOOD1  = "1\tAma Mensah\t41\tFemale\tChest pain\t1\t1\tWaiting\n";
[[maybe_unused]] static const std::string GOOD2  = "2\tKofi Boateng\t30\tMale\tFever\t3\t2\tServed\n";

// A malformed file must be rejected completely, leave the existing registry
// and the file itself untouched, and block saving over it.
[[maybe_unused]] static void expectRejected(const std::string& label, const std::string& content) {
    const fs::path path = workDir / "malformed.txt";
    writeFile(path, content);
    HospitalManagementSystem sys(path.string());
    registerVia(sys, "Existing", 30, "F", "Cold", 3);
    const Outcome load = loadVia(sys);
    const bool registryUnchanged = sys.getQueue().totalRegistered() == 1 && sys.getQueue().size() == 1 &&
                                   sys.getQueue().peekNext().getFullName() == "Existing";
    const Outcome save = saveVia(sys);
    const bool fileIntact = readFile(path) == content;
    const bool good = !load.ok && contains(load.text, "No records were loaded") && registryUnchanged &&
                      !save.ok && fileIntact;
    check(good, "Rejected + registry/file untouched + save blocked: " + label);
}


int main() {
    testfw::TempDir temp("persistence");
    workDir = temp.path();

    // =====================================================================
    // PART B: SAVING AND LOADING
    // =====================================================================
    std::cout << "--- B1: save the standard scenario and inspect the file ---\n";
    const fs::path mainFile = workDir / "data" / "patients.txt";   // data/ does not exist yet
    HospitalManagementSystem original(mainFile.string());
    buildScenario(original);
    const QueueStatistics originalStats = original.getQueue().getStatistics();
    const std::vector<int> originalWaiting = waitingIds(original.getQueue());
    {
        check(!fs::exists(mainFile.parent_path()), "Folder does not exist before the first save");
        const Outcome save = saveVia(original);
        check(save.ok && contains(save.text, "Saved 7 patient record(s)"), "Save succeeds and reports 7 records");
        check(fs::exists(mainFile), "Save created the missing folder and the file");
        check(!fs::exists(fs::path(mainFile.string() + ".tmp")), "No temporary file is left behind");
        const std::vector<std::string> lines = linesOf(readFile(mainFile));
        check(lines.size() == 9, "File has header + 7 records + END = 9 lines");
        check(!lines.empty() && lines.front() == "HQMS-PATIENT-RECORDS\t1", "First line is the format name and version 1");
        check(!lines.empty() && lines.back() == "END\t7", "Last line is END with the record count");
        check(lines.size() == 9 && lines[3] == "3\tEmergency-A\t50\tFemale\tChest pain\t1\t3\tServed",
              "Served patient saved with all fields and status Served");
        check(lines.size() == 9 && lines[5] == "5\tUrgent-B\t60\tFemale\tBurn\t2\t5\tCancelled",
              "Cancelled patient saved with status Cancelled");
        check(lines.size() == 9 && lines[2] == "2\tUrgent-A\t40\tMale\tFracture\t2\t2\tWaiting",
              "Waiting patient saved with status Waiting");
        std::cout << "  ---- saved file ----\n";
        for (const std::string& line : lines) std::cout << "  | " << line << "\n";
        std::cout << "  --------------------\n";
    }

    std::cout << "--- B2: load into a fresh system (a simulated restart) ---\n";
    {
        HospitalManagementSystem restarted(mainFile.string());
        const Outcome load = loadVia(restarted);
        check(load.ok && contains(load.text, "Loaded 7 patient record(s)") &&
              contains(load.text, "4 waiting, 1 served, 2 cancelled"), "Load succeeds: 4 waiting, 1 served, 2 cancelled");
        check(allRecordsEqual(original.getQueue().getAllRecords(), restarted.getQueue().getAllRecords()),
              "Every record restored exactly (ID, name, age, gender, condition, priority, sequence, status)");
        check(statsEqual(originalStats, restarted.getQueue().getStatistics()), "Statistics identical after restart");
        check(restarted.getQueue().getStatistics().isConsistent(), "Registered = Waiting + Served + Cancelled after load");
        check(waitingIds(restarted.getQueue()) == originalWaiting &&
              originalWaiting == (std::vector<int>{6, 2, 1, 4}), "Waiting order restored: Emergency-B, Urgent-A, Normal-A, Normal-B");
        check(displayText(original) == displayText(restarted), "Waiting display is identical before and after restart");
        const QueueStatistics s = restarted.getQueue().getStatistics();
        check(s.waitingEmergency == 1 && s.waitingUrgent == 1 && s.waitingNormal == 2,
              "Emergency, Urgent and Normal waiting patients restored (1/1/2)");
        check(restarted.getQueue().size() == 4, "Only Waiting patients are in the active queue (served/cancelled are not)");
        check(contains(searchText(restarted, 3), "Status            : Served") &&
              contains(searchText(restarted, 5), "Status            : Cancelled") &&
              contains(searchText(restarted, 7), "Status            : Cancelled") &&
              contains(searchText(restarted, 6), "Status            : Waiting"),
              "Served, cancelled and waiting statuses restored and searchable");
    }

    std::cout << "--- B3: cancelled and served patients are never served after restart ---\n";
    {
        HospitalManagementSystem restarted(mainFile.string());
        loadVia(restarted);
        std::vector<int> servedOrder;
        while (!restarted.getQueue().isEmpty()) {
            servedOrder.push_back(restarted.getQueue().peekNext().getId());
            serveQuiet(restarted);
        }
        check(servedOrder == (std::vector<int>{6, 2, 1, 4}), "Restarted system serves exactly 6, 2, 1, 4 in that order");
        check(std::find(servedOrder.begin(), servedOrder.end(), 5) == servedOrder.end() &&
              std::find(servedOrder.begin(), servedOrder.end(), 7) == servedOrder.end(),
              "Cancelled patients 5 and 7 never served");
        check(std::find(servedOrder.begin(), servedOrder.end(), 3) == servedOrder.end(),
              "Already-served patient 3 not served a second time");
        check(!serveQuiet(restarted), "Queue then reports empty");
        const QueueStatistics s = restarted.getQueue().getStatistics();
        check(s.served == 5 && s.cancelled == 2 && s.waiting == 0 && s.totalRegistered == 7 && s.isConsistent(),
              "Final statistics: 5 served, 2 cancelled, 0 waiting");

        HospitalManagementSystem again(mainFile.string());
        loadVia(again);
        check(contains(cancelQuiet(again, 3), "already been served"), "After restart, a served patient still cannot be cancelled");
        check(contains(cancelQuiet(again, 5), "already cancelled"), "After restart, a cancelled patient is still recorded as cancelled");
    }

    std::cout << "--- B4: IDs and arrival sequences continue after loading ---\n";
    {
        HospitalManagementSystem restarted(mainFile.string());
        loadVia(restarted);
        registerVia(restarted, "NewPerson", 22, "Female", "Headache", 1);
        const auto record = restarted.getQueue().findPatient(8);
        check(record && record->patient.getFullName() == "NewPerson" && record->patient.getArrivalSequence() == 8,
              "Next patient gets ID 8 and sequence 8 (patient 7, the highest, was cancelled)");
        check(restarted.getQueue().totalRegistered() == 8 && waitingIds(restarted.getQueue()) == (std::vector<int>{6, 8, 2, 1, 4}),
              "New Emergency patient joins the restored queue behind the earlier Emergency patient (6), ahead of all others");
        check(restarted.getQueue().getStatistics().isConsistent(), "Statistics consistent after post-load registration");

        // Highest ID served, and all-cancelled variants.
        HospitalManagementSystem a((workDir / "highest_served.txt").string());
        registerVia(a, "One", 30, "F", "c", 1);
        registerVia(a, "Two", 30, "F", "c", 2);
        serveQuiet(a);
        serveQuiet(a);
        saveVia(a);
        HospitalManagementSystem a2((workDir / "highest_served.txt").string());
        loadVia(a2);
        registerVia(a2, "Three", 30, "F", "c", 3);
        const auto three = a2.getQueue().findPatient(3);
        check(a2.getQueue().size() == 1 && three && three->patient.getArrivalSequence() == 3,
              "When every patient was served, the next ID is still 3 (never reused)");
        HospitalManagementSystem c((workDir / "all_cancelled.txt").string());
        registerVia(c, "One", 30, "F", "c", 1);
        registerVia(c, "Two", 30, "F", "c", 2);
        cancelQuiet(c, 1);
        cancelQuiet(c, 2);
        saveVia(c);
        HospitalManagementSystem c2((workDir / "all_cancelled.txt").string());
        loadVia(c2);
        check(c2.getQueue().isEmpty() && c2.getQueue().totalCancelled() == 2, "All-cancelled registry restores with an empty queue");
        registerVia(c2, "Three", 30, "F", "c", 3);
        check(c2.getQueue().findPatient(3).has_value() && c2.getQueue().peekNext().getId() == 3,
              "When every patient was cancelled, the next ID is still 3");
    }

    std::cout << "--- B5: FIFO restored from arrival sequence (hand-written file) ---\n";
    {
        const fs::path path = workDir / "fifo.txt";
        // ID order differs from arrival order on purpose.
        writeFile(path, HEADER +
                            "1\tLater Arrival\t30\tF\tCold\t2\t5\tWaiting\n"
                            "2\tEarlier Arrival\t30\tF\tCold\t2\t3\tWaiting\n"
                            "3\tNormal Patient\t30\tF\tCold\t3\t1\tWaiting\n"
                            "END\t3\n");
        HospitalManagementSystem sys(path.string());
        const Outcome load = loadVia(sys);
        check(load.ok && waitingIds(sys.getQueue()) == (std::vector<int>{2, 1, 3}),
              "Equal-priority patients ordered by arrival sequence, not by ID");
        registerVia(sys, "After Load", 30, "F", "Cold", 2);
        check(sys.getQueue().findPatient(4)->patient.getArrivalSequence() == 6,
              "Next sequence continues after the highest saved sequence (5 -> 6)");
        check(waitingIds(sys.getQueue()) == (std::vector<int>{2, 1, 4, 3}), "New Urgent patient queues behind existing Urgent patients");
    }

    std::cout << "--- B6: names and conditions with spaces and delimiters ---\n";
    {
        const fs::path path = workDir / "special.txt";
        HospitalManagementSystem sys(path.string());
        registerVia(sys, "Smith, John \"Jr\"; O'Neil", 33, "Non-binary", "Pain, \"severe\"; left arm", 1);
        registerVia(sys, "Tab\tInside", 44, "F", "Path C:\\temp\\new", 2);
        registerVia(sys, "Kwaku \xC6\x86kyere \xE2\x9C\x93", 55, "M", "Literal \\n text and \\t text", 3);
        check(saveVia(sys).ok, "Save with special characters succeeds");
        const std::string raw = readFile(path);
        bool tabsOk = true;
        for (const std::string& line : linesOf(raw)) {
            const std::size_t tabs = static_cast<std::size_t>(std::count(line.begin(), line.end(), '\t'));
            const bool isHeader = line.rfind("HQMS-PATIENT-RECORDS", 0) == 0;
            const bool isFooter = line.rfind("END", 0) == 0;
            if (isHeader || isFooter) { if (tabs != 1) tabsOk = false; }
            else if (tabs != 7) tabsOk = false;
        }
        check(tabsOk && linesOf(raw).size() == 5, "Every record line has exactly 7 delimiters; 5 lines in total");
        check(contains(raw, "Tab\\tInside") && contains(raw, "C:\\\\temp\\\\new"), "Tab and backslash are escaped in the file");
        HospitalManagementSystem restored(path.string());
        check(loadVia(restored).ok && allRecordsEqual(sys.getQueue().getAllRecords(), restored.getQueue().getAllRecords()),
              "All special-character fields restored exactly");
        check(restored.getQueue().findPatient(2)->patient.getFullName() == "Tab\tInside" &&
              restored.getQueue().findPatient(2)->patient.getMedicalCondition() == "Path C:\\temp\\new",
              "Tab in a name and backslashes in a condition survive");
        check(restored.getQueue().findPatient(3)->patient.getMedicalCondition() == "Literal \\n text and \\t text",
              "A literal backslash-n typed by a user is not mistaken for a newline");

        // Real newline / carriage return characters, stored via PatientStorage directly.
        const fs::path nlPath = workDir / "newlines.txt";
        std::vector<PatientRecord> records;
        records.push_back(PatientRecord{Patient(1, "Line1\nLine2\rEnd", 30, "F", "Cond\nSecond line\r\n", PriorityLevel::Urgent, 1),
                                        PatientStatus::Waiting});
        const PatientStorage storage(nlPath);
        const PatientStorage::SaveResult s = storage.save(records);
        const PatientStorage::LoadResult l = storage.load();
        check(s.success && l.status == PatientStorage::LoadStatus::Loaded && allRecordsEqual(records, l.records),
              "Embedded newline and carriage-return characters round-trip");
        check(linesOf(readFile(nlPath)).size() == 3, "Embedded line breaks do not add lines to the file");
    }

    std::cout << "--- B7: missing and empty files ---\n";
    {
        const fs::path missing = workDir / "nothing_here.txt";
        HospitalManagementSystem sys(missing.string());
        const Outcome load = loadVia(sys);
        check(load.ok && contains(load.text, "No saved data file found") && contains(load.text, "empty registry") &&
              sys.getQueue().totalRegistered() == 0, "Missing file: handled gracefully, starts empty");
        check(!fs::exists(missing), "Loading a missing file does not create it");
        check(saveVia(sys).ok && fs::exists(missing), "Saving after a missing file works");

        const fs::path empty = workDir / "zero_bytes.txt";
        writeFile(empty, "");
        HospitalManagementSystem sys2(empty.string());
        const Outcome load2 = loadVia(sys2);
        check(load2.ok && contains(load2.text, "is empty") && sys2.getQueue().totalRegistered() == 0,
              "Empty (0-byte) file: handled gracefully, starts empty");
        check(readFile(empty).empty(), "Loading an empty file does not modify it");
        check(saveVia(sys2).ok && loadVia(sys2).ok, "Saving and reloading after an empty file works");
    }

    std::cout << "--- B8: malformed files are rejected completely ---\n";
    {
        expectRejected("wrong header text", "WRONG\t1\n" + GOOD1 + "END\t1\n");
        expectRejected("header without a version", "HQMS-PATIENT-RECORDS\n" + GOOD1 + "END\t1\n");
        expectRejected("unsupported version 2", "HQMS-PATIENT-RECORDS\t2\n" + GOOD1 + "END\t1\n");
        expectRejected("non-numeric version", "HQMS-PATIENT-RECORDS\tx\n" + GOOD1 + "END\t1\n");
        expectRejected("plain text file", "hello world\nthis is not a data file\n");
        expectRejected("binary garbage", std::string("\x01\x02\0\xFF\xFE garbage", 12));
        expectRejected("only a header (no END)", HEADER);
        expectRejected("7 fields instead of 8", HEADER + "1\tAma\t41\tFemale\tChest pain\t1\t1\nEND\t1\n");
        expectRejected("9 fields instead of 8", HEADER + "1\tAma\t41\tFemale\tChest pain\t1\t1\tWaiting\textra\nEND\t1\n");
        expectRejected("non-numeric ID", HEADER + "abc\tAma\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("ID zero", HEADER + "0\tAma\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("negative ID", HEADER + "-3\tAma\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("decimal ID", HEADER + "1.5\tAma\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("ID with leading space", HEADER + " 1\tAma\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("overflowing ID", HEADER + "99999999999999999999\tAma\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("empty ID", HEADER + "\tAma\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("negative age", HEADER + "1\tAma\t-1\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("age 121", HEADER + "1\tAma\t121\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("non-numeric age", HEADER + "1\tAma\tforty\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("priority 0", HEADER + "1\tAma\t41\tFemale\tCold\t0\t1\tWaiting\nEND\t1\n");
        expectRejected("priority 4", HEADER + "1\tAma\t41\tFemale\tCold\t4\t1\tWaiting\nEND\t1\n");
        expectRejected("non-numeric priority", HEADER + "1\tAma\t41\tFemale\tCold\tHigh\t1\tWaiting\nEND\t1\n");
        expectRejected("unknown status", HEADER + "1\tAma\t41\tFemale\tCold\t1\t1\tDone\nEND\t1\n");
        expectRejected("lower-case status", HEADER + "1\tAma\t41\tFemale\tCold\t1\t1\twaiting\nEND\t1\n");
        expectRejected("empty status", HEADER + "1\tAma\t41\tFemale\tCold\t1\t1\t\nEND\t1\n");
        expectRejected("arrival sequence 0", HEADER + "1\tAma\t41\tFemale\tCold\t1\t0\tWaiting\nEND\t1\n");
        expectRejected("negative arrival sequence", HEADER + "1\tAma\t41\tFemale\tCold\t1\t-2\tWaiting\nEND\t1\n");
        expectRejected("non-numeric arrival sequence", HEADER + "1\tAma\t41\tFemale\tCold\t1\tfirst\tWaiting\nEND\t1\n");
        expectRejected("empty name", HEADER + "1\t\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("whitespace-only name", HEADER + "1\t   \t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("empty gender", HEADER + "1\tAma\t41\t\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("empty medical condition", HEADER + "1\tAma\t41\tFemale\t\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("unknown escape sequence", HEADER + "1\tAma\\q\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("lone trailing backslash", HEADER + "1\tAma\\\t41\tFemale\tCold\t1\t1\tWaiting\nEND\t1\n");
        expectRejected("missing END line (truncated)", HEADER + GOOD1);
        expectRejected("truncated in the middle of a record", HEADER + GOOD1 + "2\tKofi\t30");
        expectRejected("END count too high", HEADER + GOOD1 + "END\t5\n");
        expectRejected("END count too low", HEADER + GOOD1 + GOOD2 + "END\t1\n");
        expectRejected("END count not a number", HEADER + GOOD1 + "END\tmany\n");
        expectRejected("END line without a count", HEADER + GOOD1 + "END\n");
        expectRejected("data after END", HEADER + GOOD1 + "END\t1\n" + GOOD2);
        expectRejected("blank line between records", HEADER + GOOD1 + "\n" + GOOD2 + "END\t2\n");
        expectRejected("duplicate patient ID", HEADER + GOOD1 + "1\tOther Person\t30\tMale\tFever\t3\t2\tWaiting\nEND\t2\n");
        expectRejected("duplicate arrival sequence", HEADER + GOOD1 + "2\tOther Person\t30\tMale\tFever\t3\t1\tWaiting\nEND\t2\n");
        expectRejected("two good records then a bad one", HEADER + GOOD1 + GOOD2 + "3\tBad\t999\tM\tC\t1\t3\tWaiting\nEND\t3\n");

        // All-or-nothing on an EMPTY system: nothing partially loaded.
        const fs::path p = workDir / "partial.txt";
        writeFile(p, HEADER + GOOD1 + GOOD2 + "3\tBad\t999\tM\tC\t1\t3\tWaiting\nEND\t3\n");
        HospitalManagementSystem sys(p.string());
        loadVia(sys);
        check(sys.getQueue().totalRegistered() == 0 && sys.getQueue().isEmpty(),
              "Two valid records before a bad one: NOTHING is loaded (all-or-nothing)");

        // Error message names the offending line.
        writeFile(p, HEADER + GOOD1 + "2\tBad\t999\tM\tC\t1\t3\tWaiting\nEND\t2\n");
        HospitalManagementSystem sys2(p.string());
        const Outcome load = loadVia(sys2);
        check(contains(load.text, "line 3") && contains(load.text, "age"), "Error message names the line number and the problem");
        std::cout << "  sample error: " << linesOf(load.text)[1] << "\n";

        // The path is a directory.
        const fs::path dir = workDir / "a_directory";
        fs::create_directories(dir);
        HospitalManagementSystem sys3(dir.string());
        const Outcome dirLoad = loadVia(sys3);
        check(!dirLoad.ok && contains(dirLoad.text, "not a regular file"), "Data path that is a directory: reported, no crash");
        check(!saveVia(sys3).ok, "Saving over a directory is refused");

        // Duplicate IDs are caught even if the file layer were bypassed.
        HospitalQueue q;
        q.registerPatient("Existing", 30, "F", "Cold", 3);
        std::vector<PatientRecord> dupId;
        dupId.push_back(PatientRecord{Patient(1, "A", 30, "F", "c", PriorityLevel::Urgent, 1), PatientStatus::Waiting});
        dupId.push_back(PatientRecord{Patient(1, "B", 30, "F", "c", PriorityLevel::Urgent, 2), PatientStatus::Served});
        std::vector<PatientRecord> dupSeq;
        dupSeq.push_back(PatientRecord{Patient(1, "A", 30, "F", "c", PriorityLevel::Urgent, 7), PatientStatus::Waiting});
        dupSeq.push_back(PatientRecord{Patient(2, "B", 30, "F", "c", PriorityLevel::Urgent, 7), PatientStatus::Waiting});
        bool idThrew = false, seqThrew = false;
        try { q.loadRecords(dupId); } catch (const std::invalid_argument&) { idThrew = true; }
        try { q.loadRecords(dupSeq); } catch (const std::invalid_argument&) { seqThrew = true; }
        check(idThrew && seqThrew, "HospitalQueue::loadRecords rejects duplicate IDs and duplicate sequences");
        check(q.totalRegistered() == 1 && q.size() == 1 && q.peekNext().getFullName() == "Existing",
              "Failed loadRecords left the queue exactly as it was");
    }

    std::cout << "--- B9: valid variations that must load ---\n";
    {
        const fs::path p = workDir / "variations.txt";
        std::string crlf = HEADER + GOOD1 + GOOD2 + "END\t2\n";
        std::string windows;
        for (const char c : crlf) { if (c == '\n') windows += '\r'; windows += c; }
        writeFile(p, windows);
        HospitalManagementSystem sys(p.string());
        const Outcome load = loadVia(sys);
        check(load.ok && sys.getQueue().totalRegistered() == 2 && sys.getQueue().size() == 1 &&
              sys.getQueue().findPatient(2)->status == PatientStatus::Served,
              "File with Windows (CRLF) line endings loads correctly");

        writeFile(p, HEADER + "END\t0\n");
        HospitalManagementSystem sys2(p.string());
        check(loadVia(sys2).ok && sys2.getQueue().totalRegistered() == 0, "Valid file with zero records loads");
    }

    std::cout << "--- B10: never silently overwrite an unreadable file ---\n";
    {
        const fs::path p = workDir / "protected.txt";
        const std::string corrupt = HEADER + GOOD1 + "this line is damaged\nEND\t2\n";
        writeFile(p, corrupt);
        HospitalManagementSystem sys(p.string());
        check(!loadVia(sys).ok, "Corrupt file: load fails");
        registerVia(sys, "Brand New", 30, "F", "Cold", 3);
        const Outcome refused = saveVia(sys);
        check(!refused.ok && contains(refused.text, "Save refused") && readFile(p) == corrupt,
              "Save is refused and the corrupt file stays byte-for-byte unchanged");
        const Outcome forced = saveVia(sys, true);
        check(forced.ok && readFile(p) != corrupt, "Explicit overwrite is possible after confirmation");
        HospitalManagementSystem again(p.string());
        check(loadVia(again).ok && again.getQueue().totalRegistered() == 1, "The overwritten file is valid and loads");
        check(saveVia(sys).ok, "After a successful overwrite, normal saving works again");
    }

    std::cout << "--- B11: save failures are handled and never damage the existing file ---\n";
    {
        // (a) parent 'folder' is actually a regular file.
        writeFile(workDir / "iamafile", "x");
        HospitalManagementSystem a((workDir / "iamafile" / "patients.txt").string());
        const Outcome ra = saveVia(a);
        check(!ra.ok && contains(ra.text, "Error"), "Save below a regular file fails with an error message");

        // (b) the temporary file name is blocked: previous file must survive.
        const fs::path p = workDir / "survivor.txt";
        HospitalManagementSystem b(p.string());
        registerVia(b, "First Version", 30, "F", "Cold", 3);
        check(saveVia(b).ok, "First save works");
        const std::string before = readFile(p);
        fs::create_directories(fs::path(p.string() + ".tmp"));              // block the temp file name
        registerVia(b, "Second Version", 31, "M", "Cough", 2);
        const Outcome rb = saveVia(b);
        check(!rb.ok && readFile(p) == before, "Failed save leaves the previous file byte-for-byte intact");
        check(fs::is_directory(fs::path(p.string() + ".tmp")), "The unrelated blocking folder was not deleted");
        fs::remove_all(fs::path(p.string() + ".tmp"));
        check(saveVia(b).ok && loadVia(b).ok, "Saving works again once the problem is removed");

        // (c) the target itself is a directory.
        const fs::path dir = workDir / "target_dir";
        fs::create_directories(dir);
        HospitalManagementSystem c(dir.string());
        registerVia(c, "X", 30, "F", "Cold", 3);
        const Outcome rc = saveVia(c);
        check(!rc.ok && !fs::exists(fs::path(dir.string() + ".tmp")), "Save onto a directory fails cleanly with no leftover temp file");
    }

    std::cout << "--- B12: repeated save/load never creates duplicates ---\n";
    {
        const fs::path p = workDir / "repeat.txt";
        HospitalManagementSystem sys(p.string());
        buildScenario(sys);
        saveVia(sys);
        const std::string firstBytes = readFile(p);
        bool identicalBytes = true, identicalRecords = true, noDuplicates = true;
        HospitalManagementSystem current(p.string());
        loadVia(current);
        for (int cycle = 0; cycle < 5; ++cycle) {
            saveVia(current);
            if (readFile(p) != firstBytes) identicalBytes = false;
            HospitalManagementSystem next(p.string());
            loadVia(next);
            if (!allRecordsEqual(sys.getQueue().getAllRecords(), next.getQueue().getAllRecords())) identicalRecords = false;
            std::set<int> ids;
            for (const PatientRecord& r : next.getQueue().getAllRecords()) ids.insert(r.patient.getId());
            if (ids.size() != 7 || next.getQueue().totalRegistered() != 7) noDuplicates = false;
            current = next;
        }
        check(identicalBytes, "Five save/load cycles produce a byte-identical file every time");
        check(identicalRecords && noDuplicates, "Records identical and exactly 7 patients after every cycle");

        // Loading twice into the same system replaces instead of merging.
        loadVia(current);
        loadVia(current);
        check(current.getQueue().totalRegistered() == 7 && current.getQueue().size() == 4,
              "Loading the same file twice into one system still yields exactly 7 patients");

        // Growing registry: each cycle adds exactly one patient.
        HospitalManagementSystem growing(p.string());
        loadVia(growing);
        bool growsByOne = true;
        for (int i = 1; i <= 4; ++i) {
            registerVia(growing, "Extra" + std::to_string(i), 30, "F", "Cold", 3);
            saveVia(growing);
            HospitalManagementSystem reloaded(p.string());
            loadVia(reloaded);
            if (reloaded.getQueue().totalRegistered() != static_cast<std::size_t>(7 + i)) growsByOne = false;
            if (reloaded.getQueue().findPatient(7 + i) == std::nullopt) growsByOne = false;
            growing = reloaded;
        }
        check(growsByOne, "Register + save + reload adds exactly one new patient each cycle, with consecutive IDs");
    }

    std::cout << "--- B13: saving and loading an empty registry ---\n";
    {
        const fs::path p = workDir / "empty_registry.txt";
        HospitalManagementSystem sys(p.string());
        const Outcome save = saveVia(sys);
        check(save.ok && contains(save.text, "Saved 0 patient record(s)"), "Empty registry saves");
        check(readFile(p) == "HQMS-PATIENT-RECORDS\t1\nEND\t0\n", "File contains just the header and END 0");
        HospitalManagementSystem restored(p.string());
        const Outcome load = loadVia(restored);
        check(load.ok && contains(load.text, "Loaded 0 patient record(s)") && restored.getQueue().totalRegistered() == 0 &&
              restored.getQueue().isEmpty(), "Empty registry loads back as empty");
        registerVia(restored, "First Ever", 30, "F", "Cold", 3);
        check(restored.getQueue().findPatient(1).has_value() && restored.getQueue().findPatient(1)->patient.getArrivalSequence() == 1,
              "IDs and sequences start at 1 after loading an empty registry");
    }

    std::cout << "--- B14: loading replaces the current registry (never merges) ---\n";
    {
        const fs::path p = workDir / "replace.txt";
        HospitalManagementSystem source(p.string());
        registerVia(source, "File One", 30, "F", "Cold", 1);
        registerVia(source, "File Two", 30, "F", "Cold", 2);
        registerVia(source, "File Three", 30, "F", "Cold", 3);
        saveVia(source);
        HospitalManagementSystem target(p.string());
        registerVia(target, "Local A", 30, "F", "Cold", 3);
        registerVia(target, "Local B", 30, "F", "Cold", 3);
        loadVia(target);
        check(target.getQueue().totalRegistered() == 3 && target.getQueue().findPatient(1)->patient.getFullName() == "File One",
              "Registry after load equals the file exactly (3 patients, no merge with the 2 local ones)");
    }

    std::cout << "--- B15: defaults and the project data folder ---\n";
    {
        HospitalManagementSystem sys;
        check(sys.getDataFilePath() == "data/patients.txt", "Default data file is data/patients.txt");
    }


    return testfw::finish();
}
