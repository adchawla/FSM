#include "Turnstile.h"

class BaseState;

class FSMWithStatePattern {
public:
    template <typename Event>
    FSMWithStatePattern & process(Event && event) {
        _state->process(std::forward<Event>(event));
        return *this;
    }

    void setState(std::unique_ptr<BaseState> newState) {
        _state = std::move(newState);
    }

    const std::unique_ptr<BaseState> & getState() {
        return _state;
    }

private:
    std::unique_ptr<BaseState> _state;
};

class BaseState {
public:
    explicit BaseState(std::reference_wrapper<FSMWithStatePattern> context) : _context(std::move(context)) {
    }
    virtual ~BaseState() = default;

    virtual std::string name() = 0;
    virtual void process(CardPresented event) {
    }
    virtual void process(TransactionDeclined event) {
    }
    virtual void process(TransactionSuccess event) {
    }
    virtual void process(PersonPassed event) {
    }
    virtual void process(Timeout event) {
    }

protected:
    std::reference_wrapper<FSMWithStatePattern> _context;
};

enum class eState { Locked, PaymentProcessing, PaymentFailed, PaymentSuccess, Unlocked };
class LockedState : public BaseState {
    explicit LockedState(std::reference_wrapper<FSMWithStatePattern> context) : BaseState(std::move(context)) {
    }
    std::string name() {
        return "Locked";
    }
    void process(CardPresented event) override;
};

class PaymentProcessingState : public BaseState {};