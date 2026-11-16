// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 2 || argc > 3 || !app.primaryScreen()) return 2;
    const WId window = argc == 3 ? QString::fromLocal8Bit(argv[2]).toULongLong(nullptr, 0) : 0;
    return app.primaryScreen()->grabWindow(window).save(QString::fromLocal8Bit(argv[1])) ? 0 : 3;
}
