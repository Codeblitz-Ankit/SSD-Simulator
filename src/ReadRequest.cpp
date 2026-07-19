#include "ReadRequest.h"

ReadRequest::ReadRequest(SSDController& controller, int address)
    : ssd(controller),
      logicalAddress(address),
      result()
{}

void ReadRequest::execute() {
    result = ssd.read(logicalAddress);
}
