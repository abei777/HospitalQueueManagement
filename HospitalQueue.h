// HospitalQueue.h
// Declares the HospitalQueue class: a priority queue of waiting patients.
//
// NOTE: This project is an educational queue-management simulation.
// It is NOT a clinically validated triage system.

#ifndef HOSPITAL_QUEUE_H
#define HOSPITAL_QUEUE_H

#include <cstddef>
#include <map>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "Patient.h"

// Life-cycle state of a registered patient. A patient starts as Waiting and
// moves to exactly one final state; Served and Cancelled never change again.
//
//   Waiting --(served)----> Served
//   Waiting --(cancelled)-> Cancelled
//
// The status is stored in the HospitalQueue registry rather than inside
// Patient, so Patient stays immutable (see Patient.h).
enum class PatientStatus {
    Waiting,
    Served,
    Cancelled
};

// Converts a PatientStatus to a label ("Waiting", "Served", "Cancelled").
std::string statusToString(PatientStatus status);

// Outcome of a cancellation request. A normal "no" (unknown ID, already
// served...) is an expected result, not an error, so it is returned as a
// value instead of being thrown.
enum class CancelResult {
    Cancelled,        // The waiting patient was cancelled
    NotFound,         // No patient has this ID
    AlreadyServed,    // The patient has already been served; cannot cancel
    AlreadyCancelled  // The registration was cancelled earlier
};

// One entry of the patient registry: the immutable patient data together
// with the patient's current status.
struct PatientRecord {
    Patient       patient;
    PatientStatus status;
};

// A snapshot of the queue's numbers at one moment.
// Invariants (always true):
//   totalRegistered == waiting + served + cancelled
//   waiting         == waitingEmergency + waitingUrgent + waitingNormal
struct QueueStatistics {
    std::size_t totalRegistered   = 0;  // Every patient ever registered
    std::size_t waiting           = 0;  // Status Waiting
    std::size_t served            = 0;  // Status Served
    std::size_t cancelled         = 0;  // Status Cancelled
    std::size_t waitingEmergency  = 0;  // Waiting patients with priority 1
    std::size_t waitingUrgent     = 0;  // Waiting patients with priority 2
    std::size_t waitingNormal     = 0;  // Waiting patients with priority 3

    // True if both invariants above hold.
    bool isConsistent() const {
        return totalRegistered == waiting + served + cancelled &&
               waiting == waitingEmergency + waitingUrgent + waitingNormal;
    }
};

class HospitalQueue {
public:
    // Registers a NEW patient from raw data and places them in the queue.
    //
    // The caller never supplies an ID or arrival sequence: both are generated
    // here (IDs 1, 2, 3, ...; sequences 1, 2, 3, ...), so they are unique and
    // strictly increasing. Text fields are trimmed of surrounding whitespace.
    //
    // Throws std::invalid_argument (and changes NOTHING - no ID or sequence is
    // consumed, the queue and records are untouched) if:
    //   - the name, gender or medical condition is empty/blank
    //   - the age is outside [0, Patient::MAX_AGE]
    //   - priorityValue is not 1, 2 or 3
    // Returns the newly registered patient (with its generated ID/sequence).
    Patient registerPatient(const std::string& fullName,
                            int age,
                            const std::string& gender,
                            const std::string& medicalCondition,
                            int priorityValue);

    // Records an already-built patient and inserts them into the waiting queue.
    // Throws std::invalid_argument if a patient with the same ID is already
    // registered (the queue is left untouched). The ID and arrival-sequence
    // generators are moved past this patient's values so that later calls to
    // registerPatient can never reuse them.            Time: O(log n)
    void insertPatient(const Patient& patient);

    // Returns the WAITING patient who would be served next, without removing
    // them. Cancelled patients are never returned.
    // Time: O(1). Throws std::out_of_range if nobody is waiting.
    const Patient& peekNext() const;

    // SERVES the next patient: removes them from the waiting queue, marks
    // them Served in the registry and returns a copy of their data.
    // Cancelled patients are skipped automatically.
    // Time: O(log n) amortised. Throws std::out_of_range if nobody is waiting;
    // in that case nothing is changed.
    Patient popNext();

    // Cancels the registration of a WAITING patient (see CancelResult).
    // The record is kept in the registry with status Cancelled, and the
    // patient will never be served. Nothing changes unless the result is
    // CancelResult::Cancelled.
    // Time: O(log n) for the lookup, plus amortised O(log n) clean-up.
    CancelResult cancelPatient(int patientId);

    // Looks a patient up by ID. Returns a copy of the record (patient data
    // and status), or std::nullopt if the ID does not exist.   Time: O(log n)
    std::optional<PatientRecord> findPatient(int patientId) const;

    // Copies of all WAITING patients in the exact order they will be served.
    // The real queue is not modified. Time: O(n log n).
    std::vector<Patient> getWaitingPatients() const;

    bool        isEmpty() const;  // O(1): true when nobody is WAITING
    std::size_t size() const;     // O(1): patients currently WAITING

    // Counters per status. Invariant (always true):
    //   size() + totalServed() + totalCancelled() == totalRegistered()
    std::size_t totalRegistered() const;  // O(1): every patient ever registered
    std::size_t totalServed() const;      // O(1)
    std::size_t totalCancelled() const;   // O(1)

    // Copies of every registered patient (any status), ordered by patient ID.
    std::vector<Patient> getRegisteredPatients() const;  // O(n)

    // Current statistics. The four totals come from the O(1) status counters;
    // the waiting-by-priority split is counted from the registry, so a
    // cancelled or served patient can never be counted as waiting. O(n).
    QueueStatistics getStatistics() const;

    // Copies of every record (patient data + status), ordered by patient ID.
    // This is what gets saved to disk. O(n).
    std::vector<PatientRecord> getAllRecords() const;

    // REPLACES the entire contents of this queue with `records` (used when
    // loading a saved file; any current patients are discarded, never merged,
    // so loading the same file twice cannot create duplicates).
    //
    //  - Every record goes into the registry with its saved status.
    //  - ONLY records with status Waiting enter the priority queue, so served
    //    and cancelled patients can never be served after a restart.
    //  - The ID and arrival-sequence generators continue after the highest
    //    values found in `records`, whatever the status of those patients.
    //
    // Throws std::invalid_argument if two records share a patient ID or an
    // arrival sequence. All-or-nothing: on any error the queue is left exactly
    // as it was. Time: O(n log n).
    void loadRecords(const std::vector<PatientRecord>& records);

private:
    // Custom comparator that defines "who goes first".
    //
    // std::priority_queue is a MAX-heap: top() is the element that compares
    // as "greatest". operator()(a, b) must return true when `a` should be
    // served AFTER `b` (i.e. a has LOWER precedence than b). The patient
    // that is never "after" anyone else ends up on top and is served first.
    //
    // Rule 1: a larger priority number (3 = Normal) is served after a
    //         smaller one (1 = Emergency).
    // Rule 2: for equal priority, the larger arrival sequence (arrived later)
    //         is served after the smaller one -> FIFO within each level.
    struct PatientComparator {
        bool operator()(const Patient& a, const Patient& b) const {
            if (a.getPriority() != b.getPriority()) {
                return static_cast<int>(a.getPriority()) >
                       static_cast<int>(b.getPriority());
            }
            return a.getArrivalSequence() > b.getArrivalSequence();
        }
    };

    // Heap of patients ordered by PatientComparator.
    //
    // CANCELLATION STRATEGY - "lazy deletion": std::priority_queue cannot
    // remove an element from the middle, so cancelling only marks the
    // patient Cancelled in patientRecords_. The patient's heap entry stays as
    // a harmless "ghost" until it reaches the top, where it is discarded.
    //
    // CLASS INVARIANT: if the heap is not empty, its top is a Waiting
    // patient. discardCancelledFromTop() restores this after every operation
    // that could break it, which keeps peekNext() O(1) and const. Ghosts can
    // only sit below the top, so they can never be served or displayed.
    std::priority_queue<Patient, std::vector<Patient>, PatientComparator> waitingQueue_;

    // Permanent registry of EVERY registered patient (waiting, served and
    // cancelled), keyed by patient ID. It is the single source of truth for
    // status: a patient appears here exactly once, so no duplicates exist.
    std::map<int, PatientRecord> patientRecords_;

    // Removes ghost (non-Waiting) entries from the top of the heap until the
    // top is a Waiting patient or the heap is empty. Each heap entry is
    // discarded at most once, so the cost is amortised O(log n) per cancel.
    void discardCancelledFromTop();

    // Status counters, kept in step with patientRecords_ on every change.
    std::size_t waitingCount_   = 0;
    std::size_t servedCount_    = 0;
    std::size_t cancelledCount_ = 0;

    // Generators for the next unused ID and arrival sequence number.
    int  nextPatientId_       = 1;
    long nextArrivalSequence_ = 1;
};

#endif  // HOSPITAL_QUEUE_H
