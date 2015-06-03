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



#include "string.h"
#include "noDB/NoDBEOLCacheWorld.h"
#include "noDB/auxiliary/NoDBMalloc.h"
#include "noDB/auxiliary/NoDBRow.h"

#define NODB_CACHE_DEFAULT_ROWS         5                       // Default number of rows to add to a cache.


NoDBEOLCacheWorld_t *EOLCaches = NULL;


NoDBCache_t *NoDBInitEOLCache(char* relation, NoDBRow_t rows)
{
	int i;
	NoDBRelation_t *rel;

	//Initialize EOLCacheVector
	if (!EOLCaches)
	{
		EOLCaches = (NoDBEOLCacheWorld_t*) NoDBMalloc ( 1 * sizeof(NoDBEOLCacheWorld_t));
		EOLCaches->vector = (NoDBEOLCache_t*) NoDBMalloc (NELLEM_ALLOC * sizeof(NoDBEOLCache_t));
		EOLCaches->size = NELLEM_ALLOC;

		for (i = 0 ; i < EOLCaches->size; i++)
			strcpy(EOLCaches->vector[i].relation,"\0");
	}

	//If EOLCache exists then return it
	for (i = 0 ; i < EOLCaches->size; i++)
	{
		if ( strcmp(EOLCaches->vector[i].relation,relation) == 0 )
			return EOLCaches->vector[i].EOLCache;
	}


	//Find the first empty slot
	for (i = 0 ; i < EOLCaches->size; i++)
	{
		if (strlen(EOLCaches->vector[i].relation) == 0 )
			break;
	}

	//Extend module to store more relations
	if ( i == EOLCaches->size )
	{
		NoDBEOLCache_t *tmp;
		int cur = EOLCaches->size;
		EOLCaches->size += NELLEM_ALLOC;
		tmp = (NoDBEOLCache_t*) NoDBRealloc (EOLCaches->vector, EOLCaches->size * sizeof(NoDBEOLCache_t));
		if ( tmp != NULL )
		{
			int j;
			EOLCaches->vector = tmp;
	        for (j = cur ; j < EOLCaches->size; j++)
	            strcpy(EOLCaches->vector[j].relation,"\0");
		}
	}

	//Enter record of new EOLCache
	strcpy(EOLCaches->vector[i].relation, relation);
	rel = NoDBRelationInit(relation);
	NoDBRelationAddColumn(rel, sizeof(short int), true);
	if ( rows <= 0 )
		rows = NODB_CACHE_DEFAULT_ROWS;
	EOLCaches->vector[i].EOLCache = NoDBCacheInit(rel, NODB_POSITIONS_CACHE, 0, sizeof(short int));
	NoDBCacheAddColumn(EOLCaches->vector[i].EOLCache, 0, sizeof(short int));
	NoDBCacheSetMaxRows(EOLCaches->vector[i].EOLCache, rows);

	return EOLCaches->vector[i].EOLCache;
}

NoDBCache_t *NoDBGetEOLCache(char* relation)
{
	int i;
	for (i = 0 ; i < EOLCaches->size; i++)
	{
		if ( strcmp(EOLCaches->vector[i].relation,relation) == 0 )
			return EOLCaches->vector[i].EOLCache;
	}
	return NULL;
}

void NoDBFreeEOLCache(char* relation)
{
	int i;
	if (!EOLCaches)
		return;

	for (i = 0 ; i < EOLCaches->size; i++)
	{
		if ( strcmp(EOLCaches->vector[i].relation, relation) == 0)
		{
			strcpy(EOLCaches->vector[i].relation,"\0");

			/* Free */
			NoDBCacheDestroy(EOLCaches->vector[i].EOLCache);
			free(EOLCaches->vector[i].EOLCache);
			break;
		}
	}
}












