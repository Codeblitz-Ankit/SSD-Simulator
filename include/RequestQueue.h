#ifndef REQUESTQUEUE_H
#define REQUESTQUEUE_H

#include <queue>
#include <memory>
#include <mutex>

#include "IORequest.h"

class RequestQueue {
private:
    std::queue<
        std::unique_ptr<IORequest>
    > requests;

    mutable std::mutex queueMutex;

public:
    void addRequest(
        std::unique_ptr<IORequest> request
    );

    bool isEmpty() const;

    std::unique_ptr<IORequest>
    getNextRequest();
};

#endif