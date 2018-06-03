#ifndef TORCXMLREADER_H
#define TORCXMLREADER_H

// Qt
#include <QVariant>
#include <QXmlStreamReader>

class TorcXMLReader
{
  public:
    explicit TorcXMLReader(const QString &File);
    explicit TorcXMLReader(QByteArray &Data);
   ~TorcXMLReader();

    bool              IsValid     (QString &Message) const;
    QVariantMap       GetResult   (void) const;

  private:
    bool              ReadXML     (void);
    bool              ReadElement (QVariantMap &Map);

  private:
    QXmlStreamReader  m_reader;
    QVariantMap       m_map;
    bool              m_valid;
    QString           m_message;
};

#endif // TORCXMLREADER_H
