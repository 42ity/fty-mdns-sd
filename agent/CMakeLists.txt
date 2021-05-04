cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

########################################################################################################################

#Create the target
etn_target(exe ${PROJECT_NAME}
    SOURCES
        src/*.cc
    USES
        avahi-client
        czmq
        mlm
        fty_proto
        fty_common_logging
    USES_PRIVATE
        ${PROJECT_NAME}-lib
)
