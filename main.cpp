// #include <QApplication>
// #include "include/MainWindow.h"

// int main(int argc, char *argv[])
// {
//     QApplication app(argc, argv);
    
//     // Optional: Set Application Font
//     QFont font("Segoe UI");
//     font.setStyleHint(QFont::SansSerif);
//     app.setFont(font);

//     MainWindow window;
//     window.show();

//     return app.exec();
// }

#include "include/MainWindow.h"
#include <QApplication>


auto main(int argc, char *argv[]) -> int {
    QApplication a(argc, argv);

    Quiver::MainWindow w {};
    w.show();

    return a.exec();
}
