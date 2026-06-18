#include "RequestQueue.h"

void RequestQueue::addRequest(
    std::unique_ptr<IORequest> request
) {

    requests.push(
        std::move(request)
    );
}

bool RequestQueue::isEmpty() const {

    return requests.empty();
}

std::unique_ptr<IORequest>
RequestQueue::getNextRequest() {

    if (requests.empty()) {
        return nullptr;
    }

    std::unique_ptr<IORequest> next =
        std::move(requests.front());

    requests.pop();

    return next;
}