#include "include/MainWindow.h"
#include <QApplication>


auto main(int argc, char *argv[]) -> int {
    QApplication a(argc, argv);

    Quiver::MainWindow w {};
    w.show();

    return a.exec();
}
