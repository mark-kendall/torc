#ifndef TORCHTMLDYNAMICCONTENT_H
#define TORCHTMLDYNAMICCONTENT_H

// Torc
#include "torchttphandler.h"

class TorcHTMLDynamicContent : public TorcHTTPHandler
{
  public:
    TorcHTMLDynamicContent();
    void ProcessHTTPRequest          (TorcHTTPRequest *Request, TorcHTTPConnection*);
};

#endif // TORCHTMLDYNAMICCONTENT_H
