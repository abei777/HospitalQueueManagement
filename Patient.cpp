// Patient.cpp
// Implementation of the Patient class and priority helper functions.

#include "Patient.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace {

// Returns true if the string is empty or contains only whitespace.
bool isBlank(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
}

}  // namespace

std::string priorityToString(PriorityLevel level) {
    switch (level) {
        case PriorityLevel::Emergency: return "Emergency";
        case PriorityLevel::Urgent:    return "Urgent";
        case PriorityLevel::Normal:    return "Normal";
    }
    return "Unknown";  // Unreachable for valid enum values; keeps compilers quiet
}

PriorityLevel priorityFromInt(int value) {
    switch (value) {
        case 1: return PriorityLevel::Emergency;
        case 2: return PriorityLevel::Urgent;
        case 3: return PriorityLevel::Normal;
        default:
            throw std::invalid_argument(
                "Priority must be 1 (Emergency), 2 (Urgent) or 3 (Normal).");
    }
}

Patient::Patient(int id,
                 std::string fullName,
                 int age,
                 std::string gender,
                 std::string medicalCondition,
                 PriorityLevel priority,
                 long arrivalSequence)
    : id_(id),
      fullName_(std::move(fullName)),
      age_(age),
      gender_(std::move(gender)),
      medicalCondition_(std::move(medicalCondition)),
      priority_(priority),
      arrivalSequence_(arrivalSequence) {
    // Validate after the member-initializer list. If any check fails the
    // exception stops construction, so an invalid Patient can never exist.
    if (id_ <= 0) {
        throw std::invalid_argument("Patient ID must be a positive number.");
    }
    if (isBlank(fullName_)) {
        throw std::invalid_argument("Patient name must not be empty.");
    }
    if (age_ < 0 || age_ > MAX_AGE) {
        throw std::invalid_argument("Patient age must be between 0 and " +
                                    std::to_string(MAX_AGE) + ".");
    }
    if (arrivalSequence_ <= 0) {
        throw std::invalid_argument("Arrival sequence must be a positive number.");
    }
}

int Patient::getId() const { return id_; }
const std::string& Patient::getFullName() const { return fullName_; }
int Patient::getAge() const { return age_; }
const std::string& Patient::getGender() const { return gender_; }
const std::string& Patient::getMedicalCondition() const { return medicalCondition_; }
PriorityLevel Patient::getPriority() const { return priority_; }
long Patient::getArrivalSequence() const { return arrivalSequence_; }
