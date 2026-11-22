// tests/test_registration.cpp
// Registration tests: automatic IDs and arrival sequences, input validation,
// failure safety, duplicate-ID protection, and the interactive registration
// dialogue with its confirmation message.

#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "HospitalManagementSystem.h"
#include "HospitalQueue.h"
#include "Patient.h"
#include "test_framework.h"

using namespace testfw;

// Calls registerPatient and reports whether it threw std::invalid_argument.
static bool rejected(HospitalQueue& q, const std::string& name, int age,
                     const std::string& gender, const std::string& cond, int prio) {
    try {
        q.registerPatient(name, age, gender, cond, prio);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    TempDir temp("registration");  // only used to give the system a data path; no file is written

    // =====================================================================
    // PART A: HospitalQueue::registerPatient
    // =====================================================================
    section("A1: seven patients, mixed and identical priorities");
    HospitalQueue q;
    // (name, priority) in arrival order: N, U, E, N, U, E, N
    const std::vector<std::pair<std::string, int>> arrivals = {
        {"Normal One", 3}, {"Urgent One", 2}, {"Emergency One", 1},
        {"Normal Two", 3}, {"Urgent Two", 2}, {"Emergency Two", 1},
        {"Normal Three", 3}};
    std::vector<Patient> registered;
    for (const auto& a : arrivals) {
        registered.push_back(q.registerPatient(a.first, 30, "Female", "Condition", a.second));
    }

    std::set<int> ids;
    bool idsConsecutive = true;
    bool seqConsecutive = true;
    bool fieldsOk = true;
    for (std::size_t i = 0; i < registered.size(); ++i) {
        ids.insert(registered[i].getId());
        if (registered[i].getId() != static_cast<int>(i) + 1) idsConsecutive = false;
        if (registered[i].getArrivalSequence() != static_cast<long>(i) + 1) seqConsecutive = false;
        if (registered[i].getFullName() != arrivals[i].first ||
            static_cast<int>(registered[i].getPriority()) != arrivals[i].second) {
            fieldsOk = false;
        }
    }
    check(ids.size() == 7, "All 7 patient IDs are unique");
    check(idsConsecutive, "IDs are generated automatically as 1..7");
    check(seqConsecutive, "Arrival sequences increase automatically 1..7");
    check(fieldsOk, "Returned patients carry the registered name and priority");
    check(q.size() == 7, "Every registration entered the queue (size 7)");
    check(q.totalRegistered() == 7, "Records hold all 7 registered patients");

    const std::vector<Patient> records = q.getRegisteredPatients();
    bool recordsMatch = records.size() == 7;
    for (std::size_t i = 0; recordsMatch && i < records.size(); ++i) {
        recordsMatch = records[i].getId() == registered[i].getId() &&
                       records[i].getFullName() == registered[i].getFullName() &&
                       records[i].getArrivalSequence() == registered[i].getArrivalSequence();
    }
    check(recordsMatch, "Records equal what registration returned, ordered by ID");

    section("A2: serving order from registered patients");
    std::vector<int> served;
    while (!q.isEmpty()) served.push_back(q.popNext().getId());
    // Emergency: 3,6  Urgent: 2,5  Normal: 1,4,7  (FIFO inside each level)
    check((served == std::vector<int>{3, 6, 2, 5, 1, 4, 7}),
          "Served E(3,6) then U(2,5) then N(1,4,7), FIFO within each level");
    check(q.totalRegistered() == 7, "Records persist after patients are served");

    section("A3: whitespace trimming and age boundaries");
    HospitalQueue t;
    Patient trimmed = t.registerPatient("  Kofi Boateng \t", 0, "  Male ", " Fever  ", 2);
    check(trimmed.getFullName() == "Kofi Boateng" && trimmed.getGender() == "Male" &&
              trimmed.getMedicalCondition() == "Fever",
          "Text fields are trimmed");
    check(trimmed.getAge() == 0, "Age 0 accepted");
    check(!rejected(t, "Old Person", 120, "F", "Checkup", 3), "Age 120 accepted");
    check(t.size() == 2 && t.totalRegistered() == 2, "Boundary registrations entered the queue");

    section("A4: invalid input is rejected and corrupts nothing");
    HospitalQueue v;
    v.registerPatient("First Valid", 25, "F", "Cough", 3);   // id 1, seq 1
    v.registerPatient("Second Valid", 40, "M", "Burn", 1);   // id 2, seq 2
    const int topBefore = v.peekNext().getId();

    struct Bad {
        std::string label;
        std::string name;
        int age;
        std::string gender;
        std::string cond;
        int prio;
    };
    const std::vector<Bad> badCases = {
        {"empty name", "", 30, "F", "Cough", 2},
        {"whitespace-only name", "   \t ", 30, "F", "Cough", 2},
        {"negative age (-1)", "Bad Age", -1, "F", "Cough", 2},
        {"very negative age", "Bad Age", -50, "F", "Cough", 2},
        {"age 121", "Bad Age", 121, "F", "Cough", 2},
        {"age 500", "Bad Age", 500, "F", "Cough", 2},
        {"empty gender", "Bad Gen", 30, "", "Cough", 2},
        {"empty condition", "Bad Cond", 30, "F", "  ", 2},
        {"priority 0", "Bad Pri", 30, "F", "Cough", 0},
        {"priority 4", "Bad Pri", 30, "F", "Cough", 4},
        {"priority -1", "Bad Pri", 30, "F", "Cough", -1},
        {"priority 99", "Bad Pri", 30, "F", "Cough", 99},
    };
    for (const Bad& b : badCases) {
        const std::size_t waitingBefore = v.size();
        const std::size_t recordsBefore = v.totalRegistered();
        const bool threw = rejected(v, b.name, b.age, b.gender, b.cond, b.prio);
        check(threw && v.size() == waitingBefore && v.totalRegistered() == recordsBefore,
              "Rejected, queue and records unchanged: " + b.label);
    }
    check(v.peekNext().getId() == topBefore, "Next patient to serve unchanged after all failures");
    Patient afterFailures = v.registerPatient("Third Valid", 33, "M", "Fracture", 2);
    check(afterFailures.getId() == 3 && afterFailures.getArrivalSequence() == 3,
          "Failures consumed no IDs/sequences: next patient gets ID 3, sequence 3");
    check(v.size() == 3 && v.totalRegistered() == 3, "Exactly the 3 valid patients are registered");

    section("A5: duplicate IDs and generator safety (insertPatient)");
    HospitalQueue d;
    d.registerPatient("Registered One", 20, "F", "Rash", 3);   // id 1
    bool dupThrew = false;
    try {
        d.insertPatient(Patient(1, "Impostor", 20, "M", "X", PriorityLevel::Emergency, 99));
    } catch (const std::invalid_argument&) {
        dupThrew = true;
    }
    check(dupThrew, "insertPatient rejects a duplicate patient ID");
    check(d.size() == 1 && d.totalRegistered() == 1 && d.peekNext().getFullName() == "Registered One",
          "Duplicate attempt left the queue and records unchanged");
    d.insertPatient(Patient(50, "External Fifty", 60, "M", "Stroke", PriorityLevel::Emergency, 40));
    Patient next = d.registerPatient("After External", 22, "F", "Sprain", 3);
    check(next.getId() == 51 && next.getArrivalSequence() == 41,
          "Generators move past externally inserted IDs (51) and sequences (41)");
    check(d.peekNext().getId() == 50, "Externally inserted Emergency patient is first in line");

    // =====================================================================
    // PART B: HospitalManagementSystem::registerPatientInteractive
    // =====================================================================
    section("B1: valid interactive registration + confirmation");
    HospitalManagementSystem sys((temp.path() / "patients.txt").string());
    {
        std::istringstream in("Ama Mensah\n41\nFemale\nChest pain\n1\n");
        std::ostringstream out;
        const bool ok = sys.registerPatientInteractive(in, out);
        const std::string text = out.str();
        check(ok && sys.getQueue().size() == 1, "Registration succeeded and patient is queued");
        check(contains(text, "REGISTRATION SUCCESSFUL"), "Confirmation shows success banner");
        check(contains(text, "Patient ID       : 1"), "Confirmation shows patient ID");
        check(contains(text, "Patient Name     : Ama Mensah"), "Confirmation shows patient name");
        check(contains(text, "Priority         : Emergency (1)"), "Confirmation shows assigned priority");
        check(contains(text, "Arrival Sequence : 1"), "Confirmation shows arrival sequence");
        check(contains(text, "registered successfully"), "Confirmation shows success message");
    }

    section("B2: invalid entries are re-prompted, then accepted");
    {
        const std::string script =
            "\n"             // blank name
            "   \n"          // whitespace name
            "Kwame Asante\n"
            "abc\n"          // age: not a number
            "-5\n"           // age: negative
            "121\n"          // age: too large
            "3.5\n"          // age: decimal
            "30x\n"          // age: trailing junk
            "99999999999\n"  // age: overflow
            "\n"             // age: empty
            "30\n"
            "\n"             // blank gender
            "Male\n"
            "  \n"           // blank condition
            "Fever\n"
            "0\n"            // priority too low
            "4\n"            // priority too high
            "x\n"            // priority not a number
            "2.0\n"          // priority decimal
            "3\n";
        std::istringstream in(script);
        std::ostringstream out;
        const bool ok = sys.registerPatientInteractive(in, out);
        const std::string text = out.str();
        check(ok, "Registration succeeded after invalid entries");
        check(contains(text, "Name cannot be empty."), "Empty-name error shown");
        check(contains(text, "Age must be a whole number between 0 and 120."), "Age error shown");
        check(contains(text, "Gender cannot be empty."), "Empty-gender error shown");
        check(contains(text, "Medical condition cannot be empty."), "Empty-condition error shown");
        check(contains(text, "Priority must be 1, 2 or 3."), "Priority error shown");
        check(sys.getQueue().size() == 2 && sys.getQueue().totalRegistered() == 2,
              "Only ONE new patient was added despite many invalid attempts");
        const std::vector<Patient> all = sys.getQueue().getRegisteredPatients();
        const Patient& p = all.back();
        check(p.getId() == 2 && p.getArrivalSequence() == 2 && p.getFullName() == "Kwame Asante" &&
                  p.getAge() == 30 && p.getGender() == "Male" && p.getMedicalCondition() == "Fever" &&
                  p.getPriority() == PriorityLevel::Normal,
              "Stored patient has the final valid values, ID 2, sequence 2");
    }

    section("B3: input ends mid-registration");
    {
        std::istringstream in("Incomplete Person\n30\n");   // stops before gender
        std::ostringstream out;
        const bool ok = sys.registerPatientInteractive(in, out);
        check(!ok, "Returns false when input ends early");
        check(contains(out.str(), "Registration cancelled"), "Cancellation message shown");
        check(sys.getQueue().size() == 2 && sys.getQueue().totalRegistered() == 2,
              "Cancelled registration added nothing");
    }
    {
        std::istringstream in("");                           // completely empty input
        std::ostringstream out;
        check(!sys.registerPatientInteractive(in, out), "Completely empty input handled safely");
    }

    section("B4: next registration after a cancelled one");
    {
        std::istringstream in("Efua Owusu\r\n 25 \r\nFemale\r\nMalaria\r\n2\r\n");  // CRLF, padded number
        std::ostringstream out;
        const bool ok = sys.registerPatientInteractive(in, out);
        check(ok, "Handles CRLF line endings and padded numbers");
        const Patient last = sys.getQueue().getRegisteredPatients().back();
        check(last.getId() == 3 && last.getArrivalSequence() == 3 && last.getFullName() == "Efua Owusu",
              "Cancelled attempts consumed no ID: next patient is ID 3, sequence 3");
        check(sys.getQueue().peekNext().getId() == 1, "Emergency patient (ID 1) is still first in line");
    }

    return finish();
}
