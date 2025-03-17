#include "synchronization/fifo_read_write_lock.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace std::chrono_literals;

class FIFOReadWriteLockTest : public ::testing::Test {
 protected:
  void SetUp() override { shared_data_ = 0; }

  // Shared variables for tests
  FIFOReadWriteLock lock;
  uint64_t shared_data_;
};

// Basic functionality test: single reader and writer
TEST_F(FIFOReadWriteLockTest, BasicFunctionality) {
  constexpr size_t kNumReads = 100;
  constexpr size_t kNumWrites = 50;

  std::thread reader([this]() {
    for (size_t i = 0; i < kNumReads; i++) {
      lock.read_lock();
      uint64_t value = shared_data_;
      std::this_thread::sleep_for(1us);
      // Verify data hasn't changed during our read
      EXPECT_EQ(value, shared_data_);
      lock.read_unlock();
    }
  });

  std::thread writer([this]() {
    for (size_t i = 0; i < kNumWrites; i++) {
      lock.write_lock();
      shared_data_++;
      std::this_thread::sleep_for(2us);
      lock.write_unlock();
      std::this_thread::sleep_for(100us);
    }
  });

  reader.join();
  writer.join();

  EXPECT_EQ(shared_data_, kNumWrites)
      << "Writer should increment shared_data_ " << kNumWrites << " times";
}

// Test multiple readers can access simultaneously
TEST_F(FIFOReadWriteLockTest, MultipleReaders) {
  constexpr size_t kNumReaders = 10;
  constexpr size_t kIterationsPerReader = 100;

  std::atomic<uint64_t> readers_in_critical_section{0};
  std::atomic<uint64_t> max_concurrent_readers{0};

  std::vector<std::thread> readers;
  readers.reserve(kNumReaders);
  for (size_t i = 0; i < kNumReaders; i++) {
    readers.emplace_back(
        [this, &readers_in_critical_section, &max_concurrent_readers]() {
          for (size_t j = 0; j < kIterationsPerReader; j++) {
            lock.read_lock();

            // Track how many readers are in the critical section
            uint64_t current = ++readers_in_critical_section;
            max_concurrent_readers =
                std::max(max_concurrent_readers.load(), current);

            // Hold the lock briefly
            std::this_thread::sleep_for(10us);

            readers_in_critical_section--;
            lock.read_unlock();

            // Small delay between iterations
            std::this_thread::sleep_for(5us);
          }
        });
  }

  for (auto& t : readers) {
    t.join();
  }

  EXPECT_GT(max_concurrent_readers, 1)
      << "Multiple readers should be able to access simultaneously";
}

// Test writers have exclusive access
TEST_F(FIFOReadWriteLockTest, ExclusiveWriter) {
  constexpr size_t kNumWriters = 5;
  constexpr size_t kIterationsPerWriter = 100;

  // Although write lock guarantees mutual exclusion, we must use atomic
  // variables since our write lock implementation may be broken
  std::atomic<uint64_t> writers_in_critical_section{0};
  std::atomic<uint64_t> max_concurrent_writers{0};
  std::atomic<bool> error_detected{false};

  std::vector<std::thread> writers;
  for (size_t i = 0; i < kNumWriters; i++) {
    writers.push_back(std::thread([this, &writers_in_critical_section,
                                   &max_concurrent_writers, &error_detected]() {
      for (size_t j = 0; j < kIterationsPerWriter; j++) {
        lock.write_lock();

        // Track how many writers are in the critical section
        uint64_t current = ++writers_in_critical_section;
        max_concurrent_writers =
            std::max(max_concurrent_writers.load(), current);

        // If more than one writer is in the critical section, that's an error
        if (current > 1) {
          error_detected = true;
        }

        // Hold the lock briefly
        std::this_thread::sleep_for(10us);

        writers_in_critical_section--;
        lock.write_unlock();

        // Small delay between iterations
        std::this_thread::sleep_for(5us);
      }
    }));
  }

  for (auto& t : writers) {
    t.join();
  }

  EXPECT_EQ(max_concurrent_writers, 1)
      << "Only one writer should be in the critical section at a time";
  EXPECT_FALSE(error_detected)
      << "Detected multiple writers in the critical section simultaneously";
}

// Test writers block readers
TEST_F(FIFOReadWriteLockTest, WriterBlocksReaders) {
  constexpr size_t kNumReaders = 5;

  std::atomic<bool> writer_in_critical_section{false};
  std::atomic<bool> reader_entered_during_write{false};

  // Start a writer that holds the lock for a while
  std::thread writer([this, &writer_in_critical_section]() {
    lock.write_lock();
    writer_in_critical_section = true;

    // Hold the write lock for a significant time
    std::this_thread::sleep_for(5ms);

    writer_in_critical_section = false;
    lock.write_unlock();
  });

  // Give the writer a chance to acquire the lock
  std::this_thread::sleep_for(1ms);

  // Start readers that try to read while writer holds the lock
  std::vector<std::thread> readers;
  readers.reserve(kNumReaders);
  for (size_t i = 0; i < kNumReaders; i++) {
    readers.push_back(std::thread(
        [this, &writer_in_critical_section, &reader_entered_during_write]() {
          lock.read_lock();

          // Check if we entered while a writer was in the critical section
          if (writer_in_critical_section) {
            reader_entered_during_write = true;
          }

          lock.read_unlock();
        }));
  }

  writer.join();
  for (auto& t : readers) {
    t.join();
  }

  EXPECT_FALSE(reader_entered_during_write)
      << "Readers should be blocked while a writer holds the lock";
}

// Test readers block writers
TEST_F(FIFOReadWriteLockTest, ReadersBlockWriter) {
  constexpr size_t kNumReaders = 5;

  std::atomic<uint64_t> readers_in_critical_section{0};
  std::atomic<bool> writer_entered_during_read{false};

  // Start multiple readers that hold the lock for a while
  std::vector<std::thread> readers;
  readers.reserve(kNumReaders);
  for (size_t i = 0; i < kNumReaders; i++) {
    readers.emplace_back([this, &readers_in_critical_section]() {
      lock.read_lock();
      readers_in_critical_section++;

      // Hold the read lock for a significant time
      std::this_thread::sleep_for(5ms);

      readers_in_critical_section--;
      lock.read_unlock();
    });
  }

  // Give the readers a chance to acquire the locks
  std::this_thread::sleep_for(1ms);

  // Start a writer that tries to write while readers hold locks
  std::thread writer(
      [this, &readers_in_critical_section, &writer_entered_during_read]() {
        lock.write_lock();

        // Check if we entered while readers were in the critical section
        if (readers_in_critical_section > 0) {
          writer_entered_during_read = true;
        }

        lock.write_unlock();
      });

  for (auto& t : readers) {
    t.join();
  }
  writer.join();

  EXPECT_FALSE(writer_entered_during_read)
      << "Writer should be blocked while readers hold the lock";
}

// Test alternating readers and writers
TEST_F(FIFOReadWriteLockTest, AlternatingReadersWriters) {
  constexpr size_t kNumIterations = 50;
  constexpr size_t kNumReaders = 3;
  constexpr size_t kNumWriters = 2;

  std::atomic<uint64_t> write_count = 0;
  auto writer_task = [this, &write_count]() {
    for (size_t i = 0; i < kNumIterations; ++i) {
      lock.write_lock();
      // Write operation
      shared_data_++;

      // Simulate some work
      std::this_thread::sleep_for(2us);

      write_count++;
      lock.write_unlock();

      // Random delay between operations
      std::this_thread::sleep_for(std::chrono::microseconds(rand() % 20));
    }
  };

  // Launch writer threads
  std::vector<std::thread> writers;
  writers.reserve(kNumWriters);
  for (size_t i = 0; i < kNumWriters; ++i) {
    writers.emplace_back(writer_task);
  }

  std::atomic<uint64_t> read_count;
  std::atomic<uint64_t> error_count;
  auto reader_task = [this, &read_count, &error_count]() {
    for (size_t i = 0; i < kNumIterations; ++i) {
      lock.read_lock();
      // Read operation
      uint64_t value = shared_data_;

      // Simulate some work
      std::this_thread::sleep_for(1us);

      // Verify data hasn't changed during our read
      if (value != shared_data_) {
        error_count++;
      }

      read_count++;
      lock.read_unlock();

      // Random delay between operations
      std::this_thread::sleep_for(std::chrono::microseconds(rand() % 10));
    }
  };

  // Launch reader threads
  std::vector<std::thread> readers;
  readers.reserve(kNumReaders);
  for (size_t i = 0; i < kNumReaders; ++i) {
    readers.emplace_back(reader_task);
  }

  // Join all threads
  for (auto& t : writers) {
    t.join();
  }
  for (auto& t : readers) {
    t.join();
  }

  EXPECT_EQ(write_count, kNumWriters * kNumIterations)
      << "All write operations should complete";
  EXPECT_EQ(read_count, kNumReaders * kNumIterations)
      << "All read operations should complete";
  EXPECT_EQ(error_count, 0) << "No read errors should occur";
  EXPECT_EQ(shared_data_, write_count)
      << "Shared data should match number of write operations";
}
