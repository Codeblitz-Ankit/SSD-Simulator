#include "Page.h"

Page::Page() {
    data = "";
    isFree = true;
    isValid = false;
}

void Page::writeData(const std::string& newData) {
    data = newData;
    isFree = false;
    isValid = true;
}

std::string Page::readData() const {
    return data;
}

void Page::invalidate() {
    isValid = false;
}

bool Page::getIsFree() const {
    return isFree;
}

bool Page::getIsValid() const {
    return isValid;
}
