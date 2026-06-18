#ifndef PAGE_H
#define PAGE_H

#include <string>

class Page {
private:
    std::string data;
    bool isFree;
    bool isValid;

public:
    Page();

    void writeData(const std::string& newData);

    std::string readData() const;

    void invalidate();

    bool getIsFree() const;

    bool getIsValid() const;
};

#endif
