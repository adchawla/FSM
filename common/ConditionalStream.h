#pragma once

#include <iostream>
#include <iterator>
#include <streambuf>

extern std::ostream & LOGGER;

template <typename FSM>
void logFSM(FSM & fsm) {
    LOGGER << "STATE: " << to_string(fsm.getState()) << " :: Door[" << to_string(fsm.getDoor().getStatus())
           << "], LED: [" << to_string(fsm.getLED().getStatus()) << "] and PosTerminal[" << fsm.getPOS().getRows()
           << "]\n";
}

inline void logTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
    LOGGER << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
           << amount << "]\n";
}
