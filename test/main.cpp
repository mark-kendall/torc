// Qt
#include <QtTest/QtTest>
#include <QCoreApplication>

// Torc
#include "testserialisers.h"
#include "testtorclocalcontext.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    TestSerialisers testSerialisers;
    TestTorcLocalContext testLocalContext;
    int status = QTest::qExec(&testSerialisers);
    status    |= QTest::qExec(&testLocalContext);
    return status;
}
