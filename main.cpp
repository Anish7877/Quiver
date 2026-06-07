#include "include/MainWindow.h"
#include "include/AuthManager.h"
#include "include/AuthWindow.h"
#include <QApplication>
#include <QDialog>


auto main(int argc, char *argv[]) -> int {
    QApplication a(argc, argv);
    // // 1. Check if the user is already logged in
    // if (!Quiver::AuthManager::get_instance().is_logged_in()) {
        
    //     // 2. Not logged in? Show the beautiful Auth Window!
    //     Quiver::AuthWindow auth_win;
        
    //     // 3. If they click the 'X' to close the login window, exit the app entirely.
    //     if (auth_win.exec() != QDialog::Accepted) {
    //         return 0;
    //     }
    // }
    Quiver::MainWindow w {};
    w.show();

    return a.exec();
}
