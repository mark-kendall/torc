#ifndef TORCDIRECTORIES_H
#define TORCDIRECTORIES_H

// Qt
#include <QString>

// Torc
#include "torccommandline.h"

void      InitialiseTorcDirectories (TorcCommandLine* CommandLine);
QString   GetTorcConfigDir          (void);
QString   GetTorcShareDir           (void);
QString   GetTorcTransDir           (void);
QString   GetTorcContentDir         (void);

#endif // TORCDIRECTORIES_H
