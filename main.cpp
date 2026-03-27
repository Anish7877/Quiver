#include "include/MainWindow.h"
#include <QApplication>


auto main(int argc, char *argv[]) -> int {
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/assets/icons/logo.png"));
    Quiver::MainWindow w {};
    w.show();

    return a.exec();
}
