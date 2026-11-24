#include "chat_endpoint.h"
#include "chat_window.h"
#include "connection_factory.h"
#include "process_manager.h"
#include "transport_name.h"

#include <QApplication>

#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

namespace {

struct ChildContext {
    SharedSemaphores* sync;
    bool self_test;
};

int child_main(void* raw_context) {
    auto& context = *static_cast<ChildContext*>(raw_context);

    try {
        if (!context.sync->wait_for_client()) {
            return 11;
        }

        // The client opens its IPC resources after fork().
        auto connection = make_connection(getppid(), false);

        chat::ChatEndpoint endpoint(
            *connection,
            chat::Participant::Client
        );

        context.sync->notify_host();

        if (!endpoint.send(
                chat::MessageKind::Broadcast,
                "Client connected"
            )) {
            return 12;
        }

        if (context.self_test) {
            const auto request = endpoint.receive();

            if (!request || request->text != "ping") {
                return 13;
            }

            return endpoint.send(
                chat::MessageKind::Private,
                "pong"
            ) ? 0 : 14;
        }

        std::atomic<bool> running{true};
        std::mutex output_mutex;

        // Incoming messages are received independently
        // of terminal input.
        std::jthread receiver(
            [&](std::stop_token stop) {
                while (!stop.stop_requested() &&
                       running.load()) {
                    try {
                        const auto message = endpoint.receive();

                        if (!message) {
                            continue;
                        }

                        std::lock_guard lock(output_mutex);

                        const char* kind =
                            message->kind ==
                                chat::MessageKind::Private
                                ? "private"
                                : "broadcast";

                        std::cout
                            << "\n[Host][" << kind << "] "
                            << message->text
                            << "\n> " << std::flush;

                    } catch (const std::exception& error) {
                        std::lock_guard lock(output_mutex);

                        std::cerr
                            << "\nConnection error: "
                            << error.what()
                            << '\n';

                        running.store(false);
                        break;
                    }
                }
            }
        );

        {
            std::lock_guard lock(output_mutex);

            std::cout
                << "\nClient connected.\n"
                << "Write a message to send a broadcast.\n"
                << "Use /private MESSAGE for a private message.\n"
                << "Use /quit to exit.\n"
                << "> " << std::flush;
        }

        std::string line;

        while (running.load() && std::getline(std::cin, line)) {
            if (line == "/quit") {
                break;
            }

            auto kind = chat::MessageKind::Broadcast;

            if (line.starts_with("/private ")) {
                kind = chat::MessageKind::Private;
                line.erase(0, 9);
            }

            if (line.empty()) {
                continue;
            }

            const bool sent = endpoint.send(
                kind,
                std::move(line)
            );

            std::lock_guard lock(output_mutex);

            if (!sent) {
                std::cout
                    << "Message was not sent.\n";
            }

            std::cout << "> " << std::flush;
        }

        running.store(false);
        receiver.request_stop();

        return 0;

    } catch (const std::exception& error) {
        std::cerr
            << "Client error: "
            << error.what()
            << '\n';

        return 20;
    }
}

int run_host(int argc, char* argv[], bool self_test) {
    // Create shared semaphores before fork().
    SharedSemaphores sync;

    ChildContext context{&sync, self_test};

    // Fork before creating QApplication or starting threads.
    auto child = ChildProcess::spawn(
        child_main,
        &context
    );

    // The host creates transport resources after fork().
    auto connection = make_connection(getpid(), true);

    sync.notify_client();

    if (!sync.wait_for_host()) {
        throw std::runtime_error(
            "client connection initialization timed out"
        );
    }

    chat::ChatEndpoint endpoint(
        *connection,
        chat::Participant::Host
    );

    // Bootstrap message also establishes the accepted
    // connection for the Unix domain socket transport.
    const auto greeting = endpoint.receive();

    if (!greeting ||
        greeting->sender != chat::Participant::Client ||
        greeting->text != "Client connected") {
        throw std::runtime_error(
            "client handshake message was not received"
        );
    }

    if (self_test) {
        if (!endpoint.send(
                chat::MessageKind::Broadcast,
                "ping"
            )) {
            throw std::runtime_error(
                "self-test ping failed"
            );
        }

        const auto reply = endpoint.receive();
        const int status = child.wait();

        if (!reply ||
            reply->text != "pong" ||
            !WIFEXITED(status) ||
            WEXITSTATUS(status) != 0) {
            throw std::runtime_error(
                "IPC end-to-end test failed"
            );
        }

        std::cout
            << "IPC self-test passed: "
            << transport_name()
            << '\n';

        return 0;
    }

    // QApplication is initialized only in the host,
    // after the child has already been created.
    QApplication app(argc, argv);

    std::atomic<bool> connected{true};
    std::mutex error_mutex;
    std::string error_text;

    // Blocking IPC reads must not freeze the Qt event loop.
    std::jthread receiver(
        [&](std::stop_token stop) {
            while (!stop.stop_requested() &&
                   connected.load()) {
                try {
                    (void)endpoint.receive();

                } catch (const std::exception& error) {
                    {
                        std::lock_guard lock(error_mutex);
                        error_text = error.what();
                    }

                    connected.store(false);
                    break;
                }
            }
        }
    );

    ChatWindow window(
        endpoint,
        child,
        connected,
        error_mutex,
        error_text
    );

    window.show();

    const int result = app.exec();

    connected.store(false);

    if (child.pid() > 0) {
        child.terminate(SIGKILL);
    }

    receiver.request_stop();
    receiver.join();

    return result;
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string_view mode =
        argc == 2 ? std::string_view(argv[1])
                  : std::string_view{};

    if (mode == "--transport") {
        std::cout
            << "Transport: "
            << transport_name()
            << '\n';

        return 0;
    }

    try {
        return run_host(
            argc,
            argv,
            mode == "--self-test"
        );

    } catch (const std::exception& error) {
        std::cerr
            << "Host error: "
            << error.what()
            << '\n';

        return 1;
    }
}