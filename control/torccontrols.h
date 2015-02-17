#ifndef TORCCONTROLS_H
#define TORCCONTROLS_H


// Torc
#include "torccentral.h"
#include "torccontrol.h"

class TorcControls : public TorcDeviceHandler
{
  public:
    TorcControls();
   ~TorcControls();

    static TorcControls* gControls;

    void Create   (const QVariantMap &Details);
    void Destroy  (void);
    void Validate (void);
    void Graph    (void);

  private:
    QMap<QString,TorcControl*> m_controls;
};

#endif // TORCCONTROLS_H
