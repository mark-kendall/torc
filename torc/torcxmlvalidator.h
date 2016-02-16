#ifndef TORCXMLVALIDATOR_H
#define TORCXMLVALIDATOR_H

// Qt
#include <QAbstractMessageHandler>

class TorcXmlValidator : public QAbstractMessageHandler
{
  public:
    TorcXmlValidator(const QString &XmlFile, const QString &XSDFile, bool Silent = false);
    TorcXmlValidator(const QString &XmlFile, const QByteArray &XSDData, bool Silent = false);
   ~TorcXmlValidator();

    bool       Validated     (void);

  protected:
    void       Validate      (void);
    void       handleMessage (QtMsgType Type, const QString &Description,
                             const QUrl &Identifier, const QSourceLocation &SourceLocation);

  private:
    QString    m_xmlFile;
    QString    m_xsdFile;
    QByteArray m_xsdData;
    bool       m_valid;
    bool       m_xsdDone;
    bool       m_silent;
};

#endif // TORCXMLVALIDATOR_H
