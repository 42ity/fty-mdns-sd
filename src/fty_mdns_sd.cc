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
void
usage(){
    puts ("fty-mdns-sd [options] ...");
    puts ("  -v|--verbose        verbose test output");
    puts ("  -h|--help           this information");
    puts ("  -c|--config         path to config file\n");
    puts ("  -e|--endpoint       malamute endpoint [ipc://@/malamute]");

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

    bool verbose = false;
    int argn;

    zsys_info ("fty_mdns_sd - started");

    // check env verbose
    char* bios_log_level = getenv ("BIOS_LOG_LEVEL");
    if (bios_log_level && streq (bios_log_level, "LOG_DEBUG")) {
        verbose = 1;
    }

    char* actor_name = (char*)"fty-mdns-sd";
    char* endpoint = (char*)"ipc://@/malamute";

    char*fty_info_command = (char*)"INFO";

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
        else {
            printf ("Unknown option: %s\n", argv [argn]);
            return 1;
        }
    }

    //parse config file
    if(config_file){
        zsys_debug ("fty_mdns_sd:LOAD: %s", config_file);
        config = zconfig_load (config_file);
        if (!config) {
            zsys_error ("Failed to load config file %s: %m", config_file);
            exit (EXIT_FAILURE);
        }
        // VERBOSE
        if (streq (zconfig_get (config, "server/verbose", "false"), "true")) {
            verbose = true;
        }
        endpoint = s_get (config, "malamute/endpoint", endpoint);
        actor_name = s_get (config, "malamute/address", actor_name);
        fty_info_command = s_get (config, "fty-info/command", fty_info_command);
    }

    zactor_t *server = zactor_new (fty_mdns_sd_server, (void*)actor_name);
    assert (server);
    zstr_sendx (server, "CONNECT", endpoint, NULL);
    zstr_sendx (server, "CONSUMER", "ANNOUNCE", ".*", NULL);

    ////do first announcement
    zstr_sendx (server, "DO-DEFAULT-ANNOUNCE", fty_info_command,NULL);

    while (!zsys_interrupted) {
        zmsg_t *msg = zactor_recv (server);
        zmsg_destroy (&msg);
    }
    zactor_destroy (&server);
    zconfig_destroy (&config);
    if (verbose)
        zsys_info ("fty_mdns_sd - exited");

    return EXIT_SUCCESS;
}
