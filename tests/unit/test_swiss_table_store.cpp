#include <gtest/gtest.h>
#include "../../src/storage/swiss_table_store.h"
#include <thread>
#include <vector>

class SwissTableStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        store_ = std::make_unique<SwissTableStore>();
    }

    void TearDown() override {
        store_.reset();
    }

    std::unique_ptr<SwissTableStore> store_;
};

TEST_F(SwissTableStoreTest, BasicOperations) {
    // Test Set and Get
    EXPECT_TRUE(store_->Set("key1", "value1"));
    auto result = store_->Get("key1");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");

    // Test exists
    EXPECT_TRUE(store_->Exists("key1"));
    EXPECT_FALSE(store_->Exists("nonexistent"));

    // Test Delete
    EXPECT_TRUE(store_->Delete("key1"));
    EXPECT_FALSE(store_->Exists("key1"));
    EXPECT_FALSE(store_->Get("key1").has_value());

    // Test Delete on non-existent key
    EXPECT_FALSE(store_->Delete("nonexistent"));
}

TEST_F(SwissTableStoreTest, IncrementOperations) {
    // Test increment on non-existent key
    int64_t result = store_->Increment("counter", 5);
    EXPECT_EQ(result, 5);
    auto value = store_->Get("counter");
    EXPECT_EQ(value.value(), "5");

    // Test increment on existing key
    result = store_->Increment("counter", 3);
    EXPECT_EQ(result, 8);
    value = store_->Get("counter");
    EXPECT_EQ(value.value(), "8");

    // Test decrement
    result = store_->Increment("counter", -2);
    EXPECT_EQ(result, 6);
    value = store_->Get("counter");
    EXPECT_EQ(value.value(), "6");

    // Test increment with default (delta=1)
    result = store_->Increment("counter");
    EXPECT_EQ(result, 7);
    value = store_->Get("counter");
    EXPECT_EQ(value.value(), "7");
}

TEST_F(SwissTableStoreTest, IncrementErrors) {
    // Set a non-numeric value
    store_->Set("text_key", "hello");

    // Test increment on non-numeric value
    EXPECT_THROW(store_->Increment("text_key", 1), std::invalid_argument);

    // Test increment on empty non-existent key (should work)
    int64_t result = store_->Increment("empty_key");
    EXPECT_EQ(result, 1);
}

TEST_F(SwissTableStoreTest, SizeAndClear) {
    EXPECT_EQ(store_->Size(), 0);

    store_->Set("key1", "value1");
    store_->Set("key2", "value2");
    store_->Set("key3", "value3");

    EXPECT_EQ(store_->Size(), 3);

    store_->Clear();
    EXPECT_EQ(store_->Size(), 0);
    EXPECT_FALSE(store_->Exists("key1"));
}

TEST_F(SwissTableStoreTest, KeysRetrieval) {
    store_->Set("key1", "value1");
    store_->Set("key2", "value2");
    store_->Set("key3", "value3");

    auto keys = store_->Keys();
    EXPECT_EQ(keys.size(), 3);

    // Check that all keys are present
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys[0], "key1");
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key3");
}

TEST_F(SwissTableStoreTest, BatchOperations) {
    std::vector<std::pair<std::string, std::string>> key_values = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"}
    };

    // Test batch set
    EXPECT_TRUE(store_->SetBatch(key_values));
    EXPECT_EQ(store_->Size(), 3);

    // Test batch get
    std::vector<std::string> keys = {"key1", "key2", "key4"};
    auto results = store_->GetBatch(keys);

    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_EQ(results[0].value(), "value1");
    EXPECT_TRUE(results[1].has_value());
    EXPECT_EQ(results[1].value(), "value2");
    EXPECT_FALSE(results[2].has_value());  // key4 doesn't exist
}

TEST_F(SwissTableStoreTest, MemoryUsage) {
    size_t initial_memory = store_->MemoryUsage();
    EXPECT_GT(initial_memory, 0);

    store_->Set("key1", "value1");
    size_t after_add = store_->MemoryUsage();
    EXPECT_GT(after_add, initial_memory);

    store_->Clear();
    size_t after_clear = store_->MemoryUsage();
    EXPECT_LT(after_clear, after_add);
    EXPECT_GE(after_clear, initial_memory);
}

TEST_F(SwissTableStoreTest, ThreadSafety) {
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;

    auto worker = [&](int thread_id) {
        for (int i = 0; i < operations_per_thread; ++i) {
            std::string key = "thread_" + std::to_string(thread_id) + "_key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i);

            store_->Set(key, value);
            auto retrieved = store_->Get(key);
            EXPECT_TRUE(retrieved.has_value());
            EXPECT_EQ(retrieved.value(), value);

            if (i % 10 == 0) {
                store_->Delete(key);
            }
        }
    };

    // Start multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that some operations completed
    EXPECT_GT(store_->Size(), 0);
}

TEST_F(SwissTableStoreTest, LargeValues) {
    // Test with large values
    std::string large_value(1024 * 1024, 'x');  // 1MB string
    EXPECT_TRUE(store_->Set("large_key", large_value));

    auto result = store_->Get("large_key");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), large_value);

    // Test memory usage increases significantly
    size_t large_memory = store_->MemoryUsage();
    EXPECT_GT(large_memory, 1024 * 1024);  // Should be at least 1MB
}

TEST_F(SwissTableStoreTest, EdgeCases) {
    // Test empty key
    EXPECT_TRUE(store_->Set("", "empty_key_value"));
    auto result = store_->Get("");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "empty_key_value");

    // Test empty value
    EXPECT_TRUE(store_->Set("empty_value_key", ""));
    result = store_->Get("empty_value_key");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");

    // Test special characters in keys and values
    std::string special_key = "key with spaces & symbols!@#$%^&*()";
    std::string special_value = "value with \n newlines \r carriage returns \0 nulls";
    EXPECT_TRUE(store_->Set(special_key, special_value));
    result = store_->Get(special_key);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), special_value);
}