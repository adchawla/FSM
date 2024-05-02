#pragma once

#include "ConditionalStream.h"
#include "FSM.h"
#include "States.h"
#include "Turnstile.h"

namespace old_fsm_state_transitions {
    class FSM;

    using Locked = states::TLocked<FSM>;
    using PaymentProcessing = states::TPaymentProcessing<FSM>;
    using PaymentFailed = states::TPaymentFailed<FSM>;
    using PaymentSuccess = states::TPaymentSuccess<FSM>;
    using Unlocked = states::TUnlocked<FSM>;
    using State = std::variant<Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked>;
    using OptState = std::optional<State>;

    class FSM {
    public:
        FSM() : _fsm{Locked{std::ref(*this)}} {
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
        adc::old::TFSMStateTransitions<Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked> _fsm;

        // for testing
        std::tuple<std::string, std::string, int> _lastTransaction;

    public:
        const auto & getLastTransaction() const {
            return _lastTransaction;
        }
    };
} // namespace old_fsm_state_transitions