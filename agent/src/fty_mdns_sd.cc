/*  =========================================================================
    fty_mdns_sd - to manage network announcement(mDNS) and discovery (DNS-SD)
    Copyright (C) 2016 - 2020 Eaton

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

#include "fty_mdns_sd.h"
#include "../src/avahi_wrapper.h"

void
usage(){
    puts ("fty-mdns-sd [options] ...");
    puts ("  -v|--verbose        verbose output");
    puts ("  -c|--config         path to config file");
    puts ("  -e|--endpoint       malamute endpoint [ipc://@/malamute]");
    puts ("  -h|--help           this information");
}

static char*
s_get (zconfig_t *config, const char* key, std::string &dfl) {
    assert (config);
    char *ret = zconfig_get (config, key, dfl.c_str());
    if (!ret || streq (ret, ""))
        return (char*)dfl.c_str();
    return ret;
}

static char*
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
    #define default_log_config FTY_COMMON_LOGGING_DEFAULT_CFG

    char *config_file = NULL;
    zconfig_t *config = NULL;
    char *log_config = NULL;

    bool verbose = false;
    char* actor_name = (char*)"fty-mdns-sd";
    char* endpoint = (char*)"ipc://@/malamute";
    char* fty_info_command = (char*)"INFO";

    ManageFtyLog::setInstanceFtylog(actor_name);

    //parse command line
    int argn;
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
        else {
            printf ("Unknown option: %s\n", argv [argn]);
            return EXIT_FAILURE;
        }
    }

    //parse config file
    if (config_file) {
        log_debug ("fty_mdns_sd:LOAD: %s", config_file);
        config = zconfig_load (config_file);
        if (!config) {
            log_error ("Failed to load config file %s: %m", config_file);
            exit (EXIT_FAILURE);
        }
        // VERBOSE
        if (streq (zconfig_get (config, "server/verbose", "false"), "true")) {
            verbose = true;
        }

        endpoint = s_get (config, "malamute/endpoint", endpoint);
        actor_name = s_get (config, "malamute/address", actor_name);

        fty_info_command = s_get (config, "fty-info/command", fty_info_command);

        log_config = zconfig_get (config, "log/config", default_log_config);
    }
    else {
        log_config = (char *)default_log_config;
    }

    ManageFtyLog::getInstanceFtylog()->setConfigFile(std::string (log_config));
    if (verbose) {
        ManageFtyLog::getInstanceFtylog()->setVerboseMode();
    }

    log_info ("fty_mdns_sd - starting...");

    zactor_t *server = zactor_new (fty_mdns_sd_server, (void*)actor_name);
    if (!server) {
        log_fatal("Failed to create server");
        return EXIT_FAILURE;
    }
    zstr_sendx (server, "CONNECT", endpoint, NULL);
    zstr_sendx (server, "CONSUMER", "ANNOUNCE", ".*", NULL);

    ////do first announcement
    zclock_sleep (5000);
    zstr_sendx (server, "DO-DEFAULT-ANNOUNCE", fty_info_command, NULL);

    log_info ("fty_mdns_sd - started");

    // main loop, accept any message back from server
    // copy from src/malamute.c under MPL license
    while (!zsys_interrupted)
    {
        char *msg = zstr_recv (server);
        if (!msg) break;

        log_debug ("Recv msg '%s'", msg);
        zstr_free (&msg);
    }

    log_info ("fty_mdns_sd - ended");

    zactor_destroy (&server);
    zconfig_destroy (&config);

    return EXIT_SUCCESS;
}
