cmake_minimum_required(VERSION 3.23)

enable_testing()

add_executable(unitTests
    ConditionalStream.cpp
    ConditionalStream.h
    testFSMv1.cpp
    testFSMWithEnums.cpp
    testFSMWithStatePattern.cpp
    Turnstile.cpp
    Turnstile.h
)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG d83fee138a9ae6cb7c03688a2d08d4043a39815d # release-1.10.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

target_link_libraries(unitTests
    FSM
    gtest_main
)

target_include_directories(unitTests PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(unitTests)