#ifndef TORCHTMLSTATICCONTENT_H
#define TORCHTMLSTATICCONTENT_H

// Torc
#include "torchttphandler.h"

class TorcHTMLStaticContent : public TorcHTTPHandler
{
  public:
    TorcHTMLStaticContent();

    void ProcessHTTPRequest          (TorcHTTPRequest *Request, TorcHTTPConnection* Connection);

  protected:
    static void GetJavascriptConfiguration  (TorcHTTPRequest *Request, TorcHTTPConnection* Connection);

  private:
    QString m_pathToContent;
};

#endif // TORCHTMLSTATICCONTENT_H
