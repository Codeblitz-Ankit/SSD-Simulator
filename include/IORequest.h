#ifndef IOREQUEST_H
#define IOREQUEST_H

class IORequest {
public:
    virtual void execute() = 0;

    virtual ~IORequest() {}
};

#endif