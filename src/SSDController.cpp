#include "SSDController.h"
#include <iostream>

SSDController::SSDController(
    int numBlocks,
    int pagesPerBlock
)
    : flashMemory(
          numBlocks,
          pagesPerBlock
      ) {}

void SSDController::write(
    int logicalAddress,
    const std::string& data
) {
    int freeBlock =
        flashMemory.findLeastUsedFreeBlock();

    if (freeBlock == -1) {

        std::cout
            << "ERROR: SSD FULL\n";

        return;
    }

    int freePage =
        flashMemory.getBlock(
            freeBlock
        ).getFreePageIndex();

    flashMemory.getBlock(
        freeBlock
    ).getPage(
        freePage
    ).writeData(data);

    PhysicalAddress addr;

    addr.blockIndex = freeBlock;
    addr.pageIndex = freePage;

    if (mapper.hasMapping(logicalAddress)) {

        PhysicalAddress oldAddr =
            mapper.removeMapping(
                logicalAddress
            );

        flashMemory.getBlock(
            oldAddr.blockIndex
        ).getPage(
            oldAddr.pageIndex
        ).invalidate();
    }

    mapper.mapLogicalToPhysical(
        logicalAddress,
        addr
    );
}

std::string SSDController::read(
    int logicalAddress
) {

    PhysicalAddress addr =
        mapper.getPhysicalAddress(
            logicalAddress
        );

    return flashMemory.getBlock(
               addr.blockIndex
           ).getPage(
               addr.pageIndex
           ).readData();
}

void SSDController::displayStatus() {

    std::cout
        << "\n===== SSD STATUS =====\n";

    for (int i = 0;
         i < flashMemory.getTotalBlocks();
         i++) {

        Block& block =
            flashMemory.getBlock(i);

        int freePages = 0;
        int validPages = 0;
        int invalidPages = 0;

        for (int j = 0;
             j < block.getTotalPages();
             j++) {

            Page& page =
                block.getPage(j);

            if (page.getIsFree()) {
                freePages++;
            }
            else if (page.getIsValid()) {
                validPages++;
            }
            else {
                invalidPages++;
            }
        }

        std::cout
            << "Block " << i
            << " | Free: " << freePages
            << " | Valid: " << validPages
            << " | Invalid: " << invalidPages
            << " | Erase Count: "
            << block.getEraseCount()
            << std::endl;
    }

    std::cout
        << "======================\n";
}

void SSDController::garbageCollect() {

    for (int i = 0;
         i < flashMemory.getTotalBlocks();
         i++) {

        Block& block =
            flashMemory.getBlock(i);

        int invalidPages =
            block.getInvalidPageCount();

        if (invalidPages ==
            block.getTotalPages()) {

            std::cout
                << "Garbage Collecting Block "
                << i << std::endl;

            block.eraseBlock();
        }
    }
}