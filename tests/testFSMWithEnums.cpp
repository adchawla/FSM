#include <array>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

// connected devices
struct SwingDoor {
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

struct POSTerminal {
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

struct LEDController {
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
enum class eStates { Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked };

const char * to_string(eStates e) {
    switch (e) {
    case eStates::Locked:
        return "Locked";
    case eStates::PaymentProcessing:
        return "PaymentProcessing";
    case eStates::PaymentFailed:
        return "PaymentFailed";
    case eStates::PaymentSuccess:
        return "PaymentSuccess";
    case eStates::Unlocked:
        return "Unlocked";
    }
    return "unknown";
}

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

class FSMWithEnums {
public:
    FSMWithEnums() = default;

    FSMWithEnums & process(CardPresented event);
    FSMWithEnums & process(TransactionDeclined event);
    FSMWithEnums & process(TransactionSuccess event);
    FSMWithEnums & process(PersonPassed event);
    FSMWithEnums & process(Timeout event);

    void dump();

    eStates getState() const {
        return _state;
    }

private:
    // External Actions
    void initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount);

    // helper functions
    void transitionToPaymentProcessing(const std::string & gateway, std::string cardNumber);
    void transitionToPaymentFailed(const std::string & reason);
    void transitionToLocked() {
        _state = eStates::Locked;
        _door.close();
        _pos.setRows("Touch Card");
        _led.setStatus(LEDController::eStatus::RedCross);
    }

    void transitionToPaymentSuccessful(int fare, int balance) {
        _state = eStates::PaymentSuccess;
        _door.open();
        _pos.setRows(
            "Approved", std::string("Fare: ") + std::to_string(fare),
            std::string("Balance: ") + std::to_string(balance));
        _led.setStatus(LEDController::eStatus::GreenArrow);
    }

    eStates _state{eStates::Locked};

    // Connected Devices
    SwingDoor _door;
    POSTerminal _pos{"Touch Card"};
    LEDController _led;

    int _retryCounts{0};
    std::string _cardNumber{};
    std::string _reasonForDecline{};
    int _fare{0};
    int _balance{0};
    std::array<std::string, 3> _gateways{"Gateway1", "Gateway2", "Gateway3"};

    // for testing
    std::tuple<std::string, std::string, int> _lastTransaction;

public:
    static int getFare();

    const auto & getLastTransaction() const {
        return _lastTransaction;
    }
    const SwingDoor & getDoor() const {
        return _door;
    }
    const POSTerminal & getPOS() const {
        return _pos;
    }

    const LEDController & getLEDController() const {
        return _led;
    }
};

FSMWithEnums & FSMWithEnums::process(CardPresented event) {
    std::cout << "EVENT: CardPresent\n";
    switch (_state) {
    case eStates::Locked:
        transitionToPaymentProcessing(_gateways[0], std::move(event.cardNumber));
        break;

    default:
        break;
    }
    return *this;
}

FSMWithEnums & FSMWithEnums::process(TransactionDeclined event) {
    std::cout << "EVENT: TransactionDeclined\n";
    switch (_state) {
    case eStates::PaymentProcessing:
        transitionToPaymentFailed(std::move(event.reason));
        break;
    default:
        break;
    }
    return *this;
}

FSMWithEnums & FSMWithEnums::process(TransactionSuccess event) {
    std::cout << "EVENT: TransactionSuccess\n";
    switch (_state) {
    case eStates::PaymentProcessing:
        transitionToPaymentSuccessful(event.fare, event.balance);
        break;
    default:
        break;
    }
    return *this;
}

FSMWithEnums & FSMWithEnums::process(PersonPassed event) {
    return *this;
}

FSMWithEnums & FSMWithEnums::process(Timeout event) {
    std::cout << "EVENT: Timeout\n";
    switch (_state) {
    case eStates::PaymentProcessing:
        _retryCounts++;
        if (_retryCounts == 3) {
            transitionToPaymentFailed("Network Error");
        } else {
            initiateTransaction(_gateways[_retryCounts], _cardNumber, getFare());
        }
        break;
    case eStates::PaymentFailed:
        transitionToLocked();
        break;
    default:
        break;
    }
    return *this;
}

void FSMWithEnums::dump() {
    std::cout << "STATE: " << to_string(_state) << " :: Door[" << to_string(_door.getStatus()) << "], LED: ["
              << to_string(_led.getStatus()) << "] and PosTerminal[" << _pos.getRows() << "]\n";
}

void FSMWithEnums::initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
    std::cout << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
              << amount << "]\n";
    _lastTransaction = std::make_tuple(gateway, cardNum, amount);
}

void FSMWithEnums::transitionToPaymentProcessing(const std::string & gateway, std::string cardNumber) {
    _cardNumber = std::move(cardNumber);
    initiateTransaction(gateway, _cardNumber, getFare());
    _door.close();
    _pos.setRows("Processing");
    _led.setStatus(LEDController::eStatus::OrangeCross);
    _state = eStates::PaymentProcessing;
}

void FSMWithEnums::transitionToPaymentFailed(const std::string & reason) {
    _door.close();
    _pos.setRows("Declined", reason);
    _led.setStatus(LEDController::eStatus::FlashRedCross);
    _state = eStates::PaymentFailed;
}

int FSMWithEnums::getFare() {
    const auto now = std::time(nullptr);
    const auto calTime = *std::localtime(&now);
    const auto currentHour = calTime.tm_hour;
    constexpr int rates[] = {3, 3, 3, 3, 3, 3, 7, 7, 7, 7, 5, 5, 5, 5, 5, 7, 7, 7, 5, 5, 5, 5, 3, 3};
    return rates[currentHour % 24];
}

TEST(FSMWithEnums, TestInitialState) {
    FSMWithEnums fsm;
    fsm.dump();

    // state transition
    EXPECT_EQ(eStates::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_TRUE(fsm.getPOS().getSecondRow().empty());
    EXPECT_TRUE(fsm.getPOS().getThirdRow().empty());
}

TEST(FSMWithEnums, TestPaymentProcessing) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eStates::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_TRUE(fsm.getPOS().getSecondRow().empty());
    EXPECT_TRUE(fsm.getPOS().getThirdRow().empty());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", FSMWithEnums::getFare()));
}

TEST(FSMWithEnums, TestPaymentFailed) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eStates::PaymentFailed);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::FlashRedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Declined", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Insufficient Funds", fsm.getPOS().getSecondRow());
    EXPECT_TRUE(fsm.getPOS().getThirdRow().empty());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", FSMWithEnums::getFare()));
}

TEST(FSMWithEnums, TestTimeoutOnPaymentProcessing) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eStates::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway2", "A", FSMWithEnums::getFare()));
}

TEST(FSMWithEnums, TestLockedFromPaymentFailed) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eStates::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_TRUE(fsm.getPOS().getSecondRow().empty());
    EXPECT_TRUE(fsm.getPOS().getThirdRow().empty());
}

TEST(FSMWithEnums, TestPaymentSuccessful) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{5, 25});
    fsm.dump();

    // state transition
    EXPECT_EQ(eStates::PaymentSuccess, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Open, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::GreenArrow, fsm.getLEDController().getStatus());
    EXPECT_EQ("Approved", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Fare: 5", fsm.getPOS().getSecondRow());
    EXPECT_EQ("Balance: 25", fsm.getPOS().getThirdRow());
}
