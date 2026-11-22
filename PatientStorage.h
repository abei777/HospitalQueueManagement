// PatientStorage.h
// Saves patient records to a text file and loads them back.
//
// This class has one job: translating between PatientRecord objects and a
// file. It knows nothing about queues or menus; HospitalQueue owns the data
// and HospitalManagementSystem decides when to save or load.
//
// FILE FORMAT (version 1) - one record per line, fields separated by TAB:
//
//   HQMS-PATIENT-RECORDS<TAB>1                       <- format name + version
//   id<TAB>name<TAB>age<TAB>gender<TAB>condition<TAB>priority<TAB>arrival<TAB>status
//   ... one line per patient, in ascending ID order ...
//   END<TAB>3                                         <- number of records above
//
//   - priority is 1, 2 or 3; status is Waiting, Served or Cancelled.
//   - Text fields are escaped so that ANY text is stored safely:
//         backslash -> \\    tab -> \t    newline -> \n    carriage return -> \r
//     After escaping, a field can never contain a real TAB or line break, so
//     names such as "Smith, John \"Jr\"" or conditions with tabs survive intact.
//   - The END line lets the loader detect a truncated file.
//
// SAFETY:
//   - save() writes a temporary file and then renames it over the real file,
//     so a crash or write error can never leave a half-written data file.
//   - load() is all-or-nothing: if ANY line is invalid, no records are
//     returned at all and the file is not touched.
//
// NOTE: This project is an educational queue-management simulation.
// It is NOT a clinically validated triage system.

#ifndef PATIENT_STORAGE_H
#define PATIENT_STORAGE_H

#include <filesystem>
#include <string>
#include <vector>

#include "HospitalQueue.h"

class PatientStorage {
public:
    static constexpr const char* FORMAT_NAME    = "HQMS-PATIENT-RECORDS";
    static constexpr int         FORMAT_VERSION = 1;

    // Outcome categories of load().
    enum class LoadStatus {
        Loaded,       // File read and fully validated (it may hold 0 records)
        FileNotFound, // No file at this path (normal on the first ever run)
        EmptyFile,    // The file exists but has 0 bytes
        InvalidData,  // The file is malformed; nothing was accepted
        IoError       // The file exists but cannot be read (permissions, directory...)
    };

    struct LoadResult {
        LoadStatus                 status = LoadStatus::Loaded;
        std::vector<PatientRecord> records;  // empty unless status == Loaded
        std::string                message;  // human-readable explanation
    };

    struct SaveResult {
        bool        success = false;
        std::string message;  // human-readable explanation
    };

    explicit PatientStorage(std::filesystem::path filePath);

    const std::filesystem::path& getFilePath() const;

    // Writes all records to the file (creating missing parent folders such
    // as data/). Never throws for I/O problems; failures are reported in the
    // result, and an existing file is left untouched when a save fails.
    SaveResult save(const std::vector<PatientRecord>& records) const;

    // Reads and validates the file. Never throws for I/O or format problems;
    // they are reported through LoadResult::status and ::message.
    // Every record is validated (positive unique ID, non-blank name, age
    // 0-120, priority 1-3, known status, positive unique arrival sequence,
    // non-blank gender and condition) before anything is returned.
    LoadResult load() const;

    // Copies the existing data file to "<file>.bak" (replacing an older
    // backup). Used before deliberately overwriting a damaged file, so its
    // contents are never lost. Fails, without changing anything, if the file
    // does not exist or cannot be copied.
    SaveResult createBackupCopy() const;

private:
    std::filesystem::path filePath_;
};

#endif  // PATIENT_STORAGE_H
