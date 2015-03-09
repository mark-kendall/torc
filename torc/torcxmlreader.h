#ifndef TORCXMLREADER_H
#define TORCXMLREADER_H

// Qt
#include <QVariant>
#include <QMultiMap>
#include <QXmlStreamReader>

typedef QMultiMap<QString,QVariant> QVariantMultiMap;

class TorcXMLReader
{
  public:
    TorcXMLReader(const QString &File);
   ~TorcXMLReader();

    bool              IsValid     (QString &Message);
    QVariantMultiMap  GetResult   (void);

  private:
    bool              ReadXML     (void);
    bool              ReadElement (QVariantMultiMap *Map);

  private:
    QXmlStreamReader *m_reader;
    QVariantMultiMap  m_map;
    bool              m_valid;
    QString           m_message;
};

#endif // TORCXMLREADER_H
