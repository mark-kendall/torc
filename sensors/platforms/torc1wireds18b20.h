#ifndef TORC1WIREDS18B20_H
#define TORC1WIREDS18B20_H

// Torc
#include "../torctemperaturesensor.h"

#define DS18B20NAME QString("DS18B20")

class Torc1WireDS18B20 : public TorcTemperatureSensor
{
  public:
    Torc1WireDS18B20(const QString &UniqueId);
    ~Torc1WireDS18B20();
};

#endif // TORC1WIREDS18B20_H
