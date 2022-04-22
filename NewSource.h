#pragma once
#include <string>
#include <stdio.h>


#ifndef source_h
#define source_h


typedef struct barcodes_new
{
	std::string barcode;
	std::string name;
	//string binaryvalues[12];
} barcodes_new;
extern barcodes_new product_barcodes_new[25];

void InitNewProductTable();

//Value at the jth digit of ith barcode
void digitsNew(int barcode_number, int digit_in_barcode);

#endif


