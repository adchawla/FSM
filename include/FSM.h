#include <variant>

namespace adc::details {
    template <typename Transitions>
    struct ExternalTransitions {
        explicit ExternalTransitions(Transitions transitions) : _transitions(std::move(transitions)) {
        }
        template <typename Machine, typename State, typename Event>
        auto execute(Machine * machine, State & state, Event && event) {
            return _transitions.operator()(state, std::forward<Event>(event));
        }

    private:
        Transitions _transitions{};
    };

    template <typename Derived>
    struct InbuiltTransitions {
        template <typename Machine, typename State, typename Event>
        auto execute(Machine * machine, State & state, Event && event) {
            return static_cast<Derived *>(machine)->process(state, std::forward<Event>(event));
        }
    };

    struct StatesHandlingTransitions {
        template <typename Machine, typename State, typename Event>
        auto execute(Machine * machine, State & state, Event && event) {
            return state.process(std::forward<Event>(event));
        }
    };

    template <typename Strategy, typename... States>
    class FSMBase {
    public:
        template <typename InitialState>
        explicit FSMBase(Strategy strategy, InitialState && state)
            : _strategy{std::move(strategy)}, _state{std::forward<InitialState>(state)} {
        }

        template <typename Event>
        void process(Event event) {
            auto optResult = std::visit(
                [&](auto & state) {
                    return _strategy.execute(this, state, std::move(event));
                },
                _state);
            if (optResult) {
                _state = std::move(optResult.value());
            }
        }

        auto getStateIndex() const {
            return _state.index();
        }

    protected:
        Strategy _strategy;
        std::variant<States...> _state;
    };
} // namespace adc::details

namespace adc {
    template <typename... States>
    class FSMStateTransitions : public details::FSMBase<details::StatesHandlingTransitions, States...> {
        using BaseType = details::FSMBase<details::StatesHandlingTransitions, States...>;
        using StrategyType = details::StatesHandlingTransitions;

    public:
        template <typename InitialState>
        explicit FSMStateTransitions(InitialState && state)
            : BaseType{StrategyType{}, std::forward<InitialState>(state)} {
        }
    };

    template <typename Transitions, typename... States>
    class FSMExternalTransitions : public details::FSMBase<details::ExternalTransitions<Transitions>, States...> {
        using BaseType = details::FSMBase<details::ExternalTransitions<Transitions>, States...>;
        using StrategyType = details::ExternalTransitions<Transitions>;

    public:
        template <typename InitialState>
        explicit FSMExternalTransitions(Transitions transitions, InitialState && state)
            : BaseType{StrategyType{std::move(transitions)}, std::forward<InitialState>(state)} {
        }
    };
} // namespace adc
