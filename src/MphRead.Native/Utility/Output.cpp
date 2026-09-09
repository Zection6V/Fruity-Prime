#include "Utility/output.hpp"

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

namespace fruityprime::utility::output {
namespace {

enum class Operation {
    Write,
    Read,
    Clear
};

struct QueueItem {
    Operation operation = Operation::Write;
    std::string message;
    std::size_t input_count = 0;
};

struct QueueInput {
    std::size_t count = 0;
    std::string message;
};

struct State {
    std::mutex mutex;
    std::condition_variable queue_changed;
    std::condition_variable input_changed;
    std::deque<QueueItem> items;
    std::deque<QueueInput> input;
    std::optional<BatchId> batch;
    BatchId next_batch = 1;
    bool initialized = false;
    bool stopping = false;
    std::thread worker;
};

State& state() {
    static State value;
    return value;
}

void wait_for_batch(std::unique_lock<std::mutex>& lock,
                    std::optional<BatchId> batch) {
    State& current = state();
    current.input_changed.wait(lock, [batch, &current] {
        return !current.batch.has_value()
            || (batch.has_value() && current.batch == batch);
    });
}

void run() {
    State& current = state();
    for (;;) {
        QueueItem item;
        {
            std::unique_lock lock(current.mutex);
            current.queue_changed.wait(lock, [&current] {
                return current.stopping || !current.items.empty();
            });
            if (current.stopping && current.items.empty()) {
                return;
            }
            item = std::move(current.items.front());
            current.items.pop_front();
        }

        switch (item.operation) {
        case Operation::Write:
            std::cout << item.message << '\n';
            std::cout.flush();
            break;
        case Operation::Clear:
            // This is understood by Windows Terminal, conhost with VT
            // enabled, and Unix terminals. It also avoids spawning a shell
            // just to implement Console.Clear.
            std::cout << "\x1b[2J\x1b[H";
            std::cout.flush();
            break;
        case Operation::Read: {
            std::cout << item.message;
            std::cout.flush();
            std::string value;
            if (!std::getline(std::cin, value)) {
                std::cin.clear();
            }
            {
                std::lock_guard lock(current.mutex);
                current.input.push_back(QueueInput{item.input_count,
                                                   std::move(value)});
            }
            current.input_changed.notify_all();
            break;
        }
        }
    }
}

void ensure_started() {
    State& current = state();
    std::lock_guard lock(current.mutex);
    if (current.initialized) {
        return;
    }
    current.stopping = false;
    current.initialized = true;
    current.worker = std::thread(run);
}

void enqueue(Operation operation, std::string message,
             std::optional<BatchId> batch, std::size_t input_count = 0) {
    ensure_started();
    State& current = state();
    {
        std::unique_lock lock(current.mutex);
        wait_for_batch(lock, batch);
        current.items.push_back(QueueItem{
            operation, std::move(message), input_count});
    }
    current.queue_changed.notify_one();
}

} // namespace

void Console::begin() {
    ensure_started();
}

BatchId Console::start_batch() {
    ensure_started();
    State& current = state();
    std::unique_lock lock(current.mutex);
    current.input_changed.wait(lock, [&current] {
        return !current.batch.has_value();
    });
    const BatchId result = current.next_batch++;
    // Do not hand out zero if the process has wrapped the token counter.
    if (current.next_batch == 0) {
        current.next_batch = 1;
    }
    current.batch = result;
    return result;
}

void Console::end_batch() {
    State& current = state();
    std::lock_guard lock(current.mutex);
    if (!current.initialized) {
        return;
    }
    current.batch.reset();
    current.input_changed.notify_all();
}

void Console::write(std::string_view message, std::optional<BatchId> batch) {
    enqueue(Operation::Write, std::string(message), batch);
}

void Console::clear(std::optional<BatchId> batch) {
    enqueue(Operation::Clear, {}, batch);
}

std::string Console::read(std::optional<std::string_view> message,
                          std::optional<BatchId> batch) {
    ensure_started();
    State& current = state();
    std::size_t input_count = 0;
    {
        std::unique_lock lock(current.mutex);
        wait_for_batch(lock, batch);
        input_count = current.input.size();
        current.items.push_back(QueueItem{
            Operation::Read,
            message.has_value() ? std::string(*message) : std::string{},
            input_count});
    }
    current.queue_changed.notify_one();

    std::unique_lock lock(current.mutex);
    current.input_changed.wait(lock, [input_count, &current] {
        return !current.input.empty()
            && current.input.front().count == input_count;
    });
    std::string result = std::move(current.input.front().message);
    current.input.pop_front();
    return result;
}

void Console::end() {
    State& current = state();
    std::thread worker;
    {
        std::lock_guard lock(current.mutex);
        if (!current.initialized) {
            return;
        }
        current.stopping = true;
        current.batch.reset();
        worker = std::move(current.worker);
        current.initialized = false;
    }
    current.input_changed.notify_all();
    current.queue_changed.notify_one();
    if (worker.joinable()) {
        worker.join();
    }
    std::lock_guard lock(current.mutex);
    current.stopping = false;
    current.items.clear();
    current.input.clear();
}

} // namespace fruityprime::utility::output
