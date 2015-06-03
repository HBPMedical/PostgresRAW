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


#ifndef NODB_RELATION_H
#define NODB_RELATION_H

#include "noDB/auxiliary/NoDBCol.h"

typedef struct NoDBRelationAttr_t {
    int             size;
    int             byValue;
} NoDBRelationAttr_t;

typedef struct NoDBRelation_t {
    char                *name;
    int                 used;
    int                 max;
    NoDBRelationAttr_t  *attrs;
} NoDBRelation_t;

NoDBRelation_t  *NoDBRelationInit(char *name);
void            NoDBRelationAddColumn(NoDBRelation_t *rel, unsigned int size, int byValue);
void            NoDBRelationDestroy(NoDBRelation_t *rel);
unsigned int    NoDBRelationGetColumnSize(NoDBRelation_t *rel, NoDBCol_t col);
int             NoDBRelationIsColumnByValue(NoDBRelation_t *rel, NoDBCol_t col);
int             NoDBRelationGetNumberColumns(NoDBRelation_t *rel);
#endif	/* NODB_RELATION_H */
