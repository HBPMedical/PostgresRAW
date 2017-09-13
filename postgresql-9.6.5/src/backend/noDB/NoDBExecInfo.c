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


#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pg_config.h"
#include "noDB/NoDBExecInfo.h"
#include "noDB/NoDBCache.h"
#include "noDB/NoDBEOLCacheWorld.h"
#include "noDB/auxiliary/NoDBRow.h"
#include "noDB/auxiliary/NoDBMalloc.h"
#include "noDB/auxiliary/NoDBBitmap.h"

#include "snooping/common.h"


static NoDBExecInfoVector_t *ExecInfo = NULL;

static uint32 NoDBComputeNumberOfBlocks(char* filename);


NoDBExecInfo_t *NoDBExecInfoInit(NoDBRelation_t *rel, char *filename, long budgetPM, long budgetCache)
{
    int i;
    int j;
    int natts = NoDBRelationGetNumberColumns(rel);

    //First time accessing the PolicyInfo
    if ( !ExecInfo )
    {
        ExecInfo = (NoDBExecInfoVector_t*) NoDBMalloc ( 1 * sizeof(NoDBExecInfoVector_t));
        ExecInfo->size = NELLEM_ALLOC;
        ExecInfo->used = 0;
        ExecInfo->vector = (NoDBExecInfo_t*) NoDBMalloc( NELLEM_ALLOC * sizeof(NoDBExecInfo_t));

    }

    //Find the first empty slot
    i = ExecInfo->used;

    //Extend module to store more relations
    if ( i == ExecInfo->size )
    {
        NoDBExecInfo_t *tmp;
        ExecInfo->size += NELLEM_ALLOC;
        tmp = (NoDBExecInfo_t*) NoDBRealloc (ExecInfo->vector, ExecInfo->size * sizeof(NoDBExecInfo_t));
        if ( tmp != NULL )
            ExecInfo->vector = tmp;
    }

    //Enter record of new PolicyInfo
    ExecInfo->vector[i].budgetPM      = budgetPM;
    ExecInfo->vector[i].budgetCache   = budgetCache;

    //Init cached columns
    ExecInfo->vector[i].info.colsCache = NoDBBitmapInit(natts);
    NoDBBitmapClearAll(ExecInfo->vector[i].info.colsCache);
    ExecInfo->vector[i].info.colsPM    = NoDBBitmapInit(natts);
    NoDBBitmapClearAll(ExecInfo->vector[i].info.colsPM);

    //Init WriteLocks
    ExecInfo->vector[i].locks.size = natts;
    ExecInfo->vector[i].locks.CacheLock = (NoDBLock_t*) NoDBMalloc( natts * sizeof(NoDBLock_t));
    ExecInfo->vector[i].locks.PMLock = (NoDBLock_t*) NoDBMalloc( natts * sizeof(NoDBLock_t));

    for(j = 0; j < natts; j++ )
    {
        ExecInfo->vector[i].locks.CacheLock[j] = 0;
        ExecInfo->vector[i].locks.PMLock[j] = 0;
    }

    ExecInfo->vector[i].integrityCheck.firstTime = true;
    ExecInfo->vector[i].integrityCheck.disable_need_transcoding = false;

    ExecInfo->vector[i].relStats.blocks = 0;
    ExecInfo->vector[i].relStats.rows = 0;
    ExecInfo->vector[i].relStats.statsCollected = NoDBBitmapInit(natts);;
    NoDBBitmapClearAll(ExecInfo->vector[i].relStats.statsCollected);


    ExecInfo->vector[i].rel = rel;
    strcpy(ExecInfo->vector[i].filename, filename);

    ExecInfo->used++;

    return &ExecInfo->vector[i];
}


NoDBExecInfo_t *NoDBGetExecInfo(char* relation)
{
    int i;

    //First time accessing the PolicyInfo
    if ( !ExecInfo )
        return NULL;

    for (i = 0 ; i < ExecInfo->used; i++)
    {
        if ( strcmp(ExecInfo->vector[i].rel->name, relation) == 0 )
            return &ExecInfo->vector[i];
    }
    return NULL;
}



NoDBColVector_t NoDBNothingPolicy(void)
{
    return NoDBColVectorInit(0);
}

/*
 * FIXME
 * Consider budget in future version
 * Just for testing with the rest of the system
 */
NoDBColVector_t NoDBAggressiveCachePolicy(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts)
{
    NoDBColVector_t writeDataCols;
    int i, j;
    int counter = 0;
    for ( i = 0; i < NoDBColVectorSize(queryAtts); i++)
    {
		NoDBCol_t column = NoDBColVectorGet(queryAtts, i);
    	if(!execinfo->locks.CacheLock[column])
    		counter++;
    }

    writeDataCols = NoDBColVectorInit(counter);
    j =0;
    for ( i = 0; i < NoDBColVectorSize(queryAtts); i++)
    {
		NoDBCol_t column = NoDBColVectorGet(queryAtts, i);
    	if(!execinfo->locks.CacheLock[column])
    	{
    		NoDBColVectorSet(writeDataCols, j, column);
    		NoDBBitmapSet(execinfo->info.colsCache,column);
    		execinfo->locks.CacheLock[column] = 1;
    		j++;
    	}
    }

    return writeDataCols;
}


NoDBColVector_t NoDBAggressivePMPolicy(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts)
{
    NoDBColVector_t writePositionCols;
    int i, j;
    int counter = 0;

/*
    int hasFirst = 1;
    int hasLast = 1;
    NoDBCol_t lastColumn = NoDBRelationGetNumberColumns(execinfo->rel) - 1;

    if (!NoDBColVectorContains(queryAtts,0))
    {
        if(!execinfo->locks.PMLock[0])
        {
    		counter++;
        	hasFirst = 0;
        }
    }
    if (!NoDBColVectorContains(queryAtts,lastColumn))
    {
        if(!execinfo->locks.PMLock[lastColumn])
        {
    		counter++;
        	hasLast = 0;
        }
    }
*/
    for ( i = 0; i < NoDBColVectorSize(queryAtts); i++)
    {
		NoDBCol_t column = NoDBColVectorGet(queryAtts, i);
    	if(!execinfo->locks.PMLock[column])
    		counter++;
    }


    writePositionCols = NoDBColVectorInit(counter);
    j = 0;
    for ( i = 0; i < NoDBColVectorSize(queryAtts); i++)
    {
		NoDBCol_t column = NoDBColVectorGet(queryAtts, i);
    	if(!execinfo->locks.PMLock[column])
    	{
			NoDBColVectorSet(writePositionCols, j, column);
			NoDBBitmapSet(execinfo->info.colsPM,column);
			execinfo->locks.PMLock[column] = 1;
			j++;
    	}
    }
/*
    if(!hasFirst)
	{
		NoDBColVectorSet(writePositionCols, j, 0);
		NoDBBitmapSet(execinfo->info.colsPM,0);
		execinfo->locks.PMLock[0] = 1;
		j++;
	}
    if(!hasLast)
	{
		NoDBColVectorSet(writePositionCols, j, lastColumn);
		NoDBBitmapSet(execinfo->info.colsPM,lastColumn);
		execinfo->locks.PMLock[lastColumn] = 1;
		j++;
	}
*/
    return writePositionCols;
}


void NoDBExecIntegrityCheckCompleted(NoDBExecInfo_t *execinfo)
{
    execinfo->integrityCheck.firstTime = false;
    execinfo->integrityCheck.disable_need_transcoding = true;
}

//Release locks if any
int NoDBExecReleaseCacheLocks(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts)
{
    int i;
    int changed = 0;

    for ( i = 0; i < NoDBColVectorSize(queryAtts); i++)
    {
		NoDBCol_t column = NoDBColVectorGet(queryAtts, i);
		if (execinfo->locks.CacheLock[column])
			changed = 1;
    	execinfo->locks.CacheLock[column] = 0;
    }
    return changed;
}

int NoDBExecReleasePMLocks(NoDBExecInfo_t *execinfo, NoDBColVector_t queryAtts)
{
    int i;
    int changed = 0;

    for ( i = 0; i < NoDBColVectorSize(queryAtts); i++)
    {
		NoDBCol_t column = NoDBColVectorGet(queryAtts, i);
		if (execinfo->locks.PMLock[column])
			changed = 1;
		execinfo->locks.PMLock[column] = 0;
    }
    return changed;
}

bool NoDBGetNeedTranscoding(NoDBExecInfo_t *execinfo)
{
    return execinfo->integrityCheck.disable_need_transcoding;
}

void NoDBExecInfoDestroy(NoDBExecInfo_t *execinfo)
{
    if (!execinfo)
        return;

    NoDBRelationDestroy(execinfo->rel);
    /* Free */
    NoDBBitmapDestroy(execinfo->info.colsCache);
    NoDBBitmapDestroy(execinfo->info.colsPM);

    NoDBFree(execinfo->locks.CacheLock);
    NoDBFree(execinfo->locks.PMLock);
    execinfo->locks.size = 0;
}



void NoDBExecInfoVectorDestroy(void)
{
    int i;
    for (i = 0 ; i < ExecInfo->size; i++)
        NoDBExecInfoDestroy(&ExecInfo->vector[i]);
    NoDBFree(ExecInfo);
}

void NoDBExecSetNumberOffBlocks(NoDBExecInfo_t *execinfo)
{
    uint32 nBlocks;
    struct stat st;

    stat(execinfo->filename, &st);
    nBlocks = st.st_size / BLCKSZ;
    if (nBlocks == 0)
        nBlocks = 1;
    execinfo->relStats.blocks = nBlocks;
}

void NoDBExecSetNumberOffRows(NoDBExecInfo_t *execinfo)
{

    NoDBCache_t *cache = NULL;
    cache = NoDBGetEOLCache(execinfo->rel->name);
    if( cache )
        execinfo->relStats.rows = NoDBCacheGetUsedRows(cache);
    else
        execinfo->relStats.rows = 100;
}

//FIXME: avoid using common.h from previous version
uint32 NoDBExecGetNumberOfBlocks(NoDBExecInfo_t *execinfo, char *relation)
{
    if(!execinfo)
    {
        char *file = getInputFilename(relation);
        if (file)
            return NoDBComputeNumberOfBlocks(file);
        else
            return 0;
    }

    return execinfo->relStats.blocks;
}


static uint32 NoDBComputeNumberOfBlocks(char* filename)
{
    uint32 nBlocks;
    struct stat st;

    stat(filename, &st);
    nBlocks = st.st_size / BLCKSZ;
    return nBlocks;
}

//FIXME: add estimation for the first query
uint32 NoDBExecGetNumberOfRows(NoDBExecInfo_t *execinfo)
{
    if(!execinfo)
    {

        return 0;
    }
    return execinfo->relStats.rows;
}



