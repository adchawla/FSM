cmake_minimum_required(VERSION 3.23)


add_library(common STATIC
    ConditionalStream.cpp
    ConditionalStream.h
    FSMExternalTransitions.h
    FSMStateTransitions.h
    FSMWithEnums.h
    FSMWithStatePattern.h
    OldFSMExternalTransitions.h
    OldFSMStateTransitions.h
    Turnstile.cpp
    Turnstile.h
)

target_link_libraries(common PUBLIC
    FSM
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
