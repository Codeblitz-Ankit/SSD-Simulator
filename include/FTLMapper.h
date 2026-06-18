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
};

#endif