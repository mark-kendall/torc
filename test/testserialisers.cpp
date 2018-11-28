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
    QByteArray data;
    jsonserialiser->Serialise(data, map);
    delete jsonserialiser;
    QJsonDocument doc2             = QJsonDocument::fromJson(data);
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
    QByteArray array2;
    xmlserialiser->Serialise(array2, map);
    delete xmlserialiser;
    QVERIFY(Data.size() == array2.size());
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
        QByteArray t = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + it.value() + "\n";
        doTestXMLSerialiser(t);
    }
}
