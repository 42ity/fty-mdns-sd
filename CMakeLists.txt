cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

##############################################################################################################

project(fty-mdns-sd
  DESCRIPTION "Manages network announcement (mDNS) and discovery (DNS-SD)"
  VERSION 1.0.0
)

########################################################################################################################

find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR})

########################################################################################################################

add_subdirectory(lib)
add_subdirectory(agent)

## agent configuration
## https://cmake.org/cmake/help/v3.0/module/GNUInstallDirs.html

set(AGENT_USER "bios")
set(AGENT_CONF_DIR ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME})
set(AGENT_CONF_FILE ${AGENT_CONF_DIR}/${PROJECT_NAME}.cfg)

set(res "resources")

## ipc-meta-setup-avahi.conf -> lib/systemd/system/ipc-meta-setup.service.d/
install(FILES ${PROJECT_SOURCE_DIR}/${res}/ipc-meta-setup-avahi.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/ipc-meta-setup.service.d/)

## 10-avahi-lxc.sh -> usr/share/bios/setup
install(FILES ${PROJECT_SOURCE_DIR}/${res}/10-avahi-lxc.sh DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/bios/setup/)

## cfg -> etc/
set(file ${PROJECT_NAME}.cfg)
configure_file(${PROJECT_SOURCE_DIR}/${res}/${file}.in ${PROJECT_BINARY_DIR}/${res}/${file} @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/${res}/${file} DESTINATION ${AGENT_CONF_DIR})

## service -> lib/systemd/system
set(file ${PROJECT_NAME}.service)
configure_file(${PROJECT_SOURCE_DIR}/${res}/${file}.in ${PROJECT_BINARY_DIR}/${res}/${file} @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/${res}/${file} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/)
