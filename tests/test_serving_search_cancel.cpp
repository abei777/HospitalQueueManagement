// tests/test_serving_search_cancel.cpp
// Serving, searching, cancellation and waiting-list display tests, including
// lazy deletion, status management, the "no duplicate records" guarantee and a
// randomized comparison against an independent reference model.

#include <algorithm>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "HospitalManagementSystem.h"
#include "HospitalQueue.h"
#include "Patient.h"
#include "test_framework.h"

using namespace testfw;

static bool popThrowsOutOfRange(HospitalQueue& q) {
    try {
        q.popNext();
    } catch (const std::out_of_range&) {
        return true;
    }
    return false;
}

static std::vector<int> ids(const std::vector<Patient>& patients) {
    std::vector<int> result;
    for (const Patient& p : patients) result.push_back(p.getId());
    return result;
}

// Drains a queue by serving everyone, returning the IDs in serving order.
static std::vector<int> serveAll(HospitalQueue& q) {
    std::vector<int> order;
    while (!q.isEmpty()) order.push_back(q.popNext().getId());
    return order;
}

static bool invariantHolds(const HospitalQueue& q) {
    return q.size() + q.totalServed() + q.totalCancelled() == q.totalRegistered();
}

// ---- helpers that drive the user-facing functions with scripted input -------
static void registerVia(HospitalManagementSystem& s, const std::string& name, int age,
                        const std::string& gender, const std::string& cond, int prio) {
    std::istringstream in(name + "\n" + std::to_string(age) + "\n" + gender + "\n" + cond +
                          "\n" + std::to_string(prio) + "\n");
    std::ostringstream out;
    s.registerPatientInteractive(in, out);
}

struct IdResult {
    bool ok;
    std::string text;
};

static IdResult searchVia(HospitalManagementSystem& s, const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    const bool ok = s.searchPatient(in, out);
    return {ok, out.str()};
}

static IdResult cancelVia(HospitalManagementSystem& s, const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    const bool ok = s.cancelPatient(in, out);
    return {ok, out.str()};
}

static IdResult serveVia(HospitalManagementSystem& s) {
    std::ostringstream out;
    const bool ok = s.serveNextPatient(out);
    return {ok, out.str()};
}

static std::string displayVia(const HospitalManagementSystem& s) {
    std::ostringstream out;
    s.displayWaitingPatients(out);
    return out.str();
}

int main() {
    TempDir temp("serving");  // gives the systems a data path; no file is ever written here

    // =====================================================================
    // PART A: HospitalQueue
    // =====================================================================
    section("A1: serving Emergency, Urgent, Normal");
    {
        HospitalQueue q;
        q.registerPatient("N", 30, "F", "c", 3);   // id 1
        q.registerPatient("U", 30, "F", "c", 2);   // id 2
        q.registerPatient("E", 30, "F", "c", 1);   // id 3
        const Patient first = q.popNext();
        check(first.getId() == 3 && first.getPriority() == PriorityLevel::Emergency,
              "1st served is Emergency although it arrived last");
        check(q.popNext().getId() == 2, "2nd served is Urgent");
        check(q.popNext().getId() == 1, "3rd served is Normal");
        check(q.isEmpty() && q.size() == 0, "Queue empty after serving all");
        check(q.totalServed() == 3 && q.totalCancelled() == 0 && q.totalRegistered() == 3,
              "Counters: 3 served, 0 cancelled, 3 registered");
        check(q.findPatient(3)->status == PatientStatus::Served, "Served patient recorded as Served");
        check(invariantHolds(q), "Invariant waiting+served+cancelled == registered");
    }

    section("A2: FIFO among equal priorities");
    {
        HospitalQueue q;
        for (int i = 0; i < 4; ++i) q.registerPatient("E" + std::to_string(i), 30, "F", "c", 1);  // 1-4
        for (int i = 0; i < 4; ++i) q.registerPatient("U" + std::to_string(i), 30, "F", "c", 2);  // 5-8
        for (int i = 0; i < 4; ++i) q.registerPatient("N" + std::to_string(i), 30, "F", "c", 3);  // 9-12
        check((serveAll(q) == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}),
              "Equal priorities served in arrival order within each level");
    }

    section("A3: serving from an empty queue");
    {
        HospitalQueue q;
        check(popThrowsOutOfRange(q), "popNext on a new queue throws out_of_range");
        q.registerPatient("Only", 30, "F", "c", 2);
        q.popNext();
        check(popThrowsOutOfRange(q), "popNext after everyone was served throws out_of_range");
        check(q.totalServed() == 1 && q.totalRegistered() == 1 && invariantHolds(q),
              "Failed serves changed nothing");
    }

    section("A4: getWaitingPatients is a true, non-destructive view");
    {
        HospitalQueue q;
        const int prios[] = {3, 1, 2, 3, 2, 1, 3};
        for (int p : prios) q.registerPatient("P", 30, "F", "c", p);   // ids 1..7
        const std::size_t sizeBefore = q.size();
        const int nextBefore = q.peekNext().getId();
        const std::vector<int> shown = ids(q.getWaitingPatients());
        const std::vector<int> shownAgain = ids(q.getWaitingPatients());
        check(q.size() == sizeBefore && q.peekNext().getId() == nextBefore,
              "Listing did not change the size or the next patient");
        check(shown == shownAgain, "Listing twice gives the same result");
        check(shown.size() == 7, "All 7 waiting patients are listed");
        check(shown == (std::vector<int>{2, 6, 3, 5, 1, 4, 7}),
              "Listed in serving order: E(2,6) U(3,5) N(1,4,7)");
        check(serveAll(q) == shown, "Actual serving order equals the displayed order");
    }

    section("A5: findPatient");
    {
        HospitalQueue q;
        q.registerPatient("Waiting Wanda", 31, "F", "Cough", 3);   // 1
        q.registerPatient("Served Sam", 45, "M", "Burn", 1);       // 2
        q.popNext();                                               // serves id 2
        const auto waiting = q.findPatient(1);
        const auto served = q.findPatient(2);
        check(waiting && waiting->status == PatientStatus::Waiting &&
                  waiting->patient.getFullName() == "Waiting Wanda",
              "Finds a waiting patient");
        check(served && served->status == PatientStatus::Served &&
                  served->patient.getFullName() == "Served Sam",
              "Finds a served patient");
        check(!q.findPatient(999) && !q.findPatient(0) && !q.findPatient(-5),
              "Unknown IDs (999, 0, -5) return nothing");
    }

    section("A6: cancellation outcomes");
    {
        HospitalQueue q;
        q.registerPatient("A", 30, "F", "c", 3);   // 1
        q.registerPatient("B", 30, "F", "c", 2);   // 2
        q.registerPatient("C", 30, "F", "c", 1);   // 3
        q.popNext();                               // serves 3 (C)
        check(q.cancelPatient(3) == CancelResult::AlreadyServed, "Served patient cannot be cancelled");
        check(q.findPatient(3)->status == PatientStatus::Served && q.totalCancelled() == 0,
              "Refused cancel left the patient Served");
        check(q.cancelPatient(2) == CancelResult::Cancelled, "Waiting patient is cancelled");
        check(q.findPatient(2)->status == PatientStatus::Cancelled, "Status is now Cancelled");
        check(q.findPatient(2)->patient.getFullName() == "B", "Cancelled patient's record is preserved");
        check(q.cancelPatient(2) == CancelResult::AlreadyCancelled, "Cancelling twice is refused");
        check(q.cancelPatient(777) == CancelResult::NotFound, "Unknown ID reports NotFound");
        check(q.size() == 1 && q.totalServed() == 1 && q.totalCancelled() == 1 && q.totalRegistered() == 3,
              "Counters: 1 waiting, 1 served, 1 cancelled, 3 registered");
        check(ids(q.getWaitingPatients()) == std::vector<int>{1}, "Cancelled patient not in waiting list");
        check(invariantHolds(q), "Invariant holds after cancellations");
    }

    section("A7: cancelled patients are never served");
    {
        // Cancel the patient who is NEXT in line.
        HospitalQueue q;
        q.registerPatient("N", 30, "F", "c", 3);   // 1
        q.registerPatient("E", 30, "F", "c", 1);   // 2  (next in line)
        q.registerPatient("U", 30, "F", "c", 2);   // 3
        q.cancelPatient(2);
        check(q.peekNext().getId() == 3, "Cancelling the front patient promotes the next one immediately");
        check((serveAll(q) == std::vector<int>{3, 1}), "Cancelled front patient is never served");
    }
    {
        // Cancel a patient in the MIDDLE of the heap (becomes a ghost entry).
        HospitalQueue q;
        for (int i = 0; i < 6; ++i) q.registerPatient("P", 30, "F", "c", 2);   // ids 1-6, same priority
        q.cancelPatient(3);
        q.cancelPatient(5);
        check(q.size() == 4, "Two mid-queue cancellations leave 4 waiting");
        check((ids(q.getWaitingPatients()) == std::vector<int>{1, 2, 4, 6}), "Listing skips cancelled patients");
        check((serveAll(q) == std::vector<int>{1, 2, 4, 6}), "Serving skips mid-queue cancelled patients");
        check(popThrowsOutOfRange(q), "Nothing left to serve afterwards");
    }
    {
        // Cancel the LAST patient in line, and then everyone.
        HospitalQueue q;
        q.registerPatient("A", 30, "F", "c", 1);   // 1
        q.registerPatient("B", 30, "F", "c", 3);   // 2 (last)
        q.cancelPatient(2);
        check((serveAll(q) == std::vector<int>{1}), "Cancelled last-in-line patient is never served");
        HospitalQueue all;
        for (int i = 0; i < 5; ++i) all.registerPatient("P", 30, "F", "c", 1 + i % 3);
        for (int id = 1; id <= 5; ++id) all.cancelPatient(id);
        check(all.isEmpty() && all.size() == 0, "Cancelling everyone empties the queue");
        check(popThrowsOutOfRange(all), "Serving when everyone cancelled throws out_of_range");
        all.registerPatient("Late", 30, "F", "c", 3);
        check(all.popNext().getId() == 6, "A patient registered afterwards is served normally");
        check(all.totalCancelled() == 5 && all.totalServed() == 1 && invariantHolds(all),
              "Counters consistent after cancelling everyone");
    }

    section("A8: no duplicate records");
    {
        HospitalQueue q;
        q.registerPatient("A", 30, "F", "c", 2);   // 1
        q.registerPatient("B", 30, "F", "c", 2);   // 2
        q.cancelPatient(1);
        q.popNext();                               // serves 2
        bool dupRejected = true;
        for (int id = 1; id <= 2; ++id) {          // cancelled and served IDs
            try {
                q.insertPatient(Patient(id, "Clone", 20, "F", "c", PriorityLevel::Emergency, 50 + id));
                dupRejected = false;
            } catch (const std::invalid_argument&) {
            }
        }
        check(dupRejected, "Re-using a cancelled or served ID is rejected");
        check(q.totalRegistered() == 2 && q.size() == 0 && q.totalCancelled() == 1 && q.totalServed() == 1,
              "Rejected duplicates changed nothing");
        const std::vector<Patient> all = q.getRegisteredPatients();
        std::set<int> unique;
        for (const Patient& p : all) unique.insert(p.getId());
        check(all.size() == 2 && unique.size() == 2, "Registry lists each patient exactly once, any status");
    }

    section("A9: randomized model-based stress test (4000 operations)");
    {
        struct Ref {
            int id;
            int prio;
            long seq;
            PatientStatus status;
        };
        std::mt19937 rng(20261120);
        HospitalQueue q;
        std::vector<Ref> ref;
        std::string firstFailure;
        int registers = 0;
        int serves = 0;
        int cancels = 0;

        auto expectedOrder = [&ref]() {
            std::vector<Ref> waiting;
            for (const Ref& r : ref) {
                if (r.status == PatientStatus::Waiting) waiting.push_back(r);
            }
            std::sort(waiting.begin(), waiting.end(), [](const Ref& a, const Ref& b) {
                return a.prio != b.prio ? a.prio < b.prio : a.seq < b.seq;
            });
            std::vector<int> order;
            for (const Ref& r : waiting) order.push_back(r.id);
            return order;
        };

        for (int step = 0; step < 4000 && firstFailure.empty(); ++step) {
            const int op = static_cast<int>(rng() % 10);
            if (op < 5) {                                           // register
                const int prio = static_cast<int>(rng() % 3) + 1;
                const Patient p = q.registerPatient("P", 30, "F", "c", prio);
                ref.push_back({p.getId(), prio, p.getArrivalSequence(), PatientStatus::Waiting});
                ++registers;
            } else if (op < 7) {                                    // serve
                const std::vector<int> expected = expectedOrder();
                if (expected.empty()) {
                    if (!popThrowsOutOfRange(q)) firstFailure = "serve on empty should throw";
                } else {
                    const int got = q.popNext().getId();
                    if (got != expected.front()) firstFailure = "wrong patient served";
                    for (Ref& r : ref) {
                        if (r.id == expected.front()) r.status = PatientStatus::Served;
                    }
                    ++serves;
                }
            } else {                                                // cancel (also unknown IDs)
                const int id = static_cast<int>(rng() % (ref.size() + 3));
                CancelResult want = CancelResult::NotFound;
                for (Ref& r : ref) {
                    if (r.id != id) continue;
                    if (r.status == PatientStatus::Served) {
                        want = CancelResult::AlreadyServed;
                    } else if (r.status == PatientStatus::Cancelled) {
                        want = CancelResult::AlreadyCancelled;
                    } else {
                        want = CancelResult::Cancelled;
                        r.status = PatientStatus::Cancelled;
                    }
                }
                if (q.cancelPatient(id) != want) firstFailure = "wrong cancel result";
                if (want == CancelResult::Cancelled) ++cancels;
            }
            // Verify the whole state against the reference after every step.
            const std::vector<int> expected = expectedOrder();
            if (firstFailure.empty() && ids(q.getWaitingPatients()) != expected)
                firstFailure = "waiting list differs from reference";
            if (firstFailure.empty() && q.size() != expected.size())
                firstFailure = "size differs from reference";
            if (firstFailure.empty() && !expected.empty() && q.peekNext().getId() != expected.front())
                firstFailure = "peekNext differs from reference";
            if (firstFailure.empty() && !invariantHolds(q))
                firstFailure = "counter invariant broken";
            if (firstFailure.empty() && q.isEmpty() != expected.empty())
                firstFailure = "isEmpty differs from reference";
        }
        std::cout << "  operations: " << registers << " registers, " << serves << " serves, "
                  << cancels << " cancels, " << ref.size() << " patients registered\n";
        check(firstFailure.empty(), "Queue matched the reference model after every operation" +
                                        (firstFailure.empty() ? std::string() : " (" + firstFailure + ")"));
        // Drain the rest: it must equal the reference order exactly.
        const std::vector<int> expected = expectedOrder();
        check(serveAll(q) == expected, "Final drain matches the reference order exactly");
    }

    // =====================================================================
    // PART B: HospitalManagementSystem user-facing functions
    // =====================================================================
    section("B1: serving and displaying with an empty queue");
    HospitalManagementSystem sys((temp.path() / "patients.txt").string());
    {
        const IdResult r = serveVia(sys);
        check(!r.ok && contains(r.text, "no patients waiting"),
              "Serve on empty queue: friendly message, returns false");
        check(contains(displayVia(sys), "No patients are currently waiting."),
              "Display on empty queue: friendly message");
    }

    registerVia(sys, "Normal-A", 30, "Female", "Cold", 3);          // 1
    registerVia(sys, "Urgent-A", 40, "Male", "Fracture", 2);        // 2
    registerVia(sys, "Emergency-A", 50, "Female", "Chest pain", 1); // 3
    registerVia(sys, "Normal-B", 20, "Male", "Sprain", 3);          // 4
    registerVia(sys, "Urgent-B", 60, "Female", "Burn", 2);          // 5
    registerVia(sys, "Emergency-B", 10, "Male", "Bleeding", 1);     // 6
    // Serving order: 3, 6, 2, 5, 1, 4

    section("B2: display shows all fields in serving order, without changing the queue");
    {
        const std::size_t sizeBefore = sys.getQueue().size();
        const int nextBefore = sys.getQueue().peekNext().getId();
        const std::string text = displayVia(sys);
        const char* orderedNames[] = {"Emergency-A", "Emergency-B", "Urgent-A",
                                      "Urgent-B", "Normal-A", "Normal-B"};
        bool inOrder = true;
        std::size_t lastPos = 0;
        for (const char* name : orderedNames) {
            const std::size_t pos = text.find(name);
            if (pos == std::string::npos || pos < lastPos) inOrder = false;
            lastPos = pos;
        }
        check(inOrder, "Names appear in serving order E-A, E-B, U-A, U-B, N-A, N-B");
        check(contains(text, "Chest pain") && contains(text, "Fracture") && contains(text, "Bleeding"),
              "Medical conditions are shown");
        check(contains(text, "Emergency") && contains(text, "Urgent") && contains(text, "Normal"),
              "Priority labels are shown");
        check(contains(text, "Gender") && contains(text, "Female") && contains(text, "Male"),
              "Gender is shown");
        check(contains(text, "Total waiting: 6"), "Total waiting count shown");
        check(displayVia(sys) == text, "Displaying twice gives identical output");
        check(sys.getQueue().size() == sizeBefore && sys.getQueue().peekNext().getId() == nextBefore,
              "Displaying did not modify the queue");
        std::cout << text;
    }

    section("B3: long text cannot break the table");
    {
        HospitalManagementSystem longSys((temp.path() / "long.txt").string());
        registerVia(longSys, "A Very Long Patient Name That Keeps Going", 33, "NonBinaryOrOther", "Cond", 2);
        const std::string text = displayVia(longSys);
        check(contains(text, "A Very Long Patient... 33 ") && !contains(text, "That Keeps Going"),
              "Over-long name is cut to 22 characters ending in '...', Age column still aligned");
        check(contains(text, "NonBina... Urgent") && !contains(text, "NonBinaryOrOther"),
              "Over-long gender is cut to 10 characters ending in '...'");
    }

    section("B4: cancellation through the interface");
    {
        const IdResult ok = cancelVia(sys, "5\n");
        check(ok.ok && contains(ok.text, "Registration cancelled for patient ID 5") &&
                  contains(ok.text, "Urgent-B") && contains(ok.text, "will not be served"),
              "Cancelling waiting patient 5 succeeds with a clear message");
        const std::string text = displayVia(sys);
        check(!contains(text, "Urgent-B") && contains(text, "Total waiting: 5"),
              "Cancelled patient is gone from the waiting display");
        const IdResult again = cancelVia(sys, "5\n");
        check(!again.ok && contains(again.text, "already cancelled"), "Cancelling again is refused");
        const IdResult missing = cancelVia(sys, "99\n");
        check(!missing.ok && contains(missing.text, "No patient with ID 99 exists."), "Unknown ID is reported");
        const IdResult reprompt = cancelVia(sys, "abc\n0\n-3\n\n7\n");
        check(!reprompt.ok && contains(reprompt.text, "Patient ID must be a positive whole number.") &&
                  contains(reprompt.text, "No patient with ID 7 exists."),
              "Invalid ID text is re-prompted until a valid ID is entered");
        const IdResult eof = cancelVia(sys, "");
        check(!eof.ok && contains(eof.text, "Cancellation aborted"), "Ended input aborts safely");
        check(sys.getQueue().totalCancelled() == 1, "Only one cancellation happened in total");
    }

    section("B5: serving through the interface");
    {
        // Waiting now: 3, 6, 2, 1, 4  (5 cancelled)
        const IdResult first = serveVia(sys);
        check(first.ok && contains(first.text, "NOW SERVING") && contains(first.text, "Emergency-A") &&
                  contains(first.text, "Chest pain") && contains(first.text, "Female") &&
                  contains(first.text, "Age               : 50") && contains(first.text, "Emergency (1)") &&
                  contains(first.text, "Arrival Sequence  : 3") &&
                  contains(first.text, "Status            : Served") &&
                  contains(first.text, "Patients still waiting: 4"),
              "Serve shows complete patient information and marks them Served");
        std::cout << first.text;
        check(!contains(displayVia(sys), "Emergency-A"), "Served patient no longer in the waiting display");
        const IdResult refused = cancelVia(sys, "3\n");
        check(!refused.ok && contains(refused.text, "already been served"),
              "Served patient cannot be cancelled");
    }

    section("B6: searching through the interface");
    {
        // Waiting now: 6, 2, 1, 4. Served: 3. Cancelled: 5.
        const IdResult waiting = searchVia(sys, "2\n");
        check(waiting.ok && contains(waiting.text, "PATIENT FOUND") && contains(waiting.text, "Urgent-A") &&
                  contains(waiting.text, "Fracture") &&
                  contains(waiting.text, "Status            : Waiting") &&
                  contains(waiting.text, "Queue Position    : 2 of 4"),
              "Waiting patient: full details, status Waiting, position 2 of 4");
        const IdResult first = searchVia(sys, "6\n");
        check(contains(first.text, "Queue Position    : 1 of 4"), "Next patient has position 1");
        const IdResult served = searchVia(sys, "3\n");
        check(served.ok && contains(served.text, "Emergency-A") &&
                  contains(served.text, "Status            : Served") &&
                  !contains(served.text, "Queue Position"),
              "Served patient: status Served, no queue position");
        const IdResult cancelled = searchVia(sys, "5\n");
        check(cancelled.ok && contains(cancelled.text, "Urgent-B") &&
                  contains(cancelled.text, "Status            : Cancelled") &&
                  !contains(cancelled.text, "Queue Position"),
              "Cancelled patient: status Cancelled");
        const IdResult none = searchVia(sys, "999\n");
        check(!none.ok && contains(none.text, "No patient with ID 999 exists."),
              "Nonexistent ID reports not found");
        const IdResult messy = searchVia(sys, "xyz\n2.5\n\n1\n");
        check(messy.ok && contains(messy.text, "Normal-A"),
              "Invalid IDs are re-prompted, then the search works");
        const IdResult eof = searchVia(sys, "");
        check(!eof.ok && contains(eof.text, "Search cancelled"), "Ended input cancels the search safely");
    }

    section("B7: end-to-end - cancelled patients are never served");
    {
        cancelVia(sys, "4\n");                       // cancel Normal-B (waiting: 6, 2, 1)
        std::vector<std::string> served;
        for (int i = 0; i < 3; ++i) {
            const IdResult r = serveVia(sys);
            served.push_back(r.ok ? r.text : "");
        }
        check(contains(served[0], "Emergency-B") && contains(served[1], "Urgent-A") &&
                  contains(served[2], "Normal-A"),
              "Served in order Emergency-B, Urgent-A, Normal-A");
        const IdResult empty = serveVia(sys);
        check(!empty.ok && contains(empty.text, "no patients waiting"),
              "Cancelled patients are skipped: queue reports empty after the last real patient");
        check(sys.getQueue().size() == 0 && sys.getQueue().totalServed() == 4 &&
                  sys.getQueue().totalCancelled() == 2 && sys.getQueue().totalRegistered() == 6 &&
                  invariantHolds(sys.getQueue()),
              "Final counters: 0 waiting, 4 served, 2 cancelled, 6 registered");
        check(contains(searchVia(sys, "4\n").text, "Status            : Cancelled"),
              "Cancelled record is still searchable after the queue emptied");
    }

    return finish();
}
