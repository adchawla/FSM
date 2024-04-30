#pragma once

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

// connected devices
class SwingDoor {
public:
    enum class eStatus { Closed, Open };

    void open() {
        _status = eStatus::Open;
    }

    void close() {
        _status = eStatus::Closed;
    }

    eStatus getStatus() const {
        return _status;
    }

private:
    eStatus _status{eStatus::Closed};
};

class POSTerminal {
public:
    explicit POSTerminal(std::string firstRow, std::string secondRow = "", std::string thirdRow = "")
        : _firstRow(std::move(firstRow)), _secondRow(std::move(secondRow)), _thirdRow(std::move(thirdRow)) {
    }

    void setRows(std::string firstRow, std::string secondRow = "", std::string thirdRow = "") {
        _firstRow = std::move(firstRow);
        _secondRow = std::move(secondRow);
        _thirdRow = std::move(thirdRow);
    }

    std::string getRows() const {
        std::stringstream stm;
        stm << _firstRow << ", " << _secondRow << ", " << _thirdRow;
        return stm.str();
    }

    const std::string & getFirstRow() const {
        return _firstRow;
    }

    const std::string & getSecondRow() const {
        return _secondRow;
    }

    const std::string & getThirdRow() const {
        return _thirdRow;
    }

private:
    std::string _firstRow;
    std::string _secondRow;
    std::string _thirdRow;
};

class LEDController {
public:
    enum class eStatus { RedCross, FlashRedCross, GreenArrow, OrangeCross };

    void setStatus(eStatus status) {
        _status = status;
    }

    eStatus getStatus() const {
        return _status;
    }

private:
    eStatus _status{eStatus::RedCross};
};

// events
struct CardPresented {
    std::string cardNumber;
};

struct TransactionDeclined {
    std::string reason;
};

struct TransactionSuccess {
    int fare;
    int balance;
};

struct PersonPassed {};
struct Timeout {};

const char * to_string(SwingDoor::eStatus e) {
    switch (e) {
    case SwingDoor::eStatus::Closed:
        return "Closed";
    case SwingDoor::eStatus::Open:
        return "Open";
    }
    return "unknown";
}

const char * to_string(LEDController::eStatus e) {
    switch (e) {
    case LEDController::eStatus::RedCross:
        return "RedCross";
    case LEDController::eStatus::FlashRedCross:
        return "FlashRedCross";
    case LEDController::eStatus::GreenArrow:
        return "GreenArrow";
    case LEDController::eStatus::OrangeCross:
        return "OrangeCross";
    }
    return "unknown";
}

int getFare() {
    const auto now = std::time(nullptr);
    const auto calTime = *std::localtime(&now);
    const auto currentHour = calTime.tm_hour;
    constexpr int rates[] = {3, 3, 3, 3, 3, 3, 7, 7, 7, 7, 5, 5, 5, 5, 5, 7, 7, 7, 5, 5, 5, 5, 3, 3};
    return rates[currentHour % 24];
}
