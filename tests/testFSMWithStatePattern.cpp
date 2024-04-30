#include "ConditionalStream.h"
#include "Turnstile.h"

#include <array>
#include <chrono>
#include <gtest/gtest.h>

using namespace std::chrono_literals;

class BaseState;

class FSMWithStatePattern {
public:
    FSMWithStatePattern();

    template <typename Event>
    FSMWithStatePattern & process(Event && event);

    eState getState() const;

    [[nodiscard]] SwingDoor & getDoor() {
        return _door;
    }

    [[nodiscard]] POSTerminal & getPOS() {
        return _pos;
    }

    [[nodiscard]] LEDController & getLED() {
        return _led;
    }

    // External Actions
    void initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount);

private:
    // Connected Devices
    SwingDoor _door;
    POSTerminal _pos{""};
    LEDController _led;

    std::unique_ptr<BaseState> _state;

    // for testing
    std::tuple<std::string, std::string, int> _lastTransaction;

public:
    void dump() const;
    const auto & getLastTransaction() const {
        return _lastTransaction;
    }
};

class BaseState {
public:
    explicit BaseState(std::reference_wrapper<FSMWithStatePattern> context) : _context(context) {
    }
    virtual ~BaseState() = default;

    BaseState(const BaseState & other) = delete;
    BaseState(BaseState && other) noexcept = delete;
    BaseState & operator=(const BaseState & other) = delete;
    BaseState & operator=(BaseState && other) noexcept = delete;

    virtual eState state() = 0;
    virtual std::unique_ptr<BaseState> process(CardPresented event) {
        return nullptr;
    }
    virtual std::unique_ptr<BaseState> process(TransactionDeclined event) {
        return nullptr;
    }
    virtual std::unique_ptr<BaseState> process(TransactionSuccess event) {
        return nullptr;
    }
    virtual std::unique_ptr<BaseState> process(PersonPassed event) {
        return nullptr;
    }
    virtual std::unique_ptr<BaseState> process(Timeout event) {
        return nullptr;
    }

protected:
    std::reference_wrapper<FSMWithStatePattern> _context;
};

class BaseStateWithTimeout : public BaseState { // NOLINT(cppcoreguidelines-special-member-functions)
public:
    BaseStateWithTimeout(std::reference_wrapper<FSMWithStatePattern> context, std::chrono::milliseconds duration)
        : BaseState(context)
        , _timeoutHandler(createTimer(
              [&] {
                  _timeoutHandler = nullptr;
                  process(Timeout{});
              },
              duration)) {
    }

    void restartTimer(std::chrono::milliseconds duration) {
        if (_timeoutHandler) {
            cancelTimer(_timeoutHandler);
        }
        _timeoutHandler = createTimer(
            [&] {
                _timeoutHandler = nullptr;
                process(Timeout{});
            },
            duration);
    }

    ~BaseStateWithTimeout() override {
        if (_timeoutHandler) {
            cancelTimer(_timeoutHandler);
        }
    }

private:
    void * _timeoutHandler{nullptr};
};

class LockedState final : public BaseState {
public:
    explicit LockedState(std::reference_wrapper<FSMWithStatePattern> context);

    eState state() override {
        return eState::Locked;
    }
    std::unique_ptr<BaseState> process(CardPresented event) override;
};

class PaymentProcessingState final : public BaseStateWithTimeout {
public:
    explicit PaymentProcessingState(std::reference_wrapper<FSMWithStatePattern> context, std::string cardNumber);

    eState state() override {
        return eState::PaymentProcessing;
    }
    std::unique_ptr<BaseState> process(TransactionDeclined event) override;
    std::unique_ptr<BaseState> process(TransactionSuccess event) override;
    std::unique_ptr<BaseState> process(Timeout event) override;

private:
    size_t _retryCount{0};
    std::string _cardNumber;
    static std::array<std::string, 3> _gateways;
};

class PaymentFailed final : public BaseStateWithTimeout {
public:
    PaymentFailed(std::reference_wrapper<FSMWithStatePattern> context, std::string reason);

    eState state() override {
        return eState::PaymentFailed;
    }

    std::unique_ptr<BaseState> process(Timeout event) override;

private:
    std::string _reason;
};

class PaymentSuccess final : public BaseStateWithTimeout {
public:
    PaymentSuccess(std::reference_wrapper<FSMWithStatePattern> context, int fare, int balance);

    eState state() override {
        return eState::PaymentSuccess;
    }
    std::unique_ptr<BaseState> process(PersonPassed event) override;
    std::unique_ptr<BaseState> process(Timeout event) override;
};

class Unlocked final : public BaseState {
public:
    explicit Unlocked(std::reference_wrapper<FSMWithStatePattern> context);

    eState state() override {
        return eState::Unlocked;
    }
    std::unique_ptr<BaseState> process(PersonPassed event) override;
};

template <typename Event>
FSMWithStatePattern & FSMWithStatePattern::process(Event && event) {
    LOGGER << "EVENT: " << type_name<std::decay_t<Event>> << "\n";
    if (auto newState = _state->process(std::forward<Event>(event))) {
        _state = std::move(newState);
    }
    return *this;
}

FSMWithStatePattern::FSMWithStatePattern() : _state(std::make_unique<LockedState>(std::ref(*this))) {
}

eState FSMWithStatePattern::getState() const {
    return _state->state();
}

void FSMWithStatePattern::initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
    LOGGER << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
           << amount << "]\n";
    _lastTransaction = std::make_tuple(gateway, cardNum, amount);
}

void FSMWithStatePattern::dump() const {
    LOGGER << "STATE: " << to_string(_state->state()) << " :: Door[" << to_string(_door.getStatus()) << "], LED: ["
           << to_string(_led.getStatus()) << "] and PosTerminal[" << _pos.getRows() << "]\n";
}

LockedState::LockedState(std::reference_wrapper<FSMWithStatePattern> context) : BaseState(context) {
    auto & fsm = _context.get();
    fsm.getDoor().close();
    fsm.getLED().setStatus(LEDController::eStatus::RedCross);
    fsm.getPOS().setRows("Touch Card");
}

std::unique_ptr<BaseState> LockedState::process(CardPresented event) {
    return std::make_unique<PaymentProcessingState>(_context, std::move(event.cardNumber));
}

PaymentProcessingState::PaymentProcessingState(
    std::reference_wrapper<FSMWithStatePattern> context, std::string cardNumber)
    : BaseStateWithTimeout(context, 2s), _cardNumber(std::move(cardNumber)) {
    auto & fsm = _context.get();
    fsm.getDoor().close();
    fsm.getLED().setStatus(LEDController::eStatus::OrangeCross);
    fsm.getPOS().setRows("Processing");
    fsm.initiateTransaction(_gateways[_retryCount], _cardNumber, getFare());
}

std::array<std::string, 3> PaymentProcessingState::_gateways{"Gateway1", "Gateway2", "Gateway3"};

std::unique_ptr<BaseState> PaymentProcessingState::process(TransactionDeclined event) {
    return std::make_unique<PaymentFailed>(_context, std::move(event.reason));
}

std::unique_ptr<BaseState> PaymentProcessingState::process(TransactionSuccess event) {
    return std::make_unique<PaymentSuccess>(_context, event.fare, event.balance);
}

std::unique_ptr<BaseState> PaymentProcessingState::process(Timeout event) {
    _retryCount++;
    if (_retryCount >= _gateways.size()) {
        return std::make_unique<PaymentFailed>(_context, "Network Failure");
    }
    _context.get().initiateTransaction(_gateways[_retryCount], _cardNumber, getFare());
    return BaseStateWithTimeout::process(event);
}

PaymentFailed::PaymentFailed(std::reference_wrapper<FSMWithStatePattern> context, std::string reason)
    : BaseStateWithTimeout(context, 2s), _reason(std::move(reason)) {
    auto & fsm = _context.get();
    fsm.getDoor().close();
    fsm.getLED().setStatus(LEDController::eStatus::FlashRedCross);
    fsm.getPOS().setRows("Declined", _reason);
}

std::unique_ptr<BaseState> PaymentFailed::process(Timeout event) {
    return std::make_unique<LockedState>(_context);
}

PaymentSuccess::PaymentSuccess(std::reference_wrapper<FSMWithStatePattern> context, int fare, int balance)
    : BaseStateWithTimeout(context, 2s) {
    auto & fsm = _context.get();
    fsm.getDoor().open();
    fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
    fsm.getPOS().setRows(
        "Approved", std::string("Fare: ") + std::to_string(fare), std::string("Balance: ") + std::to_string(balance));
}

std::unique_ptr<BaseState> PaymentSuccess::process(PersonPassed event) {
    return std::make_unique<LockedState>(_context);
}

std::unique_ptr<BaseState> PaymentSuccess::process(Timeout event) {
    return std::make_unique<Unlocked>(_context);
}

Unlocked::Unlocked(std::reference_wrapper<FSMWithStatePattern> context) : BaseState(context) {
    auto & fsm = _context.get();
    fsm.getDoor().open();
    fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
    fsm.getPOS().setRows("Approved");
}

std::unique_ptr<BaseState> Unlocked::process(PersonPassed event) {
    return std::make_unique<LockedState>(_context);
}

TEST(FSMWithStatePattern, TestInitialState) {
    FSMWithStatePattern fsm;
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithStatePattern, TestPaymentProcessing) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLED().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", getFare()));
}

TEST(FSMWithStatePattern, TestPaymentFailed) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentFailed);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::FlashRedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Declined", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Insufficient Funds", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway1", "A", getFare()));
}

TEST(FSMWithStatePattern, TestTimeoutOnPaymentProcessing) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(fsm.getState(), eState::PaymentProcessing);

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::OrangeCross, fsm.getLED().getStatus());
    EXPECT_EQ("Processing", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());

    // Actions
    EXPECT_EQ(fsm.getLastTransaction(), std::make_tuple("Gateway2", "A", getFare()));
}

TEST(FSMWithStatePattern, TestLockedFromPaymentFailed) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(TransactionDeclined{"Insufficient Funds"}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithStatePattern, TestPaymentSuccessful) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{5, 25});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::PaymentSuccess, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Open, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::GreenArrow, fsm.getLED().getStatus());
    EXPECT_EQ("Approved", fsm.getPOS().getFirstRow());
    EXPECT_EQ("Fare: 5", fsm.getPOS().getSecondRow());
    EXPECT_EQ("Balance: 25", fsm.getPOS().getThirdRow());
}

TEST(FSMWithStatePattern, TestUnlocked) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{5, 25}).process(Timeout{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Unlocked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Open, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::GreenArrow, fsm.getLED().getStatus());
    EXPECT_EQ("Approved", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithStatePattern, TestLockedFromUnlocked) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{}).process(Timeout{}).process(PersonPassed{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithStatePattern, TestLockedFromPaymentSuccessful) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(TransactionSuccess{}).process(PersonPassed{});
    fsm.dump();

    // state transition
    EXPECT_EQ(eState::Locked, fsm.getState());

    // Device States
    EXPECT_EQ(SwingDoor::eStatus::Closed, fsm.getDoor().getStatus());
    EXPECT_EQ(LEDController::eStatus::RedCross, fsm.getLED().getStatus());
    EXPECT_EQ("Touch Card", fsm.getPOS().getFirstRow());
    EXPECT_EQ("", fsm.getPOS().getSecondRow());
    EXPECT_EQ("", fsm.getPOS().getThirdRow());
}

TEST(FSMWithStatePattern, TestBug) {
    FSMWithStatePattern fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{}).process(Timeout{}).process(Timeout{}).process(Timeout{});
    EXPECT_EQ(eState::Locked, fsm.getState());
    fsm.process(CardPresented{"A"}).process(Timeout{});
    EXPECT_EQ(eState::PaymentProcessing, fsm.getState());
}