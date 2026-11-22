// HospitalManagementSystem.cpp
// Implementation of the user-facing registration workflow.

#include "HospitalManagementSystem.h"

#include "PatientStorage.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// True if the text is empty or contains only whitespace.
bool isBlank(const std::string& text) {
    return text.find_first_not_of(" \t\r\n\v\f") == std::string::npos;
}

// Asks for a line of text until the user types something non-blank.
// Returns false only if the input stream ends (end-of-file / closed).
bool promptNonBlankText(std::istream& in, std::ostream& out,
                        const std::string& prompt,
                        const std::string& errorMessage,
                        std::string& result) {
    std::string line;
    while (true) {
        out << prompt;
        if (!std::getline(in, line)) {
            return false;  // input ended; the caller cancels the registration
        }
        if (!isBlank(line)) {
            result = line;
            return true;
        }
        out << "  Error: " << errorMessage << "\n";
    }
}

// Asks for a whole number in [minValue, maxValue] until one is entered.
// Rejects letters ("abc"), trailing junk ("12x"), decimals ("3.5"), numbers
// too large for an int, empty lines and out-of-range values.
// Returns false only if the input stream ends.
bool promptInteger(std::istream& in, std::ostream& out,
                   const std::string& prompt,
                   const std::string& errorMessage,
                   int minValue, int maxValue,
                   int& result) {
    std::string line;
    while (true) {
        out << prompt;
        if (!std::getline(in, line)) {
            return false;
        }
        std::istringstream parser(line);
        int value = 0;
        char leftover = '\0';
        // `parser >> value` fails for non-numbers and for overflow.
        // `parser >> leftover` SUCCEEDS only if non-space characters remain
        // after the number, which means the line was not a pure integer.
        const bool isPureInteger = static_cast<bool>(parser >> value) &&
                                   !static_cast<bool>(parser >> leftover);
        if (isPureInteger && value >= minValue && value <= maxValue) {
            result = value;
            return true;
        }
        out << "  Error: " << errorMessage << "\n";
    }
}

// Prints the registration confirmation required by the specification.
void displayRegistrationConfirmation(const Patient& patient, std::ostream& out) {
    out << "\n========== REGISTRATION SUCCESSFUL ==========\n"
        << "Patient ID       : " << patient.getId() << "\n"
        << "Patient Name     : " << patient.getFullName() << "\n"
        << "Priority         : " << priorityToString(patient.getPriority())
        << " (" << static_cast<int>(patient.getPriority()) << ")\n"
        << "Arrival Sequence : " << patient.getArrivalSequence() << "\n"
        << "Patient registered successfully and added to the waiting queue.\n"
        << "=============================================\n";
}

// Prints every stored field of a patient, one per line.
void printPatientDetails(const Patient& patient, std::ostream& out) {
    out << "Patient ID        : " << patient.getId() << "\n"
        << "Full Name         : " << patient.getFullName() << "\n"
        << "Age               : " << patient.getAge() << "\n"
        << "Gender            : " << patient.getGender() << "\n"
        << "Medical Condition : " << patient.getMedicalCondition() << "\n"
        << "Priority          : " << priorityToString(patient.getPriority())
        << " (" << static_cast<int>(patient.getPriority()) << ")\n"
        << "Arrival Sequence  : " << patient.getArrivalSequence() << "\n";
}

// Shortens text to at most maxLength characters (ending in "...") so that
// long values cannot push the columns of the waiting-list table out of line.
// maxLength must be at least 3.
std::string shorten(const std::string& text, std::size_t maxLength) {
    if (text.size() <= maxLength) {
        return text;
    }
    return text.substr(0, maxLength - 3) + "...";
}

// Asks for a patient ID (a positive whole number). Returns false only if the
// input stream ends.
bool promptPatientId(std::istream& in, std::ostream& out, int& patientId) {
    return promptInteger(in, out, "Enter patient ID: ",
                         "Patient ID must be a positive whole number.",
                         1, std::numeric_limits<int>::max(), patientId);
}

// Returns the text without leading and trailing whitespace.
std::string trimmed(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n\v\f");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n\v\f");
    return text.substr(first, last - first + 1);
}

// Highest valid main-menu choice (8 = Exit System).
constexpr int MENU_EXIT_CHOICE = 8;

// Parses one line typed at the main menu. Accepts only a whole number from
// 1 to MENU_EXIT_CHOICE (surrounding spaces are fine). Rejects letters,
// decimals, trailing junk, blank lines, out-of-range and overflowing numbers.
bool parseMenuChoice(const std::string& line, int& choice) {
    std::istringstream parser(line);
    int value = 0;
    char leftover = '\0';
    const bool isPureInteger = static_cast<bool>(parser >> value) &&
                               !static_cast<bool>(parser >> leftover);
    if (!isPureInteger || value < 1 || value > MENU_EXIT_CHOICE) {
        return false;
    }
    choice = value;
    return true;
}

// Answer to a yes/no question.
enum class Answer { Yes, No, EndOfInput };

// Asks a yes/no question until the user answers "yes"/"y" or "no"/"n"
// (any letter case). Any other text is rejected and the question is asked
// again. EndOfInput is returned if the input stream ends.
Answer askYesNo(std::istream& in, std::ostream& out, const std::string& prompt) {
    std::string line;
    while (true) {
        out << prompt;
        if (!std::getline(in, line)) {
            return Answer::EndOfInput;
        }
        std::string answer = trimmed(line);
        std::transform(answer.begin(), answer.end(), answer.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (answer == "yes" || answer == "y") {
            return Answer::Yes;
        }
        if (answer == "no" || answer == "n") {
            return Answer::No;
        }
        out << "  Please answer yes or no.\n";
    }
}

}  // namespace

HospitalManagementSystem::HospitalManagementSystem(std::string dataFilePath,
                                                   std::string staffPin)
    : dataFilePath_(std::move(dataFilePath)), staffPin_(std::move(staffPin)) {}

void HospitalManagementSystem::displayWelcomeScreen(std::ostream& out) const {
    out << "==========================================================\n"
        << "            HOSPITAL QUEUE MANAGEMENT SYSTEM\n"
        << "       Priority-based patient queue (C++17 project)\n"
        << "==========================================================\n"
        << "Patients are served by medical priority first, then by\n"
        << "arrival order:  1 = Emergency   2 = Urgent   3 = Normal\n"
        << "\n"
        << "NOTE: This is an educational queue-management simulation,\n"
        << "NOT a clinically validated triage system. Real hospital\n"
        << "triage requires approved medical protocols and qualified\n"
        << "healthcare professionals.\n";
}

void HospitalManagementSystem::displayMainMenu(std::ostream& out) const {
    out << "\n========== HOSPITAL QUEUE MANAGEMENT SYSTEM ==========\n"
        << "\n"
        << "1. Register New Patient\n"
        << "2. View All Waiting Patients\n"
        << "3. Serve Next Patient\n"
        << "4. Search Patient\n"
        << "5. Cancel Patient Registration\n"
        << "6. View Queue Statistics\n"
        << "7. Save Patient Records\n"
        << "8. Exit System\n"
        << "\n"
        << "Enter your choice: ";
}

int HospitalManagementSystem::run(std::istream& in, std::ostream& out) {
    displayWelcomeScreen(out);

    // Startup: load the saved records and say plainly how that went. A
    // missing or empty file starts an empty registry; a corrupt file is left
    // untouched and saving is blocked (see loadSavedRecords).
    out << "\n---------- STARTUP ----------";
    loadSavedRecords(out);

    while (true) {
        displayMainMenu(out);

        std::string line;
        if (!std::getline(in, line)) {
            // The input ended (closed pipe, Ctrl-D...). Nobody can be asked
            // any questions, so save what can be saved and stop.
            out << "\n\nEnd of input reached. Closing the system.\n";
            finishSession(in, out, false);
            return unsavedChanges_ ? EXIT_UNSAVED_CHANGES : EXIT_OK;
        }

        int choice = 0;
        if (!parseMenuChoice(line, choice)) {
            out << "\nInvalid choice. Please enter a whole number from 1 to "
                << MENU_EXIT_CHOICE << ".\n";
            continue;  // show the menu again
        }

        // Automatic saving happens ONLY after a successful state change: the
        // handlers return true exactly when something changed.
        switch (choice) {
            case 1:
                if (registerPatientInteractive(in, out)) { autoSave(out); }
                break;
            case 2:
                displayWaitingPatients(out);
                break;
            case 3:
                if (serveNextPatient(out)) { autoSave(out); }
                break;
            case 4:
                searchPatient(in, out);
                break;
            case 5:
                if (cancelPatientAuthorized(in, out)) { autoSave(out); }
                break;
            case 6:
                displayStatistics(out);
                break;
            case 7:
                saveRecordsInteractive(in, out);
                break;
            case MENU_EXIT_CHOICE:
                if (confirmExit(in, out)) {
                    return unsavedChanges_ ? EXIT_UNSAVED_CHANGES : EXIT_OK;
                }
                break;
            default:
                break;  // unreachable: parseMenuChoice only returns 1..8
        }
    }
}

bool HospitalManagementSystem::registerPatientInteractive(std::istream& in,
                                                          std::ostream& out) {
    out << "\n---------- REGISTER NEW PATIENT ----------\n";

    const std::string ageRangeText = "0 and " + std::to_string(Patient::MAX_AGE);

    std::string fullName;
    std::string gender;
    std::string medicalCondition;
    int age = 0;
    int priorityValue = 0;

    // && short-circuits: the first field that hits end-of-input stops the
    // chain. Nothing has touched the queue yet at this point.
    const bool allFieldsCollected =
        promptNonBlankText(in, out, "Full name: ",
                           "Name cannot be empty.", fullName) &&
        promptInteger(in, out, "Age (0-" + std::to_string(Patient::MAX_AGE) + "): ",
                      "Age must be a whole number between " + ageRangeText + ".",
                      0, Patient::MAX_AGE, age) &&
        promptNonBlankText(in, out, "Gender: ",
                           "Gender cannot be empty.", gender) &&
        promptNonBlankText(in, out, "Medical condition: ",
                           "Medical condition cannot be empty.", medicalCondition) &&
        promptInteger(in, out,
                      "Priority (1 = Emergency, 2 = Urgent, 3 = Normal): ",
                      "Priority must be 1, 2 or 3.",
                      1, 3, priorityValue);

    if (!allFieldsCollected) {
        out << "\nInput ended before registration was complete. "
               "Registration cancelled; no patient was added.\n";
        return false;
    }

    // HospitalQueue re-validates everything. The checks above exist to give
    // friendly re-prompts; the checks inside HospitalQueue guarantee that
    // invalid data can never enter the queue, whoever calls it.
    try {
        const Patient patient = hospitalQueue_.registerPatient(
            fullName, age, gender, medicalCondition, priorityValue);
        displayRegistrationConfirmation(patient, out);
        unsavedChanges_ = true;  // memory now differs from the data file
        return true;
    } catch (const std::invalid_argument& error) {
        out << "\nRegistration failed: " << error.what() << "\n";
        return false;
    }
}

bool HospitalManagementSystem::serveNextPatient(std::ostream& out) {
    if (hospitalQueue_.isEmpty()) {
        out << "\nThere are no patients waiting. The queue is empty.\n";
        return false;
    }

    // popNext() removes the patient, marks them Served in the registry and
    // skips any cancelled patients automatically.
    const Patient patient = hospitalQueue_.popNext();
    unsavedChanges_ = true;  // memory now differs from the data file

    out << "\n========== NOW SERVING ==========\n";
    printPatientDetails(patient, out);
    out << "Status            : " << statusToString(PatientStatus::Served) << "\n"
        << "Patients still waiting: " << hospitalQueue_.size() << "\n"
        << "=================================\n";
    return true;
}

void HospitalManagementSystem::displayWaitingPatients(std::ostream& out) const {
    // getWaitingPatients() works on a copy of the heap, so showing the list
    // never alters the real queue.
    const std::vector<Patient> waiting = hospitalQueue_.getWaitingPatients();

    out << "\n========== WAITING PATIENTS (in serving order) ==========\n";
    if (waiting.empty()) {
        out << "No patients are currently waiting.\n";
        return;
    }

    const std::ios_base::fmtflags savedFlags = out.flags();  // restored below
    out << std::left
        << std::setw(7)  << "Order"
        << std::setw(6)  << "ID"
        << std::setw(23) << "Name"
        << std::setw(5)  << "Age"
        << std::setw(11) << "Gender"
        << std::setw(11) << "Priority"
        << std::setw(9)  << "Arrival"
        << "Condition\n"
        << std::string(85, '-') << "\n";

    std::size_t position = 1;
    for (const Patient& patient : waiting) {
        out << std::setw(7)  << position
            << std::setw(6)  << patient.getId()
            << std::setw(23) << shorten(patient.getFullName(), 22)
            << std::setw(5)  << patient.getAge()
            << std::setw(11) << shorten(patient.getGender(), 10)
            << std::setw(11) << priorityToString(patient.getPriority())
            << std::setw(9)  << patient.getArrivalSequence()
            << patient.getMedicalCondition() << "\n";  // last column: never padded
        ++position;
    }
    out.flags(savedFlags);

    out << "Total waiting: " << waiting.size() << "\n";
}

bool HospitalManagementSystem::searchPatient(std::istream& in, std::ostream& out) const {
    out << "\n---------- SEARCH PATIENT ----------\n";

    int patientId = 0;
    if (!promptPatientId(in, out, patientId)) {
        out << "\nInput ended before an ID was entered. Search cancelled.\n";
        return false;
    }

    const std::optional<PatientRecord> record = hospitalQueue_.findPatient(patientId);
    if (!record) {
        out << "\nNo patient with ID " << patientId << " exists.\n";
        return false;
    }

    out << "\n========== PATIENT FOUND ==========\n";
    printPatientDetails(record->patient, out);
    out << "Status            : " << statusToString(record->status) << "\n";

    if (record->status == PatientStatus::Waiting) {
        // Position = place in the true serving order (1 = served next).
        const std::vector<Patient> waiting = hospitalQueue_.getWaitingPatients();
        for (std::size_t index = 0; index < waiting.size(); ++index) {
            if (waiting[index].getId() == patientId) {
                out << "Queue Position    : " << (index + 1) << " of "
                    << waiting.size() << "\n";
                break;
            }
        }
    }
    out << "===================================\n";
    return true;
}

bool HospitalManagementSystem::cancelPatient(std::istream& in, std::ostream& out) {
    out << "\n---------- CANCEL PATIENT REGISTRATION ----------\n";

    int patientId = 0;
    if (!promptPatientId(in, out, patientId)) {
        out << "\nInput ended before an ID was entered. Cancellation aborted.\n";
        return false;
    }

    // Look the patient up first so the messages below can name them.
    const std::optional<PatientRecord> record = hospitalQueue_.findPatient(patientId);

    switch (hospitalQueue_.cancelPatient(patientId)) {
        case CancelResult::Cancelled:
            unsavedChanges_ = true;  // memory now differs from the data file
            out << "\nRegistration cancelled for patient ID " << patientId << " ("
                << record->patient.getFullName() << ").\n"
                << "The patient was removed from the waiting queue and will not be served.\n";
            return true;
        case CancelResult::AlreadyServed:
            out << "\nCannot cancel: patient ID " << patientId << " ("
                << record->patient.getFullName() << ") has already been served.\n";
            return false;
        case CancelResult::AlreadyCancelled:
            out << "\nNothing to do: the registration of patient ID " << patientId
                << " (" << record->patient.getFullName()
                << ") was already cancelled.\n";
            return false;
        case CancelResult::NotFound:
            out << "\nNo patient with ID " << patientId << " exists.\n";
            return false;
    }
    return false;  // Unreachable for valid enum values; keeps compilers quiet
}

bool HospitalManagementSystem::authorizeStaff(std::istream& in, std::ostream& out) const {
    out << "Authorization required: only authorized staff may cancel a registration.\n"
           "(Educational mechanism: a shared staff PIN checked by the program. This is a\n"
           "classroom demonstration of access control, not real security.)\n";

    for (int attempt = 1; attempt <= MAX_PIN_ATTEMPTS; ++attempt) {
        out << "Enter staff PIN: ";
        std::string entered;
        if (!std::getline(in, entered)) {
            out << "\nInput ended before a PIN was entered. Cancellation aborted.\n";
            return false;
        }
        if (trimmed(entered) == staffPin_) {
            out << "Authorization accepted.\n";
            return true;
        }
        out << "Incorrect PIN (attempt " << attempt << " of " << MAX_PIN_ATTEMPTS << ").\n";
    }

    out << "\nAccess denied: no registration was cancelled.\n";
    return false;
}

bool HospitalManagementSystem::cancelPatientAuthorized(std::istream& in, std::ostream& out) {
    out << "\n---------- CANCEL PATIENT REGISTRATION ----------\n";

    // Step 1: authorization comes first, so an unauthorized user cannot even
    // use the ID prompt to find out which patients exist.
    if (!authorizeStaff(in, out)) {
        return false;
    }

    // Step 2: identify the patient and refuse impossible requests at once.
    int patientId = 0;
    if (!promptPatientId(in, out, patientId)) {
        out << "\nInput ended before an ID was entered. Cancellation aborted.\n";
        return false;
    }
    const std::optional<PatientRecord> record = hospitalQueue_.findPatient(patientId);
    if (!record) {
        out << "\nNo patient with ID " << patientId << " exists.\n";
        return false;
    }
    if (record->status == PatientStatus::Served) {
        out << "\nCannot cancel: patient ID " << patientId << " ("
            << record->patient.getFullName() << ") has already been served.\n";
        return false;
    }
    if (record->status == PatientStatus::Cancelled) {
        out << "\nNothing to do: the registration of patient ID " << patientId
            << " (" << record->patient.getFullName() << ") was already cancelled.\n";
        return false;
    }

    // Step 3: show who would be cancelled and require an explicit "yes".
    out << "\nPatient to cancel:\n";
    printPatientDetails(record->patient, out);
    const Answer answer = askYesNo(in, out, "\nCancel this patient's registration? (yes/no): ");
    if (answer == Answer::EndOfInput) {
        out << "\nInput ended before confirmation. Cancellation aborted; "
               "the patient remains in the queue.\n";
        return false;
    }
    if (answer == Answer::No) {
        out << "\nCancellation aborted. The patient remains in the queue.\n";
        return false;
    }

    if (hospitalQueue_.cancelPatient(patientId) != CancelResult::Cancelled) {
        // Cannot happen (status was checked above); reported rather than assumed.
        out << "\nError: the registration could not be cancelled.\n";
        return false;
    }
    unsavedChanges_ = true;  // memory now differs from the data file
    out << "\nRegistration cancelled for patient ID " << patientId << " ("
        << record->patient.getFullName() << ").\n"
        << "The patient was removed from the waiting queue and will not be served.\n";
    return true;
}

void HospitalManagementSystem::autoSave(std::ostream& out) {
    // saveRecords() prints what happened. It reports success only when the
    // file really was written, so a failure can never be shown as a save.
    if (!saveRecords(out)) {
        out << "WARNING: The latest change is NOT saved to disk. It is held in memory only.\n"
               "The system will try again after the next change, when you choose\n"
               "'Save Patient Records', and when you exit.\n";
    }
}

void HospitalManagementSystem::saveRecordsInteractive(std::istream& in, std::ostream& out) {
    out << "\n---------- SAVE PATIENT RECORDS ----------";
    if (saveRecords(out)) {
        return;
    }
    if (!dataFileUnreadable_) {
        out << "WARNING: The records are NOT saved. They are held in memory only.\n";
        return;
    }

    // The data file is damaged. The user may replace it, but only
    // deliberately, and a backup copy of the damaged file is kept first.
    out << "The existing data file is damaged or unreadable. It can be replaced with the\n"
           "current records; a backup copy of the damaged file is kept as '"
        << dataFilePath_ << ".bak' first.\n";
    const Answer answer = askYesNo(in, out, "Replace the damaged data file? (yes/no): ");
    if (answer != Answer::Yes) {
        out << "\nThe data file was not changed. The records are NOT saved.\n";
        return;
    }

    const PatientStorage storage(dataFilePath_);
    const PatientStorage::SaveResult backup = storage.createBackupCopy();
    if (!backup.success) {
        out << "\nError: " << backup.message << "\n"
            << "The data file was not changed. The records are NOT saved.\n";
        return;
    }
    out << "\n" << backup.message << "\n";
    if (!saveRecords(out, true)) {
        out << "WARNING: The records are NOT saved. They are held in memory only.\n";
    }
}

bool HospitalManagementSystem::finishSession(std::istream& in, std::ostream& out, bool canAsk) {
    if (unsavedChanges_) {
        out << "\nSaving outstanding changes before exit...";
        if (!saveRecords(out)) {
            out << "WARNING: Your latest changes could NOT be saved and will be lost if you exit.\n";
            if (canAsk) {
                // Only an explicit "yes" may give up the unsaved changes.
                // "no" and a closed input stream both keep the program open:
                // end-of-input is never taken as permission to discard data.
                // (If the input really is closed, run() reaches its own
                // end-of-input shutdown next, which reports the loss loudly
                // and returns EXIT_UNSAVED_CHANGES.)
                const Answer answer = askYesNo(
                    in, out, "Exit anyway and lose the unsaved changes? (yes/no): ");
                if (answer == Answer::No) {
                    out << "\nReturning to the main menu.\n";
                    return false;
                }
                if (answer == Answer::EndOfInput) {
                    out << "\nInput ended before you answered. Exiting is NOT assumed and the\n"
                           "unsaved changes have not been given up by choice.\n";
                    return false;
                }
            } else {
                out << "\nInput has ended, so no confirmation is possible and the program must close.\n";
            }
            out << "\nExiting with UNSAVED changes.\n";
        }
    }
    out << "\nThank you for using the Hospital Queue Management System. Goodbye.\n";
    return true;
}

bool HospitalManagementSystem::confirmExit(std::istream& in, std::ostream& out) {
    const Answer answer = askYesNo(in, out, "\nAre you sure you want to exit? (yes/no): ");
    if (answer == Answer::No) {
        out << "\nReturning to the main menu.\n";
        return false;
    }
    if (answer == Answer::EndOfInput) {
        // A closed input stream is not an answer. Do not treat it as "yes":
        // go back to the menu loop, which finds the closed input and runs the
        // end-of-input shutdown (it still tries to save outstanding changes).
        out << "\nInput ended before exit was confirmed. Exit is not assumed.\n";
        return false;
    }
    return finishSession(in, out, true);
}

void HospitalManagementSystem::displayStatistics(std::ostream& out) const {
    const QueueStatistics stats = hospitalQueue_.getStatistics();

    const std::ios_base::fmtflags savedFlags = out.flags();  // restored below
    out << "\n========== QUEUE STATISTICS ==========\n"
        << std::left
        << std::setw(28) << "Total patients registered" << ": " << stats.totalRegistered << "\n"
        << std::setw(28) << "Patients currently waiting" << ": " << stats.waiting << "\n"
        << std::setw(28) << "Patients served" << ": " << stats.served << "\n"
        << std::setw(28) << "Registrations cancelled" << ": " << stats.cancelled << "\n"
        << "--------------------------------------\n"
        << "Waiting patients by priority:\n"
        << std::setw(28) << "  Emergency (1)" << ": " << stats.waitingEmergency << "\n"
        << std::setw(28) << "  Urgent (2)" << ": " << stats.waitingUrgent << "\n"
        << std::setw(28) << "  Normal (3)" << ": " << stats.waitingNormal << "\n"
        << "======================================\n";
    out.flags(savedFlags);
}

bool HospitalManagementSystem::saveRecords(std::ostream& out, bool overwriteUnreadableFile) {
    if (dataFileUnreadable_ && !overwriteUnreadableFile) {
        out << "\nSave refused: the data file '" << dataFilePath_
            << "' exists but could not be loaded, so saving now would overwrite it.\n"
               "Repair or move that file first.\n";
        return false;
    }

    const PatientStorage storage(dataFilePath_);
    const PatientStorage::SaveResult result = storage.save(hospitalQueue_.getAllRecords());
    if (!result.success) {
        out << "\nError: " << result.message << "\n";
        return false;
    }

    dataFileUnreadable_ = false;  // the file now holds valid data again
    unsavedChanges_     = false;  // the file now matches memory
    out << "\n" << result.message << "\n";
    return true;
}

bool HospitalManagementSystem::loadSavedRecords(std::ostream& out) {
    const PatientStorage storage(dataFilePath_);
    const PatientStorage::LoadResult result = storage.load();

    switch (result.status) {
        case PatientStorage::LoadStatus::FileNotFound:
            dataFileUnreadable_ = false;
            out << "\n" << result.message << " Starting with an empty registry.\n";
            return true;

        case PatientStorage::LoadStatus::EmptyFile:
            dataFileUnreadable_ = false;
            out << "\n" << result.message << " Starting with an empty registry.\n";
            return true;

        case PatientStorage::LoadStatus::InvalidData:
        case PatientStorage::LoadStatus::IoError:
            dataFileUnreadable_ = true;  // block saving over this file
            out << "\nError: " << result.message << "\n"
                << "The saved file was left untouched and saving is disabled until it is repaired.\n";
            return false;

        case PatientStorage::LoadStatus::Loaded:
            break;
    }

    try {
        // HospitalQueue re-checks duplicate IDs and sequences and is
        // all-or-nothing, so a failure here leaves the registry unchanged.
        hospitalQueue_.loadRecords(result.records);
    } catch (const std::invalid_argument& problem) {
        dataFileUnreadable_ = true;
        out << "\nError: invalid data in '" << dataFilePath_ << "': " << problem.what()
            << " No records were loaded.\n"
            << "The saved file was left untouched and saving is disabled until it is repaired.\n";
        return false;
    }

    dataFileUnreadable_ = false;
    unsavedChanges_     = false;  // memory was just replaced by the file contents
    const QueueStatistics stats = hospitalQueue_.getStatistics();
    out << "\n" << result.message << "\n"
        << stats.waiting << " waiting, " << stats.served << " served, "
        << stats.cancelled << " cancelled.\n";
    return true;
}

const std::string& HospitalManagementSystem::getDataFilePath() const { return dataFilePath_; }

bool HospitalManagementSystem::hasUnsavedChanges() const { return unsavedChanges_; }

bool HospitalManagementSystem::isSavingBlocked() const { return dataFileUnreadable_; }

const HospitalQueue& HospitalManagementSystem::getQueue() const {
    return hospitalQueue_;
}
