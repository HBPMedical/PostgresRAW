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

#ifndef NODBEXECINFO_H_
#define NODBEXECINFO_H_

#include "postgres.h"
#include "noDB/NoDBCache.h"
#include "noDB/auxiliary/NoDBCol.h"
#include "noDB/auxiliary/NoDBRow.h"
#include "noDB/auxiliary/NoDBBitmap.h"
#include "noDB/auxiliary/NoDBRelation.h"


#define FILENAME_SIZE        256
#define NELLEM_ALLOC         5



typedef int NoDBLock_t;


typedef enum NoDBCachingPolicy_t {
    NODB_AGGRESSIVE_CACHING,
    NODB_NO_CACHING
} NoDBCachingPolicy_t;


typedef struct NoDBController_t
{
    bool firstTime;
    bool disable_need_transcoding;
} NoDBController_t;


typedef struct WritingLock_t {
    int             size;
    NoDBLock_t      *CacheLock;
    NoDBLock_t      *PMLock;
} NoDBWritingLock_t;


typedef struct CachedInfo_t {
    NoDBBitmap_t     *colsPM;
    NoDBBitmap_t     *colsCache;
} NoDBCachedInfo_t;


typedef struct NoDBStatsInfo_t {
    NoDBBitmap_t     *statsCollected;;
    uint32          blocks;
    NoDBRow_t       rows;
} NoDBStatsInfo_t;



typedef struct NoDBExecInfo_t {
    long                    budgetPM;
    long                    budgetCache;
    struct CachedInfo_t     info;
    struct WritingLock_t    locks;
    NoDBController_t        integrityCheck;
    NoDBStatsInfo_t         relStats;
    NoDBRelation_t          *rel;
    char                    filename[FILENAME_SIZE];
} NoDBExecInfo_t;


typedef struct NoDBExecInfoVector_t
{
    int                     size;
    int                     used;
    struct NoDBExecInfo_t   *vector;
} NoDBExecInfoVector_t;



NoDBExecInfo_t      *NoDBExecInfoInit(NoDBRelation_t *rel, char *filename, long budgetPM, long budgetCache);
NoDBExecInfo_t      *NoDBGetExecInfo(char* relation);


NoDBColVector_t     NoDBNothingPolicy(void);
NoDBColVector_t     NoDBAggressiveCachePolicy(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts);
NoDBColVector_t     NoDBAggressivePMPolicy(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts);
void                NoDBExecIntegrityCheckCompleted(NoDBExecInfo_t *execinfo);
int 				NoDBExecReleaseCacheLocks(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts);
int					 NoDBExecReleasePMLocks(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts);
bool                NoDBGetNeedTranscoding(NoDBExecInfo_t *execinfo);

void               NoDBExecInfoDestroy(NoDBExecInfo_t *execinfo);
void               NoDBExecInfoVectorDestroy(void);

void                NoDBExecSetNumberOffBlocks(NoDBExecInfo_t *execinfo);
void                NoDBExecSetNumberOffRows(NoDBExecInfo_t *execinfo);
uint32              NoDBExecGetNumberOfBlocks(NoDBExecInfo_t *execinfo, char *relation);

uint32              NoDBExecGetNumberOfRows(NoDBExecInfo_t *execinfo);



#endif /* NODBEXECINFO_H_ */

