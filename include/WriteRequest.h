#ifndef WRITEREQUEST_H
#define WRITEREQUEST_H

#include <string>

#include "IORequest.h"
#include "SSDController.h"

class WriteRequest : public IORequest {
private:
    SSDController& ssd;

    int logicalAddress;

    std::string data;

public:
    WriteRequest(
        SSDController& controller,
        int address,
        const std::string& inputData
    );

    void execute() override;
};

#endif