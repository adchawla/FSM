#include <variant>

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace adc { namespace v1 {
    template <typename... States>
    class FSM {
    public:
        template <typename InitialState>
        explicit FSM(InitialState && state) : _state{std::forward<InitialState>(state)} {
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

        auto getStateIndex() const {
            return _state.index();
        }

    private:
        std::variant<States...> _state;
    };
}} // namespace adc::v1
