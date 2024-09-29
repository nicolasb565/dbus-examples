#include <iostream>
#include <cstdlib>
#include <csignal>
#include <atomic>

#include <systemd/sd-bus.h>

/* Dbus bus can be monitored and the api(s) are also discoverable.
 * For this example, we want to be able to start and stop service using systemd dbus api.
 * I used the command "sudo busctl monitor" while from another shell I sent the command "sudo systemctl"
 * I observed the following from busctl:
 * 
 * Type=method_call  Endian=l  Flags=4  Version=1 Cookie=2  Timestamp="Sun 2024-09-29 19:29:37.968499 UTC"
  Sender=:1.103  Destination=org.freedesktop.systemd1  Path=/org/freedesktop/systemd1  Interface=org.freedesktop.systemd1.Manager  Member=ListUnitsByPatterns
  UniqueName=:1.103
  MESSAGE "asas" {
          ARRAY "s" {
          };
          ARRAY "s" {
          };
  };

‣ Type=method_return  Endian=l  Flags=1  Version=1 Cookie=2762  ReplyCookie=2  Timestamp="Sun 2024-09-29 19:29:37.970018 UTC"
  Sender=:1.7  Destination=:1.103
  UniqueName=:1.7
  MESSAGE "a(ssssssouso)" {
          ARRAY "(ssssssouso)" {
                  STRUCT "ssssssouso" {
                          STRING "rescue.target";
                          STRING "Rescue Mode";
                          STRING "loaded";
                          STRING "inactive";
                          STRING "dead";
                          STRING "";
                          OBJECT_PATH "/org/freedesktop/systemd1/unit/rescue_2etarget";
                          UINT32 0;
                          STRING "";
                          OBJECT_PATH "/";
                  };
                  <...>
          };
  };
  
  We see here that the process management is done by Destination=org.freedesktop.systemd1  Path=/org/freedesktop/systemd1  Interface=org.freedesktop.systemd1.Manager
  Let's see what we get by introspecting that object:
  
  sudo busctl introspect org.freedesktop.systemd1 /org/freedesktop/systemd1 org.freedesktop.systemd1.Manager
  
NAME                                       TYPE      SIGNATURE        RESULT/VALUE                             FLAGS
<...>
.StartTransientUnit                        method    ssa(sv)a(sa(sv)) o                                        -
.StartUnit                                 method    ss               o                                        -
.StartUnitReplace                          method    sss              o                                        -
.StartUnitWithFlags                        method    sst              o                                        -
.StopUnit                                  method    ss               o                                        -
<...>
 * 
 * The list of method and signals is very long, but we see here that StartUnit and StopUnit are probably what we are looking for
 * 
 * I know from looking at output from systemctl that cups (common unix printing service) is running, but I really do not care because I have no printer
 * It is unfortunate that there is not more documentation about the args for now, would either need to use systemctl while monitoring or look at documentation from systemd
 * I tried monitoring command "sudo systemctl stop cups" but I do not see any use of StopUnit and the whole thing is very chatty and convoluted
 * Documentation from systemd says the folowing:
 * 
 * StartUnit() enqueues a start job and possibly depending jobs.
 * It takes the unit to activate and a mode string as arguments.
 * The mode needs to be one of "replace", "fail", "isolate", "ignore-dependencies", or "ignore-requirements".
 * If "replace", the method will start the unit and its dependencies, possibly replacing already queued jobs that conflict with it.
 * If "fail", the method will start the unit and its dependencies, but will fail if this would change an already queued job.
 * If "isolate", the method will start the unit in question and terminate all units that aren't dependencies of it.
 * If "ignore-dependencies", it will start a unit but ignore all its dependencies.
 * If "ignore-requirements", it will start a unit but only ignore the requirement dependencies.
 * It is not recommended to make use of the latter two options.
 * On reply, if successful, this method returns the newly created job object which has been enqueued for asynchronous activation.
 * Callers that want to track the outcome of the actual start operation need to monitor the result of this job.
 * This can be achieved in a race-free manner by first subscribing to the JobRemoved() signal, then calling StartUnit() 
 * and using the returned job object to filter out unrelated JobRemoved() signals, until the desired one is received, which will then carry the result of the start operation.
 * StopUnit() is similar to StartUnit() but stops the specified unit rather than starting it. Note that the "isolate" mode is invalid for this method.
 * 
 * Let's try those StartUnit and StopUnit methods from the command line using busctl
 * After having tried "sudo systemctl stop cups", I need to start cups again:
 * sudo busctl call org.freedesktop.systemd1 /org/freedesktop/systemd1 org.freedesktop.systemd1.Manager StartUnit ss cups.service replace 
o "/org/freedesktop/systemd1/job/3506"
 * 
 * I observe cups is now running in systemctl, let's try the StopUnit method now
 * 
 * sudo busctl call org.freedesktop.systemd1 /org/freedesktop/systemd1 org.freedesktop.systemd1.Manager StopUnit ss cups.service replace 
o "/org/freedesktop/systemd1/job/3607"
 * 
 * I no longer see cups in systemctl, so this is working
 * 
 * Knowing this, I wrote the following C program to start cups, then sleep until we get sigint or sigterm and stop cups before exiting.
 * Note this program need to be run as root to have required permission to start/stop services.
 * If this is not the case, I would probably have to use policykit to ask for additional permissions,
 * and enduser would typicaly get prompted to enter login credentials
 * 
 * I used the command "systemctl is-active cups" to see if this program start and stop cups service
 * */

std::atomic_bool stop_process = false;

void sighandler(int signum) {
    stop_process = true;
}


int main() {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    const char *response;
    int r;
    
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    /* Connect to the system bus */
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        std::cerr << "Failed to connect to user bus: " << strerror(-r) << std::endl;
        goto finish;
    }

    /* Call method to start cups service */
    r = sd_bus_call_method(bus,
                            "org.freedesktop.systemd1",           /* service to contact */
                            "/org/freedesktop/systemd1",          /* object path */
                            "org.freedesktop.systemd1.Manager",   /* interface name */
                            "StartUnit",                          /* method name */
                            &error,                               /* object to return error in */
                            &m,                                   /* return message on success */
                            "ss",                                 /* input signature */
                            "cups.service",                       /* first argument */
                            "replace");                           /* second argument */
    if (r < 0) {
        std::cerr << "Failed to issue method call to start cups service: " << error.message << std::endl;
        goto finish;
    }

    /* Parse the response message */
    r = sd_bus_message_read(m, "o", &response);
    if (r < 0) {
        std::cerr << "Failed to parse response message for starting cups service: " << strerror(-r) << std::endl;
        goto finish;
    }
    
    std::cout << "start cups response: " << response << std::endl;
    
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    error = SD_BUS_ERROR_NULL;
    m = NULL;
    
    while(!stop_process) {
        usleep(1);
    }
    
    r = sd_bus_call_method(bus,
                        "org.freedesktop.systemd1",           /* service to contact */
                        "/org/freedesktop/systemd1",          /* object path */
                        "org.freedesktop.systemd1.Manager",   /* interface name */
                        "StopUnit",                           /* method name */
                        &error,                               /* object to return error in */
                        &m,                                   /* return message on success */
                        "ss",                                 /* input signature */
                        "cups.service",                       /* first argument */
                        "replace");                           /* second argument */
    if (r < 0) {
        std::cerr << "Failed to issue method call to stop cups service: " << error.message << std::endl;
        goto finish;
    }

    /* Parse the response message */
    r = sd_bus_message_read(m, "o", &response);
    if (r < 0) {
        std::cerr << "Failed to parse response message to stop cups service: " << strerror(-r) << std::endl;
        goto finish;
    }
    
    std::cout << "stop cups response: " << response << std::endl;

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

