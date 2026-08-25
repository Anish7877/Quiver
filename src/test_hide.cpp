#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    
    QTimer *timer = new QTimer();
    QObject::connect(timer, &QTimer::timeout, []() {
        qDebug() << "Timer fired!";
    });
    timer->start(1000);
    
    QTimer::singleShot(3000, [&]() {
        a.quit();
    });
    
    return a.exec();
}
