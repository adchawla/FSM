#include "ConditionalStream.h"
#include "Turnstile.h"

#include <gtest/gtest.h>

// states
enum class eState { Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked };

class FSMWithEnums {
public:
    FSMWithEnums & process(CardPresented event);
    FSMWithEnums & process(TransactionDeclined event);
    FSMWithEnums & process(TransactionSuccess event);
    FSMWithEnums & process(PersonPassed event);
    FSMWithEnums & process(Timeout event);

    eState getState() const {
        return _state;
    }

private:
    // External Actions
    void initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount);

    // helper functions
    void transitionToPaymentProcessing(const std::string & gateway, std::string cardNumber);
    void transitionToPaymentFailed(const std::string & reason);
    void transitionToLocked();
    void transitionToPaymentSuccessful(int fare, int balance);
    void transitionToUnlocked();

    eState _state{eState::Locked};

    // Connected Devices
    SwingDoor _door;
    POSTerminal _pos{"Touch Card"};
    LEDController _led;

    int _retryCounts{0};
    std::string _cardNumber{};

    // for testing
    std::array<std::string, 3> _gateways{"Gateway1", "Gateway2", "Gateway3"};
    std::tuple<std::string, std::string, int> _lastTransaction;

public:
    void dump();

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

const char * to_string(eState e) {
    switch (e) {
    case eState::Locked:
        return "Locked";
    case eState::PaymentProcessing:
        return "PaymentProcessing";
    case eState::PaymentFailed:
        return "PaymentFailed";
    case eState::PaymentSuccess:
        return "PaymentSuccess";
    case eState::Unlocked:
        return "Unlocked";
    }
    return "unknown";
}

FSMWithEnums & FSMWithEnums::process(CardPresented event) {
    LOGGER << "EVENT: CardPresent\n";
    switch (_state) {
    case eState::Locked:
        transitionToPaymentProcessing(_gateways[0], std::move(event.cardNumber));
        break;

    default:
        break;
    }
    return *this;
}

FSMWithEnums & FSMWithEnums::process(TransactionDeclined event) {
    LOGGER << "EVENT: TransactionDeclined\n";
    switch (_state) {
    case eState::PaymentProcessing:
        transitionToPaymentFailed(std::move(event.reason));
        break;
    default:
        break;
    }
    return *this;
}

FSMWithEnums & FSMWithEnums::process(TransactionSuccess event) {
    LOGGER << "EVENT: TransactionSuccess\n";
    switch (_state) {
    case eState::PaymentProcessing:
        transitionToPaymentSuccessful(event.fare, event.balance);
        break;
    default:
        break;
    }
    return *this;
}

FSMWithEnums & FSMWithEnums::process(PersonPassed event) {
    LOGGER << "EVENT: PersonPassed\n";
    switch (_state) {
    case eState::PaymentSuccess:
    case eState::Unlocked:
        transitionToLocked();
        break;
    default:
        break;
    }
    return *this;
}

FSMWithEnums & FSMWithEnums::process(Timeout event) {
    LOGGER << "EVENT: Timeout\n";
    switch (_state) {
    case eState::PaymentProcessing:
        _retryCounts++;
        if (_retryCounts > 2) {
            transitionToPaymentFailed("Network Error");
        } else {
            initiateTransaction(_gateways[_retryCounts], _cardNumber, getFare());
        }
        break;
    case eState::PaymentFailed:
        transitionToLocked();
        break;

    case eState::PaymentSuccess:
        transitionToUnlocked();
        break;
    default:
        break;
    }
    return *this;
}

void FSMWithEnums::dump() {
    LOGGER << "STATE: " << to_string(_state) << " :: Door[" << to_string(_door.getStatus()) << "], LED: ["
           << to_string(_led.getStatus()) << "] and PosTerminal[" << _pos.getRows() << "]\n";
}

void FSMWithEnums::initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
    LOGGER << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
           << amount << "]\n";
    _lastTransaction = std::make_tuple(gateway, cardNum, amount);
}

void FSMWithEnums::transitionToPaymentProcessing(const std::string & gateway, std::string cardNumber) {
    _cardNumber = std::move(cardNumber);
    initiateTransaction(gateway, _cardNumber, getFare());
    _door.close();
    _pos.setRows("Processing");
    _led.setStatus(LEDController::eStatus::OrangeCross);
    _state = eState::PaymentProcessing;
}

void FSMWithEnums::transitionToPaymentFailed(const std::string & reason) {
    _door.close();
    _pos.setRows("Declined", reason);
    _led.setStatus(LEDController::eStatus::FlashRedCross);
    _state = eState::PaymentFailed;
}

void FSMWithEnums::transitionToLocked() {
    _state = eState::Locked;
    _door.close();
    _pos.setRows("Touch Card");
    _led.setStatus(LEDController::eStatus::RedCross);
}

void FSMWithEnums::transitionToPaymentSuccessful(int fare, int balance) {
    _state = eState::PaymentSuccess;
    _door.open();
    _pos.setRows(
        "Approved", std::string("Fare: ") + std::to_string(fare), std::string("Balance: ") + std::to_string(balance));
    _led.setStatus(LEDController::eStatus::GreenArrow);
}

void FSMWithEnums::transitionToUnlocked() {
    _state = eState::Unlocked;
    _door.open();
    _pos.setRows("Approved");
    _led.setStatus(LEDController::eStatus::GreenArrow);
}

TEST(FSMWithEnums, TestInitialState) {
    FSMWithEnums fsm;
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestPaymentProcessing) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", getFare()));
}

TEST(FSMWithEnums, TestPaymentFailed) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentFailed);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::FlashRedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Declined", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Insufficient Funds", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", getFare()));
}

TEST(FSMWithEnums, TestTimeoutOnPaymentProcessing) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway2", "A", getFare()));
}

TEST(FSMWithEnums, TestLockedFromPaymentFailed) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestPaymentSuccessful) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{5, 25});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::PaymentSuccess, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Open, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::GreenArrow, fsm.getLEDController().getStatus());
    EXPECT_EQ("Approved", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Fare: 5", fsm.getPOS().getSecondRow());
    EXPECT_EQ("Balance: 25", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestUnlocked) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{5, 25}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Unlocked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Open, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::GreenArrow, fsm.getLEDController().getStatus());
    EXPECT_EQ("Approved", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestLockedFromUnlocked) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{}).process(Timeout{}).process(PersonPassed{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestLockedFromPaymentSuccessful) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{}).process(PersonPassed{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLEDController().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithEnums, TestBug) {
    FSMWithEnums fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{}).process(Timeout{}).process(Timeout{}).process(Timeout{});
    EXPECT_EQ(eState::Locked, fsm.getState());
    fsm.process(CardPresented{"A"}).process(Timeout{});
    EXPECT_EQ(eState::PaymentProcessing, fsm.getState());
}