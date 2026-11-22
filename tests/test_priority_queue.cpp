// tests/test_priority_queue.cpp
// HospitalQueue ordering tests: the custom comparator, Emergency > Urgent >
// Normal, FIFO among equal priorities, and empty-queue safety.

#include <stdexcept>
#include <string>
#include <vector>

#include "HospitalQueue.h"
#include "Patient.h"
#include "test_framework.h"

using namespace testfw;

static Patient make(int id, const std::string& name, PriorityLevel p, long seq) {
    return Patient(id, name, 30, "F", "test condition", p, seq);
}

int main() {
    section("Empty queue safety");
    HospitalQueue q;
    check(q.isEmpty() && q.size() == 0, "New queue is empty");
    bool peekThrew = false;
    bool popThrew = false;
    try {
        q.peekNext();
    } catch (const std::out_of_range&) {
        peekThrew = true;
    }
    try {
        q.popNext();
    } catch (const std::out_of_range&) {
        popThrew = true;
    }
    check(peekThrew && popThrew, "peekNext/popNext on empty queue throw out_of_range");

    section("Priority ordering: arrival order Normal, Urgent, Emergency");
    q.insertPatient(make(1, "Normal-1", PriorityLevel::Normal, 1));
    q.insertPatient(make(2, "Urgent-1", PriorityLevel::Urgent, 2));
    q.insertPatient(make(3, "Emergency-1", PriorityLevel::Emergency, 3));
    check(q.size() == 3, "Size is 3 after three inserts");
    check(q.peekNext().getId() == 3, "peekNext shows Emergency patient first");
    check(q.size() == 3, "peekNext does not remove");
    check(q.popNext().getId() == 3, "1st served: Emergency (arrived last)");
    check(q.popNext().getId() == 2, "2nd served: Urgent");
    check(q.popNext().getId() == 1, "3rd served: Normal (arrived first)");
    check(q.isEmpty(), "Queue empty after serving all");

    section("FIFO within the same priority");
    HospitalQueue fifo;
    for (int i = 1; i <= 5; ++i) {
        fifo.insertPatient(make(i, "Same-" + std::to_string(i), PriorityLevel::Urgent, i));
    }
    bool inOrder = true;
    for (int expected = 1; expected <= 5; ++expected) {
        if (fifo.popNext().getId() != expected) {
            inOrder = false;
        }
    }
    check(inOrder, "Equal-priority patients served in arrival order (FIFO)");

    section("Mixed stress case: inserted out of order");
    HospitalQueue mixed;
    mixed.insertPatient(make(10, "N1", PriorityLevel::Normal, 1));
    mixed.insertPatient(make(11, "U1", PriorityLevel::Urgent, 2));
    mixed.insertPatient(make(12, "N2", PriorityLevel::Normal, 3));
    mixed.insertPatient(make(13, "E1", PriorityLevel::Emergency, 4));
    mixed.insertPatient(make(14, "U2", PriorityLevel::Urgent, 5));
    mixed.insertPatient(make(15, "E2", PriorityLevel::Emergency, 6));
    mixed.insertPatient(make(16, "N3", PriorityLevel::Normal, 7));
    const std::vector<int> expected = {13, 15, 11, 14, 10, 12, 16};
    std::vector<int> actual;
    while (!mixed.isEmpty()) {
        actual.push_back(mixed.popNext().getId());
    }
    check(actual == expected, "Mixed insertions -> E1,E2,U1,U2,N1,N2,N3");

    return finish();
}
