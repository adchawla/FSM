#pragma once

#include "ConditionalStream.h"
#include "FSM.h"
#include "States.h"
#include "Turnstile.h"

namespace old_fsm_external_transitions {
    class FSM;

    using Locked = states::TLocked<FSM>;
    using PaymentProcessing = states::TPaymentProcessing<FSM>;
    using PaymentFailed = states::TPaymentFailed<FSM>;
    using PaymentSuccess = states::TPaymentSuccess<FSM>;
    using Unlocked = states::TUnlocked<FSM>;
    using State = std::variant<Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked>;
    using OptState = std::optional<State>;

    struct TransitionTable {
        OptState operator()(Locked & state, CardPresented event) {
            return PaymentProcessing(state._context, std::move(event.cardNumber));
        }
        OptState operator()(PaymentProcessing & state, TransactionDeclined event) {
            return PaymentFailed(state._context, std::move(event.reason));
        }
        OptState operator()(PaymentProcessing & state, TransactionSuccess event) {
            return PaymentSuccess(state._context, event.fare, event.balance);
        }
        OptState operator()(PaymentProcessing & state, Timeout event) {
            return state.tryRetry() ? OptState{} : PaymentFailed(state._context, "Network Failure");
        }
        OptState operator()(PaymentFailed & state, Timeout) {
            return Locked(state._context);
        }
        OptState operator()(PaymentSuccess & state, Timeout) {
            return Unlocked(state._context);
        }
        OptState operator()(PaymentSuccess & state, PersonPassed) {
            return Locked(state._context);
        }
        OptState operator()(Unlocked & state, PersonPassed) {
            return Locked(state._context);
        }
        template <typename State, typename Event>
        auto operator()(State & s, Event e) const {
            return OptState{};
        }
    };

    class FSM {
    public:
        FSM() : _fsm{TransitionTable{}, Locked{std::ref(*this)}} {
        }

        template <typename Event>
        FSM & process(Event event) {
            _fsm.process(std::move(event));
            return *this;
        }

        eState getState() const {
            return _fsm.getState();
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
        void initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
            logTransaction(gateway, cardNum, amount);
            _lastTransaction = std::make_tuple(gateway, cardNum, amount);
        }

    private:
        // Connected Devices
        SwingDoor _door;
        POSTerminal _pos{""};
        LEDController _led;
        adc::old::TFSMExternalTransitions<
            TransitionTable, Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked>
            _fsm;

        // for testing
        std::tuple<std::string, std::string, int> _lastTransaction;

    public:
        const auto & getLastTransaction() const {
            return _lastTransaction;
        }
    };
} // namespace old_fsm_external_transitions
