// PatientStorage.cpp
// Implementation of PatientStorage (see PatientStorage.h for the file format).

#include "PatientStorage.h"

#include <charconv>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace {

constexpr char        FIELD_SEPARATOR = '\t';
constexpr std::size_t FIELD_COUNT     = 8;
constexpr const char* FOOTER_NAME     = "END";

// ---------------------------------------------------------------------
// Escaping
// ---------------------------------------------------------------------

// Makes a text field safe to store: afterwards it contains no real TAB,
// newline or carriage return, and every backslash is doubled.
std::string escapeField(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '\t': escaped += "\\t";  break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            default:   escaped += c;      break;
        }
    }
    return escaped;
}

// Reverses escapeField. Returns false for an unknown escape sequence or a
// lone backslash at the end, which means the field was not written by us.
bool unescapeField(const std::string& text, std::string& result) {
    result.clear();
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\') {
            result += text[i];
            continue;
        }
        if (i + 1 >= text.size()) {
            return false;  // backslash with nothing after it
        }
        switch (text[++i]) {
            case '\\': result += '\\'; break;
            case 't':  result += '\t'; break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            default:   return false;   // unknown escape such as \q
        }
    }
    return true;
}

// ---------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------

// Splits a line on TAB characters. "a\tb" -> {"a","b"}; "a\t" -> {"a",""}.
std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = line.find(FIELD_SEPARATOR, start);
        if (end == std::string::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
}

// Strict whole-string integer parse: rejects "", " 5", "5 ", "5x", "+5",
// "3.5" and numbers that do not fit in the target type.
template <typename Integer>
bool parseWholeNumber(const std::string& text, Integer& value) {
    if (text.empty()) {
        return false;
    }
    const char* first = text.data();
    const char* last  = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc() && result.ptr == last;
}

bool parseStatus(const std::string& text, PatientStatus& status) {
    if (text == "Waiting")   { status = PatientStatus::Waiting;   return true; }
    if (text == "Served")    { status = PatientStatus::Served;    return true; }
    if (text == "Cancelled") { status = PatientStatus::Cancelled; return true; }
    return false;
}

bool isBlank(const std::string& text) {
    return text.find_first_not_of(" \t\r\n\v\f") == std::string::npos;
}

// Removes ONE trailing carriage return, so files that were edited on
// Windows (CRLF line endings) still load.
void stripTrailingCarriageReturn(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

// Builds one record line (without the final newline).
std::string formatRecord(const PatientRecord& record) {
    const Patient& p = record.patient;
    std::string line;
    line += std::to_string(p.getId());                              line += FIELD_SEPARATOR;
    line += escapeField(p.getFullName());                           line += FIELD_SEPARATOR;
    line += std::to_string(p.getAge());                             line += FIELD_SEPARATOR;
    line += escapeField(p.getGender());                             line += FIELD_SEPARATOR;
    line += escapeField(p.getMedicalCondition());                   line += FIELD_SEPARATOR;
    line += std::to_string(static_cast<int>(p.getPriority()));      line += FIELD_SEPARATOR;
    line += std::to_string(p.getArrivalSequence());                 line += FIELD_SEPARATOR;
    line += statusToString(record.status);
    return line;
}

// Validates and converts one record line. On failure returns std::nullopt
// and puts the reason in `error`.
std::optional<PatientRecord> parseRecord(const std::string& line, std::string& error) {
    const std::vector<std::string> fields = splitFields(line);
    if (fields.size() != FIELD_COUNT) {
        error = "expected " + std::to_string(FIELD_COUNT) + " fields but found " +
                std::to_string(fields.size());
        return std::nullopt;
    }

    int           id = 0;
    int           age = 0;
    int           priorityValue = 0;
    long          arrival = 0;
    PatientStatus status = PatientStatus::Waiting;
    std::string   name, gender, condition;

    if (!parseWholeNumber(fields[0], id))            { error = "patient ID is not a valid whole number"; return std::nullopt; }
    if (!unescapeField(fields[1], name))             { error = "name contains an invalid escape sequence"; return std::nullopt; }
    if (!parseWholeNumber(fields[2], age))           { error = "age is not a valid whole number"; return std::nullopt; }
    if (!unescapeField(fields[3], gender))           { error = "gender contains an invalid escape sequence"; return std::nullopt; }
    if (!unescapeField(fields[4], condition))        { error = "medical condition contains an invalid escape sequence"; return std::nullopt; }
    if (!parseWholeNumber(fields[5], priorityValue)) { error = "priority is not a valid whole number"; return std::nullopt; }
    if (!parseWholeNumber(fields[6], arrival))       { error = "arrival sequence is not a valid whole number"; return std::nullopt; }
    if (!parseStatus(fields[7], status))             { error = "unknown status '" + fields[7] + "'"; return std::nullopt; }

    // Patient and priorityFromInt validate the remaining rules (positive ID
    // and sequence, non-blank name, age 0-120, priority 1-3) and throw
    // std::invalid_argument with a clear message when one is broken.
    // Gender and condition are required by registration, so they are
    // required here too.
    try {
        if (isBlank(gender))    { throw std::invalid_argument("gender must not be empty"); }
        if (isBlank(condition)) { throw std::invalid_argument("medical condition must not be empty"); }
        return PatientRecord{Patient(id, name, age, gender, condition,
                                     priorityFromInt(priorityValue), arrival),
                             status};
    } catch (const std::invalid_argument& problem) {
        error = problem.what();
        return std::nullopt;
    }
}

}  // namespace

// ---------------------------------------------------------------------
// PatientStorage
// ---------------------------------------------------------------------

PatientStorage::PatientStorage(fs::path filePath) : filePath_(std::move(filePath)) {}

const fs::path& PatientStorage::getFilePath() const { return filePath_; }

PatientStorage::SaveResult PatientStorage::save(const std::vector<PatientRecord>& records) const {
    const auto failure = [this](const std::string& reason) {
        return SaveResult{false, "Could not save to '" + filePath_.string() + "': " + reason};
    };

    // Step 1: make sure the folder (for example data/) exists.
    std::error_code ec;
    const fs::path folder = filePath_.parent_path();
    if (!folder.empty()) {
        fs::create_directories(folder, ec);
        if (ec) {
            return failure("cannot create folder '" + folder.string() + "' (" + ec.message() + ")");
        }
    }

    // Step 2: write everything to a temporary file next to the real one.
    // The real file is not touched until the temporary file is complete.
    fs::path tempPath = filePath_;
    tempPath += ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            return failure("cannot create temporary file '" + tempPath.string() + "'");
        }
        out << FORMAT_NAME << FIELD_SEPARATOR << FORMAT_VERSION << '\n';
        for (const PatientRecord& record : records) {
            out << formatRecord(record) << '\n';
        }
        out << FOOTER_NAME << FIELD_SEPARATOR << records.size() << '\n';
        out.flush();
        out.close();
        if (out.fail()) {  // disk full, I/O error...
            fs::remove(tempPath, ec);
            return failure("writing the file failed (disk full or I/O error)");
        }
    }

    // Step 3: swap the finished file into place. rename() replaces the old
    // file in one step, so readers see either the old or the new file, never
    // a mixture.
    fs::rename(tempPath, filePath_, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(tempPath, ignored);
        return failure("cannot replace the data file (" + ec.message() + ")");
    }

    return SaveResult{true, "Saved " + std::to_string(records.size()) + " patient record(s) to '" +
                                filePath_.string() + "'."};
}

PatientStorage::SaveResult PatientStorage::createBackupCopy() const {
    fs::path backupPath = filePath_;
    backupPath += ".bak";

    std::error_code ec;
    if (!fs::is_regular_file(filePath_, ec)) {
        return SaveResult{false, "Cannot back up '" + filePath_.string() + "': it is not a regular file."};
    }
    fs::copy_file(filePath_, backupPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return SaveResult{false, "Cannot create the backup '" + backupPath.string() + "' (" + ec.message() + ")."};
    }
    return SaveResult{true, "A backup copy of the damaged file was saved as '" + backupPath.string() + "'."};
}

PatientStorage::LoadResult PatientStorage::load() const {
    LoadResult result;
    const std::string where = "'" + filePath_.string() + "'";

    const auto fail = [&result](LoadStatus status, const std::string& message) {
        result.status = status;
        result.records.clear();  // all-or-nothing: never return partial data
        result.message = message;
        return result;
    };

    // --- Does the file exist and can it be read? ---
    std::error_code ec;
    const bool exists = fs::exists(filePath_, ec);
    if (ec) {
        return fail(LoadStatus::IoError, "Cannot access " + where + ": " + ec.message());
    }
    if (!exists) {
        return fail(LoadStatus::FileNotFound, "No saved data file found at " + where + ".");
    }
    if (!fs::is_regular_file(filePath_, ec)) {
        return fail(LoadStatus::IoError, where + " is not a regular file.");
    }
    const std::uintmax_t size = fs::file_size(filePath_, ec);
    if (ec) {
        return fail(LoadStatus::IoError, "Cannot read the size of " + where + ": " + ec.message());
    }
    if (size == 0) {
        return fail(LoadStatus::EmptyFile, "The data file " + where + " is empty.");
    }

    std::ifstream in(filePath_, std::ios::binary);
    if (!in) {
        return fail(LoadStatus::IoError, "Cannot open " + where + " for reading.");
    }

    const auto invalid = [&](std::size_t lineNumber, std::string reason) {
        // Some reasons come from Patient's exception text, which already ends
        // with a full stop; drop it so the message does not end in "..".
        if (!reason.empty() && reason.back() == '.') {
            reason.pop_back();
        }
        return fail(LoadStatus::InvalidData,
                    "Invalid data file " + where + ", line " + std::to_string(lineNumber) +
                        ": " + reason + ". No records were loaded.");
    };

    // --- Header: format name and version ---
    std::string line;
    std::size_t lineNumber = 1;
    if (!std::getline(in, line)) {
        return fail(LoadStatus::IoError, "Cannot read from " + where + ".");
    }
    stripTrailingCarriageReturn(line);
    const std::string namePrefix = std::string(FORMAT_NAME) + FIELD_SEPARATOR;
    if (line.compare(0, namePrefix.size(), namePrefix) != 0) {
        return invalid(lineNumber, "this is not a hospital patient records file (bad header)");
    }
    if (line != namePrefix + std::to_string(FORMAT_VERSION)) {
        return invalid(lineNumber, "unsupported format version '" +
                                       line.substr(namePrefix.size()) + "' (this program reads version " +
                                       std::to_string(FORMAT_VERSION) + ")");
    }

    // --- Records, then the END footer ---
    std::set<int>  seenIds;
    std::set<long> seenSequences;
    bool           footerSeen = false;
    std::size_t    declaredCount = 0;

    while (std::getline(in, line)) {
        ++lineNumber;
        stripTrailingCarriageReturn(line);

        if (footerSeen) {
            return invalid(lineNumber, "unexpected data after the END line");
        }

        if (line.compare(0, std::char_traits<char>::length(FOOTER_NAME), FOOTER_NAME) == 0) {
            const std::string expected = std::string(FOOTER_NAME) + FIELD_SEPARATOR;
            if (line.compare(0, expected.size(), expected) != 0 ||
                !parseWholeNumber(line.substr(expected.size()), declaredCount)) {
                return invalid(lineNumber, "malformed END line");
            }
            footerSeen = true;
            continue;
        }

        std::string error;
        std::optional<PatientRecord> record = parseRecord(line, error);
        if (!record) {
            return invalid(lineNumber, error);
        }
        if (!seenIds.insert(record->patient.getId()).second) {
            return invalid(lineNumber, "duplicate patient ID " + std::to_string(record->patient.getId()));
        }
        if (!seenSequences.insert(record->patient.getArrivalSequence()).second) {
            return invalid(lineNumber, "duplicate arrival sequence " +
                                           std::to_string(record->patient.getArrivalSequence()));
        }
        result.records.push_back(std::move(*record));
    }

    if (in.bad()) {
        return fail(LoadStatus::IoError, "A read error occurred while reading " + where + ".");
    }
    if (!footerSeen) {
        return invalid(lineNumber, "the END line is missing (the file looks truncated)");
    }
    if (declaredCount != result.records.size()) {
        return invalid(lineNumber, "the END line declares " + std::to_string(declaredCount) +
                                       " record(s) but " + std::to_string(result.records.size()) +
                                       " were found");
    }

    result.status  = LoadStatus::Loaded;
    result.message = "Loaded " + std::to_string(result.records.size()) + " patient record(s) from " +
                     where + ".";
    return result;
}
