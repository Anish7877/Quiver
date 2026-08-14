#include <QProcess>
#include <QStringList>
#include <QDebug>
int main() {
    QString str = "bash -c \"while true; do echo; done\"";
    QStringList args = QProcess::splitCommand(str);
    qDebug() << args;
    return 0;
}
