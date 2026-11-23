#pragma once

#include <sys/types.h>

class SharedSemaphores {
public:
    SharedSemaphores();
    ~SharedSemaphores();

    SharedSemaphores(const SharedSemaphores&) = delete;
    SharedSemaphores& operator=(const SharedSemaphores&) = delete;

    void notify_host() const;
    void notify_client() const;

    [[nodiscard]] bool wait_for_host() const;
    [[nodiscard]] bool wait_for_client() const;

private:
    struct State;

    State* state_;
    pid_t creator_pid_;
};

class ChildProcess {
public:
    using Entry = int (*)(void*);

    static ChildProcess spawn(Entry entry, void* context);

    ~ChildProcess();

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    ChildProcess(ChildProcess&& other) noexcept;
    ChildProcess& operator=(ChildProcess&&) = delete;

    [[nodiscard]] pid_t pid() const noexcept;

    void terminate(int signal_number) const;

    [[nodiscard]] int wait();

private:
    explicit ChildProcess(pid_t pid) noexcept;

    pid_t pid_;
};