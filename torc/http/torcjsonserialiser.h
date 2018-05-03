#ifndef TORCJSONSERIALISER_H
#define TORCJSONSERIALISER_H

// Qt
#include <QTextStream>

// Torc
#include "torcserialiser.h"

class TorcJSONSerialiser : public TorcSerialiser
{
  public:
    explicit TorcJSONSerialiser(bool Javascript = false);
    virtual ~TorcJSONSerialiser();

    HTTPResponseType ResponseType       (void) Q_DECL_OVERRIDE;

  protected:
    void             Prepare            (void) Q_DECL_OVERRIDE;
    void             Begin              (void) Q_DECL_OVERRIDE;
    void             AddProperty        (const QString &Name, const QVariant &Value) Q_DECL_OVERRIDE;
    void             End                (void) Q_DECL_OVERRIDE;

  private:
    bool             m_javaScriptType;
};

#endif // TORCJSONSERIALISER_H
