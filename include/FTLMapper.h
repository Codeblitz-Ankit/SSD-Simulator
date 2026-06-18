#ifndef FTLMAPPER_H
#define FTLMAPPER_H

#include <unordered_map>
#include "PhysicalAddress.h"

class FTLMapper {
private:
    std::unordered_map<int, PhysicalAddress> mappingTable;

public:
    void mapLogicalToPhysical(
        int logicalAddress,
        PhysicalAddress physicalAddress
    );

    bool hasMapping(int logicalAddress) const;

    PhysicalAddress getPhysicalAddress(
        int logicalAddress
    ) const;

    PhysicalAddress removeMapping(
        int logicalAddress
    );

    // Reverse lookup: given a physical address, find the LBA that maps to it.
    // Returns -1 if no LBA currently maps to this physical page.
    // O(n) scan — only called from GC, so performance is acceptable.
    int findLogicalAddress(
        const PhysicalAddress& physAddr
    ) const;
};

#endif