#include "timing.h"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//#include <zbar.h>
#include <vector>
#include <string>
#include <math.h>
#include "dc_image.h"
//#include "source.h"
#include "NewSource.h"
#include <stdio.h>

//#include <experimental/iterator>
#include <list>
#include <iostream>
#include <aspose_ocr.h>

#define MIN(a,b) ( (a)<(b) ? (a) : (b) )
#define MAX(a,b) ( (a)>(b) ? (a) : (b) )

using namespace std;
using namespace cv;

typedef struct ContigSeg
{
    unsigned int first, last, sum;
} ContigSeg;

typedef struct LineSeg
{
    ContigSeg start;    // The starting segment
    ContigSeg longest;  // The longest segment
    ContigSeg end;      // The ending segment
} LineSeg;

typedef struct K_Radon
{
    LineSeg**** T;
    unsigned int N; // N = width =height  populate this with row/height of the image
    int L; //level
    int K; //skew
 //4
    int S; // stack
    int F; //Offset
    int H; //height=2^L
}K_Radon;

typedef struct XY {
    int x, y, npeaks;
} XY;

typedef struct Threshold
{
    float best_thres;
    float score;
} Threshold;

typedef struct values
{
    vector<int> result;
    vector<int> val;
    bool rot = 0;
} values;

typedef struct bars
{
    vector<int> res;
    int numValid;
    //bool rot = 0;
}bars;
#define MAX_INT 0x7fffffff
#define Grid_size 95
#define byte unsigned char
#define Const 255/(log(1+255))

#define TO_BYTE(a,b,c,d,e,f,g)    (  (((byte)(a))<<6) | (((byte)(b))<<5) | (((byte)(c))<<4) | (((byte)(d))<<3) | (((byte)(e))<<2) | (((byte)(f))<<1) | (((byte)(g))<<1)  )

signed char barcode_table[256];

// Describes digits for writing barcodes on the left side of the gaurd
int barcode_bars[10][7] = {
    {0, 1, 0, 0, 1, 1, 1},  // zero
    {0, 1, 1, 0, 0, 1, 1},  // one
    {0, 0, 1, 1, 0, 1, 1},  // two
    {0, 1, 0, 0, 0, 0, 1},  // three
    {0, 0, 1, 1, 1, 0, 1},  // four
    {0, 1, 1, 1, 0, 0, 1},  // five
    {0, 0, 0, 0, 1, 0, 1},  // six
    {0, 0, 1, 0, 0, 0, 1},  // seven
    {0, 0, 0, 1, 0, 0, 1},  // eight
    {0, 0, 1, 0, 1, 1, 1}   // nine
};

void DrawBarcodeToArray(int64* out, string digits)
{
    int b=0, i, d;

    // Draw the start guard
    out[b++] = 1.0f;
    out[b++] = 0.0f;
    out[b++] = 1.0f;

    // Draw the first six digits
    int sidx = 0;
    for (d=0; d<6; d++) {
        while (digits[sidx] == ' ') { sidx++; }    // Skip whitespace
        char cdigit = digits[sidx++];              // Character of digit
        int digit   = cdigit - '0';                // Integer of digit
        
        //printf("digit index %d, digit '%c' %d\n", d, cdigit, digit);

        // Fill in the bars
        for (i = 0; i < 7; i++) {
            out[b++] = 1-barcode_bars[digit][6-i];
            //printf("bar[%d] %lld\n", b-1, out[b-1]);
        }
    }

    // Draw the middle guard
    out[b++] = 0.0f;
    out[b++] = 1.0f;
    out[b++] = 0.0f;
    out[b++] = 1.0f;
    out[b++] = 0.0f;

    // Draw the last six digits
    for (; d < 12; d++) {
        while (digits[sidx] == ' ') { sidx++; }    // Skip whitespace
        char cdigit = digits[sidx++];              // Character of digit
        int digit = cdigit - '0';                // Integer of digit

        //printf("digit index %d, digit '%c' %d\n", d, cdigit, digit);

        // Fill in the bars
        for (i = 0; i < 7; i++) {
            out[b++] = barcode_bars[digit][6-i];
            //printf("bar[%d] %lld\n", b-1, out[b-1]);
        }
    }

    // Draw the end guard
    out[b++] = 1.0f;
    out[b++] = 0.0f;
    out[b++] = 1.0f;

}

void FlipBars(int64* out, int64* in) {
    int i;
    for (i = 0; i < 95; i++) {
        out[i] = in[95 - i - 1];
    }
}

//Initializing barcode table to its values
void InitBarcodeTable()
{
    int i;

    // By default all vaules are invalid
    for (i = 0; i < 256; i++)
        barcode_table[i] = -1;

    // But certain values are valid
    barcode_table[TO_BYTE(0, 0, 0, 1, 1, 0, 1)] = 0;        //  zero 
    barcode_table[TO_BYTE(1, 1, 1, 0, 0, 1, 0)] = 0;        //  zero_1 
    barcode_table[TO_BYTE(1, 0, 1, 1, 0, 0, 0)] = 0;        //  zero_rotate 
    barcode_table[TO_BYTE(0, 1, 0, 0, 1, 1, 1)] = 0;        //  zero_1_rotate 
    barcode_table[TO_BYTE(0, 0, 1, 1, 0, 0, 1)] = 1;        //  one 
    barcode_table[TO_BYTE(1, 0, 0, 1, 1, 0, 0)] = 1;        //  one_rotate 
    barcode_table[TO_BYTE(1, 1, 0, 0, 1, 1, 0)] = 1;        //  one_1 
    barcode_table[TO_BYTE(0, 1, 1, 0, 0, 1, 1)] = 1;        //  one_1_rotate 
    barcode_table[TO_BYTE(0, 0, 1, 0, 0, 1, 1)] = 2;        //  two 
    barcode_table[TO_BYTE(1, 1, 0, 1, 1, 0, 0)] = 2;        //  two_1 
    barcode_table[TO_BYTE(1, 1, 0, 0, 1, 0, 0)] = 2;        //  two_rotate 
    barcode_table[TO_BYTE(0, 0, 1, 1, 0, 1, 1)] = 2;        //  two_1_rotate 
    barcode_table[TO_BYTE(0, 1, 1, 1, 1, 0, 1)] = 3;        //  three 
    barcode_table[TO_BYTE(1, 0, 0, 0, 0, 1, 0)] = 3;        //  three_1 
    barcode_table[TO_BYTE(1, 0, 1, 1, 1, 1, 0)] = 3;        //  three_rotate 
    barcode_table[TO_BYTE(0, 1, 0, 0, 0, 0, 1)] = 3;        //  three_1_rotate 
    barcode_table[TO_BYTE(0, 1, 0, 0, 0, 1, 1)] = 4;        //  four 
    barcode_table[TO_BYTE(1, 0, 1, 1, 1, 0, 0)] = 4;        //  four_1 
    barcode_table[TO_BYTE(1, 1, 0, 0, 0, 1, 0)] = 4;        //  four_rotate 
    barcode_table[TO_BYTE(0, 0, 1, 1, 1, 0, 1)] = 4;        //  four_1_rotate 
    barcode_table[TO_BYTE(0, 1, 1, 0, 0, 0, 1)] = 5;        //  five 
    barcode_table[TO_BYTE(1, 0, 0, 1, 1, 1, 0)] = 5;        //  five_1 
    barcode_table[TO_BYTE(1, 0, 0, 0, 1, 1, 0)] = 5;        //  five_rotate 
    barcode_table[TO_BYTE(0, 1, 1, 1, 0, 0, 1)] = 5;        //  five_1_rotate 
    barcode_table[TO_BYTE(0, 1, 0, 1, 1, 1, 1)] = 6;        //  six 
    barcode_table[TO_BYTE(1, 0, 1, 0, 0, 0, 0)] = 6;        //  six_1 
    barcode_table[TO_BYTE(1, 1, 1, 1, 0, 1, 0)] = 6;        //  six_rotate 
    barcode_table[TO_BYTE(0, 0, 0, 0, 1, 0, 1)] = 6;        //  six_1_rotate 
    barcode_table[TO_BYTE(0, 1, 1, 1, 0, 1, 1)] = 7;        //  seven 
    barcode_table[TO_BYTE(1, 0, 0, 0, 1, 0, 0)] = 7;        //  seven_1 
    barcode_table[TO_BYTE(1, 1, 0, 1, 1, 1, 0)] = 7;        //  seven_rotate 
    barcode_table[TO_BYTE(0, 0, 1, 0, 0, 0, 1)] = 7;        //  seven_1_rotate 
    barcode_table[TO_BYTE(0, 1, 1, 0, 1, 1, 1)] = 8;        //  eight 
    barcode_table[TO_BYTE(1, 0, 0, 1, 0, 0, 0)] = 8;        //  eight_1 
    barcode_table[TO_BYTE(1, 1, 1, 0, 1, 1, 0)] = 8;        //  eight_rotate 
    barcode_table[TO_BYTE(0, 0, 0, 1, 0, 0, 1)] = 8;        //  eight_1_rotate 
    barcode_table[TO_BYTE(0, 0, 0, 1, 0, 1, 1)] = 9;        //  nine 
    barcode_table[TO_BYTE(1, 1, 1, 0, 1, 0, 0)] = 9;        //  nine_1 
    barcode_table[TO_BYTE(1, 1, 0, 1, 0, 0, 0)] = 9;        //  nine_rotate 
    barcode_table[TO_BYTE(0, 0, 1, 0, 1, 1, 1)] = 9;        //  nine_1_rotate 
}

Mat cropped(int start_x, int end_x, int y, int height, Mat img)
{
    Rect crop_region(start_x, y, end_x - start_x, y + height);
    return img(crop_region);
}
/*
bars barcodeDecode(vector<int> temp)
{
    bars B;
    B.rot = 0;
    B.res = -1;
    //Left side codes
    vector<int> zero = {0, 0, 0, 1, 1, 0, 1 };
    vector<int> zero_1 = { 1, 1, 1, 0, 0, 1, 0 };
    vector<int> zero_rotate = { 1, 0, 1, 1, 0, 0, 0 };
    vector<int> zero_1_rotate = { 0, 1, 0, 0, 1, 1, 1 };
    vector<int> one = { 0, 0, 1, 1, 0, 0 ,1 };
    vector<int> one_rotate = { 1, 0, 0, 1, 1, 0, 0 };
    vector<int> one_1 = { 1, 1, 0, 0, 1, 1 ,0 };
    vector<int> one_1_rotate = { 0, 1, 1, 0, 0, 1, 1 };
    vector<int> two = { 0, 0, 1, 0, 0, 1, 1 };
    vector<int> two_1 = {1, 1, 0, 1, 1, 0, 0 };
    vector<int> two_rotate = { 1, 1, 0, 0, 1, 0, 0 };
    vector<int> two_1_rotate = { 0, 0, 1, 1, 0, 1, 1 };
    vector<int> three = { 0, 1, 1, 1, 1, 0, 1 };
    vector<int> three_1 = { 1, 0, 0, 0, 0, 1, 0 };
    vector<int> three_rotate = { 1, 0, 1, 1, 1, 1, 0 };
    vector<int> three_1_rotate = { 0, 1, 0, 0, 0, 0, 1 };
    vector<int> four = { 0, 1, 0, 0, 0, 1, 1 };
    vector<int> four_1 = { 1, 0, 1, 1, 1, 0, 0 };
    vector<int> four_rotate = { 1, 1, 0, 0, 0, 1, 0 };
    vector<int> four_1_rotate = { 0, 0, 1, 1, 1, 0, 1 };
    vector<int> five = { 0, 1, 1, 0, 0, 0, 1 };
    vector<int> five_1 = { 1, 0, 0, 1, 1, 1, 0 };
    vector<int> five_rotate = { 1, 0, 0, 0, 1, 1, 0 };
    vector<int> five_1_rotate = { 0, 1, 1, 1, 0, 0, 1};
    vector<int> six = { 0, 1, 0, 1, 1, 1, 1 };
    vector<int> six_1 = { 1, 0, 1, 0, 0, 0, 0 };
    vector<int> six_rotate = { 1, 1, 1, 1, 0, 1, 0 };
    vector<int> six_1_rotate = { 0, 0, 0, 0, 1, 0, 1 };
    vector<int> seven = { 0, 1, 1, 1, 0 , 1, 1 };
    vector<int> seven_1 = { 1, 0, 0, 0, 1 , 0, 0 };
    vector<int> seven_rotate = { 1, 1, 0, 1, 1, 1, 0 }; 
    vector<int> seven_1_rotate = { 0, 0, 1, 0, 0, 0, 1 };
    vector<int> eight = { 0, 1, 1, 0 , 1, 1, 1 };
    vector<int> eight_1 = { 1, 0, 0, 1 , 0, 0, 0 };
    vector<int> eight_rotate = { 1, 1, 1, 0, 1, 1, 0 };
    vector<int> eight_1_rotate = { 0, 0, 0, 1, 0, 0, 1 };
    vector<int> nine = { 0, 0, 0, 1, 0, 1, 1 };
    vector<int> nine_1 = { 1, 1, 1, 0, 1, 0, 0 };
    vector<int> nine_rotate = { 1, 1, 0, 1, 0, 0, 0 };
    vector<int> nine_1_rotate = { 0, 0, 1, 0, 1, 1, 1 };

    
    cout << " next: " << endl;
    
    if (temp == zero || temp == zero_1)
    { 
        B.res = 0;
       //cout << "--" << res << endl;
        return B;

        //res.push_back(0);
    } 
    else if (temp == zero_rotate || temp == zero_1_rotate)
    {
        B.res = 0;
        B.rot = 1;
        return B;
    }

    if (temp == one || temp == one_1)
    {
        B.res = 1;
       // cout << "--" << res << endl;
        return B;
        //res.push_back(1);
    }
    else if (temp == one_rotate || temp == one_1_rotate)
    {
        B.res = 1;
        B.rot = 1;
        return B;
    }
    if (temp == two || temp == two_1)
    {
        B.res = 2;
        //cout << "--" << res << endl;
        return B;
        //res.push_back(2);
    }
    else if (temp == two_rotate || temp == two_1_rotate)
    {
        B.res = 2;
        B.rot = 1;
        return B;
    }
    if (temp == three || temp == three_1)
    {
        B.res = 3;
        //cout << "--" << res << endl;
        return B;

        //res.push_back(3);
    }
    else if (temp == three_rotate || temp == three_1_rotate)
    {
        B.res = 3;
        B.rot = 1;
        return B;
    }
    if (temp == four || temp == four_1)
    {
        B.res = 4;
        //cout << "--" << res << endl;
        return B;

        //res.push_back(4);
    }
    else if (temp == four_rotate || temp == four_1_rotate)
    {
        B.res = 4;
        B.rot = 1;
        return B;
    }
    if (temp == five || temp == five_1)
    {
        B.res = 5;
        //cout << "--" << res << endl;
        return B;
        //res.push_back(5);
    }
    else if (temp == five_rotate || temp == five_1_rotate)
    {
        B.res = 5;
        B.rot = 1;
        return B;
    }
    if (temp == six || temp == six_1)
    {
        B.res = 6;
        //cout << "--" << res << endl;
        return B;
        //res.push_back(6);
    }
    else if (temp == six_rotate || temp == six_1_rotate)
    {
        B.res = 6;
        B.rot = 1;
        return B;
    }
    if (temp == seven || temp == seven_1)
    {
        //cout << "why  " << endl;
        B.res = 7;        
       // cout << "--" << res << endl;
        return B;
        //res.push_back(7);
    }
    else if (temp == seven_rotate || temp == seven_1_rotate)
    {
        B.res = 7;
        B.rot = 1;
        return B;
    }
    if (temp == eight || temp == eight_1)
    {
        B.res = 8;
        //cout << "--" << res << endl;
        return B;
        //res.push_back(8);
    }
    else if (temp == eight_rotate || temp == eight_1_rotate)
    {
        B.res = 8;
        B.rot = 1;
        return B;
    }
    if (temp == nine || temp == nine_1)
    {
        B.res = 9;
        //cout << "--" << res << endl;
        return B;
        //res.push_back(9);
    }
    else if (temp == nine_rotate || temp == nine_1_rotate)
    {
        B.res = 9;
        B.rot = 1;
        return B;
    }
    return B;
}*/
/*
values printNestedList(list<list<int> > nested_list, std::vector< int > res, bars c, int *flag)
{
    *flag = true;
    values V;
    //cout << "[\n";

    // nested_list`s iterator(same type as nested_list) 
    // to iterate the nested_list 
    list<list<int> >::iterator nested_list_itr;

    // Print the nested_list 
    for (nested_list_itr = nested_list.begin();
        nested_list_itr != nested_list.end();
        ++nested_list_itr)
    {

        //cout << "  [";
        // normal_list`s iterator(same type as temp_list) 
        // to iterate the normal_list 
        list<int>::iterator single_list_itr;
        // pointer of each list one by one in nested list 
        // as loop goes on 
        list<int>& single_list_pointer = *nested_list_itr;
        vector<int> temp;

        for (single_list_itr = single_list_pointer.begin();
            single_list_itr != single_list_pointer.end();
            single_list_itr++)
        {
            temp.push_back(*single_list_itr);
            //cout << " " << *single_list_itr;          
        }
        //cout << " " << endl;
     c = barcodeDecode(temp);
     if (c.res != -1)
     {
         flag += 1;
         V.val.push_back(c.res);
         res.push_back(c.res);
         //break;
         //continue;
     }
     else 
     {
         V.val.push_back(c.res);
         res.push_back(c.res);
     }
    
    
     //cout << c.res;
            //barcodeDecoding(list<int>)
            //cout << nested_list_itr << endl;
          //cout << "]\n";
    }
    V.result = res;
    V.rot = c.rot;
    //cout << "]";
    return V;

}
*/
bars ParseBinaries(byte B[96], unsigned char upc[12])
{
    int i;
    bars b;
    InitBarcodeTable();


    // gaurd 0 1 2
    

    upc[0] = barcode_table[TO_BYTE(B[3], B[4], B[5], B[6], B[7], B[8], B[9])];
    upc[1] = barcode_table[TO_BYTE(B[10], B[11], B[12], B[13], B[14], B[15], B[16])];
    upc[2] = barcode_table[TO_BYTE(B[17], B[18], B[19], B[20], B[21], B[22], B[23])];
    upc[3] = barcode_table[TO_BYTE(B[24], B[25], B[26], B[27], B[28], B[29], B[30])];
    upc[4] = barcode_table[TO_BYTE(B[31], B[32], B[33], B[34], B[35], B[36], B[37])];
    upc[5] = barcode_table[TO_BYTE(B[38], B[39], B[40], B[41], B[42], B[43], B[44])];
    // gaurd  45 46 47 48 49
    upc[6] = barcode_table[TO_BYTE(B[50], B[51], B[52], B[53], B[54], B[55], B[56])];
    upc[7] = barcode_table[TO_BYTE(B[57], B[58], B[59], B[60], B[61], B[62], B[63])];
    upc[8] = barcode_table[TO_BYTE(B[64], B[65], B[66], B[67], B[68], B[69], B[70])];
    upc[9] = barcode_table[TO_BYTE(B[71], B[72], B[73], B[74], B[75], B[76], B[77])];
    upc[10] = barcode_table[TO_BYTE(B[78], B[79], B[80], B[81], B[82], B[83], B[84])];
    upc[11] = barcode_table[TO_BYTE(B[85], B[86], B[87], B[88], B[89], B[90], B[91])];
    // gaurd 92 93 94

    // Return the number of valid
    int numValid = 0;
    for (i = 0; i < 12; i++)
    {
        b.res.push_back(upc[i]);
        if (upc[i] != 255)
            numValid++;
    }
    b.numValid = numValid;

    // If there are no gaurds, then the number is not valid
   if (!((B[0] == 1 && B[1] == 0 && B[2] == 1 && B[45] == 0 && B[46] == 1 && B[47] == 0 && B[48] == 1 && B[49] == 0 && B[92] == 1 && B[93] == 0 && B[94] == 1)
        || (B[0] == 0 && B[1] == 1 && B[2] == 0 && B[45] == 1 && B[46] == 0 && B[47] == 1 && B[48] == 0 && B[49] == 1 && B[92] == 0 && B[93] == 1 && B[94] == 0)))
        b.numValid = 0;
    //cout << numValid;
    return b;
}
void IntegrateScanline(int64* scanline, int** gray, int rows, int cols, int y)
{
    int x;
    scanline[0] = 0;
    for (x = 0; x < cols; x++)
        scanline[x + 1] = scanline[x] + gray[y][x];
}

void PopulateArray(int64* arr, int64* scanline, int x1, int x2, int N_bars)
{
    int i;
    //float final_intensity_val = Const * log(1 + intensity_val);
    float x_curr = (float)x1;
    float x_step = (float)(x2 - x1) / (float)N_bars;
    for (i = 0; i < N_bars; i++) {

        int xA = (int)x_curr;
        int xB = (int)(x_curr + x_step);
        if (xB > x2) xB = x2;
        // Applying logarithmic transformations
        //arr[i] = (Const*log(1+(float)(scanline[xB + 1] - scanline[xA]))) / (float)(xB + 1 - xA);
        arr[i] = (scanline[xB + 1] - scanline[xA]) / (float)(xB + 1 - xA);
        x_curr += x_step;
    }
}
//---------------------------------
// I am calculating the value of fbar in the below function 
// Subtracting f from fbar is giving me the result of our measured value
//-------------------------------------
void PopulatingArrayMean(int64* arr, int64* result, int N_bars)
{
    // Using Sliding Window
    // Every subarray would be of size 5
    int j;
    int k = 5;

    //Outer loop is to get the values inside the result 
    for (int i = 0; i < N_bars; i++)
    {
        int64 sum = 0;
        int64 count = 0;
        for (j = i-k/2; j < k+i-k/2; j++)
        {   
            if (j >= 0 && j < N_bars)
            {
                count += 1;
                sum += arr[j];
            }
        }
        result[i] = arr[i] - (sum / count);
    }
}

float mean(int64 arr[], int N_bars)
{
    int64 sum = 0;
    for (int i = 0; i < N_bars; i++)
        sum += arr[i];
    return float(sum) / float(N_bars);
}
//-------------------------
//function to calculate the co-variance of true_values with measure values
//-------------------------
float co_variance(int64* true_value, int64* measured_value, int N_bars)
{
    float sum = 0;
    for (int i = 0; i < N_bars; i++)
        sum += (float(true_value[i]) - mean(true_value, N_bars)) * (float(measured_value[i]) - mean(measured_value, N_bars));
    return sum / float(N_bars - 1);
}

int* populateArray(int64* arr, int** gray, int rows, int cols, int x1, int x2, int y1, int y2, byte*** img)
{
    // Create an array of all of the bins
    std::vector<float> arr_total;
    std::vector<float> arr_count;
    arr_total.resize(Grid_size);
    arr_count.resize(Grid_size);
    //binaries.resize(Grid_size);
    //arr.resize(Grid_size);
    //int x1 = 66, x2 = 948, y1 = 400, y2 = 500; // gray_0 image of upca.png
     // radon image of upca.png
    //int x1 = 141, x2 = 884, y1 = 502, y2 = 503;
    //int x1 = 15, x2 = 205, y1 = 16, y2 = 46;  //another barcode UPCA
    //int x1 = 15, x2 = 307, y1 = 60, y2 = 135;
    
    //int x1 = 32, x2 = 278, y1 = 5, y2 = 30;  // barcode 1 config
    //uchar* img_data = img.data;
    //printf("%d CV_8uC3 %d CV_8uC3 %d   press enter\n", img.type(), CV_8UC3, CV_8UC4);
    //fgetc(stdin);

    float bins_per_pixel = Grid_size / ((float)x2 - (float)x1);
    float pixels_per_bin = ((float)x2 - (float)x1) / Grid_size;

    //cout << x1 << " " << x2 << " " << y1 << " " << y2;
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {

            float intensity_val = gray[y][x];
            // Applying log transformations 
            //float final_intensity_val = Const * log(1 + intensity_val);
            //intensity_val = pow(intensity_val, 1.5);
            //float intensity_val = intensity;

            // Figure out the bins in floating point
            float binA = ((float)x - (float)x1) * bins_per_pixel;
            float binB = ((float)(x + 1) - (float)x1) * bins_per_pixel;

            // Are the bins on the barcode ?
            if (binB > 0.0 && binA < Grid_size)
            {
                int iA = (int)(floor(binA));
                int iB = (int)(floor(binB));

                // If the pixel is fully within a bin
                if (iA == iB && iA >= 0 && iA < Grid_size) {
                    arr_total[iA] += (float)(intensity_val);
                    arr_count[iA] ++;


                    // Visualize
                    //if (y % 10 == 0) {
                    if (img != NULL) {
                        int color = 255 * (iA % 2);
                        img[y][x][0] = color;
                        img[y][x][2] = 255 - color;
                        img[y][x][1] = 0;
                    }
                    //}
                }

                // If the pixel is crossing a bin
                if (iA < iB) {
                    float deltaB = (binB - iB) * pixels_per_bin;
                    float deltaA = (iB - binA) * pixels_per_bin;

                    // Visualize
                   // if (y % 10 == 0) {
                    if (img != NULL) 
                    {
                        int colorA = 255 * (iA % 2);
                        int colorB = 255 * (iB % 2);
                        int color = deltaA * colorA + deltaB * colorB;
                        img[y][x][0] = color;
                        img[y][x][2] = 255 - color;
                        img[y][x][1] = 0;
                    }
                    //}

                    if (iA >= 0 && iA < Grid_size) {
                        arr_total[iA] += deltaA * (float)(intensity_val);
                        arr_count[iA] += deltaA;
                    }
                    if (iB >= 0 && iB < Grid_size) {
                        arr_total[iB] += deltaB * (float)(intensity_val);
                        arr_count[iB] += deltaB;
                    }
                }
            }
        }
    }
    //SaveRgbPng(img, "visualization_img.png", rows, cols);
    for (int i = 0; i < Grid_size; i++)
        arr[i] =  arr_total[i]/ arr_count[i];
}
// Merge Sort

void Mergesort(int* A, int* B, int N)
{
    if (N <= 1)   // base case
        return;
    else          // recursive case
    {
        int mid = N / 2;

        // Mergesort the two halves
        Mergesort(A, B, mid);
        Mergesort(A + mid, B, N - mid);

        // Merge the array
        int i = 0;
        int j = mid;
        int k = 0;
        while (i < mid && j < N) 
        {
            if (A[i] <= A[j])
                B[k++] = A[i++];
            else
                B[k++] = A[j++];
        }
        while (i < mid)
            B[k++] = A[i++];
        while (j < N)
            B[k++] = A[j++];

        // Copy the merged array back to the result
        for (k = 0; k < N; k++)
            A[k] = B[k];
    }
}
Threshold calculateThreshold(int64 arr[95], int64 min_thresh, int64 max_thresh)
//Threshold calculateThreshold(int64 arr[95])
{
    Threshold T;
    int i;

    // Use only sorted values for calculating the threshold
    int X[Grid_size], X_backup[Grid_size];
    int64 X2[Grid_size];

    // Sort the array
    for (i = 0; i < Grid_size; i++)
        X[i] = arr[i];
    Mergesort(X, X_backup, Grid_size);

    
    // Square the array
    for (i = 0; i < Grid_size; i++)
    { 
        //cout << i << X[i] << endl;
        X2[i] = ((int64)X[i]) * ((int64)X[i]);
    }
       
    //---------
    // Integral arrays
    //---------
    int intg_X[Grid_size + 1];
    int64 intg_X2[Grid_size + 1];
    intg_X[0] = 0;
    intg_X2[0] = 0;
    for (i = 0; i < Grid_size; i++) {
        intg_X[i + 1] = intg_X[i] + X[i];
        intg_X2[i + 1] = intg_X2[i] + X2[i];
    }


    //---------
    // Calculate variance for every possible threshold
    //---------
    float best_loss = 9999999;
    float best_score = -1;
    float best_thres = -1;

    for (i = 1; i < Grid_size; i++) {

        // Use only the last of any duplicate values
        if (i < Grid_size - 1 && X[i] == X[i + 1]) {
            /*cout << "-------------------------" << endl;
            cout << " thres- " << X[i] << " SKIPPED " << endl;
            cout << "-------------------------" << endl;*/
            continue;
        }

        // Use only thresholds in the specified range
        //if (X[i] < min_thresh || X[i] >= max_thresh)
           // continue;

        float Ex_a = (float)intg_X[i + 1] / (float)i;
        float Ex2_a = (float)intg_X2[i + 1] / (float)i;
        float Ex_b = (float)(intg_X[Grid_size] - intg_X[i + 1]) / (float)(Grid_size - i);
        float Ex2_b = (float)(intg_X2[Grid_size] - intg_X2[i + 1]) / (float)(Grid_size - i);

        float mean_a = Ex_a;
        float mean_b = Ex_b;
        float vari_a = Ex2_a - Ex_a * Ex_a;
        float vari_b = Ex2_b - Ex_b * Ex_b;
        float loss = vari_b + vari_a;
        /*cout << "-------------------------" << endl;
        cout << "mean1- " << mean_a << " mean2- " << mean_b << endl;
        cout << "var1- " << vari_a << " var2- " << vari_b << endl;
        cout << " thres- " << X[i] << " loss- " << loss << endl;
        cout << "-------------------------" << endl;*/
        if (best_thres == -1 || loss < best_loss) {
            best_loss = loss;
            best_score = abs(mean_a - mean_b);
            best_thres = X[i];
        }
    }
    
   /* double var1 = 0;
    double var2 = 0;
    int score = 0;
    // Using otsu's formulae to calculate precise threshold
    int best_thres = -1;
    double best_loss = -1;

    //for (int thres = 0; thres <= 255; thres++)
    //{
    for (int q = 0; q < Grid_size; q++)
    {
        int thres = arr[q];
        // Calculate the mean
        double sum1 = 0;
        double sum2 = 0;
        int count1 = 0;
        int count2 = 0;
        for (int bin = 0; bin < 95; bin++)
        {
            if (arr[bin] < thres)
            {
                sum2 += arr[bin];
                count2++;
            }
            else
            {
                sum1 += arr[bin];
                count1++;
            }
        }
        double mean1 = 0, mean2 = 0;
        if (count1 != 0)
            mean1 = sum1 / count1;
        if (count2 != 0)
            mean2 = sum2 / count2;


        // Calculate the variance
        sum1 = 0;
        sum2 = 0;
        count1 = 0;
        count2 = 0;
        for (int bin = 0; bin < 95; bin++)
        {
            if (arr[bin] < thres)
            {
                sum2 += (arr[bin] - mean2) * (arr[bin] - mean2);
                count2++;
            }
            else
            {
                sum1 += (arr[bin] - mean1) * (arr[bin] - mean1);
                count1++;
            }

            //printf("sum1 %f sum2 %f\n", sum1, sum2);
            //fgetc(stdin);
        }

        if (count1 != 0)
            var1 = sum1;
        else
            var1 = -1;

        if (count2 != 0)
            var2 = sum2;
        else
            var2 = -1;

        //
        // Is this the best loss ?
        //
        double loss = var1 + var2;
        if (var1 >= 0 && var2 >= 0)
        {
            
            if (loss < best_loss || best_loss < 0)
            {
                best_loss = loss;
                best_thres = thres;
                score = abs(mean1 - mean2);
            }
        }
        
        //printf("loss %f best_loss %f thresh %d best_thresh %d", loss, best_loss, thres, best_thres);
        //fgetc(stdin);
    }*/
    T.score = best_score;
    T.best_thres = best_thres;
    return T;
}
/*
list<list<int>> removeGuards(std::vector< bool > binaries)
{
    //Dividing binary array splitting into multiple arrays 
    int count_ = 0;
    list<list<int> > nested_sections_list;
    list<list<int> > nested_guards_list;
    //std::array<std::array<int, 3>, 3> guards;
    //std::array<std::array<int, 7>, 12> sections;

    //int guards[1][3];
    //int sections[1][7];
    //Created two dimensional arrays for storing binary values
    for (int i = 0; i < 15; i++)
    {
        if (i == 0)
        {
            list<int> single_guards_list;

            //front guard
            for (int p = 0; p < 3; p++)
            {
                single_guards_list.push_back(binaries[count_]);
                //guards[0].fill(binaries[count_]);
                count_++;
            }
            nested_guards_list.push_back(single_guards_list);
            continue;
        }
        if (i == 7)
        {
            list<int> single_guards_list;

            //middle guard
            int temp;
            temp = count_ + 5;
            while (count_ < temp)
            {
                single_guards_list.push_back(binaries[count_]);
                //guards[1].fill(binaries[count_]);
                count_++;
            }
            nested_guards_list.push_back(single_guards_list);
            continue;
        }
        if (i == 14)
        {
            list<int> single_guards_list;

            //end guard
            int temp2;
            temp2 = count_ + 3;
            while (count_ < temp2)
            {
                single_guards_list.push_back(binaries[count_]);

                //guards[1].fill(binaries[count_]);
                count_++;
            }
            nested_guards_list.push_back(single_guards_list);

            continue;
        }
        else
        {
            int temp1;
            list<int> single_section_list;

            //cout << i << " " << endl;
            temp1 = count_ + 7;
            while (count_ < temp1)
            {
                single_section_list.push_back(binaries[count_]);
                //sections[i].fill(binaries[count_]);
                //single_section_list.reverse();
                count_++;
            }
            nested_sections_list.push_back(single_section_list);
            continue;
        }
    }
    return nested_sections_list;
}
*/
K_Radon K_radon_transform(int N, int** gray)
{
    bool flag_1 = 0;
    bool flag_2 = 0;
    int L = 0, S = 0, K = 0, H = 0, F = 0;
    unsigned int nLev = 1;
    unsigned int subsize = 1;
    // To calculate number of levels  
    while (subsize < N) {
        subsize *= 2;
        nLev++;
    }

    //printf("nlev %d\n", nLev);

    LineSeg**** T;
    //allocating 4-dimensional array of size equal to nLev * size of lineseg***

    T = (LineSeg****)malloc(nLev * sizeof(LineSeg***));

    //Initialization of these variables before performing loops

    //Copy problems of size 1 Take a look at this
    T[0] = (LineSeg***)malloc(N * sizeof(LineSeg**));

    S = 0, L = 0;

    //
    // copy problems of size 1
    //
    for (int s = 0; s < N; s++)
    {
        T[0][s] = (LineSeg**)malloc(1 * sizeof(LineSeg*));
        T[0][s][0] = (LineSeg*)malloc(N * sizeof(LineSeg));
        for (int f = 0; f < N; f++) {
            //
            // Below code snippet performs threshold
            // //if(gray[s][f] != 0){

            //     //If there is a peak 

            //     T[0][s][0][f].start.first = s;
            //     T[0][s][0][f].start.last = s+1;
            //     T[0][s][0][f].start.sum = 1;
            //     T[0][s][0][f].longest.first  = s;
            //     T[0][s][0][f].longest.last  = s+1;
            //     T[0][s][0][f].longest.sum = 1;
            //     T[0][s][0][f].end.first   = s;
            //     T[0][s][0][f].end.last   = s+1;
            //     T[0][s][0][f].end.sum = 1;
            // }else{
            //     //If we dont have a peak
            //     T[0][s][0][f].start.first = s;
            //     T[0][s][0][f].start.last = s;
            //     T[0][s][0][f].start.sum = 0;
            //     T[0][s][0][f].longest.first  = s;
            //     T[0][s][0][f].longest.last  = s;
            //     T[0][s][0][f].longest.sum = 0;
            //     T[0][s][0][f].end.first   = s+1;
            //     T[0][s][0][f].end.last   = s+1;
            //     T[0][s][0][f].end.sum = 0;
            // }
            T[0][s][0][f].start.first = s;
            T[0][s][0][f].start.last = s + 1;
            T[0][s][0][f].start.sum = gray[s][f];
            T[0][s][0][f].longest.first = s;
            T[0][s][0][f].longest.last = s + 1;
            T[0][s][0][f].longest.sum = gray[s][f];
            T[0][s][0][f].end.first = s;
            T[0][s][0][f].end.last = s + 1;
            T[0][s][0][f].end.sum = gray[s][f];
        }
    }

    ////////
    L = 1;
    S = N / 2;
    H = 2;
    while (H <= N)
    {
        // for every stack 
        T[L] = (LineSeg***)malloc(S * sizeof(LineSeg**));
        for (int s = 0; s < S; s++) {
            //printf("s = %d , L= %d \n", s,L);
            //for every skew
            K = H;
            T[L][s] = (LineSeg**)malloc(K * sizeof(LineSeg*));
            for (int k = 0; k < K; k++) {
                F = N + k;
                T[L][s][k] = (LineSeg*)malloc(F * sizeof(LineSeg));
                // for every offset
                for (int f = 0; f < F; f++) {
                    LineSeg C;
                    LineSeg A;
                    LineSeg B;
                    //calculate A and B and combine into C
                    if (f < (N + k / 2))
                    {
                        A = T[L - 1][2 * s][k / 2][f];
                        flag_1 = 1;
                    }
                    else
                    {
                        flag_1 = 0;
                    }
                    if (f - (k + 1) / 2 >= 0)
                    {
                        B = T[L - 1][(2 * s) + 1][k / 2][f - (k + 1) / 2];
                        flag_2 = 1;
                    }
                    else
                    {
                        flag_2 = 0;
                    }

                    if (flag_1 == 1 && flag_2 == 1)
                    {
                        flag_1 = 0, flag_2 = 0;

                        if (s == 0 && k == 0) {

                            // printf("A start.first %d, start.last %d, start.sum %d  \n", A.start.first, A.start.last,  A.start.sum);
                             //printf("B start.first %d, start.last %d, start.sum %d  \n", B.start.first, B.start.last,  B.start.sum);
                        }

                        // Append the segments to a list of segments
                        //  xx xxxx xx|xxx xx xxxx
                        ContigSeg segs[6];   // appended segments
                        //int nSegs=6;
                        segs[0] = A.start;
                        segs[1] = A.longest;
                        segs[2] = A.end;
                        segs[3] = B.start;
                        segs[4] = B.longest;
                        segs[5] = B.end;

                        // printf("==== segs ====\n");
                         // for (int i=0; i<nSegs; i++)
                         //     printf("segs[%d] idx1 %d idx2 %d sum %d\n",
                         //         i, segs[i].first, segs[i].last, segs[i].sum);

                         //Remove zero length segments
                        ContigSeg noZerosegs[6];
                        int numnonzerosegs = 0;
                        for (int i = 0; i < 6; i++)
                        {
                            if (segs[i].first != segs[i].last)
                            {
                                noZerosegs[numnonzerosegs++] = segs[i];
                            }
                        }
                        // printf("%d\n", numnonzerosegs);
                        // printf("====non zero segs ====\n");
                        // for (int i=0; i<numnonzerosegs; i++)
                        //     printf("noZerosegs[%d] first  %d last %d sum %d\n",
                        //         i, noZerosegs[i].first, noZerosegs[i].last, noZerosegs[i].sum);

                        if (numnonzerosegs != 0)
                        {
                            // Remove Duplicates
                            ContigSeg nodup_segs[6];
                            int nNodup_segs = 1;
                            nodup_segs[0] = noZerosegs[0];
                            for (int i = 1; i < numnonzerosegs; i++)
                            {
                                if ((noZerosegs[i].first != nodup_segs[nNodup_segs - 1].first ||
                                    noZerosegs[i].last != nodup_segs[nNodup_segs - 1].last))
                                    nodup_segs[nNodup_segs++] = noZerosegs[i];
                            }

                            // printf("==== nodup_segs ====\n");
                            // for (int i=0; i<nNodup_segs; i++)
                            //     printf("nodup_segs[%d] idx1 %d idx2 %d sum %d\n",
                            //         i, nodup_segs[i].first, nodup_segs[i].last, nodup_segs[i].sum);

                            // Merge the segments into a single list of combined segments
                           // printf("%d\n", k);
                            ContigSeg merged_segs[6];
                            int nMerged_segs = 0;
                            for (int i = 0; i < nNodup_segs; i++)
                            {
                                //				if (i<nNodup_segs-1 && nodup_segs[i].last == nodup_segs[i+1].first)
                                                //
                                                // This is the k-contiguous line segment threshold
                                                //
                                                //if(i<nNodup_segs-1 && nodup_segs[i+1].first - nodup_segs[i].last <= 4)
                                                // This is the original radon transform
                                if (i < nNodup_segs - 1)
                                {
                                    //printf("%d\n", nodup_segs[i+1].first - nodup_segs[i].last);
                                    merged_segs[nMerged_segs].first = nodup_segs[i].first;
                                    merged_segs[nMerged_segs].last = nodup_segs[i + 1].last;
                                    merged_segs[nMerged_segs++].sum = nodup_segs[i].sum + nodup_segs[i + 1].sum;
                                    i++;
                                }
                                else
                                {
                                    merged_segs[nMerged_segs++] = nodup_segs[i];
                                }
                            }

                            //			printf("==== merged_segs ====\n");
                                        // for (int i=0; i<nMerged_segs; i++)
                                        //     printf("merged_segs[%d] idx1 %d idx2 %d sum %d\n",
                                        //         i, merged_segs[i].first, merged_segs[i].last, merged_segs[i].sum);

                                        // Keep the start, longest, and end into C
                            C.start = merged_segs[0];                  // What if these are length 0 ??
                            C.end = merged_segs[nMerged_segs - 1];

                            int longest_idx = 0;
                            for (int i = 1; i < nMerged_segs; i++)
                            {
                                if (merged_segs[i].sum > merged_segs[longest_idx].sum)
                                    longest_idx = i;
                            }
                            C.longest = merged_segs[longest_idx];
                        }
                        else
                        {
                            C.start.first = A.start.first;
                            C.start.last = A.start.first;
                            C.start.sum = 0;
                            C.longest.first = A.start.first;
                            C.longest.last = A.start.first;
                            C.longest.sum = 0;
                            C.end.first = B.end.last;
                            C.end.last = B.end.last;
                            C.end.sum = 0;
                        }
                        //

                    }
                    else if (flag_1 == 0 && flag_2 == 1)
                    {
                        C = B;
                    }
                    else if (flag_1 == 1 && flag_2 == 0)
                    {
                        C = A;
                    }
                    T[L][s][k][f] = C;
                    //  if(k == 0 && f == 148)
                    //  {
                    //     //  printf("L is %d \n", L);
                    //     //  printf("s is %d \n", s);
                    //     //  printf("C-start-first %d , C-start-last %d, C-start-sum %d\n", C.start.first, C.start.last, C.start.sum);
                    //     //  printf("C-longest-first %d , C-longest-last %d, C-longest-sum %d\n", C.longest.first, C.longest.last, C.longest.sum);
                    //     //  printf("C-end-first %d , C-end-last %d, C-end-sum %d\n", C.end.first, C.end.last, C.end.sum);
                    //     //  fgetc(stdin);
                    //  }
                }


            }

        }

        //printf("yes\n");

        // In end of while loop
        L++;
        H = 2 * H;
        S = S / 2;
        //    printf("%d", &H);
        //    printf("%d", &S);
    }
    K_Radon rad;
    rad.T = T;
    rad.F = F;
    rad.H = H;
    rad.K = K;
    rad.L = L;
    rad.N = N;
    rad.S = S;
    return rad;
    printf("Successfully finished dynamic programming !!!* \n now step to output image \n");
}

int** mirror_image_y(int** img, int rows, int cols)
{
    int x, y;

    // allocate
    int** out = new int *[rows];
    for (y = 0; y < rows; y++)
        out[y] = new int[cols];

    // mirror
    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            out[rows - y - 1][x] = img[y][x];
        }
    }
    return out;
}

int** transpose_image(int** img, int rows, int cols)
{
    int x, y;

    // allocate
    int** out = new int* [rows];
    for (y = 0; y < rows; y++)
        out[y] = new int[cols];

    // transopose
    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            out[x][y] = img[y][x];
        }
    }
    return out;
}
XY orientCoordinate(int x, int y, int image_number, int rows, int cols)
{
    XY peak;
    switch (image_number)
    {
        // grays[0] = gray;
        // grays[1] = mirror_image_x(gray, rows, cols);
        // grays[2] = transpose_image(gray, rows, cols);
        // grays[3] = mirror_image_x(grays[2], rows, cols);
    case 0:
        peak.x = x;
        peak.y = y;
        break;
    case 1:
        peak.x = x;
        peak.y = rows - y - 1;
        break;
    case 2:
        peak.x = y;
        peak.y = x;
        break;
    case 3:
        peak.x = cols - y - 1;
        peak.y = x;
        break;
    }
    return peak;
}

byte*** plotRadonTransform(K_Radon rad)
{
    byte*** output_image = malloc3d(rad.N, rad.N, 3);
    //printf("ok \n");
    int pixel_value = 255;
    int num = 0;
    //printf("%d \n", rad.N);
    // Below loop is where the problem lies
    int val = 0;
    for (int y = 0; y < rad.N; y++)
    {
        //int num = 0;
        for (int x = 0; x < rad.N; x++)
        {
            // if(!(rad.T[L-1][0][y][x].longest.sum))
            //      output_image[y][x] = 0;
            // else{
            //     if(rad.T[L-1][0][y][x].longest.sum != 0)
            //         printf(" img[%d][%d]: rad.T[L-1][0][y][x].start: %d , rad.T[L-1][0][y][x].end - %d, %d\n",y,x, rad.T[L-1][0][y][x].start, rad.T[L-1][0][y][x].end, rad.T[L-1][0][y][x].longest.sum );
            val = rad.T[rad.L - 1][0][y][x].longest.sum;
            val = val / 500;
            if (val > 255)
            {
                // printf("Press enter if loop");
                // fgetc(stdin);  
                output_image[y][x][0] = pixel_value;
                output_image[y][x][1] = pixel_value;
                output_image[y][x][2] = pixel_value;
            }
            else
            {
                // printf("Press enter else loop");
                // fgetc(stdin);  
                output_image[y][x][0] = val;
                output_image[y][x][1] = val;
                output_image[y][x][2] = val;
                // printf("%p \n", output_image[y][x][0]);
                // printf("%p \n", output_image[y][x][1]);
                // printf("%p \n", output_image[y][x][2]);

            }
            // num = num + 1;
        // }

        }
        // printf("%d \n", num);
        num = num + 1;
    }
    printf("assigned outputimage \n");
    // for(int i=0; i<10; i++)
    // {
    //     for(int j=0; j<10; j++)
    //     {
    //      output_image[i][j][0] = 255;
    //      output_image[i][j][1] = 0;
    //      output_image[i][j][2] = 0;
    //     }     
    // }

    return output_image;
}

Mat rotateImage(Mat cvImage, double_t angle)
{
    cv::Mat M;
    cv::Mat newImage = cvImage.clone();
    int h = newImage.rows;
    int w = newImage.cols;
    M = cv::getRotationMatrix2D(Point2f(int(w / 2), int(h / 2)), -1.0*angle, 1.75);
    cv::warpAffine(cvImage, newImage, M, newImage.size(), INTER_CUBIC, BORDER_REPLICATE);
    return newImage;
}
int main()
{

    //
    // Read the image
    //
    //int rows, cols, chan;
    //std:string imagePath = "C:\\Users\\baap1\\Computer Vision\\projects\\radon\\radon_transform\\UPCA_testbmp.bmp";
    //byte ***img = LoadRgb(imagePath, &rows, &cols, &chan);

    /*byte** gray = malloc2d(rows, cols);
    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            int r = 255 - img[y][x][0];
            int g = 255 - img[y][x][1];
            int b = 255 - img[y][x][2];
            int a = img[y][x][3];
            gray[y][x] = (r + g + b + a) / 3;  //Creating a grayscale image from RGBA format image.
        }
    }*/

    /*float** noise_imge = new float* [rows];
    for (int y = 0; y < rows; y++) {
        noise_imge[y] = new float[cols];
        for (int x = 0; x < cols; x++) {

            // sample gaussian distribution using the box muller method
            double U = (double)(rand() + 1) / (double)(RAND_MAX + 1);   // U and V are uniform distributions between 0 and 1
            double V = (double)(rand() + 1) / (double)(RAND_MAX + 1);
            double X = sqrt(-2.0 * log(U)) * cos(2.0 * 3.14159265358979323846264 * V);
            if (isnan(X) || isinf(X))
                noise_imge[y][x] = 0;
            noise_imge[y][x] = noise_amt * X;
        }
    }*/
    std::string image_path = "C:\\Users\\baap1\\Computer Vision\\projects\\radon\\radon_transform\\RapidScan_bmp\\Garlic Powder.bmp";
    //std::string image_path = "C:\\Users\\baap1\\Desktop\\UPCA_test.png";

    //calculating the skew angle of the image 
    double_t skew_angle = aspose::ocr::get_skew(image_path.c_str());

    cout << skew_angle << endl;

    InitBarcodeTable();

    //-------------------
    // Read the image
    //-------------------
    Mat_<Vec3b> img = imread(image_path, IMREAD_COLOR);
    //Mat_<Vec3b> img = img2(Range(1024, 1024), Range(1024, 1024));
    

    //if(skew_angle!=0)
        //img = rotateImage(img, -1.0*skew_angle);

    //imwrite("rotated_img.png", img);
    int width = img.cols;
    int height = img.rows;
    //int Grid_size = 95;
    int count = 0;
    int mean = 0;
    int sum = 0;
    int next1 = 0;
    //Mat gray;
    Rect grid_rect;
   // std::vector< float > arr;
    std::vector< int > res;
    unsigned char binaries[96];

    Scalar intensity;
    int** gray_arr;
    double noise_amt = 0; // Gives tolerance upto 1800 noise amount

    // Fish eye correction
    /*int halfWidth = img.rows / 2;
    int halfHeight = img.cols / 2;
    double strength = 0.001;
    double correctionRadius = sqrt(pow(img.rows, 2) + pow(img.cols, 2)) / strength;
    Mat_<Vec3b> dstImage = img.clone();
    int newX, newY;
    double distance;
    double theta;
    int sourceX;
    int sourceY;
    double r;
    for (int i = 0; i < dstImage.rows; ++i)
    {
        for (int j = 0; j < dstImage.cols; j++)
        {
            newX = i - halfWidth;
            newY = j - halfHeight;
            distance = sqrt(pow(newX, 2) + pow(newY, 2));
            r = distance / correctionRadius;
            if (r == 0.0)
                theta = 1;
            else
                theta = atan(r) / r;

            sourceX = round(halfWidth + theta * newX);
            sourceY = round(halfHeight + theta * newY);

            dstImage(i, j)[0] = img(sourceX, sourceY)[0];
            dstImage(i, j)[1] = img(sourceX, sourceY)[1];
            dstImage(i, j)[2] = img(sourceX, sourceY)[2];
        }
    }*/
    Mat dstImage = img;
    Mat gray;
    cvtColor(dstImage, gray, cv::ColorConversionCodes::COLOR_BGR2GRAY);
    //imwrite("fisheyecorrection.png", dstImage);
    //exit;
    //------------------
    // Add noise
    //------------------
    float** noise_img = new float*[dstImage.rows];
    gray_arr = new int*[dstImage.rows];
    for (int y = 0; y < dstImage.rows; y++) {
        noise_img[y] = new float[dstImage.cols];
        gray_arr[y] = new int[dstImage.cols];
        for (int x = 0; x < dstImage.cols; x++) {
        
            // sample gaussian distribution using the box muller method
            double U = (double)(rand()+1) / (double)(RAND_MAX+1);   // U and V are uniform distributions between 0 and 1
            double V = (double)(rand()+1) / (double)(RAND_MAX+1);
            double X = sqrt(-2.0 * log(U)) * cos(2.0 * 3.14159265358979323846264 * V);
            if (isnan(X) || isinf(X))
                noise_img[y][x] = 0;
            noise_img[y][x] = noise_amt * X;
            //cout << "noise image" << noise_img[y][x] << endl;
            intensity = gray.at<uchar>(Point(x, y));
            //cout << "intensity" << intensity << endl;
            gray_arr[y][x] = int(intensity[0]) + noise_img[y][x];
            //cout << gray_arr[y][x];
            //fgetc(stdin);
        }
    }
    //Initializing start-end values of x-axis and y-axis
    //int x1 = 72, x2 = 950, y1 = 0, y2 = 1;  // new_rotate, upca code sample
    //int x1 = 85, x2 = 1023, y1 = 0, y2 = 1;  //EAN-13
   // int x1 = 101, x2 = 924, y1 = 0, y2 = 1;  //x-ray test image works fine 
    int x1 = 180, x2 = 924, y1 = 24, y2 = 25;
    //int x1 = 171, x2 = 1013, y1 = 24, y2 = 25; // test1.bmp
    //int x1 = 114, x2 = 913, y1 = 0, y2 = 1;
    //int x1 = 102, x2 = 924, y1 = 0, y2 = 1; // new_orig
    //int x1 = 0, x2 = 95, y1 = 0, y2 = 1;
    // Save a noisy image
    /*uchar* gray_data = (uchar*)gray.data;
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            int val = gray_data[y * img.cols + x];
            val = val + noise_img[y][x];
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            gray_data[y * img.cols + x] = val;
        }
    }*/
    //imwrite("noisy_gray_new.png", gray);

    // Save the visualization
   
    /*
    // Set region of interest
    Mat croppedImage = cropped(30, 280, 5, 10, img);
    //cropped = img[height:height, start_x : end_x]
    //imshow("crop", croppedImage);
    //waitKey(0);
    cvtColor(croppedImage, gray, cv::ColorConversionCodes::COLOR_BGR2GRAY);
    // Dividing the image into 95 evenly spaced columns
    */
    
    /*
    while (count <= Grid_size-1)
    {
        int sum = 0;
        // Dividing the barcode into 95 evenly spaced columns
        Rect grid_rect((gray.size().width / 95)*count, 0, gray.size().width / 95, gray.size().height);
        // calculating intensity for each of the 95 columns
        intensity = gray.at<uchar>(Point((gray.size().width / 95) * count, 0));
        arr.push_back(int(intensity[0]));
        cout << intensity << " " << count << endl;
        
        //threshold(img(grid_rect), divs, 0, 255, 0);
        //imshow("Threshold", img(divs));
        //imshow("new", gray(grid_rect));
        //waitKey(0);
        count++;
        //cout << count;
    }*/

    //--------------------
    // Run the Radon transform
    //--------------------

    // Create 4 grayscale images
    int** grays[4];
    grays[0] = mirror_image_y(gray_arr, img.rows, img.cols);
    SaveGrayPngInt(grays[0], "gray_0.png", img.rows, img.cols);
    grays[1] = mirror_image_y(grays[0], img.rows, img.cols);
    SaveGrayPngInt(grays[1], "gray_1.png", img.rows, img.cols);
    grays[2] = transpose_image(grays[0], img.rows, img.cols);
    SaveGrayPngInt(grays[2], "gray_2.png", img.rows, img.cols);
    grays[3] = mirror_image_y(grays[2], img.rows, img.cols);
    SaveGrayPngInt(grays[3], "gray_3.png", img.rows, img.cols);





    //Initializing an array of 4 K_Radon variables as to calculate radon transform for all 4 of the grayscale 
    K_Radon rads[4];
    byte*** outputImage[4];
    // printf("gray %p rows %d cols %d chan %d\n", gray, rows, cols, chan);
    // XY peakss[4];
    //----------------------------------
    //Running the Dynamic programming algorithm 
    //-----------------------------------
    /*
   for (int i = 0; i < 1; i++)
    {
        int sum = 0;
        
        // Perform the radon transform
        rads[i] = K_radon_transform(gray.rows, grays[i]);

        // Copy the radon image into an int buffer
        int** radonSum = new int* [rads[i].N];
        for (int y = 0; y < rads[i].N; y++) {
            radonSum[y] = new int[rads[i].N];
            for (int x = 0; x < rads[i].N; x++)
                radonSum[y][x] = rads[i].T[rads[i].L - 1][0][y][x].longest.sum;
        }

        // Plot the image for visualization
        outputImage[i] = plotRadonTransform(rads[i]);
  
        // Populate the array of 95 bins
        int64 arr[Grid_size];
        populateArray(arr, radonSum, rads[i].N, rads[i].N, x1, x2, y1, y2, outputImage[i]);

        //binaries.resize(Grid_size);
        for (int i = Grid_size - 1; i >= 0; i--)
        {
           // cout << arr[i] << " " << i << endl;
            sum += arr[i];
        }

        mean = sum / 95;
        //cout << "mean:" << mean << endl;

        // Save the visualization image
        char path[4096];
        sprintf_s(path, "result_%05d.png", i);
        SaveRgbPng(outputImage[i], path, rads[i].N, rads[i].N);
        // Calculate threshold
        //cout << best_thres << " " << best_loss << " max_int " << MAX_INT << endl;
        Threshold T = calculateThreshold(arr);
        //cout << "Best threshold " << T.best_thres << endl;
        //cout << "Best score " << T.score << endl;

        // Convert to binary
       // cout << "best_thres" << T.best_thres;
        for (int i = 0; i < 95; i++) {
            binaries[i] = (arr[i] > T.best_thres);
            //cout << arr[i] << " ";
            //cout << i << " " << binaries[i] << endl;
            //sum += arr[i];
        }


        // Read the barcode
        //list<list<int>> nested_sections_list = removeGuards(binaries);
        unsigned char result[12];
        //bars c;
        int flag = 0;
        
        bars final = ParseBinaries(binaries, result);
        //cout << flag;
        //nested_sections_list.reverse();
        //binToDigit(nested_sections_list);
        
       for (int i = 0; i < 12; i++)
       {
           cout << "Result " << i << " : " << final.res[i] << endl;
       }
        
        // Free the radon sum buffer
        for (int y = 0; y < rads[i].N; y++)
            delete[]radonSum[y];
        delete[]radonSum;
    } 
   
   */
    int64 arr[Grid_size];
    int64 arr_bar[Grid_size];
    int64 *scanline = new int64[width+1];
    int best_x1, best_x2, best_y1, best_y2, best_score=-1, best_img, best_numValid = -1, best_barcode_index = -1;
    bars best_barcode;
    int** radonSum[4];
    int64 product_barcodes_binary[24][95];
    int64 product_barcodes_flip[24][95];
    //InitProductTable();
    InitNewProductTable();
    // We have to add the part where we loop over all the true barcodes from the list
    for (int b = 0; b < 24; b++)
    {
        //cout << product_barcodes[b].name << endl;
        DrawBarcodeToArray(product_barcodes_binary[b], product_barcodes_new[b].barcode);
        FlipBars(product_barcodes_flip[b], product_barcodes_binary[b]);
        //cout << "press enter to continue" << endl;
        //fgetc(stdin);
    }
    for (int l = 2; l < 4; l++)
    {
        double sum_time1 = 0;
        double sum_time2 = 0;
        double sum_time3 = 0;
        int sum = 0;

        // Perform the radon transform
        rads[l] = K_radon_transform(gray.rows, grays[l]);

        // Copy the radon image into an int buffer
        radonSum[l] = new int* [rads[l].N];
        for (int y = 0; y < rads[l].N; y++) {
            radonSum[l][y] = new int[rads[l].N];
            for (int x = 0; x < rads[l].N; x++)
                radonSum[l][y][x] = rads[l].T[rads[l].L - 1][0][y][x].longest.sum;
        }

        // Plot the image for visualization
        outputImage[l] = plotRadonTransform(rads[l]);
        char path[4096];
        sprintf_s(path, "result_%1d.png", l);
        SaveRgbPng(outputImage[l], path, rads[l].N, rads[l].N);

        //int y_flip = 750;  //1_bmp_1
        //int y_flip = 884; // 1_bmp_2
        //int y_flip = 900; //1_bmp_3, 2_bmp_1, 11_bmp_3
        //int y_flip = 1000; // 3_bmp_1, 5_bmp_1, 11_bmp_2
        //int y_flip = 820; // 3_bmp_2 // 4_bmp_2
        //int y_flip = 925; // 4_bmp_2
        //int y_flip = 737; //8_bmp_1
        //int y_flip = 710; //9_bmp_1
        //int y_flip = 870; // 4_bmp_1     
        //int y_flip = 850; //6_bmp_1
        //int y_flip = 940;  // 8_bmp_2 
        //int y_flip = 990;  // 10_bmp_1, 7_bmp_2
        //int y_flip = 578; // 12_bmp_2
        int y_flip = 1023; //11_bmp_1, 12_bmp_1
        //int y_flip = 870; // 7_bmp_1
        // Try all possibl e x1 x2 y1 y2
        for (y1 = 1023-y_flip; y1 < 1024-y_flip; y1++) {
            //cout << y1 << endl;
            IntegrateScanline(scanline, radonSum[l], rads[l].N, rads[l].N, y1);
            for (x1 = 0; x1 < rads[l].N - Grid_size; x1++) {
                for (x2 = x1 + Grid_size; x2 < rads[l].N; x2++) {
                    y2 = y1 + 1;
                    
                    // Populate the array of 95 bins
                    double t1 = SysTime();
                    
                    //populateArray(arr, radonSum[l], rads[l].N, rads[l].N, x1, x2, y1, y2, outputImage[l]);
                    PopulateArray(arr, scanline, x1, x2, Grid_size);
                    // Sliding window f - fbar
                    PopulatingArrayMean(arr, arr_bar, Grid_size);   // Array mean using sliding window

                    float score1[25];
                    float score2[25];
                    float score_final[25];
                    for (int q = 0; q < 24; q++)
                    {
                        score1[q] = abs(co_variance(product_barcodes_binary[q], arr_bar, Grid_size));                       
                        score2[q] = abs(co_variance(product_barcodes_flip[q], arr_bar, Grid_size));
                        if (score1[q] > score2[q])
                            score_final[q] = score1[q];
                        else
                            score_final[q] = score2[q];
                    }

                    // The indices of the final scores in decreasing order
                    int bar_indices[25];
                    for (int q = 0; q < 24; q++)
                        bar_indices[q] = q;
                    for (int q = 0; q < 24; q++) {
                        for (int r = 0; r < 24; r++) {
                            if (score_final[bar_indices[r]] < score_final[bar_indices[q]]) {
                                int temp = bar_indices[r];
                                bar_indices[r] = bar_indices[q];
                                bar_indices[q] = temp;
                            }
                        }
                    }
                    
                    float max_value = score_final[bar_indices[0]];
                    int barcode_index = bar_indices[0];
                  

                    // ----- max values in score_final
                    /*
                    float max_value = score_final[0];
                    int barcode_index = 0;
                    for (int u = 0; u < 24; u++)
                    {
                        if (score_final[u] > max_value)
                        {
                            max_value = score_final[u];
                            barcode_index = u;
                        }                           
                    }*/

                    
                    double t2 = SysTime();
                    sum_time1 += t2 - t1;

                    if (max_value > best_score)
                    {
                        cout << "Image number: " << l << endl;
                        best_img = l;
                        best_x1 = x1;
                        best_x2 = x2;
                        best_y1 = y1;
                        best_y2 = y2;
                        best_score = max_value;
                        best_barcode_index = barcode_index;

                        for (int q = 0; q < 5; q++) {
                            cout << "> place " << q + 1 << " score " << score_final[bar_indices[q]] << " barcode " << product_barcodes_new[bar_indices[q]].barcode << " name " << product_barcodes_new[bar_indices[q]].name << endl;
                        }
                        /*cout << best_barcode_index << endl;
                        cout << product_barcodes[best_barcode_index].barcode << endl;
                        cout << product_barcodes[best_barcode_index].name << endl;
                        //for (int i = 0; i < 95; i++)
                            //cout << binaries[i] << endl;
                        cout << "best score: " << best_score << endl;
                        cout << "best start: " << best_x1 << endl;
                        cout << "best end: " << best_x2 << endl;*/
                    }
                    //cout << "Time taken to populate the array " << t2 - t1 << endl;
                    
                    /*// Gaurd groups are bins as part of the gaurd that must have the same binary value
                    int64 gaurd_group_a_min = arr[0];   // minimum value of critical bins group a
                    int64 gaurd_group_a_max = arr[0];   // maximum value of critical bins group a
                    gaurd_group_a_min = MIN(gaurd_group_a_min, arr[2]);
                    gaurd_group_a_max = MAX(gaurd_group_a_max, arr[2]);
                    gaurd_group_a_min = MIN(gaurd_group_a_min, arr[46]);
                    gaurd_group_a_max = MAX(gaurd_group_a_max, arr[46]);
                    gaurd_group_a_min = MIN(gaurd_group_a_min, arr[48]);
                    gaurd_group_a_max = MAX(gaurd_group_a_max, arr[48]);
                    gaurd_group_a_min = MIN(gaurd_group_a_min, arr[92]);
                    gaurd_group_a_max = MAX(gaurd_group_a_max, arr[92]);
                    gaurd_group_a_min = MIN(gaurd_group_a_min, arr[94]);
                    gaurd_group_a_max = MAX(gaurd_group_a_max, arr[94]);

                    int64 gaurd_group_b_min = arr[1];   // maximum value of critical bins group b
                    int64 gaurd_group_b_max = arr[1];   // maximum value of critical bins group a
                    gaurd_group_b_min = MIN(gaurd_group_b_min, arr[45]);
                    gaurd_group_b_max = MAX(gaurd_group_b_max, arr[45]);
                    gaurd_group_b_min = MIN(gaurd_group_b_min, arr[47]);
                    gaurd_group_b_max = MAX(gaurd_group_b_max, arr[47]);
                    gaurd_group_b_min = MIN(gaurd_group_b_min, arr[49]);
                    gaurd_group_b_max = MAX(gaurd_group_b_max, arr[49]);
                    gaurd_group_b_min = MIN(gaurd_group_b_min, arr[93]);
                    gaurd_group_b_max = MAX(gaurd_group_b_max, arr[93]);
                
                    int64 min_thresh = -1;
                    int64 max_thresh = -1;
                    if (gaurd_group_a_max < gaurd_group_b_min) {
                        min_thresh = gaurd_group_a_max;
                        max_thresh = gaurd_group_b_min;
                    }
                    else if (gaurd_group_b_max < gaurd_group_a_min) {
                        min_thresh = gaurd_group_b_max;
                        max_thresh = gaurd_group_a_min;
                    }

                    
                    // group a 0 2 46 48 92 94
                    // group b  1 45 47 49 93


                    //binaries.resize(Grid_size);
                    //for (int i = Grid_size - 1; i >= 0; i--)
                    //{
                        //cout << arr[i] << " " << i << endl;
                        //sum += arr[i];
                    //}
                   // mean = sum / 95;
                    //cout << "mean:" << mean << endl;

                    // Save the visualization image
                    //char path[4096];
                    //sprintf_s(path, "result_%05d.png", i);
                    //SaveRgbPng(outputImage[i], path, rads[i].N, rads[i].N);

                    // Calculate threshold
                    //cout << best_thres << " " << best_loss << " max_int " << MAX_INT << endl;
                    double t3 = SysTime();
                    Threshold T = calculateThreshold(arr, min_thresh, max_thresh);
                    //Threshold T = calculateThreshold(arr);
                    double t4 = SysTime();
                    sum_time2 += t4 - t3;
                    //cout << "Time taken to calculate the threshold " << t4 - t3 << endl;
                    // Convert to binary
                   // cout << "best_thres" << T.best_thres;
                    //cout << T.best_thres << endl;
                    for (int i = 0; i < 95; i++) {
                        binaries[i] = (arr[i] > T.best_thres);
                        
                        //cout << i << " " << arr[i] << " " << bool(binaries[i]) << endl;
                        //cout << binaries[i] << " " << i << endl;
                        //sum += arr[i];
                    }

                    // Read the barcode
                    //int numValid;
                    unsigned char result[12];
                    //list<list<int>> nested_sections_list = removeGuards(binaries);
                    //bars c;
                    double t5 = SysTime();
                   
                    bars final = ParseBinaries(binaries, result);
                    //values final = printNestedList(nested_sections_list, res, c, &numValid);
                    double t6 = SysTime();
                    sum_time3 += t6 - t5;
                    //cout << "Time taken to decode and print barcodes " << t6 - t5 << endl;
                
                    //nested_sections_list.reverse();
                    //binToDigit(nested_sections_list);

                    //
                    // Is this the best barcode so far?
                    //

                    if (final.numValid > best_numValid && T.score*final.numValid > best_score)
                    { 
                        cout << "Image number: " << l << endl;
                        best_img = l;
                        best_x1 = x1;
                        best_x2 = x2;
                        best_y1 = y1;
                        best_y2 = y2;
                        best_score = T.score*final.numValid;
                        best_barcode = final;
                        best_numValid = final.numValid;
                   
                        //for (int i = 0; i < 95; i++)
                            //cout << binaries[i] << endl;
                        cout << "best start: " << best_x1 << endl;
                        cout << "best end: " << best_x2 << endl;
                        cout << "best_numValid : " << final.numValid << endl;
                    }
                    */
                }
            }
            cout << "Time taken to populate the array " << sum_time1 << endl;
            cout << "Time taken to calculate the threshold " << sum_time2 << endl;
            cout << "Time taken to generate binaries, remove guards, decode and print barcodes " << sum_time3 << endl;
           // cout << "sum time of one scan line " << sum_time1 + sum_time2 + sum_time3 << endl;
           // fgetc(stdin);
        }      
    }

    populateArray(arr, radonSum[best_img], rads[best_img].N, rads[best_img].N, best_x1, best_x2, best_y1, best_y2, outputImage[best_img]);
    SaveRgbPng(outputImage[best_img], "visualization_final.png", rads[best_img].N, rads[best_img].N);
    /*if (best_score < 0)
        cout << "Barcode is not readable" << endl;
    else
    {       
          for (int i = 0; i < 12; i++)
           {
               cout << "Result " << i << " : " << best_barcode.res[i] << endl;
           }
    }
    
    // print the barcode and score
    if (img.empty())
    {
        std::cout << "Could not read the image: " << image_path << std::endl;
        return 1;
    }
    imwrite("scanned_edited_barcode.png", img);
    //imwrite("scanned_first_barcode.png", img);
    */
   
    return 0;
}



