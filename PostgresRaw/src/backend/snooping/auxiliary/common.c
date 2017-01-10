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

#include "postgres.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


#include "snooping/common.h"
//#include "snooping/metadata.h"




/* From pg_ctl.c */
static char **readfile(const char *path);
static char *xstrdup(const char *s);
static void *pg_malloc(size_t size);

static int parse(char *buf, char **args);

void initialiazeConfiguration(void);
//void printConfiguration(void);
int checkConfiguration(void);
int searchSlotInConfiguration(int value);


/*Global variables*/
//static char *build = NULL;
char *configuration_filename = "snoop.conf";

//Load information from snoop.conf file
//Mapping data-file, relation, delimiter
ConfigurationFile CF;



char *getInputFilename(char *relation)
{
	int i;
	if(CF.loaded != true)
		return NULL;
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++){
		if( CF.files[i].id != -1)
		{
			if(strcmp(CF.files[i].relation, relation) == 0)
				return CF.files[i].filename;
		}
	}
	return NULL;
}

char *getDelimiter(char *relation)
{
	int i;
	if(CF.loaded != true)
		return NULL;
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++){
		if( CF.files[i].id != -1)
		{
			if(strcmp(CF.files[i].relation, relation) == 0)
				return CF.files[i].delimiter;
		}
	}
	return NULL;
}

bool getHeader(char *relation)
{
	int i;
	if(CF.loaded != true)
		return false;
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++){
		if( CF.files[i].id != -1)
		{
			if(strcmp(CF.files[i].relation, relation) == 0)
				return CF.files[i].header;
		}
	}
	return false;
}
/*
bool isBuild(void)
{
	Assert( build != NULL);
	if(strcmp(build,"0") == 0)
		return false;
	else
		return true;
}
*/
bool isLoaded(void)
{
	return CF.loaded;
}

bool configurationExists(void)
{
	FILE *infile;
	char buf[MAX_PATH_LENGTH];
	char cwd[MAX_PATH_LENGTH];

	getcwd(cwd, MAX_PATH_LENGTH);
	Assert(cwd != NULL);

	snprintf(buf, MAX_PATH_LENGTH, "%s/%s", cwd, configuration_filename);

	if ((infile = fopen(buf, "r")) == NULL){
//		fprintf(stderr, "File %s not found...",buf);
		return false;
	}
	fclose(infile);
	return true;
}

void
initialiazeConfiguration(void)
{
	int i = 0;
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++)
	{
		strcpy(CF.files[i].filename, "\0");
		strcpy(CF.files[i].relation, "\0");
		strcpy(CF.files[i].delimiter, "\0");
		CF.files[i].id = -1;
	}
	CF.how_many = 0;
	CF.loaded = false;
}

void
printConfiguration(void)
{
	int i = 0;

	fprintf(stdout,"\nPrint configuration information:\n");
	fprintf(stdout,"Configuration filename: %s\n", CF.conf_file);
	fprintf(stdout,"PostgreSQL data folder : %s\n", CF.pg_data);
	fprintf(stdout,"PostgreSQL path : %s\n", CF.path);
//	fprintf(stdout,"Loaded : %d\n", CF.loaded);
	fprintf(stdout,"Available pointers to files: %d\n", CF.how_many);
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++)
	{
		if( CF.files[i].id != -1)
		{
//			fprintf(stdout,"ID : {%d} Pos = {%d}\n", CF.files[i].id, i);
			fprintf(stdout,"%d) File: %s\n",(i+1), CF.files[i].filename);
			fprintf(stdout,"Relation: %s\n", CF.files[i].relation);
			fprintf(stdout,"Delimiter : %s\n", CF.files[i].delimiter);
			fprintf(stdout,"Header : %s\n", CF.files[i].header ? "True" : "False");
			fflush(stdout);
		}
	}
	fprintf(stdout,"\n\n");
}

int
checkConfiguration(void)
{
	int i = 0;
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++)
		if( CF.files[i].id != -1)
			if ( strlen(CF.files[i].filename) == 0 || strlen(CF.files[i].relation) == 0 || strlen(CF.files[i].delimiter) == 0)
				return -1;
	return 1;
}


int
searchSlotInConfiguration(int value)
{
	int i = 0;
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++)
		if( CF.files[i].id == value)
			return i;
	for ( i = 0; i < NUMBER_OF_RELATIONS; i++)
		if( CF.files[i].id == -1)
		{
			CF.how_many++;
			return i;
		}
	return -1;
}

bool
isInConfigFile(char *relation)
{
	int i = 0 ;
	for( i = 0; i < NUMBER_OF_RELATIONS; i++)
	{
		if (CF.files[i].id != -1)
		{
			if( strcmp (CF.files[i].relation, relation) == 0)
				return true;
		}
	}
	return false;
}


/*
 * Load environmental parameters for SnoopDB filename: CF.path/CF.pg_data/configuration_filename
 *
 * Parameters: input filename, relation, delimiter, header
 */
void
loadEnvironment(void)
{
	char **result;
	char buf[MAX_PATH_LENGTH];
	int size = 0;

	initialiazeConfiguration();

	CF.pg_data = getenv("PGDATA");
	size = strlen(CF.pg_data);

	getcwd(buf, MAX_PATH_LENGTH);
	CF.path = (char*) pg_malloc ( MAX_PATH_LENGTH * sizeof(char));
	strcpy(CF.path, buf);
	CF.path[strlen(CF.path) - size] = '\0';

	Assert(CF.pg_data != NULL);

	//TODO: change how I locate the snoop.conf file... (need for more general solution)
	if (CF.pg_data)
	{
		CF.pg_data = xstrdup(CF.pg_data);
//		snprintf(conf_file, MAX_PATH_LENGTH, "%s/snoop.conf", pg_data);
		//Read configuration file not from data but from PostreSQL directory
//		snprintf(CF.conf_file, MAX_PATH_LENGTH, "%s/snoop.conf", CF.path);
		snprintf(CF.conf_file, MAX_PATH_LENGTH, "%s/%s/%s", CF.path,CF.pg_data,configuration_filename);
	}

	result = readfile(CF.conf_file);
	if (result != NULL)
		for (; *result != NULL; result++)
			if(strlen(*result) > 1)	//Consider "\n"
			{
				//Up to 16 tokens per line in the configuration file
				char *args[16];
				if ( parse(*result, args) == 3)
				{
					int size = strlen(args[2]);
					if(strncmp(args[0], "filename-", 9) == 0 && strcmp(args[1], "=") == 0)
					{
						int val = atoi(args[0] + 9);
						int pos = searchSlotInConfiguration(val);
						if( pos != -1)
						{
							strncpy(CF.files[pos].filename, args[2] + 1, size - 1);
							CF.files[pos].filename[size - 2] = '\0';
							CF.files[pos].id = val;
						}
					}
					else if(strncmp(args[0], "relation-" ,9) == 0 && strcmp(args[1], "=") == 0)
					{
						int val = atoi(args[0] + 9);
						int pos = searchSlotInConfiguration(val);
						int tmp;
						if( pos != -1)
						{
							strncpy(CF.files[pos].relation, args[2] + 1, size - 1);
							CF.files[pos].relation[size - 2] = '\0';
							for ( tmp = 0; tmp < strlen(CF.files[pos].relation); tmp++)
								CF.files[pos].relation[tmp] = tolower(CF.files[pos].relation[tmp]);
							CF.files[pos].id = val;
						}
					}
					else if(strncmp(args[0], "delimiter-", 10) == 0 && strcmp(args[1], "=") == 0)
					{
						int val = atoi(args[0] + 10);
						int pos = searchSlotInConfiguration(val);
						if( pos != -1)
						{
							strncpy(CF.files[pos].delimiter, args[2] + 1, size - 1);
							CF.files[pos].delimiter[size - 2] = '\0';
							CF.files[pos].id = val;
						}
					}
					else if(strncmp(args[0], "header-", 7) == 0 && strcmp(args[1], "=") == 0)
					{
						int val = atoi(args[0] + 7);
						int pos = searchSlotInConfiguration(val);
						if( pos != -1)
						{
							char arg2[size-1];
							strncpy(arg2, args[2] + 1, size - 1);
							arg2[size - 2] = '\0';
							if (strcmp( arg2, "True") == 0){
								CF.files[pos].header = true;
							}
							else {
								CF.files[pos].header = false;
							}
							CF.files[pos].id = val;
						}
					}
//					else if(strcmp(args[0], "build") == 0 && strcmp(args[1], "=") == 0)
//					{
//						build = (char*) pg_malloc ( (size - 1) * sizeof(char));
//						strncpy(build, args[2] + 1, size - 1);
//						build[size - 2] = '\0';
//					}
				}
			}
	CF.loaded = true;

	Assert( CF.how_many < NUMBER_OF_RELATIONS);
	Assert( checkConfiguration() != -1);

	//printConfiguration();
}


static char *
xstrdup(const char *s)
{
	char	   *result;

	result = strdup(s);
	if (!result)
	{
//		write_stderr(_("%s: out of memory\n"), progname);
		exit(1);
	}
	return result;
}

static void *
pg_malloc(size_t size)
{
	void	   *result;

	result = malloc(size);
	if (!result)
	{
		write_stderr(_("pg_malloc failed in common.c file...\n"));
		exit(1);
	}
	return result;
}
/*
 * get the lines from a text file - return NULL if file can't be opened
 */
static char **
readfile(const char *path)
{
	FILE	   *infile;
	int			maxlength = 1,
				linelen = 0;
	int			nlines = 0;
	char	  **result;
	char	   *buffer;
	int			c;

	if ((infile = fopen(path, "r")) == NULL){
		fprintf(stderr, "File %s not found...",path);
		return NULL;
	}
	/* pass over the file twice - the first time to size the result */

	while ((c = fgetc(infile)) != EOF)
	{
		linelen++;
		if (c == '\n')
		{
			nlines++;
			if (linelen > maxlength)
				maxlength = linelen;
			linelen = 0;
		}
	}

	/* handle last line without a terminating newline (yuck) */
	if (linelen)
		nlines++;
	if (linelen > maxlength)
		maxlength = linelen;

	/* set up the result and the line buffer */
	result = (char **) pg_malloc((nlines + 1) * sizeof(char *));
	buffer = (char *) pg_malloc(maxlength + 1);

	/* now reprocess the file and store the lines */
	rewind(infile);
	nlines = 0;
	while (fgets(buffer, maxlength + 1, infile) != NULL)
		result[nlines++] = xstrdup(buffer);

	fclose(infile);
	free(buffer);
	result[nlines] = NULL;

	return result;
}



int
parse(char *buf, char **args)
{
	int counter = 0;
	while (*buf != '\0')
	{
		while ((*buf == ' ') || (*buf == '\t') || (*buf == '\n'))
			*buf++ = '\0';
		*args++ = buf;
		counter++;
		while ((*buf != '\0') && (*buf != ' ') && (*buf != '\t') && (*buf != '\n'))
			buf++;
	}
	*--args = NULL;
	counter--;
	return counter;
}

int
getFirstFreeFromMap(int *map, int size)
{
	int i = 0;
	for (i = 0; i < size; i++)
	{
		if(map[i] == -1)
			return i;
	}
	return -1;
}


