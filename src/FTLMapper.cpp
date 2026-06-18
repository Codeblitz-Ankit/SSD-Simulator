#include "FTLMapper.h"

void FTLMapper::mapLogicalToPhysical(
    int logicalAddress,
    PhysicalAddress physicalAddress
) {
    mappingTable[logicalAddress] = physicalAddress;
}

bool FTLMapper::hasMapping(
    int logicalAddress
) const {

    return mappingTable.find(logicalAddress)
           != mappingTable.end();
}

PhysicalAddress FTLMapper::getPhysicalAddress(
    int logicalAddress
) const {

    return mappingTable.at(logicalAddress);
}

PhysicalAddress FTLMapper::removeMapping(
    int logicalAddress
) {

    PhysicalAddress addr =
        mappingTable.at(logicalAddress);

    mappingTable.erase(logicalAddress);

    return addr;
}

int FTLMapper::findLogicalAddress(
    const PhysicalAddress& physAddr
) const {

    for (const auto& entry : mappingTable) {

        if (entry.second.blockIndex == physAddr.blockIndex &&
            entry.second.pageIndex  == physAddr.pageIndex) {

            return entry.first;     // return the LBA
        }
    }

    return -1;  // no LBA maps to this physical address
}
