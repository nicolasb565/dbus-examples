#include <iostream>
#include <atomic>
#include <string>
#include <csignal>
#include <cstdlib>

#include <errno.h>
#include <systemd/sd-bus.h>

/*
 * I changed the vtable to make sure we can introspect arguments name
 * busctl --user introspect org.nicolas.ServerExample /org/nicolas/ServerExample org.nicolas.ServerExample --xml-interface
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"https://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
 <interface name="org.freedesktop.DBus.Peer">
  <method name="Ping"/>
  <method name="GetMachineId">
   <arg type="s" name="machine_uuid" direction="out"/>
  </method>
 </interface>
 <interface name="org.freedesktop.DBus.Introspectable">
  <method name="Introspect">
   <arg name="xml_data" type="s" direction="out"/>
  </method>
 </interface>
 <interface name="org.freedesktop.DBus.Properties">
  <method name="Get">
   <arg name="interface_name" direction="in" type="s"/>
   <arg name="property_name" direction="in" type="s"/>
   <arg name="value" direction="out" type="v"/>
  </method>
  <method name="GetAll">
   <arg name="interface_name" direction="in" type="s"/>
   <arg name="props" direction="out" type="a{sv}"/>
  </method>
  <method name="Set">
   <arg name="interface_name" direction="in" type="s"/>
   <arg name="property_name" direction="in" type="s"/>
   <arg name="value" direction="in" type="v"/>
  </method>
  <signal name="PropertiesChanged">
   <arg type="s" name="interface_name"/>
   <arg type="a{sv}" name="changed_properties"/>
   <arg type="as" name="invalidated_properties"/>
  </signal>
 </interface>
 <interface name="org.nicolas.ServerExample">
  <method name="Greating">
   <arg type="s" name="caller_name" direction="in"/>
   <arg type="s" name="response" direction="out"/>
  </method>
 </interface>
</node>
 *
 * We can test using following command:
 * busctl --user call org.nicolas.ServerExample /org/nicolas/ServerExample org.nicolas.ServerExample Greating s patate
s "hello patate"
 * 
*/

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
        std::cerr << "Failed to parse parameters: " << strerror(-r) << std::endl;
        return r;
    }
    
    return sd_bus_reply_method_return(m, "s", response.c_str());
}

static const sd_bus_vtable example_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD_WITH_ARGS("Greating",
            SD_BUS_ARGS("s", caller_name),
            SD_BUS_RESULT("s", response),
            method_greating,
            SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_VTABLE_END
};

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
                                     "/org/nicolas/ServerExample",  /* object path */
                                     "org.nicolas.ServerExample",   /* interface name */
                                     example_vtable,
                                     NULL);
        if(r >= 0) {
            r = sd_bus_request_name(bus, "org.nicolas.ServerExample", 0);
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
    else {
        std::cerr << "Failed to connect to user bus: " << strerror(-r) << std::endl;
        stop_process = true;
        ret = EXIT_FAILURE;
    }
    
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
    
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
    
    return ret;
}
