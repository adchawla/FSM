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

    struct TransitionTable;
    class FSMv4;

    class BaseState {
    public:
        explicit BaseState(std::reference_wrapper<FSMv4> context) : _context(context) {
        }

    protected:
        std::reference_wrapper<FSMv4> _context;
        friend struct TransitionTable;
    };

    class LockedState : public BaseState {
    public:
        explicit LockedState(std::reference_wrapper<FSMv4> context);
        friend struct TransitionTable;
    };

    class PaymentProcessingState : public BaseState {
    public:
        explicit PaymentProcessingState(std::reference_wrapper<FSMv4> context, std::string cardNumber);

        bool tryRetry();

    private:
        size_t _retryCount{0};
        std::string _cardNumber;
        TimeoutManager _timeoutManager;
        static std::array<std::string, 3> _gateways;
        friend struct TransitionTable;
    };

    class PaymentFailed : public BaseState {
    public:
        PaymentFailed(std::reference_wrapper<FSMv4> context, std::string reason);

    private:
        std::string _reason;
        TimeoutManager _timeoutManager;
        friend struct TransitionTable;
    };

    class PaymentSuccess : public BaseState {
    public:
        PaymentSuccess(std::reference_wrapper<FSMv4> context, int fare, int balance);

    private:
        TimeoutManager _timeoutManager;
        friend struct TransitionTable;
    };

    class Unlocked : public BaseState {
    public:
        explicit Unlocked(std::reference_wrapper<FSMv4> context);
        friend struct TransitionTable;
    };

    struct TransitionTable {
        OptState operator()(LockedState & state, CardPresented event) {
            return PaymentProcessingState(state._context, std::move(event.cardNumber));
        }
        OptState operator()(PaymentProcessingState & state, TransactionDeclined event) {
            return PaymentFailed(state._context, std::move(event.reason));
        }
        OptState operator()(PaymentProcessingState & state, TransactionSuccess event) {
            return PaymentSuccess(state._context, event.fare, event.balance);
        }
        OptState operator()(PaymentProcessingState & state, Timeout event) {
            return state.tryRetry() ? OptState{} : PaymentFailed(state._context, "Network Failure");
        }
        OptState operator()(PaymentFailed & state, Timeout) {
            return LockedState(state._context);
        }
        OptState operator()(PaymentSuccess & state, Timeout) {
            return Unlocked(state._context);
        }
        OptState operator()(PaymentSuccess & state, PersonPassed) {
            return LockedState(state._context);
        }
        OptState operator()(Unlocked & state, PersonPassed) {
            return LockedState(state._context);
        }
        template <typename State, typename Event>
        auto operator()(State & s, Event e) const {
            return OptState{};
        }
    };

    class FSMv4 {
    public:
        FSMv4() : _fsm{TransitionTable{}, LockedState{std::ref(*this)}} {
        }

        template <typename Event>
        FSMv4 & process(Event event) {
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
        adc::FSMExternalTransitions<
            TransitionTable, LockedState, PaymentProcessingState, PaymentFailed, PaymentSuccess, Unlocked>
            _fsm;

        // for testing
        std::tuple<std::string, std::string, int> _lastTransaction;

    public:
        void dump() const;
        const auto & getLastTransaction() const {
            return _lastTransaction;
        }
    };

    void FSMv4::initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
        LOGGER << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
               << amount << "]\n";
        _lastTransaction = std::make_tuple(gateway, cardNum, amount);
    }

    void FSMv4::dump() const {
        LOGGER << "STATE: " << to_string(getState()) << " :: Door[" << to_string(_door.getStatus()) << "], LED: ["
               << to_string(_led.getStatus()) << "] and PosTerminal[" << _pos.getRows() << "]\n";
    }

    LockedState::LockedState(std::reference_wrapper<FSMv4> context) : BaseState(context) {
        auto & fsm = _context.get();
        fsm.getDoor().close();
        fsm.getLED().setStatus(LEDController::eStatus::RedCross);
        fsm.getPOS().setRows("Touch Card");
    }

    PaymentProcessingState::PaymentProcessingState(std::reference_wrapper<FSMv4> context, std::string cardNumber)
        : BaseState(context)
        , _cardNumber(std::move(cardNumber))
        , _timeoutManager(
              [&] {
                  _context.get().process(Timeout{});
              },
              2s) {
        auto & fsm = _context.get();
        fsm.getDoor().close();
        fsm.getLED().setStatus(LEDController::eStatus::OrangeCross);
        fsm.getPOS().setRows("Processing");
        fsm.initiateTransaction(_gateways[_retryCount], _cardNumber, getFare());
    }

    bool PaymentProcessingState::tryRetry() {
        if (++_retryCount >= _gateways.size()) {
            return false;
        }
        _context.get().initiateTransaction(_gateways[_retryCount], _cardNumber, getFare());
        _timeoutManager.restart(2s);
        return true;
    }

    std::array<std::string, 3> PaymentProcessingState::_gateways{"Gateway1", "Gateway2", "Gateway3"};

    PaymentFailed::PaymentFailed(std::reference_wrapper<FSMv4> context, std::string reason)
        : BaseState(context)
        , _reason(std::move(reason))
        , _timeoutManager(
              [&] {
                  _context.get().process(Timeout{});
              },
              2s) {
        auto & fsm = _context.get();
        fsm.getDoor().close();
        fsm.getLED().setStatus(LEDController::eStatus::FlashRedCross);
        fsm.getPOS().setRows("Declined", _reason);
    }

    PaymentSuccess::PaymentSuccess(std::reference_wrapper<FSMv4> context, int fare, int balance)
        : BaseState(context)
        , _timeoutManager(
              [&] {
                  _context.get().process(Timeout{});
              },
              2s) {
        auto & fsm = _context.get();
        fsm.getDoor().open();
        fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
        fsm.getPOS().setRows(
            "Approved", std::string("Fare: ") + std::to_string(fare),
            std::string("Balance: ") + std::to_string(balance));
    }

    Unlocked::Unlocked(std::reference_wrapper<FSMv4> context) : BaseState(context) {
        auto & fsm = _context.get();
        fsm.getDoor().open();
        fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
        fsm.getPOS().setRows("Approved");
    }
} // namespace

TEST(FSMv4, TestInitialState) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestPaymentProcessing) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestPaymentFailed) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestTimeoutOnPaymentProcessing) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestLockedFromPaymentFailed) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestPaymentSuccessful) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestUnlocked) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestLockedFromUnlocked) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestLockedFromPaymentSuccessful) {
    FSMv4 fsm;
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

TEST(FSMExternalTransitions, TestBug) {
    FSMv4 fsm;
    fsm.process(CardPresented{"A"}).process(Timeout{}).process(Timeout{}).process(Timeout{}).process(Timeout{});
    EXPECT_EQ(eState::Locked, fsm.getState());
    fsm.process(CardPresented{"A"}).process(Timeout{});
    EXPECT_EQ(eState::PaymentProcessing, fsm.getState());
}
