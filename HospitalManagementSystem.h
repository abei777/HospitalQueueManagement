// HospitalManagementSystem.h
// The user-facing layer of the application. It talks to the user (reads
// input, prints messages, runs the main menu) and delegates all queue and
// record logic to HospitalQueue and all file access to PatientStorage.
//
// run() is the complete application: welcome screen, loading of the saved
// records, the main menu loop, automatic saving after every change, and a
// safe shutdown. The individual operations are also public so they can be
// tested one by one.
//
// NOTE: This project is an educational queue-management simulation.
// It is NOT a clinically validated triage system.

#ifndef HOSPITAL_MANAGEMENT_SYSTEM_H
#define HOSPITAL_MANAGEMENT_SYSTEM_H

#include <iosfwd>
#include <string>

#include "HospitalQueue.h"

class HospitalManagementSystem {
public:
    // Default location of the saved patient records.
    static constexpr const char* DEFAULT_DATA_FILE = "data/patients.txt";

    // Staff PIN used by the cancellation authorization (educational only).
    static constexpr const char* DEFAULT_STAFF_PIN = "staff2026";

    // Number of PIN attempts allowed per cancellation request.
    static constexpr int MAX_PIN_ATTEMPTS = 3;

    // Exit codes returned by run().
    static constexpr int EXIT_OK                = 0;  // clean shutdown, everything saved
    static constexpr int EXIT_UNSAVED_CHANGES   = 2;  // shut down with changes NOT saved

    // `dataFilePath` is where saveRecords() writes and loadSavedRecords()
    // reads. `staffPin` is the PIN that authorizes cancellations. The
    // constructor performs no file access; run() loads the saved records.
    explicit HospitalManagementSystem(std::string dataFilePath = DEFAULT_DATA_FILE,
                                      std::string staffPin = DEFAULT_STAFF_PIN);

    // ------------------------------------------------------------------
    // The complete application
    // ------------------------------------------------------------------

    // Runs a whole session: prints the welcome screen, loads the saved
    // records (reporting whether that worked), then repeats the main menu
    // until the user chooses Exit (or the input ends). Every successful
    // register / serve / cancel is saved automatically. Returns EXIT_OK, or
    // EXIT_UNSAVED_CHANGES if the session ended with changes that could not
    // be saved.
    int run(std::istream& in, std::ostream& out);

    // Prints the welcome screen, including the educational-simulation notice.
    void displayWelcomeScreen(std::ostream& out) const;

    // Prints the main menu and the "Enter your choice:" prompt.
    void displayMainMenu(std::ostream& out) const;

    // Interactively collects name, age, gender, medical condition and
    // priority from `in`, registers the patient and prints a confirmation
    // to `out`.
    //
    // Each field is validated as it is typed; invalid input prints an error
    // and asks again (it never crashes or loops forever). The patient is
    // registered only after ALL fields are valid.
    //
    // Returns true if a patient was registered. Returns false if the input
    // stream ended before all fields were collected; nothing is added then.
    //
    // Streams are parameters (instead of hard-coded std::cin/std::cout) so
    // that automated tests can feed scripted input and inspect the output.
    bool registerPatientInteractive(std::istream& in, std::ostream& out);

    // Serves the highest-priority waiting patient: removes them from the
    // queue, records them as Served and prints their complete information.
    // Prints a friendly message (and returns false) if nobody is waiting.
    bool serveNextPatient(std::ostream& out);

    // Prints every waiting patient in the exact order they will be served.
    // Cancelled and served patients are not listed. The queue is not modified.
    void displayWaitingPatients(std::ostream& out) const;

    // Asks for a patient ID and prints that patient's complete information
    // and status (Waiting / Served / Cancelled). For a waiting patient the
    // position in the queue is shown as well.
    // Returns true if the patient exists, false if the ID is unknown or the
    // input ended before an ID was entered.
    bool searchPatient(std::istream& in, std::ostream& out) const;

    // Asks for a patient ID and cancels that patient's registration if they
    // are still waiting. Served and already-cancelled patients are refused
    // with an explanatory message. Returns true only if a patient was
    // cancelled.
    bool cancelPatient(std::istream& in, std::ostream& out);

    // The cancellation used by the menu. In order:
    //   1. Authorization: the user must enter the staff PIN (at most
    //      MAX_PIN_ATTEMPTS tries). Without it nothing can be cancelled, and
    //      the patient ID is not even requested.
    //   2. The patient ID is requested. Unknown, served and already-cancelled
    //      patients are refused immediately, without asking for confirmation.
    //   3. The patient's details are shown and the user must explicitly
    //      confirm ("yes"). Anything else keeps the registration.
    // Returns true only if a registration was cancelled.
    bool cancelPatientAuthorized(std::istream& in, std::ostream& out);

    // Prints the queue statistics: patients registered, waiting, served and
    // cancelled, and how many of the waiting patients are Emergency, Urgent
    // and Normal. Registered = Waiting + Served + Cancelled always holds.
    void displayStatistics(std::ostream& out) const;

    // Saves every patient record (waiting, served AND cancelled, with their
    // status) to the data file. Prints the outcome to `out`.
    // Returns true on success. A failed save leaves the previous file intact.
    //
    // SAFETY: if the data file existed but could not be loaded (corrupt or
    // unreadable), saving is refused so that the file is not overwritten
    // with an empty or partial registry. Pass `overwriteUnreadableFile` =
    // true only after the user has explicitly confirmed that this is wanted.
    //
    // Designed to be called after important operations (register, serve,
    // cancel) by a later integration stage.
    bool saveRecords(std::ostream& out, bool overwriteUnreadableFile = false);

    // Loads the saved records, REPLACING the current registry (records are
    // never merged, so repeated loads cannot create duplicates). Only
    // Waiting patients re-enter the priority queue; served and cancelled
    // patients are restored as history. New IDs and arrival sequences
    // continue after the highest saved values.
    //
    // Returns true if the system is in a usable state: records were loaded,
    // or there was nothing to load (file missing or empty). Returns false if
    // the file is corrupt or unreadable; then NOTHING is loaded, the current
    // registry is unchanged, the file is not modified, and saving stays
    // blocked (see saveRecords). Messages are printed to `out`.
    bool loadSavedRecords(std::ostream& out);

    const std::string& getDataFilePath() const;

    // True while changes exist in memory that have not been saved to disk.
    bool hasUnsavedChanges() const;

    // True while saving is blocked because the data file could not be
    // loaded (see saveRecords).
    bool isSavingBlocked() const;

    // Read-only view of the queue (used by tests).
    const HospitalQueue& getQueue() const;

private:
    // Menu handlers and helpers used by run().
    bool authorizeStaff(std::istream& in, std::ostream& out) const;
    void autoSave(std::ostream& out);
    void saveRecordsInteractive(std::istream& in, std::ostream& out);
    bool confirmExit(std::istream& in, std::ostream& out);
    bool finishSession(std::istream& in, std::ostream& out, bool canAsk);

    HospitalQueue hospitalQueue_;
    std::string   dataFilePath_;
    std::string   staffPin_;

    // True after a load found the data file corrupt or unreadable. While
    // set, saveRecords() refuses to overwrite that file.
    bool dataFileUnreadable_ = false;

    // True while the in-memory registry holds changes (a registration, a
    // served patient or a cancellation) that have not been written to the
    // data file yet. Set by those operations, cleared by a successful save or
    // load.
    bool unsavedChanges_ = false;
};

#endif  // HOSPITAL_MANAGEMENT_SYSTEM_H
