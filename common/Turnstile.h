#pragma once

#include <chrono>
#include <functional>
#include <sstream>
#include <string>

using namespace std::chrono_literals;
extern const std::array<std::string, 3> GATEWAYS;

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

// states
enum class eState { Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked };

const char * to_string(SwingDoor::eStatus e);
const char * to_string(LEDController::eStatus e);
const char * to_string(eState e);
int getFare();
void * createTimer(std::function<void()> task, std::chrono::milliseconds duration);
void cancelTimer(void * handle);

template <class T>
constexpr std::string_view type_name() {
    using namespace std;
#ifdef __clang__
    string_view p = __PRETTY_FUNCTION__;
    return string_view(p.data() + 34, p.size() - 34 - 1);
#elif defined(__GNUC__)
    string_view p = __PRETTY_FUNCTION__;
#if __cplusplus < 201402
    return string_view(p.data() + 36, p.size() - 36 - 1);
#else
    return string_view(p.data() + 49, p.find(';', 49) - 49);
#endif
#elif defined(_MSC_VER)
    string_view p = __FUNCSIG__;
    return string_view(p.data() + 84, p.size() - 84 - 7);
#endif
}

class TimeoutManager {
public:
    TimeoutManager(std::function<void()> fn, std::chrono::milliseconds duration)
        : _fn(std::move(fn))
        , _timeoutHandler(createTimer(
              [&] {
                  _timeoutHandler = nullptr;
                  _fn();
              },
              duration)) {
    }

    void restart(std::chrono::milliseconds duration) {
        if (_timeoutHandler) {
            cancelTimer(_timeoutHandler);
        }
        _timeoutHandler = createTimer(
            [&] {
                _timeoutHandler = nullptr;
                _fn();
            },
            duration);
    }

    ~TimeoutManager() {
        if (_timeoutHandler) {
            cancelTimer(_timeoutHandler);
        }
    }

private:
    std::function<void()> _fn;
    void * _timeoutHandler{nullptr};
};
