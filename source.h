#include <string>
#include <stdio.h>


#ifndef source_h
#define source_h


typedef struct barcodes
{
	std::string barcode;
	std::string name;
	//string binaryvalues[12];
} barcodes;
extern barcodes product_barcodes[25];

 void InitProductTable();

 //Value at the jth digit of ith barcode
 void digits(int barcode_number, int digit_in_barcode);

#endif


