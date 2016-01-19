#ifndef TORCHTMLDYNAMICCONTENT_H
#define TORCHTMLDYNAMICCONTENT_H

// Torc
#include "torchttphandler.h"

class TorcHTMLDynamicContent : public TorcHTTPHandler
{
  public:
    TorcHTMLDynamicContent();
    void ProcessHTTPRequest          (TorcHTTPRequest *Request, TorcHTTPConnection*);

  private:
    QString m_pathToContent;
};

#endif // TORCHTMLDYNAMICCONTENT_H
