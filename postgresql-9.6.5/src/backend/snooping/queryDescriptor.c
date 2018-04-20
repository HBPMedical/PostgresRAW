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



#include "noDB/auxiliary/NoDBMalloc.h"
#include "snooping/queryDescriptor.h"

static void printColumnDescriptor(void* columnDesc);
static void freeColumnDescriptor(void* _columnDesc);
static void *pg_malloc(size_t size);
static bool equalColDesc(void *_desc1, void *_desc2);

QueryDescriptor NoDBQueryDesc;
bool isInitialized = false;
ParseStatus PSStatus = PS_idle;


void
initializeQueryDescriptor(void)
{
	int i;

	for ( i = 0 ; i < NUMBER_OF_RELATIONS; i++)
	{
		strcpy(NoDBQueryDesc.aliasMap[i].aliasName, "\0");
		strcpy(NoDBQueryDesc.aliasMap[i].tableName, "\0");
	}

	InitializeArray(&NoDBQueryDesc.targetList, INIT_SIZE, sizeof(ColumnDescriptor));
	InitializeArray(&NoDBQueryDesc.groupList, INIT_SIZE, sizeof(ColumnDescriptor));
	InitializeArray(&NoDBQueryDesc.havingList, INIT_SIZE, sizeof(ColumnDescriptor));
	InitializeArray(&NoDBQueryDesc.orderByList, INIT_SIZE, sizeof(ColumnDescriptor));
	InitializeArray(&NoDBQueryDesc.whereList, INIT_SIZE, sizeof(ColumnDescriptor));
	InitializeArray(&NoDBQueryDesc.filterList, INIT_SIZE, sizeof(ColumnDescriptor));
	InitializeArray(&NoDBQueryDesc.allAttributes, INIT_SIZE, sizeof(ColumnDescriptor));
	isInitialized = true;
	NoDBQueryDesc.limit = 0;
}

void
resetQueryDescriptor(void)
{
	int i;
	if ( isInitialized )
	{

		for ( i = 0 ; i < NUMBER_OF_RELATIONS; i++)
		{
			strcpy(NoDBQueryDesc.aliasMap[i].aliasName, "\0");
			strcpy(NoDBQueryDesc.aliasMap[i].tableName, "\0");
		}

		FreeArray(&NoDBQueryDesc.targetList, freeColumnDescriptor);
		FreeArray(&NoDBQueryDesc.groupList, freeColumnDescriptor);
		FreeArray(&NoDBQueryDesc.havingList, freeColumnDescriptor);
		FreeArray(&NoDBQueryDesc.orderByList, freeColumnDescriptor);
		FreeArray(&NoDBQueryDesc.whereList, freeColumnDescriptor);
		FreeArray(&NoDBQueryDesc.filterList, freeColumnDescriptor);
		FreeArray(&NoDBQueryDesc.allAttributes, freeColumnDescriptor);
	}
    NoDBQueryDesc.limit = 0;
	isInitialized = false;
	PSStatus = PS_idle;
}

void changeParseStatus(ParseStatus newStatus)
{
	PSStatus = newStatus;
}

ParseStatus getParseStatus(void)
{
	return PSStatus;
}

static bool equalColDesc(void *_desc1, void *_desc2)
{
	bool val = 0;
	ColumnDescriptor *desc1 = _desc1;
	ColumnDescriptor *desc2 = _desc2;

	if (desc1->columnName != NULL && desc2->columnName != NULL)
		if (strcmp(desc1->columnName,desc2->columnName) == 0)
			val++;
	if (desc1->columnName == NULL && desc2->columnName == NULL)
		val++;

	if (desc1->tableName != NULL && desc2->tableName != NULL)
		if (strcmp(desc1->tableName,desc2->tableName) == 0)
			val++;
	if (desc1->tableName == NULL && desc2->tableName == NULL)
		val++;

	if (desc1->schemaName != NULL && desc2->schemaName != NULL)
		if (strcmp(desc1->schemaName,desc2->schemaName) == 0)
			val++;
	if (desc1->schemaName == NULL && desc2->schemaName == NULL)
		val++;

	if (desc1->catalogName != NULL && desc2->catalogName != NULL)
		if (strcmp(desc1->catalogName,desc2->catalogName) == 0)
			val++;
	if (desc1->catalogName == NULL && desc2->catalogName == NULL)
		val++;

	if(desc1->whole_row == desc2->whole_row)
		val++;

	return ( val == 5 );
}

/* It does not work in all cases... */
void addColumnInformation(char* columnName, char* tableName, char* schemaName, char* catalogName, bool whole_row)
{
	ColumnDescriptor columnDesc;
	ColumnDescriptor clone;

	if(!isInitialized)
		initializeQueryDescriptor();

	if(columnName != NULL) {
		columnDesc.columnName = (char*) pg_malloc(strlen(columnName) + 1);
		strcpy(columnDesc.columnName, columnName);
		columnDesc.columnName[strlen(columnName)] = '\0';
	}
	else
		columnDesc.columnName = NULL;

	if(tableName != NULL) {
		columnDesc.tableName = (char*) pg_malloc(strlen(tableName) + 1);
		strcpy(columnDesc.tableName, tableName);
		columnDesc.tableName[strlen(tableName)] = '\0';
	}
	else
		columnDesc.tableName = NULL;

	if(schemaName != NULL) {
		columnDesc.schemaName = (char*) pg_malloc(strlen(schemaName) + 1);
		strcpy(columnDesc.schemaName, schemaName);
		columnDesc.schemaName[strlen(schemaName)] = '\0';
	}
	else
		columnDesc.schemaName = NULL;

	if(catalogName != NULL) {
		columnDesc.catalogName = (char*) pg_malloc(strlen(catalogName) + 1);
		strcpy(columnDesc.catalogName, catalogName);
		columnDesc.catalogName[strlen(catalogName)] = '\0';
	}
	else
		columnDesc.catalogName = NULL;

	columnDesc.whole_row = whole_row;

	copyColumnDescriptor(&columnDesc, &clone);
	switch(PSStatus)
	{
		case PS_idle:
			//Do nothing...
			break;
//		case PS_fromList:
//			QueryDesc.fromList = lappend(QueryDesc.fromList, &columnDesc);
//			break;
		case PS_targetList:
//			QueryDesc.targetList = lappend(QueryDesc.targetList, &columnDesc);
//			break;
			InsertToArray(&NoDBQueryDesc.targetList, &columnDesc);
			break;
		case PS_groupList:
			InsertToArray(&NoDBQueryDesc.groupList, &columnDesc);
			break;
		case PS_orderByList:
			InsertToArray(&NoDBQueryDesc.orderByList, &columnDesc);
			break;
		case PS_whereList:
			// FIX for filtering on the correct table for NoDB
			if (columnDesc.tableName != NULL) {
				char* tablename = getTableNameFromAlias(columnDesc.tableName);
				if (tablename != NULL) {
					columnDesc.tableName = (char*) malloc(strlen(tablename)+1);
					strcpy(columnDesc.tableName, tablename);
				}
			}
			InsertToArray(&NoDBQueryDesc.whereList, &columnDesc);
			break;
		case PS_havingList:
			InsertToArray(&NoDBQueryDesc.havingList, &columnDesc);
			break;
		case PS_filterList:
			// FIX for filtering on the correct table for NoDB
			if (columnDesc.tableName != NULL) {
				char* tablename = getTableNameFromAlias(columnDesc.tableName);
				if (tablename != NULL) {
					columnDesc.tableName = (char*) malloc(strlen(tablename)+1);
					strcpy(columnDesc.tableName, tablename);
				}
			}
			InsertToArray(&NoDBQueryDesc.filterList, &columnDesc);
			break;
		default:
			//Do nothing...
			break;
	}
	if (!SearchArray(&NoDBQueryDesc.allAttributes, &clone, equalColDesc))
		InsertToArray(&NoDBQueryDesc.allAttributes, &clone);
}


void
addTableAliasInformation(char* tableName, char* alias)
{

	printf("==================================> %s : %s\n", tableName, alias);

	int i;
	int pos;

	if(!isInitialized)
		initializeQueryDescriptor();

	//alias is already in the Map
//	if ( getAliasFromTableName(tableName) != NULL)
//		return;
	if ( hasTableAlias(tableName,alias) )
		return;

	for ( i = 0 ; i < NUMBER_OF_RELATIONS; i++)
	{
		if(strlen (NoDBQueryDesc.aliasMap[i].tableName) == 0)
		{
			strcpy(NoDBQueryDesc.aliasMap[i].aliasName, alias);
			strcpy(NoDBQueryDesc.aliasMap[i].tableName, tableName);
			pos = i;
			break;
		}
	}
	if ( pos >= NUMBER_OF_RELATIONS )
	{
		fprintf(stderr,"Alias Map is full!\n");
		return;
	}
}

char*
getTableNameFromAlias(char* alias)
{
	int i;
	for ( i = 0 ; i < NUMBER_OF_RELATIONS; i++)
	{
		if(strcmp (NoDBQueryDesc.aliasMap[i].aliasName,alias) == 0)
			return NoDBQueryDesc.aliasMap[i].tableName;
	}
	return NULL;
}


char*
getAliasFromTableName(char* tableName)
{
	int i;
	for ( i = 0 ; i < NUMBER_OF_RELATIONS; i++)
	{
		if(strcmp (NoDBQueryDesc.aliasMap[i].tableName,tableName) == 0)
			return NoDBQueryDesc.aliasMap[i].aliasName;
	}
	return NULL;
}

int
hasTableAlias(char *tableName, char* alias)
{
	int i;
	for ( i = 0 ; i < NUMBER_OF_RELATIONS; i++)
	{
		if(strcmp (NoDBQueryDesc.aliasMap[i].tableName,tableName) == 0 && strcmp (NoDBQueryDesc.aliasMap[i].aliasName,alias) == 0)
			return 1;
	}
	return 0;
}



NoDBColVector_t
getQueryAttributes(TupleDesc tupDesc, char *relation)
{
    NoDBColVector_t queryAtts;
    D_Array array;
    ColumnDescriptor *columnDesc;
    int *which;
    int i = 0;
    int j = 0;
    int counter = 0;


    which = (int*)NoDBMalloc(tupDesc->natts * sizeof(int));
    for ( i = 0; i < tupDesc->natts; i++)
        which[i] = 0;

    //We do not expect any duplicated in this list
    array = NoDBQueryDesc.allAttributes;

    //We have not collected so there are not interesting attributes
    if( array.usedObjects != 0)
    {
        for ( i = 0; i < array.usedObjects; i++)
        {
            columnDesc = (ColumnDescriptor*) GetObjectPos(&array, i);
            if(columnDesc->tableName != NULL)
            {
                if ( strcmp(columnDesc->tableName, relation) != 0)
                {
                	if (!hasTableAlias(relation, columnDesc->tableName))
                		continue;
                }
            }
            if(columnDesc->whole_row) //get all the attributes and return
            {
                for (i = 0 ; i < tupDesc->natts; i++)
                    which[i] = 1;
                counter = tupDesc->natts;
                break;
            }

            if(columnDesc->columnName != NULL)
            {
                for (j = 0 ; j < tupDesc->natts; j++)
                {
                    if ( strcmp(tupDesc->attrs[j]->attname.data, columnDesc->columnName) == 0 && which[j] != 1)
                    {
                        which[j] = 1;
                        counter++;
                        break;
                    }
                }
            }
        }
    }

    queryAtts = NoDBColVectorInit(counter);
    j = 0;
    for ( i = 0; i < tupDesc->natts; i++)
    {
        if(which[i])
        {
            NoDBColVectorSet(queryAtts, j, i);
            j++;
        }
    }
    NoDBFree(which);

    return queryAtts;
}


/*
 * Get interesting attributes based on the collected information in the query descriptor
 */
NoDBColVector_t
getQueryFilterAttributes(TupleDesc tupDesc, char *relation)
{
    NoDBColVector_t filterAtts;
    D_Array array;
    ColumnDescriptor *columnDesc;
    int *which;
    int i = 0;
    int j = 0;
    int counter = 0;


    which = (int*)NoDBMalloc(tupDesc->natts * sizeof(int));
    for ( i = 0; i < tupDesc->natts; i++)
        which[i] = 0;

    //We don't expect any duplicated in this list
    array = NoDBQueryDesc.filterList;
    //We haven't collected so there are not interesting attributes
    if( array.usedObjects != 0)
    {
        for ( i = 0; i < array.usedObjects; i++)
        {
            columnDesc = (ColumnDescriptor*) GetObjectPos(&array, i);
            if(columnDesc->tableName != NULL)
                if ( strcmp(columnDesc->tableName, relation) != 0)
                    continue;

            Assert(columnDesc->whole_row == false);
            if(columnDesc->tableName != NULL)
                if ( strcmp(columnDesc->tableName, relation) != 0)
                    continue;
            if(columnDesc->columnName != NULL)
            {
                for (j = 0 ; j < tupDesc->natts; j++)
                {
                    if ( strcmp(tupDesc->attrs[j]->attname.data, columnDesc->columnName) == 0 && which[j] != 1)
                    {
                        which[j] = 1;
                        counter++;
                        break;
                    }
                }
            }
        }
    }

    filterAtts = NoDBColVectorInit(counter);
    j = 0;
    for ( i = 0; i < tupDesc->natts; i++)
    {
        if(which[i])
        {
            NoDBColVectorSet(filterAtts, j, i);
            j++;
        }
    }
    NoDBFree(which);

    return filterAtts;
}

NoDBColVector_t getQueryRestAttributes(NoDBColVector_t queryAtts, NoDBColVector_t filterAtts)
{
    NoDBColVector_t restAttrs;
    int i;
    int j;

    restAttrs = NoDBColVectorInit(NoDBColVectorSize(queryAtts) - NoDBColVectorSize(filterAtts));

    j = 0;
    for ( i = 0; i < NoDBColVectorSize(queryAtts); i++)
    {
        NoDBCol_t col = NoDBColVectorGet(queryAtts, i);
        if ( !NoDBColVectorContains(filterAtts, col))
        {
            NoDBColVectorSet(restAttrs, j, col);
            j++;
        }
    }
    return restAttrs;
}


NoDBColVector_t mergeAttributeVectors(NoDBColVector_t a, NoDBColVector_t b)
{
    NoDBColVector_t ret;
    int i;
    int j;

    ret = NoDBColVectorInit(NoDBColVectorSize(a) - NoDBColVectorSize(b));


    for ( i = 0; i < NoDBColVectorSize(a); i++)
        NoDBColVectorSet(ret, i, NoDBColVectorGet(a, i));


    j = 0;
    for ( i = 0; i < NoDBColVectorSize(b); i++)
    {
        NoDBCol_t col = NoDBColVectorGet(b, i);
        if ( !NoDBColVectorContains(b, col))
        {
            NoDBColVectorSet(ret, j, col);
            j++;
        }
    }

    NoDBColVectorSort(ret);

    return ret;
}


void setQueryLimit(long int limit)
{
    NoDBQueryDesc.limit = limit;
}


long int getQueryLimit(void)
{
    return NoDBQueryDesc.limit;
}





void copyColumnDescriptor(ColumnDescriptor *old, ColumnDescriptor *new)
{

	if(old->columnName != NULL) {
		new->columnName = (char*) pg_malloc(strlen(old->columnName) + 1);
		strcpy(new->columnName, old->columnName);
		new->columnName[strlen(new->columnName)] = '\0';
	}
	else
		new->columnName = NULL;

	if(old->tableName != NULL) {
		new->tableName = (char*) pg_malloc(strlen(old->tableName) + 1);
		strcpy(new->tableName, old->tableName);
		new->tableName[strlen(new->tableName)] = '\0';
	}
	else
		new->tableName = NULL;

	if(old->schemaName != NULL) {
		new->schemaName = (char*) pg_malloc(strlen(old->schemaName) + 1);
		strcpy(new->schemaName, old->schemaName);
		new->schemaName[strlen(new->schemaName)] = '\0';
	}
	else
		new->schemaName = NULL;

	if(old->catalogName != NULL) {
		new->catalogName = (char*) pg_malloc(strlen(old->catalogName) + 1);
		strcpy(new->catalogName, old->catalogName);
		new->catalogName[strlen(new->catalogName)] = '\0';
	}
	else
		new->catalogName = NULL;

	new->whole_row = old->whole_row;
}

/*For debug...*/
void
printQueryDescriptor(void)
{
	int i;
	if(isInitialized)
	{
		fprintf(stderr,"\nPrint query descriptor information\n");

		fprintf(stderr,"Targetlist:\n");
		PrintArray(&NoDBQueryDesc.targetList, printColumnDescriptor);

		fprintf(stderr,"Grouplist:\n");
		PrintArray(&NoDBQueryDesc.groupList, printColumnDescriptor);

		fprintf(stderr,"Havinglist:\n");
		PrintArray(&NoDBQueryDesc.havingList, printColumnDescriptor);

		fprintf(stderr,"OrderBylist:\n");
		PrintArray(&NoDBQueryDesc.orderByList, printColumnDescriptor);

		fprintf(stderr,"Wherelist:\n");
		PrintArray(&NoDBQueryDesc.whereList, printColumnDescriptor);

		fprintf(stderr,"Filterlist:\n");
		PrintArray(&NoDBQueryDesc.filterList, printColumnDescriptor);

		fprintf(stderr,"All interesting attributes:\n");
		PrintArray(&NoDBQueryDesc.allAttributes, printColumnDescriptor);

		for ( i = 0 ; i < NUMBER_OF_RELATIONS; i++)
		{
			if(strlen (NoDBQueryDesc.aliasMap[i].tableName) != 0)
				fprintf(stderr,"Table:{%s} Alias:{%s} \n",NoDBQueryDesc.aliasMap[i].tableName,NoDBQueryDesc.aliasMap[i].aliasName);
		}


	}
	else
		fprintf(stderr,"\nQuery descriptor is not initialized\n");

}

void
printInterestingAttributes(void)
{
	if(isInitialized)
	{
		fprintf(stderr,"\nInteresting attributes: %d\n",NoDBQueryDesc.allAttributes.usedObjects);
		PrintArray(&NoDBQueryDesc.allAttributes, printColumnDescriptor);
		fprintf(stderr,"\n");
	}
}


//	 * A.B.C.D	catalog A, schema B, table C, col or func D.
static void
printColumnDescriptor(void* _columnDesc)
{
	ColumnDescriptor *columnDesc = _columnDesc;
	if(columnDesc->catalogName != NULL)
		fprintf(stderr,"<catalogName = %s>", columnDesc->catalogName);
	if(columnDesc->schemaName  != NULL)
		fprintf(stderr,"<schemaName = %s>", columnDesc->schemaName);
	if(columnDesc->tableName  != NULL)
		fprintf(stderr,"<tableName = %s>", columnDesc->tableName);
	if(columnDesc->columnName != NULL)
		fprintf(stderr,"<columnName = %s>", columnDesc->columnName);
	if(columnDesc->whole_row)
		fprintf(stderr,"<*>");
	fprintf(stderr,"\n");
}

static void
freeColumnDescriptor(void* _columnDesc)
{
	ColumnDescriptor *columnDesc = _columnDesc;
	if(columnDesc->catalogName != NULL) {
		free(columnDesc->catalogName);
		columnDesc->catalogName = NULL;
	}
	if(columnDesc->schemaName  != NULL)	{
		free(columnDesc->schemaName);
		columnDesc->schemaName = NULL;
	}
	if(columnDesc->tableName  != NULL) {
		free(columnDesc->tableName);
		columnDesc->tableName = NULL;
	}
	if(columnDesc->columnName != NULL) {
		free(columnDesc->columnName);
		columnDesc->columnName = NULL;
	}
}

static void *
pg_malloc(size_t size)
{
	void	   *result;
	result = malloc(size);
	if (!result)
	{
		write_stderr(_("pg_malloc failed in queryDescriptor.c file...\n"));
		exit(1);
	}
	return result;
}


