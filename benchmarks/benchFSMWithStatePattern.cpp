#include "FSMWithStatePattern.h"

#include <benchmark/benchmark.h>

static void BM_FSMWithStatePattern(benchmark::State & state) {
    with_state_pattern::FSM fsm;
    for (auto _ : state)
        fsm.process(CardPresented{})
            .process(Timeout{})
            .process(Timeout{})
            .process(TransactionSuccess{5, 25})
            .process(Timeout{})
            .process(PersonPassed{});
}
// Register the function as a benchmark
BENCHMARK(BM_FSMWithStatePattern);
