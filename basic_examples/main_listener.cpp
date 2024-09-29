#include <iostream>
#include <cstdlib>
#include <atomic>
#include <cstdint>
#include <ctime>

#include <systemd/sd-bus.h>

std::atomic_bool stop_process = false;

void sighandler(int signum) {
    stop_process = true;
}

int signalCallback(sd_bus_message *msg, void *userData, sd_bus_error *err) {
    uint64_t heartBeatIndex;
    int64_t time;
    time_t t;
    
    int r = sd_bus_message_read(msg, "tx", &heartBeatIndex, &time);
    if(r >= 0) {
        t = time;
        //ctime use internal array, not thread safe!!!
        std::cout << "received heart beat: index=" << heartBeatIndex << " " << ctime(&t) << std::endl;
    }
    else {
        std::cerr << "Failed to read signal message" << std::endl;
    }
    
    return 1;
}

int main() {
    sd_bus *bus = NULL;
    int r;

    r = sd_bus_open_user(&bus);
    if (r < 0) {
        std::cerr << "Failed to connect to user bus: " << strerror(-r) << std::endl;
        stop_process = true;
    }
    
    r = sd_bus_match_signal(bus,
                            NULL,
                            "org.nicolas.PublisherExample",
                            "/org/nicolas/PublisherExample",
                            "org.nicolas.PublisherExample",
                            "HeartBeat",
                            signalCallback,
                            NULL);
    
    while(!stop_process) {
        r = sd_bus_process(bus, NULL);
        if (r < 0)
            std::cerr << "Failed to process bus: " << strerror(-r) << std::endl;
        if (r > 0)
            continue;

        /* Wait for the next request to process */
        r = sd_bus_wait(bus, (uint64_t) -1);
        if (r < 0)
            std::cerr << "Failed to wait on bus: " << strerror(-r) << std::endl;
    }

    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
