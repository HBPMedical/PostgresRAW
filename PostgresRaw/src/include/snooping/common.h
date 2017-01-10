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

#ifndef COMMON_H
#define COMMON_H

#include "postgres.h"

#include "snooping/global.h"


typedef struct InputFile
{
	char    filename[MAX_FILENAME];
	char    relation[MAX_RELATION_NAME];
	char    delimiter[5];
	bool    header;
	int     id;

}InputFile;

typedef struct ConfigurationFile
{
	InputFile   files[NUMBER_OF_RELATIONS];
	char        conf_file[MAX_FILENAME];
	char        *pg_data;
	char        *path;
	int         how_many;
	bool        loaded;
}ConfigurationFile;

extern char *configuration_filename;


char    *getInputFilename(char *relation);
char    *getDelimiter(char *relation);
bool    getHeader(char *relation);
bool    isBuild(void);
bool    isLoaded(void);
bool    configurationExists(void);
bool    isInConfigFile(char *relation);
void    loadEnvironment(void);
void    printConfiguration(void);
int     getFirstFreeFromMap(int *map, int size);


#endif   /* COMMON_H */

