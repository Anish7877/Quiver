#include <QString>
#include <QDebug>
#include <QStringList>

int main() {
    QString output = "Starting container...\n1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    QStringList lines = output.split('\n');
    QString container_id = lines.last().trimmed();
    qDebug() << "Captured ID:" << container_id;
    return 0;
}
