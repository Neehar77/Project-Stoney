#include <fstream>
#include <iostream>
#include <filesystem>
#include <stdio.h>
#include "../ReaderBarcode/NewSource.h"

using namespace std;
namespace fs = std::filesystem;



barcodes_new product_barcodes_new[25];

void InitNewProductTable()
{
	// List of all 24 barcodes
	product_barcodes_new[0] = { "3 7 0 0 0 8 7 0 7 6 0 8", "Baby wipes" };
	product_barcodes_new[1] = { "1 9 9 0 0 0 0 3 3 3 2 0", "Baking powder" };
	product_barcodes_new[2] = { "0 0 0 0 0 0 4 0 1 1 2 8", "Banana" };
	product_barcodes_new[3] = { "1 2 8 0 0 5 1 7 7 2 5 8", "Batteries" };
	product_barcodes_new[4] = { "4 9 0 0 0 0 4 2 5 6 6 6", "Coke zero" };
	product_barcodes_new[5] = { "3 7 0 0 0 9 7 3 0 5 8 2", "Dish soap" };
	product_barcodes_new[6] = { "8 1 8 1 4 5 0 1 5 5 0 2", "Dog supplement" };
	product_barcodes_new[7] = { "7 0 5 0 1 1 1 1 6 1 1 4", "Face serum" };
	product_barcodes_new[8] = { "6 9 9 0 7 3 7 0 9 1 4 1", "Glucose monitoring" };
	product_barcodes_new[9] = { "3 4 0 0 0 0 0 3 1 8 1 0", "Hershey syrup" };
	product_barcodes_new[10] = { "2 2 0 0 0 0 0 6 6 7 7 0", "Juicy fruit gum" };
	product_barcodes_new[11] = { "7 9 2 8 5 0 1 1 0 0 0 7", "Lip balm" };
	product_barcodes_new[12] = { "0 0 3 0 0 0 0 0 1 7 5 6", "Nail polish" };
	product_barcodes_new[13] = { "7 8 7 4 2 0 6 0 4 3 9 0", "Oil spray" };
	product_barcodes_new[14] = { "6 4 1 4 4 0 4 3 2 2 4 4", "Pasta" };
	product_barcodes_new[15] = { "5 1 5 0 0 2 5 5 1 6 2 2", "Peanut butter" };
	product_barcodes_new[16] = { "5 2 1 0 0 0 2 9 9 2 4 4", "Pepper" };
	product_barcodes_new[17] = { "7 1 6 8 3 7 2 4 6 2 5 1", "Pepcis chews" };
	product_barcodes_new[18] = { "5 4 1 0 0 0 0 2 7 0 9 8", "Pickles" };
	product_barcodes_new[19] = { "4 7 4 0 0 6 5 8 8 9 9 0", "Razor" };
	product_barcodes_new[20] = { "4 6 5 0 0 7 3 7 9 6 1 8", "Room spray" };
	product_barcodes_new[21] = { "4 3 9 1 7 1 0 2 0 3 0 0", "Shaving kit" };
	product_barcodes_new[22] = { "5 1 0 0 0 0 0 0 1 1 8 6", "Tomato soup" };
	product_barcodes_new[23] = { "3 5 0 0 0 4 5 5 3 9 0 4", "Toothbrush" };
	product_barcodes_new[24] = { "1 6 0 0 0 2 8 8 0 1 0 6", "Vanilla dunkaroos" };
	//barcodes b25 = { "6 8 1 1 3 1 2 8 5 6 3 6", "70% alcohol 5 x 2.5 2540 dpi" };
}

//Value at the jth digit of ith barcode
void digitsNew(int barcode_number, int digit_in_barcode)
{
	if (digit_in_barcode == 0)
	{
		cout << product_barcodes_new[barcode_number].barcode[digit_in_barcode] - '0';
	}
	else
	{
		if (digit_in_barcode % 2 == 0)
			cout << product_barcodes_new[barcode_number].barcode[digit_in_barcode + 2] - '0';
		else
			cout << product_barcodes_new[barcode_number].barcode[digit_in_barcode + 1] - '0';
	}
}



/*int main()
{

	InitBarcodeTable();
	for (int bar = 0; bar < 23; bar++)
	{
		cout << product_barcodes_new[bar].barcode << endl;
		cout << product_barcodes_new[bar].name << endl;
	}
	//digits(0, 5);

}*/