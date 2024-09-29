#include <iostream>
#include <atomic>
#include <string>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <ctime>

#include <errno.h>
#include <systemd/sd-bus.h>

std::atomic_bool stop_process = false;

void sighandler(int signum) {
    stop_process = true;
}

static int method_greating(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    int r;
    const char* name = nullptr;
    std::string response = "hello ";
    
    r = sd_bus_message_read(m, "s", &name);
    if(r >= 0 && name) {
        if(name) {
            response.append(name);
        }
    }
    else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    return sd_bus_reply_method_return(m, "s", response.c_str());
}

static const sd_bus_vtable example_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("Greating", "s", "s", method_greating, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_VTABLE_END
};

static int sendSignal(sd_bus *bus) {
    static uint64_t beatIndex = 0;
    sd_bus_message* msg;
    int r = sd_bus_message_new_signal(bus,
                                  &msg,
                                  "/org/nicolas/PublisherExample",    /* Signal emitter path */
                                  "org.nicolas.PublisherExample", /* Signal emitter interface */
                                  "HeartBeat");
    if(r < 0) {
        std::cerr << "Failed to create new signal" << std::endl;
        return r;
    }
    
    r = sd_bus_message_append(msg, "tx", beatIndex, int64_t(std::time(NULL)));
    if(r < 0) {
        std::cerr << "Failed to append data to signal message" << std::endl;
        return r;
    }
    
    r = sd_bus_send(bus, msg, NULL);
    if(r < 0) {
        std::cerr << "Failed to send signal message" << std::endl;
        return r;
    }
    
    beatIndex++;
    
    return r;
}

int main() {
    sd_bus_slot *slot = NULL;
    sd_bus *bus = NULL;
    int r;
    int ret = EXIT_SUCCESS;
    
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    
    r = sd_bus_open_user(&bus);
    if(r >= 0) {
        r = sd_bus_add_object_vtable(bus,
                                     &slot,
                                     "/org/nicolas/PublisherExample",  /* object path */
                                     "org.nicolas.PublisherExample",   /* interface name */
                                     example_vtable,
                                     NULL);
        if(r >= 0) {
            r = sd_bus_request_name(bus, "org.nicolas.PublisherExample", 0);
            if (r < 0) {
                std::cerr << "Failed to acquire service name: " << strerror(-r) << std::endl;
                stop_process = true;
                ret = EXIT_FAILURE;
            }
        }
        else {
            std::cerr << "Failed to add example object: " << strerror(-r) << std::endl;
            stop_process = true;
            ret = EXIT_FAILURE;
        }
    }
    
    while(!stop_process) {
        r = sd_bus_process(bus, NULL);
        sendSignal(bus);
        if (r < 0)
            std::cerr << "Failed to process bus: " << strerror(-r) << std::endl;
        if (r > 0)
            continue;

        /* Wait for the next request to process */
        r = sd_bus_wait(bus, 1000 * 1000);
        if (r < 0)
            std::cerr << "Failed to wait on bus: " << strerror(-r) << std::endl;
    }
    
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
    
    return ret;
}

