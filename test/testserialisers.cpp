// Qt
#include <QtTest/QtTest>
#include <QJsonDocument>

// Torc
#include "torcjsonserialiser.h"
#include "torcxmlreader.h"
#include "testserialisers.h"

void TestSerialisers::testJSONSerialiser(void)
{
    QByteArray array("[\"number\",\"name\"]");
    QJsonDocument doc              = QJsonDocument::fromJson(array);
    QVariant      map              = doc.toVariant();
    TorcSerialiser* jsonserialiser = TorcSerialiser::GetSerialiser("application/json");
    QVERIFY(jsonserialiser);
    QByteArray *data               = jsonserialiser->Serialise(map, "");
    delete jsonserialiser;
    QVERIFY(data);
    QJsonDocument doc2             = QJsonDocument::fromJson(*data);
    delete data;
    QByteArray array2              = doc2.toJson(QJsonDocument::Compact).trimmed();

    QVERIFY(array.size() == array2.size());
    QVERIFY(qstrcmp(array.constData(), array2.constData()) == 0);
    QVERIFY(doc2 == doc);
    QVERIFY(array2 == array);
}

void TestSerialisers::doTestXMLSerialiser(QByteArray &Data)
{
    TorcXMLReader reader(Data);
    QString error;
    QVERIFY(reader.IsValid(error));
    QVariantMap map = reader.GetResult();
    TorcSerialiser *xmlserialiser = TorcSerialiser::GetSerialiser("application/xml");
    QVERIFY(xmlserialiser);
    QByteArray *array2 = xmlserialiser->Serialise(map, "");
    delete xmlserialiser;
    QVERIFY(array2);
    qWarning(Data.constData());
    qWarning(array2->constData());
    qWarning(QString("data %1 array2 %2").arg(Data.size()).arg(array2->size()).toLocal8Bit().constData());
    QVERIFY(Data.size() == array2->size());
    delete array2;
}

void TestSerialisers::testXMLSerialiser(void)
{
    QMap<QString,QByteArray> tests;
    tests.insert("Simple1", "<list><value>name</value><value>number</value></list>");
    tests.insert("Simple2", "<people><person><name>Jon</name><age>12</age></person></people>");
    tests.insert("Simple3", "<list><value>1</value><value>2</value><value>3</value></list>");
    tests.insert("Simple4", "<person>me</person><person>you</person>");

    QMap<QString,QByteArray>::iterator it = tests.begin();
    for ( ; it != tests.end(); ++it)
    {
        qInfo(QString("XML test: %1").arg(it.key()).toLocal8Bit().constData());
        QByteArray t = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + it.value() + "\n";
        doTestXMLSerialiser(t);
    }
}
