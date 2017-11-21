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


#include "snooping/policy.h"
#include "snooping/bitmap.h"

void
initializePolicy(UsageList *list, int budget, int natts)
{
	int i;
	list->budget = budget;
	list->used_elements = 0;
	list->free_elements = budget;
	list->top = 0;
	list->bottom = 0;

	if (budget > 0)
	{
		list->elements = (Record*) malloc (budget * sizeof(Record));
		for ( i = 0; i < list->budget; i++)
		{
			Record temp;
			temp.attributeID = -1;
			list->elements[i] = temp;
		}
	}
}


//Return the attributes to be cached or collected
int*
firstTimeLRU(UsageList *list, int *interestingAttributes, int natts)
{
	int i;
	int* which;
	int _temp = 0;
	int pos;
	Record newRecord;

	//Unlimited budget
	if (list->budget == -1)
		return interestingAttributes;

	//We have space
//	if (list->budget >= natts)
//		return interestingAttributes;

	/*Budget is limited*/
	//Since it's the first time we use the Storage component, let's cache the first X attributes within the budget
	which = (int*) malloc ( (natts + 1) * sizeof(int) );
	for (i = 0 ; i < (natts + 1); i++)
		which[i] = 0;
	for (i = 1 ; i <= (natts - 1); i++)
	{
		if ( _temp < list->budget && interestingAttributes[i])
		{
			which[i] = 1;
			_temp++;
		}
		else
			which[i] = 0;
	}

	//Update the reference list
	for ( i = 1 ; i <= (natts - 1); i++)
	{
		if ( which[i] )
		{
			newRecord.attributeID = i;
			pos = insertRecord(list, newRecord);
			updateCounters(list, pos);
			list->top = list->used_elements;
		}
	}


	return which;
}

int
insertRecord(UsageList *list, Record newRecord)
{
	int  i = -1;
	if (list->used_elements < list->budget )
	{
		for ( i = 0; i < list->budget; i++)
		{
			if ( list->elements[i].attributeID == -1 )
			{
				list->elements[i] = newRecord;
				list->used_elements++;
				list->free_elements--;
				break;
			}
		}
	}

	return i;
}

void
deleteRecord(UsageList *list, Record deleteRecord)
{
	int  i;
	for ( i = 0; i < list->used_elements; i++)
	{
		if ( list->elements[i].attributeID == deleteRecord.attributeID )
		{
			list->elements[i].attributeID = -1;
			list->used_elements--;
			list->free_elements++;
			break;
		}
	}
}

void
deleteRecordAtPos(UsageList *list, int pos)
{
	int  i;
	Record temp;

	list->elements[pos].attributeID = -1;
	for ( i = pos ; i < list->top; i++)
	{
		temp = list->elements[i];
		list->elements[i] = list->elements[i + 1];
		list->elements[i + 1] = temp;
	}
}



int
firstNotNeeded(UsageList *list, int *interestingAttributes, int natts)
{
	int i;
//	int j;
//	bool found;
	for ( i = list->bottom; i < list->used_elements; i++)
	{
		if (!interestingAttributes[list->elements[i].attributeID])
				return i;

//		for ( j = 0; j < natts; j++)
//		{
//			if (!interestingAttributes[j])
//				return i;
//		}
	}
	return -1;
}


void
printUsageList(UsageList list)
{
	int i;
	fprintf(stderr,"\nPrinting Usage List\n");
	fprintf(stderr,"Used pages = %d - Free pages = %d - Budget = %d\n",list.used_elements, list.free_elements,list.budget);
	fprintf(stderr,"Bottom = %d - Top = %d\n",list.bottom, list.top);
	fprintf(stderr,"Elements: { ");
	for (i = 0; i < list.budget; i++)
		fprintf(stderr, "%d ",list.elements[i].attributeID);
	fprintf(stderr," }\n");
}



/*
 *
 * Replacement policy:
 * a) Get the BitMap + the interesting attributes
 * if they don't fit
 *
 * Get the interesting positions for query X
 * Examine
 *
 */
//Return the columns or attributes to be cached
int*
applyPolicyCache(UsageList *list, BitMap *bitmap, int *neededColumns, int natts, int *deletedColumns)
{
	int i;
	int j;
//	int temp;
	int pos;
	bool found;
	int *cacheCandidates;
	int numOfCacheCandidates = 0;
	cacheCandidates = (int*) malloc ( natts * sizeof(int));

	for ( i = 0 ; i < natts; i++)
	{
		cacheCandidates[i] = 0;
		deletedColumns[i] = 0;
	}

//	temp = 0;
	for ( i = 0 ; i < natts; i++)
	{
		if( neededColumns[i] )
			if( !getBit(bitmap,i) )
			{
				cacheCandidates[i] = 1;
				numOfCacheCandidates++;
			}
	}


	if(numOfCacheCandidates == 0)
	{
		/*Just update the counters for the needed attributes in the cache*/
		for ( i = 0 ; i < natts; i++)
		{
			if (neededColumns [i])
				if( getBit(bitmap,i) )
					for ( j = 0 ; j < list->used_elements; j++)
					{
						if (list->elements[j].attributeID == i)
						{
							updateCounters(list, j);
							break;
						}
					}
		}

		return cacheCandidates;
	}

	//Which --> candidate to be cached
	for ( i = 0 ; i < natts; i++)
	{
		Record newRecord;
		if (neededColumns [i])
		{
			if ( list->budget == list->used_elements)
			{//Cache is full, replace sth
				found  = false;
				for ( j = 0 ; j < list->used_elements; j++)
				{
					if (list->elements[j].attributeID == i)
					{
						updateCounters(list, j);
						found = true;
						break;
					}
				}

				if ( !found )
				{
					pos = firstNotNeeded(list, neededColumns, natts);
					if ( pos != -1)
					{
						newRecord.attributeID = i;
						deletedColumns[list->elements[pos].attributeID] = 1;
						deleteRecord(list, list->elements[pos]);
						pos  = insertRecord(list, newRecord);
						updateCounters(list, pos);
						list->top = list->used_elements;
					}
					else
						cacheCandidates[i] = 0;
				}
			}
			else
			{//We have space in the Cache
				found  = false;
				for ( j = 0 ; j < list->used_elements; j++)
				{
					if (list->elements[j].attributeID == i)
					{
						updateCounters(list, j);
						found = true;
						break;
					}
				}
				if ( !found )
				{
					newRecord.attributeID = i;
					pos = insertRecord(list, newRecord);
					updateCounters(list, pos);
					list->top = list->used_elements;
				}
			}
		}
	}

	return cacheCandidates;
}

int*
applyPolicyPositionalMap(UsageList *list, BitMap *bitmap, int *neededMetapointers, int natts, int *deletedMetapointers)
{
	int i;
	int j;
//	int temp;
	int pos;
	bool found;
	int *cacheCandidates;
	int numOfCacheCandidates = 0;
	cacheCandidates = (int*) malloc ( (natts + 1) * sizeof(int));

	for ( i = 0 ; i < (natts + 1); i++)
	{
		cacheCandidates[i] = 0;
		deletedMetapointers[i] = 0;
	}

//	temp = 0;
	for ( i = 1 ; i <= (natts - 1); i++)
	{
		if( neededMetapointers[i] )
			if( !getBit(bitmap,i) )
			{
				cacheCandidates[i] = 1;
				numOfCacheCandidates++;
			}
	}


	if(numOfCacheCandidates == 0)
	{
		/*Just update the counters for the needed attributes in the cache*/
		for ( i = 1 ; i <= (natts - 1); i++)
		{
			if (neededMetapointers [i])
				if( getBit(bitmap,i) )
					for ( j = 0 ; j < list->used_elements; j++)
					{
						if (list->elements[j].attributeID == i)
						{
							updateCounters(list, j);
							break;
						}
					}
		}

		return cacheCandidates;
	}

	//Which --> candidate to be cached
	for ( i = 1 ; i <= (natts - 1); i++)
	{
		Record newRecord;
		if (neededMetapointers[i])
		{
			if ( list->budget == list->used_elements)
			{//Cache is full, replace sth
				found  = false;
				for ( j = 0 ; j < list->used_elements; j++)
				{
					if (list->elements[j].attributeID == i)
					{
						updateCounters(list, j);
						found = true;
						break;
					}
				}

				if ( !found )
				{
//					pos = firstNotNeeded(list, neededMetapointers+1, natts-1);
					pos = firstNotNeeded(list, neededMetapointers, natts);
					if ( pos != -1)
					{
						newRecord.attributeID = i;
						deletedMetapointers[list->elements[pos].attributeID] = 1;
						deleteRecord(list, list->elements[pos]);
						pos  = insertRecord(list, newRecord);
						updateCounters(list, pos);
						list->top = list->used_elements;
					}
					else
						cacheCandidates[i] = 0;
				}
			}
			else
			{//We have space in the Cache
				found  = false;
				for ( j = 0 ; j < list->used_elements; j++)
				{
					if (list->elements[j].attributeID == i)
					{
						updateCounters(list, j);
						found = true;
						break;
					}
				}
				if ( !found )
				{
					newRecord.attributeID = i;
					pos = insertRecord(list, newRecord);
					updateCounters(list, pos);
					list->top = list->used_elements;
				}
			}
		}
	}

	return cacheCandidates;
}


void
updateCounters(UsageList *list, int pos)
{
	int i;
	Record temp;

	for ( i = pos ; i < (list->top - 1); i++)
	{
		if ( list->elements[i].attributeID != -1 )
		{
			temp = list->elements[i];
			list->elements[i] = list->elements[i + 1];
			list->elements[i + 1] = temp;
		}
	}
}













