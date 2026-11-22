// tests/test_patient.cpp
// Patient class tests: construction, getters, encapsulation, validation and
// the priority-level helper functions.

#include <stdexcept>
#include <string>

#include "Patient.h"
#include "test_framework.h"

using namespace testfw;

// True if calling `fn` throws std::invalid_argument.
template <typename Fn>
static bool throwsInvalidArgument(Fn fn) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
    }
    return false;
}

int main() {
    section("Patient: getters and encapsulation");
    Patient p(7, "Ama Mensah", 41, "Female", "Chest pain", PriorityLevel::Emergency, 1);
    check(p.getId() == 7 && p.getFullName() == "Ama Mensah" && p.getAge() == 41 &&
              p.getGender() == "Female" && p.getMedicalCondition() == "Chest pain" &&
              p.getPriority() == PriorityLevel::Emergency && p.getArrivalSequence() == 1,
          "Patient getters return constructor values");

    section("Patient: constructor validation");
    check(throwsInvalidArgument([] { Patient(0, "A", 20, "M", "c", PriorityLevel::Normal, 1); }),
          "Rejects non-positive ID");
    check(throwsInvalidArgument([] { Patient(1, "   ", 20, "M", "c", PriorityLevel::Normal, 1); }),
          "Rejects blank name");
    check(throwsInvalidArgument([] { Patient(1, "A", -1, "M", "c", PriorityLevel::Normal, 1); }),
          "Rejects negative age");
    check(throwsInvalidArgument([] { Patient(1, "A", 121, "M", "c", PriorityLevel::Normal, 1); }),
          "Rejects age above MAX_AGE");
    check(throwsInvalidArgument([] { Patient(1, "A", 20, "M", "c", PriorityLevel::Normal, 0); }),
          "Rejects non-positive arrival sequence");

    section("Priority helper functions");
    check(throwsInvalidArgument([] { priorityFromInt(4); }), "priorityFromInt rejects 4");
    check(throwsInvalidArgument([] { priorityFromInt(0); }), "priorityFromInt rejects 0");
    check(priorityFromInt(1) == PriorityLevel::Emergency &&
              priorityFromInt(2) == PriorityLevel::Urgent &&
              priorityFromInt(3) == PriorityLevel::Normal,
          "priorityFromInt maps 1/2/3");
    check(priorityToString(PriorityLevel::Urgent) == "Urgent", "priorityToString works");

    return finish();
}
