/*
                        NoDB Project 
        Query Processing On Raw Data Files using PostgresRAW

                   Copyright (c) 2011-2013
  Data Intensive Applications and Systems Labaratory (DIAS)
           Ecole Polytechnique Federale de Lausanne

                     All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright notice
and this permission notice appear in all copies of the software, derivative
works or modified versions, and any portions thereof, and that both notices
appear in supporting documentation.

This code is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. THE AUTHORS AND ECOLE POLYTECHNIQUE FEDERALE DE LAUSANNE
DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
USE OF THIS SOFTWARE.
*/


#ifndef NODBCACHE_H_
#define NODBCACHE_H_


#include "postgres.h"
#include "noDB/auxiliary/NoDBRow.h"
#include "noDB/auxiliary/NoDBCol.h"
#include "noDB/auxiliary/NoDBPM.h"
#include "noDB/auxiliary/NoDBRelation.h"

typedef struct NoDBDatum_t {
    Datum   datum;
    bool    null;
} NoDBDatum_t;


typedef enum NoDBCacheType_t
{
    NODB_DATA_CACHE         = 0,
    NODB_POSITIONS_CACHE    = 1,
} NoDBCacheType_t;

typedef struct NoDBRowPartition_t {
   bool                         used;
   unsigned int                 size;
   struct NoDBRowPartition_t    *next;
} NoDBRowPartition_t;

#define NODB_CACHE_DEFAULT_NCOLUMNS 1

typedef struct NoDBCache_t {
    NoDBRelation_t              *rel;           // Relation.
    NoDBCacheType_t             type;           // Type of cache: data or positions.
    NoDBRow_t                   startRow;       // First (logical) row stored in the cache.
    NoDBRow_t                   usedRows;       // Number of used rows in the cache.
    NoDBRow_t                   nRows;          // Maximum number of rows that cache can hold.
    NoDBRow_t                   startOffset;    // Starting offset from ComputeOffset (startRow * rowSize).

    unsigned int                rowSize;        // Maximum size of each row (bytes).
    unsigned int                rowFree;        // Free space in each row (bytes).

    NoDBCol_t                   *cols;          // List of columns stored in cache.
    int                         nCols;          // Size of the colums array.
    int                         usedCols;       // Number of columns in use in the columns array.
    int                         *offset;        // List of cache offsets for each column.

    int                         *map;           // Map of columns to positions in the case.
    int                         nMap;           // Size of map.

    NoDBRowPartition_t          *list;          // List with memory usage per tuple.

    char                        *buf;           // Buffer.
} NoDBCache_t;


NoDBCache_t         *NoDBCacheInit(NoDBRelation_t *rel, NoDBCacheType_t type, NoDBRow_t startRow, unsigned int rowSize);
void                NoDBCacheDestroy(NoDBCache_t *cache);
NoDBCacheType_t     NoDBCacheGetType(NoDBCache_t *cache);
unsigned int        NoDBCacheGetFreeSize(NoDBCache_t *cache);
int                 NoDBCacheCanGrow(NoDBCache_t *cache, NoDBRow_t row);
int                 NoDBCacheAddColumn(NoDBCache_t *cache, NoDBCol_t col, unsigned int size);
void                NoDBCacheDeleteColumn(NoDBCache_t* cache, NoDBCol_t col, unsigned int size);        // FIXME: Why does Delete Column have the size???
int                 NoDBCacheHasColumn(NoDBCache_t *cache, NoDBCol_t col);
int                 NoDBCacheHasRow(NoDBCache_t *cache, NoDBRow_t row);
NoDBRelation_t      *NoDBCacheGetRelation(NoDBCache_t *cache);
NoDBRow_t           NoDBCacheGetBegin(NoDBCache_t *cache);
NoDBRow_t           NoDBCacheGetUsedRows(NoDBCache_t *cache);
void                NoDBCacheSetUsedRows(NoDBCache_t *cache, NoDBRow_t usedRows);
NoDBRow_t           NoDBCacheGetMaxRows(NoDBCache_t *cache);
void                NoDBCacheSetMaxRows(NoDBCache_t *cache, NoDBRow_t maxRows);
void                NoDBCacheGrow(NoDBCache_t *cache, NoDBRow_t row);
int                 NoDBCacheHit(NoDBCache_t *cache, NoDBRelation_t *rel, NoDBCol_t col, NoDBRow_t row);

char                *NoDBCacheGetOffset(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);

void                NoDBCacheSetChar(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, char val);
char                NoDBCacheGetChar(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetShortInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, short int val);
short int           NoDBCacheGetShortInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, int val);
int                 NoDBCacheGetInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetDouble(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, double val);
double              NoDBCacheGetDouble(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetBool(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, bool val);
bool                NoDBCacheGetBool(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetStringVal(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, char *val, int len);
char                *NoDBCacheGetStringVal(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetStringRef(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, char *val, int len);
char                *NoDBCacheGetStringRef(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetDatum(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, Datum val);
Datum               NoDBCacheGetDatum(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetDatumVector(NoDBCache_t *cache, Datum *datumVector, bool *null,  NoDBColVector_t cols, NoDBRow_t row);
void                NoDBCacheSetDatumRefVector(NoDBCache_t *cache, Datum *datumVector, bool *null, int2 *attlen, NoDBColVector_t cols, NoDBRow_t row);
void                NoDBCacheGetDatumVector(NoDBCache_t *cache, Datum *datumVector, bool *null, NoDBColVector_t cols, NoDBRow_t row);
void                NoDBCacheSetPMPair(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, NoDBPMPair_t val);
NoDBPMPair_t        NoDBCacheGetPMPair(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row);
void                NoDBCacheSetPMPairVector(NoDBCache_t *cache, NoDBPMPair_t *pairVector, NoDBColVector_t cols, NoDBRow_t row);
void                NoDBCacheGetPMPairVector(NoDBCache_t *cache, NoDBPMPair_t *pairVector, NoDBColVector_t cols, NoDBRow_t row);


/*** Just for debug ***/
void NoDBPrintCache(NoDBCache_t *cache);






#endif /* NODBCACHE_H_ */
