#include "WriteRequest.h"

WriteRequest::WriteRequest(
    SSDController& controller,
    int address,
    const std::string& inputData
)
    : ssd(controller),
      logicalAddress(address),
      data(inputData) {}

void WriteRequest::execute() {

    ssd.write(
        logicalAddress,
        data
    );
}