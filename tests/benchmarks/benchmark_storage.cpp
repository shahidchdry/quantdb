#include <benchmark/benchmark.h>
#include "../../src/storage/swiss_table_store.h"
#include <random>
#include <string>
#include <vector>

class SwissTableBenchmark : public ::benchmark::Fixture {
protected:
    void SetUp(const ::benchmark::State& state) override {
        store_ = std::make_unique<SwissTableStore>();

        // Pre-generate test data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100, 1000); // Random string lengths

        size_t num_keys = state.range(0);
        test_keys_.reserve(num_keys);
        test_values_.reserve(num_keys);

        for (size_t i = 0; i < num_keys; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value(dis(gen), 'x'); // Random length string filled with 'x'

            test_keys_.push_back(key);
            test_values_.push_back(value);
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        store_.reset();
    }

    std::unique_ptr<SwissTableStore> store_;
    std::vector<std::string> test_keys_;
    std::vector<std::string> test_values_;
};

// Benchmark SET operations
BENCHMARK_DEFINE_F(SwissTableBenchmark, SetOperation)(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        size_t key_index = i % test_keys_.size();
        store_->Set(test_keys_[key_index], test_values_[key_index]);
        ++i;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() *
        (test_keys_[0].length() + test_values_[0].length()));
}

// Benchmark GET operations
BENCHMARK_DEFINE_F(SwissTableBenchmark, GetOperation)(benchmark::State& state) {
    // Pre-populate the store
    for (size_t i = 0; i < test_keys_.size(); ++i) {
        store_->Set(test_keys_[i], test_values_[i]);
    }

    size_t i = 0;
    for (auto _ : state) {
        size_t key_index = i % test_keys_.size();
        auto result = store_->Get(test_keys_[key_index]);
        benchmark::DoNotOptimize(result);
        ++i;
    }

    state.SetItemsProcessed(state.iterations());
}

// Benchmark mixed SET/GET workload (80% reads, 20% writes)
BENCHMARK_DEFINE_F(SwissTableBenchmark, MixedWorkload)(benchmark::State& state) {
    // Pre-populate with some data
    for (size_t i = 0; i < test_keys_.size() / 2; ++i) {
        store_->Set(test_keys_[i], test_values_[i]);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);

    for (auto _ : state) {
        int operation = dis(gen);
        if (operation <= 80) {
            // 80% GET operations
            size_t key_index = (operation * 7) % test_keys_.size();
            auto result = store_->Get(test_keys_[key_index]);
            benchmark::DoNotOptimize(result);
        } else {
            // 20% SET operations
            size_t key_index = operation % test_keys_.size();
            store_->Set(test_keys_[key_index], test_values_[key_index]);
        }
    }

    state.SetItemsProcessed(state.iterations());
}

// Benchmark INCR operations
BENCHMARK_DEFINE_F(SwissTableBenchmark, IncrementOperation)(benchmark::State& state) {
    // Initialize counters
    for (size_t i = 0; i < std::min(static_cast<size_t>(1000), test_keys_.size()); ++i) {
        store_->Set(test_keys_[i], "0");
    }

    size_t i = 0;
    for (auto _ : state) {
        size_t key_index = i % std::min(static_cast<size_t>(1000), test_keys_.size());
        try {
            auto result = store_->Increment(test_keys_[key_index], 1);
            benchmark::DoNotOptimize(result);
        } catch (...) {
            // Ignore errors for benchmark purposes
        }
        ++i;
    }

    state.SetItemsProcessed(state.iterations());
}

// Benchmark batch operations
BENCHMARK_DEFINE_F(SwissTableBenchmark, BatchSet)(benchmark::State& state) {
    std::vector<std::pair<std::string, std::string>> batch;
    batch.reserve(100);

    for (auto _ : state) {
        batch.clear();
        for (int i = 0; i < 100; ++i) {
            batch.emplace_back(test_keys_[i % test_keys_.size()],
                             test_values_[i % test_values_.size()]);
        }

        store_->SetBatch(batch);
        state.PauseTiming();
        batch.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * 100);
    state.SetBytesProcessed(state.iterations() * 100 *
        (test_keys_[0].length() + test_values_[0].length()));
}

// Benchmark batch GET operations
BENCHMARK_DEFINE_F(SwissTableBenchmark, BatchGet)(benchmark::State& state) {
    // Pre-populate the store
    for (size_t i = 0; i < test_keys_.size(); ++i) {
        store_->Set(test_keys_[i], test_values_[i]);
    }

    std::vector<std::string> keys;
    keys.reserve(100);

    for (auto _ : state) {
        keys.clear();
        for (int i = 0; i < 100; ++i) {
            keys.push_back(test_keys_[i % test_keys_.size()]);
        }

        auto results = store_->GetBatch(keys);
        benchmark::DoNotOptimize(results);
        state.PauseTiming();
        keys.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * 100);
}

// Benchmark memory usage under load
BENCHMARK_DEFINE_F(SwissTableBenchmark, MemoryUsage)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        store_->Clear();
        state.ResumeTiming();

        // Fill with data
        for (size_t i = 0; i < test_keys_.size(); ++i) {
            store_->Set(test_keys_[i], test_values_[i]);
        }

        state.PauseTiming();
        size_t memory_usage = store_->MemoryUsage();
        state.counters["MemoryBytes"] = memory_usage;
        state.counters["MemoryKB"] = memory_usage / 1024;
        state.counters["MemoryMB"] = memory_usage / (1024 * 1024);
        state.ResumeTiming();
    }
}

// Benchmark concurrent operations
BENCHMARK_DEFINE_F(SwissTableBenchmark, ConcurrentSets)(benchmark::State& state) {
    const int num_threads = std::min(4, static_cast<int>(state.range(1)));
    const size_t operations_per_thread = 10000;

    for (auto _ : state) {
        state.PauseTiming();
        store_->Clear();
        std::vector<std::thread> threads;
        std::atomic<size_t> total_operations{0};
        state.ResumeTiming();

        auto worker = [&](int thread_id) {
            for (size_t i = 0; i < operations_per_thread; ++i) {
                size_t key_index = (thread_id * operations_per_thread + i) % test_keys_.size();
                store_->Set(test_keys_[key_index] + "_thread_" + std::to_string(thread_id),
                          test_values_[key_index]);
                total_operations.fetch_add(1);
            }
        };

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, i);
        }

        for (auto& thread : threads) {
            thread.join();
        }

        state.PauseTiming();
        state.SetItemsProcessed(total_operations.load());
        state.ResumeTiming();
    }
}

// Register benchmarks with different parameterizations
BENCHMARK_REGISTER_F(SwissTableBenchmark, SetOperation)
    ->Arg(1000)    // 1K keys
    ->Arg(10000)   // 10K keys
    ->Arg(100000)  // 100K keys
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/Set");

BENCHMARK_REGISTER_F(SwissTableBenchmark, GetOperation)
    ->Arg(1000)    // 1K keys
    ->Arg(10000)   // 10K keys
    ->Arg(100000)  // 100K keys
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/Get");

BENCHMARK_REGISTER_F(SwissTableBenchmark, MixedWorkload)
    ->Arg(10000)   // 10K keys
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/MixedWorkload");

BENCHMARK_REGISTER_F(SwissTableBenchmark, IncrementOperation)
    ->Arg(10000)   // 10K keys
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/Increment");

BENCHMARK_REGISTER_F(SwissTableBenchmark, BatchSet)
    ->Arg(10000)   // 10K keys available
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/BatchSet");

BENCHMARK_REGISTER_F(SwissTableBenchmark, BatchGet)
    ->Arg(10000)   // 10K keys available
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/BatchGet");

BENCHMARK_REGISTER_F(SwissTableBenchmark, MemoryUsage)
    ->Arg(10000)   // 10K keys
    ->Unit(benchmark::kMillisecond)
    ->Name("SwissTable/MemoryUsage");

BENCHMARK_REGISTER_F(SwissTableBenchmark, ConcurrentSets)
    ->Args({10000, 1})   // 10K keys, 1 thread
    ->Args({10000, 2})   // 10K keys, 2 threads
    ->Args({10000, 4})   // 10K keys, 4 threads
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/ConcurrentSets");

// Custom benchmark for value size scaling
static void BM_ValueSizeScaling(benchmark::State& state) {
    auto store = std::make_unique<SwissTableStore>();
    size_t value_size = state.range(0);
    std::string value(value_size, 'x');

    for (auto _ : state) {
        store_->Set("test_key", value);
        auto result = store_->Get("test_key");
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(state.iterations() * value_size * 2); // SET + GET
}

BENCHMARK(BM_ValueSizeScaling)
    ->Arg(64)       // 64 bytes
    ->Arg(256)      // 256 bytes
    ->Arg(1024)     // 1KB
    ->Arg(4096)     // 4KB
    ->Arg(16384)    // 16KB
    ->Unit(benchmark::kMicrosecond)
    ->Name("SwissTable/ValueSizeScaling");

// Run benchmarks
BENCHMARK_MAIN();