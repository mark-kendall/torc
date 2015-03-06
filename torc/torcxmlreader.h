#ifndef TORCXMLREADER_H
#define TORCXMLREADER_H

// Qt
#include <QVariant>
#include <QXmlStreamReader>

class TorcXMLReader
{
  public:
    TorcXMLReader(const QString &File);
   ~TorcXMLReader();

    bool              IsValid     (QString &Message);
    QVariantMap       GetResult   (void);

  private:
    bool              ReadXML     (void);
    bool              ReadElement (QVariantMap *Map);

  private:
    QXmlStreamReader *m_reader;
    QVariantMap       m_map;
    bool              m_valid;
    QString           m_message;
};

#endif // TORCXMLREADER_H
