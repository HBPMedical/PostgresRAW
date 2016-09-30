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

#ifndef BITMAP_H_
#define BITMAP_H_

//Updated version
typedef struct BitMap
{
	int size;
	unsigned short int* bitmap;
} BitMap;

BitMap *createBitMap(int initialSize);
void freeBitMap(BitMap *bitmap);
BitMap *copyBitMap(BitMap *input);
void copyToBitMap(BitMap input, BitMap *output);
void copyToBitMap2(int* input, int size, BitMap *output);

void setBit(BitMap *bitmap, int n);
//inline void setBit(BitMap *bitmap, int n);
void setBitValue(BitMap *bitmap, int n, int value);
void clearBit(BitMap *bitmap, int n);
int getBit(BitMap *bitmap, int n);
//inline int getBit(BitMap *bitmap, int n);
void flipBit(BitMap *bitmap, int n);
void printBitMap(BitMap bitmap);


/*
typedef struct BitMap
{
	int size;
	unsigned char* bitmap;
	int NoBytes;
} BitMap;

BitMap createBitMap(int initialSize);
void freeBitMap(BitMap *bitmap);
BitMap copyBitMap(BitMap input);
void copyToBitMap(BitMap input, BitMap *output);
//void setBit(BitMap *bitmap, int n);
inline void setBit(BitMap *bitmap, int n);
void setBitValue(BitMap *bitmap, int n, int value);
void clearBit(BitMap *bitmap, int n);
//int getBit(BitMap bitmap, int n);
inline int getBit(BitMap bitmap, int n);
void flipBit(BitMap *bitmap, int n);
void printBitMap(BitMap bitmap);
*/



#endif /* BITMAP_H_ */
