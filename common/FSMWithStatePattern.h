#pragma once

#include "ConditionalStream.h"
#include "Turnstile.h"

#include <array>

namespace with_state_pattern {
    class BaseState;

    class FSM {
    public:
        FSM();

        template <typename Event>
        FSM & process(Event && event);

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
        explicit BaseState(std::reference_wrapper<FSM> context) : _context(context) {
        }
        virtual ~BaseState() = default;

        BaseState(const BaseState & other) = delete;
        BaseState(BaseState && other) noexcept = delete;
        BaseState & operator=(const BaseState & other) = delete;
        BaseState & operator=(BaseState && other) noexcept = delete;

        virtual eState state() = 0;

        // event handlers
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
        std::reference_wrapper<FSM> _context;
    };

    class Locked final : public BaseState {
    public:
        explicit Locked(std::reference_wrapper<FSM> context);

        eState state() override {
            return eState::Locked;
        }
        std::unique_ptr<BaseState> process(CardPresented event) override;
    };

    class PaymentProcessing final : public BaseState {
    public:
        explicit PaymentProcessing(std::reference_wrapper<FSM> context, std::string cardNumber);

        eState state() override {
            return eState::PaymentProcessing;
        }
        std::unique_ptr<BaseState> process(TransactionDeclined event) override;
        std::unique_ptr<BaseState> process(TransactionSuccess event) override;
        std::unique_ptr<BaseState> process(Timeout event) override;

    private:
        size_t _retryCount{0};
        std::string _cardNumber;
        TimeoutManager _timeoutManager;
        static std::array<std::string, 3> _gateways;
    };

    class PaymentFailed final : public BaseState {
    public:
        PaymentFailed(std::reference_wrapper<FSM> context, std::string reason);

        eState state() override {
            return eState::PaymentFailed;
        }

        std::unique_ptr<BaseState> process(Timeout event) override;

    private:
        std::string _reason;
        TimeoutManager _timeoutManager;
    };

    class PaymentSuccess final : public BaseState {
    public:
        PaymentSuccess(std::reference_wrapper<FSM> context, int fare, int balance);

        eState state() override {
            return eState::PaymentSuccess;
        }
        std::unique_ptr<BaseState> process(PersonPassed event) override;
        std::unique_ptr<BaseState> process(Timeout event) override;

    private:
        TimeoutManager _timeoutManager;
    };

    class Unlocked final : public BaseState {
    public:
        explicit Unlocked(std::reference_wrapper<FSM> context);

        eState state() override {
            return eState::Unlocked;
        }
        std::unique_ptr<BaseState> process(PersonPassed event) override;
    };

    template <typename Event>
    FSM & FSM::process(Event && event) {
        LOGGER << "EVENT: " << type_name<std::decay_t<Event>>() << "\n";
        if (auto newState = _state->process(std::forward<Event>(event))) {
            _state = std::move(newState);
        }
        return *this;
    }

    FSM::FSM() : _state(std::make_unique<Locked>(std::ref(*this))) {
    }

    eState FSM::getState() const {
        return _state->state();
    }

    void FSM::initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
        LOGGER << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
               << amount << "]\n";
        _lastTransaction = std::make_tuple(gateway, cardNum, amount);
    }

    void FSM::dump() const {
        LOGGER << "STATE: " << to_string(_state->state()) << " :: Door[" << to_string(_door.getStatus()) << "], LED: ["
               << to_string(_led.getStatus()) << "] and PosTerminal[" << _pos.getRows() << "]\n";
    }

    Locked::Locked(std::reference_wrapper<FSM> context) : BaseState(context) {
        auto & fsm = _context.get();
        fsm.getDoor().close();
        fsm.getLED().setStatus(LEDController::eStatus::RedCross);
        fsm.getPOS().setRows("Touch Card");
    }

    std::unique_ptr<BaseState> Locked::process(CardPresented event) {
        return std::make_unique<PaymentProcessing>(_context, std::move(event.cardNumber));
    }

    PaymentProcessing::PaymentProcessing(std::reference_wrapper<FSM> context, std::string cardNumber)
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

    std::array<std::string, 3> PaymentProcessing::_gateways{"Gateway1", "Gateway2", "Gateway3"};

    std::unique_ptr<BaseState> PaymentProcessing::process(TransactionDeclined event) {
        return std::make_unique<PaymentFailed>(_context, std::move(event.reason));
    }

    std::unique_ptr<BaseState> PaymentProcessing::process(TransactionSuccess event) {
        return std::make_unique<PaymentSuccess>(_context, event.fare, event.balance);
    }

    std::unique_ptr<BaseState> PaymentProcessing::process(Timeout event) {
        _retryCount++;
        if (_retryCount >= _gateways.size()) {
            return std::make_unique<PaymentFailed>(_context, "Network Failure");
        }
        _context.get().initiateTransaction(_gateways[_retryCount], _cardNumber, getFare());
        _timeoutManager.restart(2s);
        return nullptr;
    }

    PaymentFailed::PaymentFailed(std::reference_wrapper<FSM> context, std::string reason)
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

    std::unique_ptr<BaseState> PaymentFailed::process(Timeout event) {
        return std::make_unique<Locked>(_context);
    }

    PaymentSuccess::PaymentSuccess(std::reference_wrapper<FSM> context, int fare, int balance)
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

    std::unique_ptr<BaseState> PaymentSuccess::process(PersonPassed event) {
        return std::make_unique<Locked>(_context);
    }

    std::unique_ptr<BaseState> PaymentSuccess::process(Timeout event) {
        return std::make_unique<Unlocked>(_context);
    }

    Unlocked::Unlocked(std::reference_wrapper<FSM> context) : BaseState(context) {
        auto & fsm = _context.get();
        fsm.getDoor().open();
        fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
        fsm.getPOS().setRows("Approved");
    }

    std::unique_ptr<BaseState> Unlocked::process(PersonPassed event) {
        return std::make_unique<Locked>(_context);
    }
} // namespace with_state_pattern