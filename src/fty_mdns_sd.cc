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
    
    map_string_t map_txt;
    
    //get info from env
    char* srv_name  = getenv("FTY_MDNS_SD_SRV_NAME");
    char* srv_type  = getenv("FTY_MDNS_SD_SRV_TYPE");
    char* srv_stype = getenv("FTY_MDNS_SD_SRV_STYPE");
    char* srv_port  = getenv("FTY_MDNS_SD_SRV_PORT");
    
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
        
        //get default announce if available
        zconfig_t *config_service =zconfig_locate(config,"default/service");
        //default service
        srv_name  = s_get (config_service, "name",  srv_name);
        srv_type  = s_get (config_service, "type",  srv_type);
        srv_stype = s_get (config_service, "stype", srv_stype);
        srv_port  = s_get (config_service, "port",  srv_port);
        
        //TXT default records
        zconfig_t *config_txt =zconfig_locate(config,"default/properties");
        if (config_txt)
            config_txt = zconfig_child (config_txt);
        zconfig_child (config_txt);
        while (config_txt) {
            map_txt[zconfig_name (config_txt)] = zconfig_value(config_txt);
            config_txt = zconfig_next (config_txt);
        }
        
        fty_info_command = s_get (config, "fty-info/command", fty_info_command);
    }
    //check that all are set to do a manual announcement
    if(srv_name==NULL || srv_type==NULL || srv_stype==NULL){
        zsys_error("default service parameters are missing (name,type,stype)");
        zsys_error("use a config file (section default)");
        zsys_error("or set env parameters :");
        zsys_error(" FTY_MDNS_SD_SRV_NAME");
        zsys_error(" FTY_MDNS_SD_SRV_TYPE");
        zsys_error(" FTY_MDNS_SD_SRV_STYPE");
        return EXIT_FAILURE;
    }
    
    
    zactor_t *server = zactor_new (fty_mdns_sd_server, (void*)actor_name);
    assert (server);
    zstr_sendx (server, "CONNECT", endpoint, NULL);
    
    ////set attributes for default service
    zstr_sendx (server, "SET-DEFAULT-SERVICE",
            srv_name,srv_type,srv_stype,srv_port, NULL);
    ////set TXT for default service
    for (map_string_t::iterator it = map_txt.begin(); it != map_txt.end(); ++it) {
        zstr_sendx (server, "SET-DEFAULT-TXT",
            it->first.c_str(),it->second.c_str(), NULL);
    }
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
