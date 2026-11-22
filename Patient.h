// Patient.h
// Declares the Patient class and the PriorityLevel enumeration.
//
// NOTE: This project is an educational queue-management simulation.
// It is NOT a clinically validated triage system. Real hospital triage
// requires approved medical protocols and qualified healthcare professionals.

#ifndef PATIENT_H
#define PATIENT_H

#include <string>

// Medical priority levels. The underlying integer values matter: a SMALLER
// number means a MORE urgent patient. The queue comparator relies on this.
enum class PriorityLevel : int {
    Emergency = 1,  // Critical or life-threatening; served first
    Urgent    = 2,  // Serious; needs prompt attention
    Normal    = 3   // Stable; can safely wait
};

// Converts a PriorityLevel to a human-readable label ("Emergency", ...).
std::string priorityToString(PriorityLevel level);

// Converts a user-supplied integer (1, 2 or 3) to a PriorityLevel.
// Throws std::invalid_argument for any other value.
PriorityLevel priorityFromInt(int value);

class Patient {
public:
    // Largest age accepted by the system (simple sanity limit).
    static constexpr int MAX_AGE = 120;

    // Builds a patient. Throws std::invalid_argument if:
    //   - id is not positive
    //   - fullName is empty or only whitespace
    //   - age is outside [0, MAX_AGE]
    //   - arrivalSequence is not positive
    Patient(int id,
            std::string fullName,
            int age,
            std::string gender,
            std::string medicalCondition,
            PriorityLevel priority,
            long arrivalSequence);

    // Read-only access (getters). There are deliberately no setters:
    // a patient's priority and arrival order must not change once the
    // patient is inside the queue, or the heap ordering would be corrupted.
    int                getId() const;
    const std::string& getFullName() const;
    int                getAge() const;
    const std::string& getGender() const;
    const std::string& getMedicalCondition() const;
    PriorityLevel      getPriority() const;
    long               getArrivalSequence() const;

private:
    int           id_;               // Unique patient identifier
    std::string   fullName_;
    int           age_;
    std::string   gender_;
    std::string   medicalCondition_;
    PriorityLevel priority_;
    long          arrivalSequence_;  // 1 for the first arrival, 2 for the next, ...
};

#endif  // PATIENT_H
