#include "ConditionalStream.h"
#include "FSM.h"
#include "Turnstile.h"

#include <array>
#include <chrono>
#include <gtest/gtest.h>

using namespace std::chrono_literals;

namespace {
    using Events = std::variant<CardPresented, TransactionDeclined, TransactionSuccess, PersonPassed, Timeout>;
    class LockedState;
    class PaymentProcessingState;
    class PaymentFailed;
    class PaymentSuccess;
    class Unlocked;
    using State = std::variant<LockedState, PaymentProcessingState, PaymentFailed, PaymentSuccess, Unlocked>;
    using OptState = std::optional<State>;

    class FSMv3;

    class BaseState {
    public:
        explicit BaseState(std::reference_wrapper<FSMv3> context) : _context(context) {
        }
        template <typename EventType>
        OptState process(EventType);

    protected:
        std::reference_wrapper<FSMv3> _context;
    };

    class LockedState : public BaseState {
    public:
        explicit LockedState(std::reference_wrapper<FSMv3> context);
        using BaseState::process;
        OptState process(CardPresented event);
    };

    class PaymentProcessingState : public BaseState {
    public:
        explicit PaymentProcessingState(std::reference_wrapper<FSMv3> context, std::string cardNumber);

        using BaseState::process;
        OptState process(TransactionDeclined event);
        OptState process(TransactionSuccess event);
        OptState process(Timeout event);

    private:
        size_t _retryCount{0};
        std::string _cardNumber;
        TimeoutManager _timeoutManager;
        static std::array<std::string, 3> _gateways;
    };

    class PaymentFailed : public BaseState {
    public:
        PaymentFailed(std::reference_wrapper<FSMv3> context, std::string reason);

        using BaseState::process;
        OptState process(Timeout event);

    private:
        std::string _reason;
        TimeoutManager _timeoutManager;
    };

    class PaymentSuccess : public BaseState {
    public:
        PaymentSuccess(std::reference_wrapper<FSMv3> context, int fare, int balance);

        using BaseState::process;
        OptState process(PersonPassed event);
        OptState process(Timeout event);

    private:
        TimeoutManager _timeoutManager;
    };

    class Unlocked : public BaseState {
    public:
        explicit Unlocked(std::reference_wrapper<FSMv3> context);

        using BaseState::process;
        OptState process(PersonPassed event);
    };

    class FSMv3 {
    public:
        FSMv3() : _fsm{LockedState{std::ref(*this)}} {
        }

        template <typename Event>
        FSMv3 & process(Event event) {
            _fsm.process(std::move(event));
            return *this;
        }

        eState getState() const {
            switch (_fsm.getStateIndex()) {
            case 0:
                return eState::Locked;
            case 1:
                return eState::PaymentProcessing;
            case 2:
                return eState::PaymentFailed;
            case 3:
                return eState::PaymentSuccess;
            case 4:
                return eState::Unlocked;
            default:
                throw std::runtime_error("Unknown state");
            }
        }

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
        adc::FSMStateTransitions<LockedState, PaymentProcessingState, PaymentFailed, PaymentSuccess, Unlocked> _fsm;

        // for testing
        std::tuple<std::string, std::string, int> _lastTransaction;

    public:
        void dump() const;
        const auto & getLastTransaction() const {
            return _lastTransaction;
        }
    };

    void FSMv3::initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
        LOGGER << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
               << amount << "]\n";
        _lastTransaction = std::make_tuple(gateway, cardNum, amount);
    }

    void FSMv3::dump() const {
        LOGGER << "STATE: " << to_string(getState()) << " :: Door[" << to_string(_door.getStatus()) << "], LED: ["
               << to_string(_led.getStatus()) << "] and PosTerminal[" << _pos.getRows() << "]\n";
    }

    LockedState::LockedState(std::reference_wrapper<FSMv3> context) : BaseState(context) {
        auto & fsm = _context.get();
        fsm.getDoor().close();
        fsm.getLED().setStatus(LEDController::eStatus::RedCross);
        fsm.getPOS().setRows("Touch Card");
    }

    OptState LockedState::process(CardPresented event) {
        return PaymentProcessingState(_context, std::move(event.cardNumber));
    }

    PaymentProcessingState::PaymentProcessingState(std::reference_wrapper<FSMv3> context, std::string cardNumber)
        : BaseState(context)
        , _cardNumber(std::move(cardNumber))
        , _timeoutManager(
              [&] {
                  process(Timeout{});
              },
              2s) {
        auto & fsm = _context.get();
        fsm.getDoor().close();
        fsm.getLED().setStatus(LEDController::eStatus::OrangeCross);
        fsm.getPOS().setRows("Processing");
        fsm.initiateTransaction(_gateways[_retryCount], _cardNumber, getFare());
    }

    std::array<std::string, 3> PaymentProcessingState::_gateways{"Gateway1", "Gateway2", "Gateway3"};

    OptState PaymentProcessingState::process(TransactionDeclined event) {
        return PaymentFailed(_context, std::move(event.reason));
    }

    OptState PaymentProcessingState::process(TransactionSuccess event) {
        return PaymentSuccess(_context, event.fare, event.balance);
    }

    OptState PaymentProcessingState::process(Timeout event) {
        _retryCount++;
        if (_retryCount >= _gateways.size()) {
            return PaymentFailed(_context, "Network Failure");
        }
        _context.get().initiateTransaction(_gateways[_retryCount], _cardNumber, getFare());
        _timeoutManager.restart(2s);
        return OptState{};
    }

    PaymentFailed::PaymentFailed(std::reference_wrapper<FSMv3> context, std::string reason)
        : BaseState(context)
        , _reason(std::move(reason))
        , _timeoutManager(
              [&] {
                  process(Timeout{});
              },
              2s) {
        auto & fsm = _context.get();
        fsm.getDoor().close();
        fsm.getLED().setStatus(LEDController::eStatus::FlashRedCross);
        fsm.getPOS().setRows("Declined", _reason);
    }

    OptState PaymentFailed::process(Timeout event) {
        return LockedState(_context);
    }

    PaymentSuccess::PaymentSuccess(std::reference_wrapper<FSMv3> context, int fare, int balance)
        : BaseState(context)
        , _timeoutManager(
              [&] {
                  process(Timeout{});
              },
              2s) {
        auto & fsm = _context.get();
        fsm.getDoor().open();
        fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
        fsm.getPOS().setRows(
            "Approved", std::string("Fare: ") + std::to_string(fare),
            std::string("Balance: ") + std::to_string(balance));
    }

    OptState PaymentSuccess::process(PersonPassed event) {
        return LockedState(_context);
    }

    OptState PaymentSuccess::process(Timeout event) {
        return Unlocked(_context);
    }

    Unlocked::Unlocked(std::reference_wrapper<FSMv3> context) : BaseState(context) {
        auto & fsm = _context.get();
        fsm.getDoor().open();
        fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
        fsm.getPOS().setRows("Approved");
    }

    OptState Unlocked::process(PersonPassed event) {
        return LockedState(_context);
    }

    template <typename EventType>
    OptState BaseState::process(EventType) {
        return OptState{};
    }
} // namespace

TEST(FSMStateTransitions, TestInitialState) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestPaymentProcessing) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestPaymentFailed) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestTimeoutOnPaymentProcessing) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestLockedFromPaymentFailed) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestPaymentSuccessful) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestUnlocked) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestLockedFromUnlocked) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestLockedFromPaymentSuccessful) {
    FSMv3 fsm;
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

TEST(FSMStateTransitions, TestBug) {
    FSMv3 fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{}).process(Timeout{}).process(Timeout{}).process(Timeout{});
    EXPECT_EQ(eState::Locked, fsm.getState());
    fsm.process(CardPresented{"A"}).process(Timeout{});
    EXPECT_EQ(eState::PaymentProcessing, fsm.getState());
}
