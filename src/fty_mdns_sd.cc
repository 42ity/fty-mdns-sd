/*  =========================================================================
    fty_mdns_sd - to manage network announcement(mDNS) and discovery (DNS-SD)
    Copyright (C) 2016 - 2017 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty-mdns-sd - to manage network announcement(mDNS) and discovery (DNS-SD)
@discuss
@end
@author Gerald Guillaume <GeraldGuillaume@eaton.com>
*/

#include "fty_mdns_sd_classes.h"
#include "avahi_wrapper.h"

#define default_log_config "/etc/fty/ftylog.cfg"

void
usage(){
    puts ("fty-mdns-sd [options] ...");
    puts ("  -v|--verbose        verbose output");
    puts ("  -h|--help           this information");
    puts ("  -c|--config         path to config file");
    puts ("  -e|--endpoint       malamute endpoint [ipc://@/malamute]");
    puts ("  -d|--daemonscan     activate scan daemon");
    puts ("  -s|--scan           scan devices and quit");
    puts ("  -o|--stdout         display scan devices result on standard output");
    puts ("  -n|--nobusout       not send scan devices result on malamute bus");
}

char*
s_get (zconfig_t *config, const char* key, std::string &dfl) {
    assert (config);
    char *ret = zconfig_get (config, key, dfl.c_str());
    if (!ret || streq (ret, ""))
        return (char*)dfl.c_str();
    return ret;
}

char*
s_get (zconfig_t *config, const char* key, char*dfl) {
    assert (config);
    char *ret = zconfig_get (config, key, dfl);
    if (!ret || streq (ret, ""))
        return dfl;
    return ret;
}


int
main (int argc, char *argv [])
{
    char *config_file = NULL;
    zconfig_t *config = NULL;
    char *log_config = NULL;

    bool scan_only = false;
    bool daemon_scan = false;
    bool verbose = false;
    int argn;

    log_info ("fty_mdns_sd - started");

    char *actor_name = (char *)"fty-mdns-sd";
    char *endpoint = (char *)"ipc://@/malamute";
    char *fty_info_command = (char *)"INFO";

    bool scan_daemon_active = false;
    bool scan_std_out = false;
    bool scan_no_bus_out = false;
    char *scan_command = (char *)"START-SCAN";
    char *scan_topic = (char *)"SCAN-ANNOUNCE";
    char *scan_type = (char *)"_https._tcp";


    ManageFtyLog::setInstanceFtylog(actor_name);

    //parse command line
    for (argn = 1; argn < argc; argn++) {
        char *param = NULL;
        if (argn < argc - 1) param = argv [argn+1];

        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            usage();
            return 0;
        }
        else if (streq (argv [argn], "--verbose") || streq (argv [argn], "-v")) {
            verbose = true;
        }
        else if (streq (argv [argn], "--endpoint") || streq (argv [argn], "-e")) {
            if (param) endpoint = param;
            ++argn;
        }
        else if (streq (argv [argn], "--config") || streq (argv [argn], "-c")) {
            if (param) config_file = param;
            ++argn;
        }
        else if (streq (argv [argn], "--scan") || streq (argv [argn], "-s")) {
            scan_only = true;
        }
        else if (streq (argv [argn], "--daemonscan") || streq (argv [argn], "-d")) {
            scan_daemon_active = true;
        }
        else if (streq (argv [argn], "--stdout") || streq (argv [argn], "-o")) {
            scan_std_out = true;
        }
        else if (streq (argv [argn], "--nobusout") || streq (argv [argn], "-n")) {
            scan_no_bus_out = true;
        }
        else {
            printf ("Unknown option: %s\n", argv [argn]);
            return 1;
        }
    }

    //parse config file
    if (config_file) {
        log_debug ("fty_mdns_sd:LOAD: %s", config_file);
        config = zconfig_load(config_file);
        if (!config) {
            log_error ("Failed to load config file %s: %m", config_file);
            exit (EXIT_FAILURE);
        }
        // VERBOSE
        if (streq (zconfig_get(config, "server/verbose", "false"), "true")) {
            verbose = true;
        }
        endpoint = s_get(config, "malamute/endpoint", endpoint);
        actor_name = s_get(config, "malamute/address", actor_name);
        fty_info_command = s_get(config, "fty-info/command", fty_info_command);

        if (streq(zconfig_get(config, "scan/daemonactive", "false"), "true")) {
            scan_daemon_active = true;
        }
        if (streq(zconfig_get(config, "scan/stdout", "true"), "true")) {
            scan_std_out = true;
        }
        if (streq(zconfig_get(config, "scan/nobusout", "false"), "true")) {
            scan_no_bus_out = true;
        }
        scan_command = s_get(config, "scan/command", scan_command);
        scan_topic = s_get(config, "scan/topic", scan_topic);
        scan_type = s_get(config, "scan/type", scan_type);

        log_config = zconfig_get(config, "log/config", default_log_config);
    }
    else {
        log_config = (char *)default_log_config;
    }
    ManageFtyLog::getInstanceFtylog()->setConfigFile(std::string (log_config));
    if (verbose)
        ManageFtyLog::getInstanceFtylog()->setVeboseMode();

    zactor_t *server = zactor_new(fty_mdns_sd_server, (void*)actor_name);
    assert(server);
    zstr_sendx(server, "CONNECT", endpoint, NULL);
    if (!scan_only) {
        zstr_sendx(server, "CONSUMER", "ANNOUNCE", ".*", NULL);
        log_info("scan_daemon_active=%u", scan_daemon_active);
        if (scan_daemon_active) {
            zstr_sendx(server, "PRODUCER", scan_topic, NULL);
            zstr_sendx(server, "SCAN-PARAMETERS", scan_command, scan_type, scan_std_out ? "true" : "false", scan_no_bus_out ? "true" : "false", NULL);
        }
        ////do first announcement
        zclock_sleep(5000);
        zstr_sendx(server, "DO-DEFAULT-ANNOUNCE", fty_info_command, NULL);
    }
    else {
        if (!scan_no_bus_out) zstr_sendx(server, "PRODUCER", scan_topic, NULL);
        zstr_sendx(server, "SCAN-PARAMETERS", scan_command, scan_type, scan_std_out ? "true" : "false", scan_no_bus_out ? "true" : "false", NULL);
        zstr_sendx(server, "DO-SCAN", NULL);
    }

    while (!zsys_interrupted) {
        zmsg_t *msg = zactor_recv(server);
        char *command = zmsg_popstr(msg);
        if (streq(command, "STOP")) {
            break;
        }
        zmsg_destroy(&msg);
    }
    zactor_destroy(&server);
    zconfig_destroy(&config);
    log_info("fty_mdns_sd - exited");
    return EXIT_SUCCESS;
}
