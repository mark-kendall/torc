#ifndef TORCSQLITEDB_H
#define TORCSQLITEDB_H

#include "torcdb.h"

class TorcSQLiteDB : public TorcDB
{
  public:
    explicit TorcSQLiteDB(const QString &DatabaseName);

  protected:
    bool InitDatabase(void) Q_DECL_OVERRIDE;
};

#endif // TORCSQLITEDB_H
