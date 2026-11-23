#include "chat_message.h"
#include "conn_mq.h"
#include "process_manager.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <mqueue.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void require(bool condition, std::string_view what) {
    if (!condition) {
        throw std::runtime_error(std::string(what));
    }
}

int client_main(void* context) {
    auto& sync = *static_cast<SharedSemaphores*>(context);

    // Wait until the host creates the queues.
    if (!sync.wait_for_client()) {
        return 1;
    }

    // Open queues in the child, after fork().
    MqConnection client(getppid(), false);

    sync.notify_host();

    // Wait for the host to send a message.
    if (!sync.wait_for_client()) {
        return 2;
    }

    std::vector<std::uint8_t> frame;

    if (!client.read(frame)) {
        return 3;
    }

    const auto incoming = chat::deserialize(frame);

    if (!incoming ||
        incoming->sender != chat::Participant::Host ||
        incoming->text != "Hello from host") {
        return 4;
    }

    const chat::ChatMessage reply{
        chat::MessageKind::Private,
        chat::Participant::Client,
        chat::Participant::Host,
        456,
        "Hello from client"
    };

    const auto bytes = chat::serialize(reply);

    if (!bytes || !client.write(*bytes)) {
        return 5;
    }

    sync.notify_host();

    return 0;
}

void test_exchange() {
    SharedSemaphores sync;

    const auto host_pid = getpid();

    const std::string queue_name =
        "/lab2_mq_" + std::to_string(host_pid) + "_h2c";

    {
        auto child = ChildProcess::spawn(client_main, &sync);

        // Host creates queues after fork().
        MqConnection host(host_pid, true);

        sync.notify_client();

        require(
            sync.wait_for_host(),
            "client must open queues"
        );

        const chat::ChatMessage request{
            chat::MessageKind::Broadcast,
            chat::Participant::Host,
            chat::Participant::All,
            123,
            "Hello from host"
        };

        const auto bytes = chat::serialize(request);

        require(
            bytes && host.write(*bytes),
            "host sends message"
        );

        sync.notify_client();

        require(
            sync.wait_for_host(),
            "client must send reply"
        );

        std::vector<std::uint8_t> received;

        require(
            host.read(received),
            "host receives reply"
        );

        const auto reply = chat::deserialize(received);

        require(
            reply &&
            reply->sender == chat::Participant::Client &&
            reply->text == "Hello from client",
            "valid response"
        );

        require(
            !host.write(
                std::vector<std::uint8_t>(1043, 0)
            ),
            "reject oversized frame"
        );

        const int status = child.wait();

        require(
            WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "child must exit successfully"
        );
    }

    // Queue names must be removed when the host exits.
    errno = 0;

    const auto descriptor = mq_open(
        queue_name.c_str(),
        O_RDONLY
    );

    const int open_error = errno;

    if (descriptor != static_cast<mqd_t>(-1)) {
        mq_close(descriptor);
    }

    require(
        descriptor == static_cast<mqd_t>(-1) &&
        open_error == ENOENT,
        "host must remove queue names"
    );
}

} // namespace

int main() {
    try {
        test_exchange();

        std::cout << "MQ integration test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MQ integration test failed: "
                  << error.what() << '\n';
        return 1;
    }
}