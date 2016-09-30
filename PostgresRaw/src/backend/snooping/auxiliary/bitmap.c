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


#include "snooping/bitmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


BitMap *createBitMap(int initialSize)
{
	BitMap *bitmap;

	bitmap = (BitMap*)malloc (sizeof(BitMap));
	bitmap->bitmap=(unsigned short int*)calloc(initialSize, sizeof(unsigned short int));
	bitmap->size=initialSize;
	return bitmap;
}

void freeBitMap(BitMap *bitmap)
{
	free(bitmap->bitmap);
	bitmap->bitmap = NULL;
}


BitMap *copyBitMap(BitMap *input)
{
	BitMap *output;
	output = (BitMap*) malloc (sizeof(BitMap));
	output->bitmap = (unsigned short int*)calloc(output->size, sizeof(unsigned short int));
	output->size = input->size;

	memcpy(output->bitmap, input->bitmap, input->size * sizeof(unsigned short int));
//	for ( i = 0; i < output->size; i++)
//		output->bitmap[i] = input->bitmap[i];

	return output;
}


void copyToBitMap(BitMap input, BitMap *output)
{
	int i;
	output->size = input.size;
	for ( i = 0; i < output->size; i++)
		output->bitmap[i] = input.bitmap[i];
}

void copyToBitMap2(int* input, int size, BitMap *output)
{
	int i;
	output->size = size + 1;
	output->bitmap[0] = 1;
	output->bitmap[size] = 1;
	for ( i = 1; i < size ; i++)
		output->bitmap[i] = 0;

	for ( i = 1; i < size ; i++)
		if(input[i - 1] != -1)
			output->bitmap[input[i - 1] + 1] = 1;

}


void setBit(BitMap *bitmap, int n)
{
	bitmap->bitmap[n] = 1;
}


void setBitValue(BitMap *bitmap, int n, int value)
{
	if (value)
		setBit(bitmap, n);
	else
		clearBit(bitmap,n);
}


void clearBit(BitMap *bitmap, int n)
{
	bitmap->bitmap[n] = 0;
}

int getBit(BitMap *bitmap, int n)
{
	return bitmap->bitmap[n];
}


void flipBit(BitMap *bitmap, int n)
{
	if (bitmap->bitmap[n])
		bitmap->bitmap[n] = 0;
	else
		bitmap->bitmap[n] = 1;
}


void printBitMap(BitMap bitmap)
{
	int i;
	fprintf(stderr, "Bitmap: ");
	for (i = 0; i < bitmap.size; i++)
		fprintf(stderr, "%d ", getBit(&bitmap, i));
	fprintf(stderr, "\n");
}

/*
BitMap createBitMap(int initialSize)
{
	BitMap bitmap;

	bitmap.NoBytes=(int)ceil( ((double)initialSize) / 8.);
	bitmap.bitmap=(unsigned char*)calloc(bitmap.NoBytes, 1);
	bitmap.size=initialSize;

	return bitmap;
}

void freeBitMap(BitMap *bitmap)
{
	free(bitmap->bitmap);
	bitmap->bitmap = NULL;
}

BitMap copyBitMap(BitMap input)
{
	int i;
	BitMap output;
	output.NoBytes = input.NoBytes;
	output.bitmap = (unsigned char*)calloc(output.NoBytes, 1);
	output.size = input.size;

	for ( i = 0; i < output.NoBytes; i++)
		output.bitmap[i] = input.bitmap[i];

	return output;
}

void copyToBitMap(BitMap input, BitMap *output)
{
	int i;
	output->NoBytes = input.NoBytes;
	output->size = input.size;

	for ( i = 0; i < output->NoBytes; i++)
		output->bitmap[i] = input.bitmap[i];
}


inline void setBit(BitMap *bitmap, int n)
{
//	unsigned char mask = 1 << (7 - (n % 8));
	bitmap->bitmap[n >> 3] |= (1 << (7 - (n % 8)));

}


//void setBit(BitMap *bitmap, int n)
//{
//	unsigned char mask = 1 << (7 - (n % 8));
//	bitmap->bitmap[(int)n/8] |= mask;
//
//}

void setBitValue(BitMap *bitmap, int n, int value)
{
	if (value)
		setBit(bitmap, n);
	else
		clearBit(bitmap,n);
}


void clearBit(BitMap *bitmap, int n)
{
	unsigned char mask = 1 << (7 - (n % 8));
	bitmap->bitmap[(int)n/8] &= ~mask;

}

inline int getBit(BitMap bitmap, int n)
{
	return ((bitmap.bitmap[n >> 3] & (1 << (7 - (n % 8)))) != 0) ? 1 : 0;
}

//int getBit(BitMap bitmap, int n)
//{
////	unsigned char mask = 1 << (7 - n);
//	unsigned char mask = 1 << (7 - (n % 8));
//
//	if ((bitmap.bitmap[(int)n/8] & mask) != 0)
//		return 1;
//	else
//		return 0;
//
//}

void flipBit(BitMap *bitmap, int n)
{
	unsigned char mask = 1 << (7 - (n % 8));
	bitmap->bitmap[(int)n/8] ^= mask;
}


void printBitMap(BitMap bitmap)
{
	int i;
	fprintf(stderr, "Bitmap: ");
	for (i = 0; i < bitmap.size; i++)
		fprintf(stderr, "%d ", getBit(bitmap, i));
	fprintf(stderr, "\n");
}
*/


