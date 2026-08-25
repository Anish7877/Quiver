#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QWidget>
#include <QCloseEvent>
#include <QFile>

class MyWidget : public QWidget {
protected:
    void closeEvent(QCloseEvent *event) override {
        event->ignore();
        hide();
    }
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    MyWidget w;
    w.show();
    QTimer *timer = new QTimer();
    QObject::connect(timer, &QTimer::timeout, []() {
        QFile f("timer.log");
        if (f.open(QIODevice::Append)) {
            f.write("Fired\n");
            f.close();
        }
    });
    timer->start(1000);
    return a.exec();
}
