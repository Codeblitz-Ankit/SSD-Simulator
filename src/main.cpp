#include <iostream>
#include <memory>

#include "SSDController.h"
#include "WriteRequest.h"
#include "RequestQueue.h"

int main() {

    SSDController ssd(2, 4);

    RequestQueue queue;

    queue.addRequest(

        std::make_unique<WriteRequest>(
            ssd,
            100,
            "Operating Systems"
        )
    );

    queue.addRequest(

        std::make_unique<WriteRequest>(
            ssd,
            200,
            "Computer Networks"
        )
    );

    while (!queue.isEmpty()) {

        std::unique_ptr<IORequest>
            request =
                queue.getNextRequest();

        request->execute();
    }

    std::cout
        << ssd.read(100)
        << std::endl;

    std::cout
        << ssd.read(200)
        << std::endl;

    ssd.displayStatus();

    return 0;
}