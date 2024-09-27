#include <iostream>
#include <cstdlib>

#include <systemd/sd-bus.h>

int main() {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    const char *response;
    int r;

    /* Connect to the system bus */
    r = sd_bus_open_user(&bus);
    if (r < 0) {
        std::cerr << "Failed to connect to user bus: " << strerror(-r) << std::endl;
        goto finish;
    }

    /* Issue the method call and store the respons message in m */
    r = sd_bus_call_method(bus,
                            "org.nicolas.ServerExample",           /* service to contact */
                            "/org/nicolas/ServerExample",          /* object path */
                            "org.nicolas.ServerExample",   /* interface name */
                            "Greating",                          /* method name */
                            &error,                               /* object to return error in */
                            &m,                                   /* return message on success */
                            "s",                                 /* input signature */
                            "client");                           /* first argument */
    if (r < 0) {
        std::cerr << "Failed to issue method call: " << error.message << std::endl;
        goto finish;
    }

    /* Parse the response message */
    r = sd_bus_message_read(m, "s", &response);
    if (r < 0) {
        std::cerr << "Failed to parse response message: " << strerror(-r) << std::endl;
        goto finish;
    }
    
    std::cout << "response: " << response << std::endl;

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
