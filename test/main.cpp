// Qt
#include <QtTest/QtTest>
#include <QCoreApplication>

// Torc
#include "testserialisers.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    TestSerialisers testSerialisers;

    return QTest::qExec(&testSerialisers, argc, argv);
}
