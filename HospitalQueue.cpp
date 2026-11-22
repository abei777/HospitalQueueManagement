// HospitalQueue.cpp
// Implementation of the HospitalQueue class.

#include "HospitalQueue.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

// Returns a copy of `text` without leading or trailing whitespace.
std::string trim(const std::string& text) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    const auto first = std::find_if_not(text.begin(), text.end(), isSpace);
    const auto last  = std::find_if_not(text.rbegin(), text.rend(), isSpace).base();
    return (first >= last) ? std::string() : std::string(first, last);
}

}  // namespace

std::string statusToString(PatientStatus status) {
    switch (status) {
        case PatientStatus::Waiting:   return "Waiting";
        case PatientStatus::Served:    return "Served";
        case PatientStatus::Cancelled: return "Cancelled";
    }
    return "Unknown";  // Unreachable for valid enum values; keeps compilers quiet
}

void HospitalQueue::insertPatient(const Patient& patient) {
    // Step 1: record the patient as Waiting. map::emplace never overwrites,
    // so a duplicate ID is detected here, BEFORE the queue has been touched.
    const auto inserted = patientRecords_.emplace(
        patient.getId(), PatientRecord{patient, PatientStatus::Waiting});
    if (!inserted.second) {
        throw std::invalid_argument("Patient ID " + std::to_string(patient.getId()) +
                                    " is already registered.");
    }

    // Step 2: push() appends the patient to the underlying vector and "sifts
    // up" through the heap using PatientComparator: O(log n). If it throws
    // (e.g. out of memory) the record is rolled back, so the records and the
    // queue can never disagree.
    try {
        waitingQueue_.push(patient);
    } catch (...) {
        patientRecords_.erase(inserted.first);
        throw;
    }

    // Step 3: the new patient is Waiting, and a Waiting patient on the heap
    // never breaks the "top is Waiting" invariant.
    ++waitingCount_;

    // Step 4: keep the generators ahead of every ID and sequence in use.
    nextPatientId_       = std::max(nextPatientId_, patient.getId() + 1);
    nextArrivalSequence_ = std::max(nextArrivalSequence_, patient.getArrivalSequence() + 1);
}

Patient HospitalQueue::registerPatient(const std::string& fullName,
                                       int age,
                                       const std::string& gender,
                                       const std::string& medicalCondition,
                                       int priorityValue) {
    // Phase 1: validate and build. Nothing below modifies the object until
    // the Patient has been constructed successfully, so any exception leaves
    // the queue, the records and both generators exactly as they were.
    const std::string name      = trim(fullName);
    const std::string genderTxt = trim(gender);
    const std::string condition = trim(medicalCondition);

    if (name.empty()) {
        throw std::invalid_argument("Patient name must not be empty.");
    }
    if (genderTxt.empty()) {
        throw std::invalid_argument("Patient gender must not be empty.");
    }
    if (condition.empty()) {
        throw std::invalid_argument("Medical condition must not be empty.");
    }
    const PriorityLevel priority = priorityFromInt(priorityValue);  // throws if not 1-3

    // The Patient constructor (Stage 1) validates the age range.
    const Patient patient(nextPatientId_, name, age, genderTxt, condition,
                          priority, nextArrivalSequence_);

    // Phase 2: commit. insertPatient records the patient, pushes them onto
    // the heap and advances the generators.
    insertPatient(patient);
    return patient;
}

void HospitalQueue::discardCancelledFromTop() {
    while (!waitingQueue_.empty()) {
        const auto record = patientRecords_.find(waitingQueue_.top().getId());
        if (record != patientRecords_.end() &&
            record->second.status == PatientStatus::Waiting) {
            return;  // The top is a genuine waiting patient: invariant holds
        }
        waitingQueue_.pop();  // Ghost entry of a cancelled patient: drop it
    }
}

const Patient& HospitalQueue::peekNext() const {
    if (waitingQueue_.empty()) {
        throw std::out_of_range("The waiting queue is empty.");
    }
    // By the class invariant the top is always a Waiting patient.  O(1)
    return waitingQueue_.top();
}

Patient HospitalQueue::popNext() {
    if (waitingQueue_.empty()) {
        throw std::out_of_range("The waiting queue is empty.");
    }
    // top() returns a const reference, so copy it before pop() destroys it.
    // The copy is the only step here that can throw, and it happens before
    // anything is modified, so a failure leaves the queue untouched.
    Patient next = waitingQueue_.top();
    waitingQueue_.pop();  // "Sift down" to restore the heap: O(log n)

    // Record the outcome. The ID is guaranteed to be in the registry because
    // every heap entry was recorded by insertPatient.
    patientRecords_.at(next.getId()).status = PatientStatus::Served;
    --waitingCount_;
    ++servedCount_;

    // The next entry may be a ghost of a patient cancelled earlier; discard
    // such entries so the "top is Waiting" invariant holds again.
    discardCancelledFromTop();
    return next;
}

CancelResult HospitalQueue::cancelPatient(int patientId) {
    const auto found = patientRecords_.find(patientId);
    if (found == patientRecords_.end()) {
        return CancelResult::NotFound;
    }

    PatientRecord& record = found->second;
    switch (record.status) {
        case PatientStatus::Served:
            return CancelResult::AlreadyServed;
        case PatientStatus::Cancelled:
            return CancelResult::AlreadyCancelled;
        case PatientStatus::Waiting:
            break;
    }

    // Lazy deletion: only the status changes. The heap entry stays where it
    // is (it cannot be removed from the middle of a std::priority_queue) and
    // is discarded when it reaches the top.
    record.status = PatientStatus::Cancelled;
    --waitingCount_;
    ++cancelledCount_;

    // If the cancelled patient was the one next in line, drop the ghost now.
    discardCancelledFromTop();
    return CancelResult::Cancelled;
}

std::optional<PatientRecord> HospitalQueue::findPatient(int patientId) const {
    const auto found = patientRecords_.find(patientId);
    if (found == patientRecords_.end()) {
        return std::nullopt;
    }
    return found->second;  // A copy: callers cannot alter the registry
}

std::vector<Patient> HospitalQueue::getWaitingPatients() const {
    // Pop from a COPY of the heap. The comparator guarantees patients come
    // out in true serving order, and the original queue is never touched.
    auto copy = waitingQueue_;
    std::vector<Patient> ordered;
    ordered.reserve(waitingCount_);
    while (!copy.empty()) {
        const Patient& candidate = copy.top();
        const auto record = patientRecords_.find(candidate.getId());
        // Ghosts of cancelled patients can sit anywhere in the heap, so each
        // entry is checked against the registry before it is listed.
        if (record != patientRecords_.end() &&
            record->second.status == PatientStatus::Waiting) {
            ordered.push_back(candidate);
        }
        copy.pop();
    }
    return ordered;
}

bool HospitalQueue::isEmpty() const { return waitingCount_ == 0; }

std::size_t HospitalQueue::size() const { return waitingCount_; }

std::size_t HospitalQueue::totalRegistered() const { return patientRecords_.size(); }

std::size_t HospitalQueue::totalServed() const { return servedCount_; }

std::size_t HospitalQueue::totalCancelled() const { return cancelledCount_; }

std::vector<Patient> HospitalQueue::getRegisteredPatients() const {
    std::vector<Patient> patients;
    patients.reserve(patientRecords_.size());
    for (const auto& entry : patientRecords_) {  // std::map iterates in ID order
        patients.push_back(entry.second.patient);
    }
    return patients;
}

QueueStatistics HospitalQueue::getStatistics() const {
    QueueStatistics stats;
    // The totals come straight from the status counters.
    stats.totalRegistered = patientRecords_.size();
    stats.waiting         = waitingCount_;
    stats.served          = servedCount_;
    stats.cancelled       = cancelledCount_;

    // Waiting-by-priority is counted from the registry. Only records whose
    // status is Waiting are counted, so served and cancelled patients (and
    // ghost entries left in the heap) can never inflate these numbers.
    for (const auto& entry : patientRecords_) {
        if (entry.second.status != PatientStatus::Waiting) {
            continue;
        }
        switch (entry.second.patient.getPriority()) {
            case PriorityLevel::Emergency: ++stats.waitingEmergency; break;
            case PriorityLevel::Urgent:    ++stats.waitingUrgent;    break;
            case PriorityLevel::Normal:    ++stats.waitingNormal;    break;
        }
    }
    return stats;
}

std::vector<PatientRecord> HospitalQueue::getAllRecords() const {
    std::vector<PatientRecord> records;
    records.reserve(patientRecords_.size());
    for (const auto& entry : patientRecords_) {  // std::map iterates in ID order
        records.push_back(entry.second);
    }
    return records;
}

void HospitalQueue::loadRecords(const std::vector<PatientRecord>& records) {
    // Build the new state in a separate object. `*this` is replaced only at
    // the very end, so an exception anywhere below changes nothing.
    HospitalQueue rebuilt;
    std::set<long> arrivalSequences;

    for (const PatientRecord& record : records) {
        const Patient& patient = record.patient;

        if (!rebuilt.patientRecords_.emplace(patient.getId(), record).second) {
            throw std::invalid_argument("Duplicate patient ID " +
                                        std::to_string(patient.getId()) + " in saved records.");
        }
        // Equal sequences would make the serving order of two patients
        // undefined, so they are rejected as well.
        if (!arrivalSequences.insert(patient.getArrivalSequence()).second) {
            throw std::invalid_argument("Duplicate arrival sequence " +
                                        std::to_string(patient.getArrivalSequence()) +
                                        " in saved records.");
        }

        switch (record.status) {
            case PatientStatus::Waiting:
                rebuilt.waitingQueue_.push(patient);  // the ONLY status that enters the heap
                ++rebuilt.waitingCount_;
                break;
            case PatientStatus::Served:
                ++rebuilt.servedCount_;
                break;
            case PatientStatus::Cancelled:
                ++rebuilt.cancelledCount_;
                break;
        }

        // Continue after the highest values ever used, including those of
        // served and cancelled patients, so no ID or sequence is ever reused.
        rebuilt.nextPatientId_ = std::max(rebuilt.nextPatientId_, patient.getId() + 1);
        rebuilt.nextArrivalSequence_ =
            std::max(rebuilt.nextArrivalSequence_, patient.getArrivalSequence() + 1);
    }

    // Only Waiting patients were pushed, so the "top is Waiting" invariant
    // already holds. Replace the current state in one step.
    *this = std::move(rebuilt);
}
