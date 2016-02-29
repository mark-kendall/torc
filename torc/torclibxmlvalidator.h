#ifndef TORCLIBXMLVALIDATOR_H
#define TORCLIBXMLVALIDATOR_H

// Qt
#include <QString>

class TorcXmlValidator
{
  public:
    TorcXmlValidator(const QString &XmlFile, const QString &XSDFile,    bool Silent = false);
    TorcXmlValidator(const QString &XmlFile, const QByteArray &XSDData, bool Silent = false);
    ~TorcXmlValidator();

    bool        GetSilent     (void);
    bool        Validated     (void);
    static void HandleMessage (void *ctx, const char *format, ...);

  protected:
    void        Validate      (void);

  private:
    QString     m_xmlFile;
    QString     m_xsdFile;
    QByteArray  m_xsdData;
    bool        m_valid;
    bool        m_xsdDone;
    bool        m_silent;
};

#endif // TORCLIBXMLVALIDATOR_H
