// main.cpp
// Entry point of the Hospital Queue Management System.
//
// All the work is done by HospitalManagementSystem::run(), which
//   1. shows the welcome screen,
//   2. loads the saved patient records from data/patients.txt and reports
//      whether that worked (a missing file starts empty; a corrupt file is
//      left untouched and saving is blocked),
//   3. runs the main menu until the user exits, saving automatically after
//      every registration, served patient and cancellation.
//
// The system owns the HospitalQueue (the priority queue and patient
// registry); the file data/patients.txt is read and written through
// PatientStorage. The data path is relative, so start the program from the
// project root folder. The data/ folder is created on the first save if it
// does not exist.
//
// NOTE: This project is an educational queue-management simulation.
// It is NOT a clinically validated triage system.

#include <exception>
#include <iostream>

#include "HospitalManagementSystem.h"

int main() {
    try {
        HospitalManagementSystem system;  // uses data/patients.txt
        return system.run(std::cin, std::cout);
    } catch (const std::exception& error) {
        // Nothing in the application is expected to throw out of run(); if
        // something unexpected does, report it instead of crashing silently.
        std::cerr << "\nFatal error: " << error.what() << "\n";
        return 1;
    }
}
