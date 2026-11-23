#pragma once

#include "conn.h"

#include <chrono>
#include <string>
#include <sys/types.h>

class SocketConnection final : public Conn {
public:
    SocketConnection(pid_t host_pid, bool create);
    ~SocketConnection() override;

    SocketConnection(const SocketConnection&) = delete;
    SocketConnection& operator=(const SocketConnection&) = delete;

    [[nodiscard]] bool read(
        std::vector<std::uint8_t>& frame
    ) override;

    [[nodiscard]] bool write(
        std::span<const std::uint8_t> frame
    ) override;

private:
    using Deadline = std::chrono::steady_clock::time_point;

    [[nodiscard]] bool ensure_connected(Deadline deadline);

    void cleanup() noexcept;
    void disconnect() noexcept;

    std::string socket_path_;
    int listen_fd_{-1};
    int connection_fd_{-1};
    bool owns_path_{false};
};