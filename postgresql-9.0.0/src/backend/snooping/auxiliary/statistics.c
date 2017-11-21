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



#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "snooping/statistics.h"

/*
Statistics statistics;
bool isInit = false;

void initStatistics(void)
{
	isInit = true;
	statistics.bytesReadFromDisk = 0.0;
	statistics.bytesParsedForEOL = 0.0;
	statistics.bytesParsedForAttributes = 0.0;
	statistics.callRealloc = 0.0;
	statistics.callMalloc = 0.0;
}

void statsDISK(long bytesReadFromDisk)
{
	statistics.bytesReadFromDisk += bytesReadFromDisk;
}

void statsEOL(long bytesParsedForEOL)
{
	statistics.bytesParsedForEOL += bytesParsedForEOL;
}

void statsAttributes(long bytesParsedForAttributes)
{
	statistics.bytesParsedForAttributes += bytesParsedForAttributes;
}

void statsRealloc(long add)
{
	statistics.callRealloc += add;
}

void statsMalloc(long add)
{
	statistics.callMalloc += add;
}


void printStatistics(void)
{
	fprintf(stderr,"---   Statistics   ---\n");
	fprintf(stderr,"MBytes read from disk = %lf\n", statistics.bytesReadFromDisk / (1024 * 1024) );
	fprintf(stderr,"MBytes read for EOL = %lf\n", statistics.bytesParsedForEOL / (1024 * 1024) );
	fprintf(stderr,"MBytes read for attributes = %lf\n", statistics.bytesParsedForAttributes / (1024 * 1024));
	fprintf(stderr,"MBytes saved from EOL = %lf\n", (statistics.bytesReadFromDisk - statistics.bytesParsedForAttributes) / (1024 * 1024) );
	fprintf(stderr,"MBytes saved from Internal = %lf\n", (statistics.bytesReadFromDisk - statistics.bytesParsedForEOL) / (1024 * 1024) );
	fprintf(stderr,"Times Realloc called = %lf\n", statistics.callRealloc);
	fprintf(stderr,"Times Malloc called = %lf\n", statistics.callMalloc);
}
*/








