#ifndef TORCHTMLSTATICCONTENT_H
#define TORCHTMLSTATICCONTENT_H

// Torc
#include "torchttphandler.h"

#define STATIC_DIRECTORY QString("/css,/img,/webfonts,/js")

class TorcHTMLStaticContent : public TorcHTTPHandler
{
  public:
    TorcHTMLStaticContent();

    void ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request) override;

  protected:
    static void GetJavascriptConfiguration  (TorcHTTPRequest &Request);

  private:
    QString m_pathToContent;
};

#endif // TORCHTMLSTATICCONTENT_H
