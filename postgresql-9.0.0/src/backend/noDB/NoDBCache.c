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


//#include "postgres.h"
#include "noDB/NoDBCache.h"
#include "noDB/auxiliary/NoDBMalloc.h"
#include <math.h>


NoDBCache_t *NoDBCacheInit(NoDBRelation_t *rel, NoDBCacheType_t type, NoDBRow_t startRow, unsigned int rowSize)
{
    NoDBCache_t *cache;
    int         i;

    cache = NoDBMalloc(sizeof(NoDBCache_t));
    cache->rel = rel;
    cache->type = type;
    cache->startRow = startRow;
    cache->usedRows = 0;
    cache->startOffset = rowSize * startRow;

    cache->rowSize = rowSize;
    cache->rowFree = rowSize;

    cache->nCols = NODB_CACHE_DEFAULT_NCOLUMNS;
    cache->cols = NoDBMalloc(cache->nCols * sizeof(unsigned int));
    for (i = 0; i < cache->nCols; i++) {
        cache->cols[i] = -1;
    }

    cache->offset = NoDBMalloc(NoDBRelationGetNumberColumns(rel) * sizeof(unsigned int));
    for (i = 0; i < NoDBRelationGetNumberColumns(rel); i++) {
        cache->offset[i] = -1;
    }

    cache->usedCols = 0;

    cache->map = NULL;
    cache->nMap = 0;

    cache->list = NoDBMalloc(sizeof(NoDBRowPartition_t));
    cache->list->size = rowSize;
    cache->list->used = 0;
    cache->list->next = NULL;

    cache->buf = NULL;

    return cache;
}

void NoDBCacheDestroy(NoDBCache_t *cache)
{
    NoDBRowPartition_t *cur;

    cur = cache->list;
    while (cur) {
        NoDBRowPartition_t *next;

        next = cur->next;
        NoDBFree(cur);
        cur = next;
    }

    NoDBFree(cache->buf);
    NoDBFree(cache->cols);
    NoDBFree(cache->offset);
    NoDBFree(cache->map);
    NoDBFree(cache);
}

NoDBCacheType_t NoDBCacheGetType(NoDBCache_t *cache)
{
    return cache->type;
}

unsigned int NoDBCacheGetFreeSize(NoDBCache_t *cache)
{
    return cache->rowFree;
}

int NoDBCacheCanGrow(NoDBCache_t *cache, NoDBRow_t row)
{
    return (cache->startRow <= row);
}

static NoDBRowPartition_t *FindFirstPartitionFit(NoDBRowPartition_t *list, unsigned int size, int *offset)
{
    NoDBRowPartition_t *fit;
    NoDBRowPartition_t *cur;

    *offset = 0;
    fit = NULL;
    cur = list;
    while (cur) {
        if (cur->size >= size && !cur->used) {
            fit = cur;
            break;
        }
        *offset += cur->size;
        cur = cur->next;
    }
    return fit;
}

static void SplitPartition(NoDBRowPartition_t* p1, int size)
{
    NoDBRowPartition_t *p2;

    p2 = NoDBMalloc(sizeof(NoDBRowPartition_t));
    p2->size = p1->size - size;
    p2->used = 0;
    p2->next = p1->next;

    p1->size = size;
    p1->next = p2;
}

int NoDBCacheAddColumn(NoDBCache_t* cache, NoDBCol_t col, unsigned int size)
{
    NoDBRowPartition_t  *fit;
    int                 offset;

    // No space for the new attribute.
    if (cache->rowFree < size) {
        return 0;
    }

    // Resize the map if needed.
    if (col >= cache->nMap) {
        int i;
        int old;

        old = cache->nMap;
        cache->nMap = col + 1;

        if (cache->map) {
            cache->map = NoDBRealloc(cache->map, cache->nMap * sizeof(int));
        } else {
            cache->map = NoDBMalloc(cache->nMap * sizeof(int));
        }
        for (i = old; i < cache->nMap; i++) {
            cache->map[i] = -1;
        }
    }

    // Column is already in the cache.
    if (cache->map[col] != -1) {
        return 0;
    }

    // Fit new column, splitting a node if needed.
    fit = FindFirstPartitionFit(cache->list, size, &offset);
    if (!fit) {
        return 0;
    }
    if (fit->size == size) {
        fit->used = 1;
    } else {
        SplitPartition(fit, size);
        fit->used = 1;
    }

    if (cache->usedCols == cache->nCols) {
        cache->nCols += NODB_CACHE_DEFAULT_NCOLUMNS;
        cache->cols = NoDBRealloc(cache->cols, cache->nCols * sizeof(unsigned int));
    }
    cache->cols[cache->usedCols] = col;
    cache->usedCols++;

    cache->offset[col] = offset;
    cache->map[col] = cache->usedCols;
    cache->rowFree -= size;

    return 1;
}

static NoDBRowPartition_t *FindPartition(NoDBRowPartition_t* list, int offset)
{
    while (offset > 0) {
        offset -= list->size;
        list = list->next;
    }
    return list;
}

static void DeletePartition(NoDBRowPartition_t* list)
{
    if (list->next && !list->next->used) {
        NoDBRowPartition_t *next;

        next = list->next;
        list->size += next->size;
        list->next = next->next;
        NoDBFree(next);
    }
}

// FIXME: Handle Free in case of NoDBCacheSetStringRef()
void NoDBCacheDeleteColumn(NoDBCache_t* cache, NoDBCol_t col, unsigned int size)
{
    NoDBRowPartition_t  *p;
    int                 pos;
    int                 offset;
    int                 i;

    pos = cache->map[col];
    offset = cache->offset[col];

    // Shift back arrays from deleted column.
    for (i = pos; i < cache->usedCols; i++) {
        cache->cols[i] = cache->cols[i + 1];
    }
    cache->cols[cache->usedCols - 1] = -1;
    cache->usedCols--;

    cache->offset[col] = -1;

    // Update Map.
    for (i = 0; i < cache->nMap; i++) {
        cache->map[i] = -1;
    }
    for (i = 0; i < cache->usedCols; i++) {
        cache->map[cache->cols[i]] = i;
    }

    // Update partitions list.
    p = FindPartition(cache->list, offset);
    p->used = 0;
    DeletePartition(p);

    // Set free space.
    cache->rowFree += size;
}

int NoDBCacheHasColumn(NoDBCache_t *cache, NoDBCol_t col)
{
    return (cache->nMap > col && cache->map[col] != -1);
}

int NoDBCacheHasRow(NoDBCache_t *cache, NoDBRow_t row)
{
    return (row >= cache->startRow && row < (cache->startRow + cache->usedRows));
}

NoDBRelation_t *NoDBCacheGetRelation(NoDBCache_t *cache)
{
    return cache->rel;
}

NoDBRow_t NoDBCacheGetBegin(NoDBCache_t *cache)
{
    return cache->startRow;
}

NoDBRow_t NoDBCacheGetUsedRows(NoDBCache_t *cache)
{
    return cache->usedRows;
}

void NoDBCacheSetUsedRows(NoDBCache_t *cache, NoDBRow_t usedRows)
{
    cache->usedRows = usedRows;
}

NoDBRow_t NoDBCacheGetMaxRows(NoDBCache_t *cache)
{
    return cache->nRows;
}

void NoDBCacheSetMaxRows(NoDBCache_t *cache, NoDBRow_t maxRows)
{
    if (!cache->buf) {
        cache->nRows = maxRows;
        cache->buf = NoDBMalloc((cache->nRows * cache->rowSize) * sizeof(char));
        return;
    }

    if (maxRows <= cache->nRows) {
        return;
    }

    cache->nRows = maxRows;
    cache->buf = NoDBRealloc(cache->buf, (cache->nRows * cache->rowSize) * sizeof(char));
}

void NoDBCacheGrow(NoDBCache_t *cache, NoDBRow_t row)
{
    NoDBRow_t   maxRows;

    maxRows = row - cache->startRow + 1;
    NoDBCacheSetMaxRows(cache, maxRows);
}


int NoDBCacheHit(NoDBCache_t *cache, NoDBRelation_t *rel, NoDBCol_t col, NoDBRow_t row)
{
    return cache->rel == rel && NoDBCacheHasColumn(cache, col) && row >= cache->startRow && row < (cache->startRow + cache->usedRows);
}

//static int abs(int a)
//{
//    return (a > 0) ? a : -a;
//}


static unsigned long ComputeOffset(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return row * cache->rowSize - cache->startOffset + cache->offset[ col ];
}

char *NoDBCacheGetOffset(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return cache->buf + ComputeOffset(cache, col, row);
}

void NoDBCacheSetChar(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, char val)
{
    *((char *) (cache->buf + ComputeOffset(cache, col, row))) = val;
}

char NoDBCacheGetChar(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *((char *) (cache->buf + ComputeOffset(cache, col, row)));
}

void NoDBCacheSetShortInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, short int val)
{
    *((short int *) (cache->buf + ComputeOffset(cache, col, row))) = val;
}

short int NoDBCacheGetShortInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *((short int *) (cache->buf + ComputeOffset(cache, col, row)));
}

void NoDBCacheSetInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, int val)
{
    *((int *) (cache->buf + ComputeOffset(cache, col, row))) = val;
}

int NoDBCacheGetInt(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *((int *) (cache->buf + ComputeOffset(cache, col, row)));
}

void NoDBCacheSetDouble(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, double val)
{
    *((double *) (cache->buf + ComputeOffset(cache, col, row))) = val;
}

double NoDBCacheGetDouble(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *((double *) (cache->buf + ComputeOffset(cache, col, row)));
}

void NoDBCacheSetBool(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, bool val)
{
    *((bool *) (cache->buf + ComputeOffset(cache, col, row))) = val;
}

bool NoDBCacheGetBool(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *((bool *) (cache->buf + ComputeOffset(cache, col, row)));
}

void NoDBCacheSetStringVal(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, char *val, int len)
{
    NoDBCopy(cache->buf + ComputeOffset(cache, col, row), val, len * sizeof(char));
}

char *NoDBCacheGetStringVal(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return cache->buf + ComputeOffset(cache, col, row);
}

// FIXME: it does not work properly (only the value is copied)
void NoDBCacheSetStringRef(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, char *val, int len)
{
    char **p;

    p = (char**) (cache->buf + ComputeOffset(cache, col, row));
    *p = (char *) NoDBMalloc ( (len + 1) * sizeof(char));

    NoDBCopy(*p, val, len * sizeof(char));
    (*p)[len] = '\0';
}

char* NoDBCacheGetStringRef(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *(char **) (cache->buf + ComputeOffset(cache, col, row));
}

void NoDBCacheSetDatum(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, Datum val)
{
    *((Datum *) (cache->buf + ComputeOffset(cache, col, row))) = val;
}

Datum NoDBCacheGetDatum(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *((Datum *) (cache->buf + ComputeOffset(cache, col, row)));
}


void NoDBCacheSetDatumVector(NoDBCache_t *cache, Datum *datumVector, bool *null,  NoDBColVector_t cols, NoDBRow_t row)
{
    int i;
    unsigned long pos = row * cache->rowSize - cache->startOffset;
    char *ptr = cache->buf + pos;
    for (i = 0 ; i < NoDBColVectorSize(cols); i++)
    {
        NoDBCol_t col = NoDBColVectorGet(cols,i);
        NoDBDatum_t *datum = ((NoDBDatum_t *) (ptr + cache->offset[col]));
        datum->datum = datumVector[col];
        datum->null = null[col];
    }
}

void NoDBCacheSetDatumRefVector(NoDBCache_t *cache, Datum *datumVector, bool *null, int2 *attlen, NoDBColVector_t cols, NoDBRow_t row)
{
    int i;
    unsigned long pos = row * cache->rowSize - cache->startOffset;
    char *ptr = cache->buf + pos;
    for (i = 0 ; i < NoDBColVectorSize(cols); i++)
    {
        NoDBCol_t col = NoDBColVectorGet(cols,i);
        NoDBDatum_t *datum = ((NoDBDatum_t *) (ptr + cache->offset[col]));
        if (DatumGetPointer(datumVector[col]) == NULL)
        {
            datum->datum = PointerGetDatum(NULL);
            datum->null = true;
        }
        else
        {
            Size  realSize;
            char       *s;
            realSize = datumGetSize(datumVector[col], false, attlen[col]);
            s = (char *) NoDBArenaAlloc(NODB_ARENA_CACHE, realSize);
            memcpy(s, DatumGetPointer(datumVector[col]), realSize);
            datum->datum = PointerGetDatum(s);
            datum->null = false;
        }
    }
}


void NoDBCacheGetDatumVector(NoDBCache_t *cache, Datum *datumVector, bool *null, NoDBColVector_t cols, NoDBRow_t row)
{
    int i;
    unsigned long pos = row * cache->rowSize - cache->startOffset;
    char *ptr = cache->buf + pos;
    for (i = 0 ; i < NoDBColVectorSize(cols); i++)
    {
        NoDBCol_t col = NoDBColVectorGet(cols,i);
        NoDBDatum_t *datum = ((NoDBDatum_t *) (ptr + cache->offset[col]));
        datumVector[col] = datum->datum;
        null[col] = datum->null;
    }
}


void NoDBCacheSetPMPair(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row, NoDBPMPair_t val)
{
    *((NoDBPMPair_t *) (cache->buf + ComputeOffset(cache, col, row))) = val;
}

NoDBPMPair_t NoDBCacheGetPMPair(NoDBCache_t *cache, NoDBCol_t col, NoDBRow_t row)
{
    return *((NoDBPMPair_t *) (cache->buf + ComputeOffset(cache, col, row)));
}


void NoDBCacheSetPMPairVector(NoDBCache_t *cache, NoDBPMPair_t *pairVector, NoDBColVector_t cols, NoDBRow_t row)
{
    int i;
    unsigned long pos = row * cache->rowSize - cache->startOffset;
    char *ptr = cache->buf + pos;
    for (i = 0 ; i < NoDBColVectorSize(cols); i++)
    {
        NoDBCol_t col = NoDBColVectorGet(cols,i);
        *((NoDBPMPair_t *) (ptr + cache->offset[ col ])) = pairVector[col];
    }
}


void NoDBCacheGetPMPairVector(NoDBCache_t *cache, NoDBPMPair_t *pairVector, NoDBColVector_t cols, NoDBRow_t row)
{
    int i;
    unsigned long pos = row * cache->rowSize - cache->startOffset;
    char *ptr = cache->buf + pos;
    for (i = 0 ; i < NoDBColVectorSize(cols); i++)
    {
        NoDBCol_t col = NoDBColVectorGet(cols,i);
        pairVector[col] = *((NoDBPMPair_t *) (ptr + cache->offset[ col ]));
    }
}





/*
	Print NoDBCache_t (for debug)
*/
void NoDBPrintCache(NoDBCache_t *cache)
{
	int i;
	NoDBRowPartition_t *cur;

	fprintf(stderr, "Cache(%p): [%lu,%lu]\n", cache, cache->startRow,(cache->startRow + cache->usedRows) );
	fprintf(stderr, "Allocated rows: %lu\n",cache->nRows);
	fprintf(stderr, "Used rows: %lu\n",cache->usedRows);

	fprintf(stderr, "Row_size: %d\n",cache->rowSize);
	fprintf(stderr, "Row_free: %d\n",cache->rowFree);

	fprintf(stderr,"Available columns (used: %d - allocated: %d ): \n", (int)cache->usedCols, (int)cache->nCols);
	fprintf(stderr,"Cached columns:  { ");
	for ( i = 0 ; i < cache->usedCols; i++)
		fprintf(stderr,"%d ",cache->cols[i]);
	fprintf(stderr,"}\n");

	fprintf(stderr,"Columns Offsets: { ");
	for ( i = 0 ; i < NoDBRelationGetNumberColumns(cache->rel); i++)
	    fprintf(stderr,"%d ",cache->offset[i]);
	fprintf(stderr,"}\n");



	fprintf(stderr,"----list----\n");
	cur = cache->list;
	while ( cur != NULL)
	{
		if (cur->used)
			fprintf(stderr," |%d bytes (used)| ",cur->size);
		else
			fprintf(stderr," |%d bytes (free)|",cur->size);
		cur = cur->next;
	}
	fprintf(stderr,"\n------------\n");
}



