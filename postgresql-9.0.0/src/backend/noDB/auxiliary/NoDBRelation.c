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



#include "noDB/auxiliary/NoDBRelation.h"
#include "noDB/auxiliary/NoDBMalloc.h"

#include <string.h>

#define NODB_RELATION_DEFAULT_NCOLUMNS  20

NoDBRelation_t *NoDBRelationInit(char *name)
{
    NoDBRelation_t  *rel;
    int             len;

    rel = NoDBMalloc(sizeof(NoDBRelation_t));
    len = NoDBStringLen(name) + 1;
    rel->name = NoDBMalloc(sizeof(char) * len);
    NoDBCopy(rel->name, name, len);
    rel->used = 0;
    rel->max = NODB_RELATION_DEFAULT_NCOLUMNS;
    rel->attrs = NoDBMalloc(sizeof(NoDBRelationAttr_t) * rel->max);

    return rel;
}

void NoDBRelationAddColumn(NoDBRelation_t *rel, unsigned int size, int byValue)
{
    if (rel->used == rel->max) {
        rel->max += NODB_RELATION_DEFAULT_NCOLUMNS;
        rel->attrs = NoDBRealloc(rel->attrs, sizeof(NoDBRelationAttr_t) * rel->max);
    }
    rel->attrs[rel->used].size = size;
    rel->attrs[rel->used].byValue = byValue;
    rel->used++;
}

void NoDBRelationDestroy(NoDBRelation_t *rel)
{
    NoDBFree(rel->name);
    NoDBFree(rel->attrs);
    NoDBFree(rel);
}

unsigned int NoDBRelationGetColumnSize(NoDBRelation_t *rel, NoDBCol_t col)
{
    return rel->attrs[col].size;
}

int NoDBRelationIsColumnByValue(NoDBRelation_t *rel, NoDBCol_t col)
{
    return rel->attrs[col].byValue;
}


int NoDBRelationGetNumberColumns(NoDBRelation_t *rel)
{
    return rel->used;
}

