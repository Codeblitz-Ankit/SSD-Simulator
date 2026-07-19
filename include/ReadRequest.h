#ifndef READREQUEST_H
#define READREQUEST_H

#include <string>

#include "IORequest.h"
#include "SSDController.h"

// ─────────────────────────────────────────────────────────────────────────────
// ReadRequest — Concrete IORequest for a host read operation
//
// Dispatches a single read to the SSDController and stores the result.
// Complements WriteRequest; both are enqueued into a RequestQueue for
// trace-driven workload replay (Phase 5).
// ─────────────────────────────────────────────────────────────────────────────

class ReadRequest : public IORequest {
private:
    SSDController& ssd;
    int            logicalAddress;
    std::string    result;   // populated after execute()

public:
    ReadRequest(SSDController& controller, int address);

    void execute() override;

    // Returns the data read by the last execute() call.
    const std::string& getResult() const { return result; }
};

#endif
