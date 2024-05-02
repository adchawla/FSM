#pragma once

#include "ConditionalStream.h"
#include "Turnstile.h"

#include <array>

namespace with_enums {
    class FSM {
    public:
        FSM & process(CardPresented event);
        FSM & process(TransactionDeclined event);
        FSM & process(TransactionSuccess event);
        FSM & process(PersonPassed event);
        FSM & process(Timeout event);

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
        void dump() const;

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

    inline FSM & FSM::process(CardPresented event) {
        LOGGER << "EVENT: CardPresent\n";
        switch (_state) { // NOLINT(clang-diagnostic-switch-enum)
        case eState::Locked:
            transitionToPaymentProcessing(_gateways[0], std::move(event.cardNumber));
            break;

        default:
            break;
        }
        return *this;
    }

    inline FSM & FSM::process(TransactionDeclined event) {
        LOGGER << "EVENT: TransactionDeclined\n";
        switch (_state) { // NOLINT(clang-diagnostic-switch-enum)
        case eState::PaymentProcessing:
            transitionToPaymentFailed(std::move(event.reason));
            break;
        default:
            break;
        }
        return *this;
    }

    inline FSM & FSM::process(TransactionSuccess event) {
        LOGGER << "EVENT: TransactionSuccess\n";
        switch (_state) { // NOLINT(clang-diagnostic-switch-enum)
        case eState::PaymentProcessing:
            transitionToPaymentSuccessful(event.fare, event.balance);
            break;
        default:
            break;
        }
        return *this;
    }

    inline FSM & FSM::process(PersonPassed event) {
        LOGGER << "EVENT: PersonPassed\n";
        switch (_state) { // NOLINT(clang-diagnostic-switch-enum)
        case eState::PaymentSuccess:
        case eState::Unlocked:
            transitionToLocked();
            break;
        default:
            break;
        }
        return *this;
    }

    inline FSM & FSM::process(Timeout event) {
        LOGGER << "EVENT: Timeout\n";
        switch (_state) { // NOLINT(clang-diagnostic-switch-enum)
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

    inline void FSM::dump() const {
        LOGGER << "STATE: " << to_string(_state) << " :: Door[" << to_string(_door.getStatus()) << "], LED: ["
               << to_string(_led.getStatus()) << "] and PosTerminal[" << _pos.getRows() << "]\n";
    }

    inline void FSM::initiateTransaction(const std::string & gateway, const std::string & cardNum, int amount) {
        LOGGER << "ACTIONS: Initiated Transaction to [" << gateway << "] with card [" << cardNum << "] for amount ["
               << amount << "]\n";
        _lastTransaction = std::make_tuple(gateway, cardNum, amount);
    }

    inline void FSM::transitionToPaymentProcessing(const std::string & gateway, std::string cardNumber) {
        _cardNumber = std::move(cardNumber);
        initiateTransaction(gateway, _cardNumber, getFare());
        _door.close();
        _pos.setRows("Processing");
        _led.setStatus(LEDController::eStatus::OrangeCross);
        _state = eState::PaymentProcessing;
    }

    inline void FSM::transitionToPaymentFailed(const std::string & reason) {
        _door.close();
        _pos.setRows("Declined", reason);
        _led.setStatus(LEDController::eStatus::FlashRedCross);
        _state = eState::PaymentFailed;
    }

    inline void FSM::transitionToLocked() {
        _state = eState::Locked;
        _door.close();
        _pos.setRows("Touch Card");
        _led.setStatus(LEDController::eStatus::RedCross);
    }

    inline void FSM::transitionToPaymentSuccessful(int fare, int balance) {
        _state = eState::PaymentSuccess;
        _door.open();
        _pos.setRows(
            "Approved", std::string("Fare: ") + std::to_string(fare),
            std::string("Balance: ") + std::to_string(balance));
        _led.setStatus(LEDController::eStatus::GreenArrow);
    }

    inline void FSM::transitionToUnlocked() {
        _state = eState::Unlocked;
        _door.open();
        _pos.setRows("Approved");
        _led.setStatus(LEDController::eStatus::GreenArrow);
    }

} // namespace with_enums
