#ifndef TORCSQLITEDB_H
#define TORCSQLITEDB_H

#include "torcdb.h"

class TorcSQLiteDB : public TorcDB
{
  public:
    explicit TorcSQLiteDB(const QString &DatabaseName);

  protected:
    bool InitDatabase(void) override;
};

#endif // TORCSQLITEDB_H
