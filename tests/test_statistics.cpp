// tests/test_statistics.cpp
// Statistics tests: counts of registered / waiting / served / cancelled patients and waiting patients per priority,
// the displayed statistics text, and a randomized comparison with an independent reference model.

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
    testfw::TempDir temp("statistics");
    workDir = temp.path();

    // =====================================================================
    // PART A: STATISTICS
    // =====================================================================
    std::cout << "--- A1: statistics of an empty system ---\n";
    {
        HospitalManagementSystem sys((workDir / "unused.txt").string());
        const QueueStatistics s = sys.getQueue().getStatistics();
        check(s.totalRegistered == 0 && s.waiting == 0 && s.served == 0 && s.cancelled == 0 &&
              s.waitingEmergency == 0 && s.waitingUrgent == 0 && s.waitingNormal == 0 && s.isConsistent(),
              "All statistics are zero and consistent");
    }

    std::cout << "--- A2: statistics after registration, serving and cancellation ---\n";
    {
        HospitalManagementSystem sys((workDir / "unused.txt").string());
        registerVia(sys, "Normal-A", 30, "F", "Cold", 3);       // 1
        registerVia(sys, "Urgent-A", 40, "M", "Fracture", 2);   // 2
        registerVia(sys, "Emergency-A", 50, "F", "Chest", 1);   // 3
        registerVia(sys, "Normal-B", 20, "M", "Sprain", 3);     // 4
        registerVia(sys, "Urgent-B", 60, "F", "Burn", 2);       // 5
        registerVia(sys, "Emergency-B", 10, "M", "Bleed", 1);   // 6
        registerVia(sys, "Normal-C", 35, "F", "Rash", 3);       // 7
        QueueStatistics s = sys.getQueue().getStatistics();
        check(s.totalRegistered == 7 && s.waiting == 7 && s.served == 0 && s.cancelled == 0 &&
              s.waitingEmergency == 2 && s.waitingUrgent == 2 && s.waitingNormal == 3 && s.isConsistent(),
              "After registering 7: 7 registered, 7 waiting (E2 U2 N3)");

        serveQuiet(sys);                                         // serves Emergency-A (3)
        s = sys.getQueue().getStatistics();
        check(s.totalRegistered == 7 && s.waiting == 6 && s.served == 1 && s.cancelled == 0 &&
              s.waitingEmergency == 1 && s.waitingUrgent == 2 && s.waitingNormal == 3 && s.isConsistent(),
              "After serving 1: served patient is not counted as waiting (E1 U2 N3)");

        cancelQuiet(sys, 5);                                     // cancel Urgent-B
        s = sys.getQueue().getStatistics();
        check(s.totalRegistered == 7 && s.waiting == 5 && s.served == 1 && s.cancelled == 1 &&
              s.waitingEmergency == 1 && s.waitingUrgent == 1 && s.waitingNormal == 3 && s.isConsistent(),
              "After cancelling an Urgent patient: not counted as waiting (E1 U1 N3)");

        cancelQuiet(sys, 7);                                     // cancel Normal-C
        s = sys.getQueue().getStatistics();
        check(s.totalRegistered == 7 && s.waiting == 4 && s.served == 1 && s.cancelled == 2 &&
              s.waitingEmergency == 1 && s.waitingUrgent == 1 && s.waitingNormal == 2 && s.isConsistent(),
              "After cancelling a Normal patient (E1 U1 N2)");

        cancelQuiet(sys, 3);                                     // refused: already served
        cancelQuiet(sys, 5);                                     // refused: already cancelled
        cancelQuiet(sys, 99);                                    // refused: unknown
        check(statsEqual(s, sys.getQueue().getStatistics()), "Refused cancellations do not change statistics");

        const std::string text = statsText(sys);
        check(contains(text, "Total patients registered   : 7") &&
              contains(text, "Patients currently waiting  : 4") &&
              contains(text, "Patients served             : 1") &&
              contains(text, "Registrations cancelled     : 2") &&
              contains(text, "Emergency (1)             : 1") &&
              contains(text, "Urgent (2)                : 1") &&
              contains(text, "Normal (3)                : 2"),
              "Statistics display shows all seven numbers");
        std::cout << text;
    }

    std::cout << "--- A3: statistics stress test (3000 random operations) ---\n";
    {
        struct Ref { int id; int prio; PatientStatus status; };
        std::mt19937 rng(424242);
        HospitalQueue q;
        std::vector<Ref> ref;
        std::string firstFailure;
        for (int step = 0; step < 3000 && firstFailure.empty(); ++step) {
            const int op = static_cast<int>(rng() % 10);
            if (op < 5) {
                const int prio = static_cast<int>(rng() % 3) + 1;
                const Patient p = q.registerPatient("P", 30, "F", "c", prio);
                ref.push_back({p.getId(), prio, PatientStatus::Waiting});
            } else if (op < 7) {
                if (!q.isEmpty()) {
                    const int id = q.popNext().getId();
                    for (Ref& r : ref) if (r.id == id) r.status = PatientStatus::Served;
                }
            } else {
                const int id = static_cast<int>(rng() % (ref.size() + 2));
                if (q.cancelPatient(id) == CancelResult::Cancelled)
                    for (Ref& r : ref) if (r.id == id) r.status = PatientStatus::Cancelled;
            }
            QueueStatistics want;
            want.totalRegistered = ref.size();
            for (const Ref& r : ref) {
                if (r.status == PatientStatus::Served) ++want.served;
                else if (r.status == PatientStatus::Cancelled) ++want.cancelled;
                else {
                    ++want.waiting;
                    if (r.prio == 1) ++want.waitingEmergency;
                    else if (r.prio == 2) ++want.waitingUrgent;
                    else ++want.waitingNormal;
                }
            }
            const QueueStatistics got = q.getStatistics();
            if (!statsEqual(got, want)) firstFailure = "statistics differ from reference at step " + std::to_string(step);
            else if (!got.isConsistent()) firstFailure = "invariant broken at step " + std::to_string(step);
        }
        check(firstFailure.empty(), "Statistics matched an independent reference after every operation" +
                                    (firstFailure.empty() ? std::string() : " (" + firstFailure + ")"));
    }


    return testfw::finish();
}
