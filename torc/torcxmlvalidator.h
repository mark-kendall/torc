#ifndef TORCXMLVALIDATOR_H
#define TORCXMLVALIDATOR_H

// Qt
#include <QAbstractMessagehandler>

class TorcXmlValidator : public QAbstractMessageHandler
{
  public:
    TorcXmlValidator(const QString &XmlFile, const QString &XSDFile);
   ~TorcXmlValidator();

    bool Validated  (void);

  protected:
    void    handleMessage(QtMsgType Type, const QString &Description,
                          const QUrl &Identifier, const QSourceLocation &SourceLocation);

  private:
    QString m_xmlFile;
    QString m_xsdFile;
    bool    m_valid;
};

#endif // TORCXMLVALIDATOR_H
