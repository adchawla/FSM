#pragma once

#include "Turnstile.h"

#include <array>
#include <optional>
#include <string>

namespace states {
    using namespace std::chrono_literals;

    template <typename FSM>
    class TLocked;
    template <typename FSM>
    class TPaymentProcessing;
    template <typename FSM>
    class TPaymentFailed;
    template <typename FSM>
    class TPaymentSuccess;
    template <typename FSM>
    class TUnlocked;
    template <typename FSM>
    using TState =
        std::variant<TLocked<FSM>, TPaymentProcessing<FSM>, TPaymentFailed<FSM>, TPaymentSuccess<FSM>, TUnlocked<FSM>>;
    template <typename FSM>
    using TOptState = std::optional<TState<FSM>>;

    template <typename FSM>
    class TBaseState {
    public:
        explicit TBaseState(std::reference_wrapper<FSM> context) : _context(context) {
        }

        template <typename EventType>
        TOptState<FSM> process(EventType) {
            return TOptState<FSM>{};
        };

    protected:
        std::reference_wrapper<FSM> _context;
    };

    template <typename FSM>
    class TLocked : public TBaseState<FSM> {
    public:
        using TBaseState<FSM>::_context;
        explicit TLocked(std::reference_wrapper<FSM> context) : TBaseState<FSM>(context) {
            auto & fsm = _context.get();
            fsm.getDoor().close();
            fsm.getLED().setStatus(LEDController::eStatus::RedCross);
            fsm.getPOS().setRows("Touch Card");
        }

        eState getState() const {
            return eState::Locked;
        }

        using TBaseState<FSM>::process;
        TOptState<FSM> process(CardPresented event) {
            return TPaymentProcessing<FSM>(_context, std::move(event.cardNumber));
        }
        // std::reference_wrapper<FSM> _context;
    };

    template <typename FSM>
    class TPaymentProcessing : public TBaseState<FSM> {
    public:
        using TBaseState<FSM>::_context;
        explicit TPaymentProcessing(std::reference_wrapper<FSM> context, std::string cardNumber)
            : TBaseState<FSM>(context)
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
            fsm.initiateTransaction(GATEWAYS[_retryCount], _cardNumber, getFare());
        }

        eState getState() const {
            return eState::PaymentProcessing;
        }

        bool tryRetry() {
            if (++_retryCount >= GATEWAYS.size()) {
                return false;
            }
            _context.get().initiateTransaction(GATEWAYS[_retryCount], _cardNumber, getFare());
            _timeoutManager.restart(2s);
            return true;
        }

        using TBaseState<FSM>::process;
        TOptState<FSM> process(TransactionDeclined event) {
            return TPaymentFailed<FSM>(_context, std::move(event.reason));
        }

        TOptState<FSM> process(TransactionSuccess event) {
            return TPaymentSuccess<FSM>(_context, event.fare, event.balance);
        }

        TOptState<FSM> process(Timeout event) {
            return tryRetry() ? TOptState<FSM>{} : TPaymentFailed<FSM>(_context, "Network Failure");
        }

    private:
        size_t _retryCount{0};
        std::string _cardNumber;
        TimeoutManager _timeoutManager;
    };

    template <typename FSM>
    class TPaymentFailed : public TBaseState<FSM> {
    public:
        using TBaseState<FSM>::_context;
        TPaymentFailed(std::reference_wrapper<FSM> context, std::string reason)
            : TBaseState<FSM>(context)
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

        eState getState() const {
            return eState::PaymentFailed;
        }

        using TBaseState<FSM>::process;
        TOptState<FSM> process(Timeout event) {
            return TLocked<FSM>(_context);
        }

    private:
        std::string _reason;
        TimeoutManager _timeoutManager;
    };

    template <typename FSM>
    class TPaymentSuccess : public TBaseState<FSM> {
    public:
        using TBaseState<FSM>::_context;
        TPaymentSuccess(std::reference_wrapper<FSM> context, int fare, int balance)
            : TBaseState<FSM>(context)
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

        eState getState() const {
            return eState::PaymentSuccess;
        }

        using TBaseState<FSM>::process;
        TOptState<FSM> process(PersonPassed event) {
            return TLocked<FSM>(_context);
        }

        TOptState<FSM> process(Timeout event) {
            return TUnlocked<FSM>(_context);
        }

    private:
        TimeoutManager _timeoutManager;
    };

    template <typename FSM>
    class TUnlocked : public TBaseState<FSM> {
    public:
        using TBaseState<FSM>::_context;
        explicit TUnlocked(std::reference_wrapper<FSM> context) : TBaseState<FSM>(context) {
            auto & fsm = _context.get();
            fsm.getDoor().open();
            fsm.getLED().setStatus(LEDController::eStatus::GreenArrow);
            fsm.getPOS().setRows("Approved");
        }

        eState getState() const {
            return eState::Unlocked;
        }

        using TBaseState<FSM>::process;
        TOptState<FSM> process(PersonPassed event) {
            return TLocked<FSM>(_context);
        }
    };
} // namespace states
