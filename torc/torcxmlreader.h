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
    explicit TorcXMLReader(const QString &File);
   ~TorcXMLReader();

    bool              IsValid     (QString &Message) const;
    QVariantMultiMap  GetResult   (void) const;

  private:
    bool              ReadXML     (void);
    bool              ReadElement (QVariantMultiMap *Map);

  private:
    QXmlStreamReader *m_reader;
    QVariantMultiMap  m_map;
    bool              m_valid;
    QString           m_message;

  private:
    // disable copy and assignment constructors
    TorcXMLReader(const TorcXMLReader &) Q_DECL_EQ_DELETE;
    TorcXMLReader &operator=(const TorcXMLReader &) Q_DECL_EQ_DELETE;
};

#endif // TORCXMLREADER_H
