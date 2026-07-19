#include "TraceParser.h"
#include "WriteRequest.h"
#include "ReadRequest.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>

// ─── Constructor ──────────────────────────────────────────────────────────────

TraceParser::TraceParser(const std::string& filePath)
    : filePath_(filePath),
      parsedWrites_(0),
      parsedReads_(0),
      skippedLines_(0)
{}

// ─── loadInto ─────────────────────────────────────────────────────────────────
//
// Opens the trace file and streams it line by line.
// Each non-blank, non-comment line must begin with 'W' or 'R'.
//
// W <LBA> <data>   → WriteRequest
// R <LBA>          → ReadRequest

bool TraceParser::loadInto(RequestQueue& queue, SSDController& ssd) {

    // Reset stats for each load
    parsedWrites_ = 0;
    parsedReads_  = 0;
    skippedLines_ = 0;

    std::ifstream file(filePath_);
    if (!file.is_open()) {
        std::cerr << "[TraceParser] ERROR: cannot open '" << filePath_ << "'\n";
        return false;
    }

    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        lineNumber++;

        // Skip blank lines and comment lines
        if (line.empty() || line[0] == '#') {
            skippedLines_++;
            continue;
        }

        std::istringstream iss(line);
        char opCode = '\0';
        iss >> opCode;

        if (opCode == 'W' || opCode == 'w') {
            int         lba;
            std::string data;
            if (!(iss >> lba)) {
                std::cerr << "[TraceParser] WARNING line " << lineNumber
                          << ": missing LBA for W — skipping.\n";
                skippedLines_++;
                continue;
            }
            // data is the rest of the line (may contain spaces)
            if (!std::getline(iss >> std::ws, data)) {
                data = "trace_data_lba" + std::to_string(lba);
            }

            queue.addRequest(
                std::make_unique<WriteRequest>(ssd, lba, data)
            );
            parsedWrites_++;

        } else if (opCode == 'R' || opCode == 'r') {
            int lba;
            if (!(iss >> lba)) {
                std::cerr << "[TraceParser] WARNING line " << lineNumber
                          << ": missing LBA for R — skipping.\n";
                skippedLines_++;
                continue;
            }

            queue.addRequest(
                std::make_unique<ReadRequest>(ssd, lba)
            );
            parsedReads_++;

        } else {
            std::cerr << "[TraceParser] WARNING line " << lineNumber
                      << ": unknown op '" << opCode << "' — skipping.\n";
            skippedLines_++;
        }
    }

    return true;
}
