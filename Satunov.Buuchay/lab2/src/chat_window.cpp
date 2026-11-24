#include "chat_window.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <csignal>
#include <exception>
#include <utility>

ChatWindow::ChatWindow(
    chat::ChatEndpoint& endpoint,
    ChildProcess& child,
    std::atomic<bool>& connected,
    std::mutex& error_mutex,
    std::string& error_text,
    QWidget* parent
)
    : QWidget(parent),
      endpoint_(endpoint),
      child_(child),
      connected_(connected),
      error_mutex_(error_mutex),
      error_text_(error_text) {

    setWindowTitle("IPC Chat - Variant 13");
    resize(720, 520);

    auto* layout = new QVBoxLayout(this);

    status_label_ = new QLabel(this);

    history_ = new QPlainTextEdit(this);
    history_->setReadOnly(true);

    events_ = new QPlainTextEdit(this);
    events_->setReadOnly(true);
    events_->setFixedHeight(95);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText("Введите сообщение...");

    message_type_ = new QComboBox(this);
    message_type_->addItem("Общее");
    message_type_->addItem("Личное");

    send_button_ = new QPushButton("Отправить", this);

    auto* controls = new QHBoxLayout();
    controls->addWidget(message_type_);
    controls->addWidget(input_, 1);
    controls->addWidget(send_button_);

    layout->addWidget(status_label_);
    layout->addWidget(new QLabel("Сообщения:", this));
    layout->addWidget(history_, 1);
    layout->addLayout(controls);
    layout->addWidget(new QLabel("События:", this));
    layout->addWidget(events_);

    connect(
        send_button_,
        &QPushButton::clicked,
        this,
        [this] { send_message(); }
    );

    connect(
        input_,
        &QLineEdit::returnPressed,
        this,
        [this] { send_message(); }
    );

    auto* timer = new QTimer(this);

    connect(
        timer,
        &QTimer::timeout,
        this,
        [this] { refresh(); }
    );

    timer->start(200);

    set_status("Хост запущен. Клиент подключён.");
    refresh_history();
}

void ChatWindow::set_status(const QString& message) {
    status_label_->setText(message);

    const QString time =
        QDateTime::currentDateTime().toString("HH:mm:ss");

    events_->appendPlainText(
        QString("[%1] %2").arg(time, message)
    );
}

void ChatWindow::disconnect_client(const QString& reason) {
    connected_.store(false);

    if (disconnected_shown_) {
        return;
    }

    disconnected_shown_ = true;

    input_->setEnabled(false);
    message_type_->setEnabled(false);
    send_button_->setEnabled(false);

    set_status(reason);
}

void ChatWindow::send_message() {
    if (!connected_.load()) {
        return;
    }

    const QByteArray text = input_->text().toUtf8();

    if (text.isEmpty()) {
        return;
    }

    const auto kind =
        message_type_->currentIndex() == 0
            ? chat::MessageKind::Broadcast
            : chat::MessageKind::Private;

    try {
        const std::string message(
            text.constData(),
            static_cast<std::size_t>(text.size())
        );

        if (!endpoint_.send(kind, message)) {
            set_status(
                "Сообщение не отправлено. "
                "Проверьте длину и состояние соединения."
            );
            return;
        }

        input_->clear();
        refresh_history();

    } catch (const std::exception& error) {
        disconnect_client(
            QString("Ошибка отправки: %1")
                .arg(QString::fromUtf8(error.what()))
        );
    }
}

void ChatWindow::refresh_history() {
    const auto messages = endpoint_.history();

    if (messages.size() == rendered_count_) {
        return;
    }

    rendered_count_ = messages.size();
    history_->clear();

    for (const auto& message : messages) {
        const QString time =
            QDateTime::fromMSecsSinceEpoch(
                static_cast<qint64>(message.sent_at_ms)
            ).toString("HH:mm:ss");

        const QString sender =
            message.sender == chat::Participant::Host
                ? "Хост"
                : "Клиент";

        const QString kind =
            message.kind == chat::MessageKind::Private
                ? "личное"
                : "общее";

        const QString line =
            QString("[%1] %2 (%3): %4")
                .arg(time)
                .arg(sender)
                .arg(kind)
                .arg(QString::fromStdString(message.text));

        history_->appendPlainText(line);
    }

    auto* scrollbar = history_->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void ChatWindow::refresh() {
    refresh_history();

    if (connected_.load() &&
        endpoint_.client_timed_out()) {

        try {
            child_.terminate(SIGKILL);

            disconnect_client(
                "Клиент завершён через SIGKILL: "
                "60 секунд без сообщений."
            );

        } catch (const std::exception& error) {
            disconnect_client(
                QString("Ошибка завершения клиента: %1")
                    .arg(QString::fromUtf8(error.what()))
            );
        }
    }

    if (child_.pid() > 0) {
        try {
            const auto status = child_.try_wait();

            if (status.has_value()) {
                disconnect_client(
                    "Клиентский процесс завершился."
                );
            }

        } catch (const std::exception& error) {
            disconnect_client(
                QString("Ошибка waitpid: %1")
                    .arg(QString::fromUtf8(error.what()))
            );
        }
    }

    if (!connected_.load() && !disconnected_shown_) {
        std::string error;

        {
            std::lock_guard lock(error_mutex_);
            error = error_text_;
        }

        disconnect_client(
            error.empty()
                ? QString("Соединение закрыто.")
                : QString("Ошибка соединения: %1")
                    .arg(QString::fromStdString(error))
        );
    }
}