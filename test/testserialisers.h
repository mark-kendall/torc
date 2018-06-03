#ifndef TESTSERIALISERS_H
#define TESTSERIALISERS_H

#include <QObject>

class TestSerialisers : public QObject
{
    Q_OBJECT

  private slots:
    void testJSONSerialiser(void);
    void testXMLSerialiser(void);

  private:
    void doTestXMLSerialiser(QByteArray &Data);
};

#endif // TESTSERIALISERS_H
