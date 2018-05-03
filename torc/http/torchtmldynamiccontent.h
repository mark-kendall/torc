#ifndef TORCHTMLDYNAMICCONTENT_H
#define TORCHTMLDYNAMICCONTENT_H

// Torc
#include "torchttphandler.h"

class TorcHTMLDynamicContent : public TorcHTTPHandler
{
  public:
    TorcHTMLDynamicContent();
    void ProcessHTTPRequest          (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest *Request) Q_DECL_OVERRIDE;

  private:
    QString m_pathToContent;
};

#endif // TORCHTMLDYNAMICCONTENT_H
