#include <variant>

namespace adc::details {
    template <typename Transitions>
    struct TExternalTransitions {
        explicit TExternalTransitions(Transitions transitions) : _transitions(std::move(transitions)) {
        }
        template <typename State, typename Event>
        auto execute(State & state, Event && event) {
            return _transitions.operator()(state, std::forward<Event>(event));
        }

    private:
        Transitions _transitions{};
    };
    struct StatesHandlingTransitions {
        template <typename State, typename Event>
        auto execute(State & state, Event && event) {
            return state.process(std::forward<Event>(event));
        }
    };

    template <typename Strategy, typename... States>
    class TFSMBase {
    public:
        template <typename InitialState>
        explicit TFSMBase(Strategy strategy, InitialState && state)
            : _strategy{std::move(strategy)}, _state{std::forward<InitialState>(state)} {
        }

        template <typename Event>
        void process(Event event) {
            auto optResult = std::visit(
                [&](auto & state) {
                    return _strategy.execute(state, std::move(event));
                },
                _state);
            if (optResult) {
                _state = std::move(optResult.value());
            }
        }

        auto getState() const {
            return std::visit(
                [](auto & state) {
                    return state.getState();
                },
                _state);
        }

    protected:
        Strategy _strategy;
        std::variant<States...> _state;
    };
} // namespace adc::details

namespace adc {
    template <typename... States>
    class TFSMStateTransitions : public details::TFSMBase<details::StatesHandlingTransitions, States...> {
        using BaseType = details::TFSMBase<details::StatesHandlingTransitions, States...>;
        using StrategyType = details::StatesHandlingTransitions;

    public:
        template <typename InitialState>
        explicit TFSMStateTransitions(InitialState && state)
            : BaseType{StrategyType{}, std::forward<InitialState>(state)} {
        }
    };

    template <typename Transitions, typename... States>
    class TFSMExternalTransitions : public details::TFSMBase<details::TExternalTransitions<Transitions>, States...> {
        using BaseType = details::TFSMBase<details::TExternalTransitions<Transitions>, States...>;
        using StrategyType = details::TExternalTransitions<Transitions>;

    public:
        template <typename InitialState>
        explicit TFSMExternalTransitions(Transitions transitions, InitialState && state)
            : BaseType{StrategyType{std::move(transitions)}, std::forward<InitialState>(state)} {
        }
    };
} // namespace adc

namespace adc::old {
    template <typename... States>
    class TFSMStateTransitions {
    public:
        template <typename InitialState>
        explicit TFSMStateTransitions(InitialState && state) : _state{std::forward<InitialState>(state)} {
        }

        template <typename Event>
        void process(Event event) {
            auto optResult = std::visit(
                [&](auto & state) {
                    return state.process(std::move(event));
                },
                _state);
            if (optResult) {
                _state = std::move(optResult.value());
            }
        }

        auto getState() const {
            return std::visit(
                [](auto & state) {
                    return state.getState();
                },
                _state);
        }

    private:
        std::variant<States...> _state;
    };

    template <typename Transitions, typename... States>
    class TFSMExternalTransitions {
    public:
        template <typename InitialState>
        explicit TFSMExternalTransitions(Transitions transitions, InitialState && state)
            : _transitions{std::move(transitions)}, _state{std::forward<InitialState>(state)} {
        }

        template <typename Event>
        void process(Event event) {
            auto optResult = std::visit(
                [&](auto & state) {
                    return _transitions.operator()(state, std::move(event));
                },
                _state);
            if (optResult) {
                _state = std::move(optResult.value());
            }
        }

        auto getState() const {
            return std::visit(
                [](auto & state) {
                    return state.getState();
                },
                _state);
        }

    private:
        std::variant<States...> _state;
        Transitions _transitions;
    };
} // namespace adc::old
