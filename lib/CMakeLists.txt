cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

########################################################################################################################

#Create the target
etn_target(static ${PROJECT_NAME}-lib
    SOURCES
        src/*.cc
    PUBLIC_INCLUDE_DIR
        include
    PUBLIC
        *.h
    USES_PRIVATE
        avahi-client
        czmq
        mlm
        fty_common_logging
    PRIVATE
)

if (BUILD_TESTING)

    etn_test_target(${PROJECT_NAME}-lib
        SOURCES
            tests/main.cc
        PREPROCESSOR -DCATCH_CONFIG_FAST_COMPILE
        USES
            stdc++fs
    )

    #copy selftest-ro, build selftest-rw for test in/out
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest-ro DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/selftest-rw)

    #copy selftest-ro, build selftest-rw for test coverage
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest-ro DESTINATION ${CMAKE_BINARY_DIR})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/selftest-rw)

endif()
