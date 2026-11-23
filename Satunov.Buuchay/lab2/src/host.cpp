#include "transport_name.h"

#include <QApplication>
#include <QLabel>
#include <QString>

#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--transport") {
        std::cout << "Transport: " << transport_name() << '\n';
        return EXIT_SUCCESS;
    }

    QApplication app(argc, argv);

    QLabel window(
        QString("Lab 2 - Variant 13\n"
                "Transport: %1\n"
                "IPC is not implemented yet.")
            .arg(QString::fromUtf8(transport_name()))
    );

    window.setWindowTitle("IPC Chat");
    window.resize(420, 160);
    window.show();

    return app.exec();
}