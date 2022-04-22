#ifdef __cplusplus
extern "C" {
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include "stb_image.h"
#include "stb_image_write.h"

#include "dc_image.h"

#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#define MIN(a,b)   ((a) < (b) ? (a) : (b))
#define ABS(x)    ( (x) <= 0 ? 0-(x) : (x) )
//-------------------------------------------------------
// A few image helper functions
//-------------------------------------------------------

// Memory Allocation

byte** malloc2d(int rows, int cols) {
	int y;
	byte** ptr = (byte**)malloc(rows * sizeof(byte*));
	for (y = 0; y < rows; y++)
		ptr[y] = (byte*)calloc(cols, sizeof(byte));
	return ptr;
}
// malloc method in c is used to dynamically allocate a single large block of memory with the specified size.
// Alllocating the space for a 3-dimensional array with rows, cols and chans.
byte*** malloc3d(int rows, int cols, int chan) {
	int y, x;
	// This will allocate space in memory for number of rows*size of byte** and ptr will hold the address of the first byte in the allocated memory.
	// Allocated all the rows 
	byte*** ptr = (byte***)malloc(rows * sizeof(byte**));
	// Allocating all the columns for every row with size cols * size of byte*.
	for (y = 0; y < rows; y++) {
		ptr[y] = (byte**)malloc(cols * sizeof(byte*));
		for (x = 0; x < cols; x++)
			// Allocating each block with chan number of channels in this case chan is equal to 3
			// This will allocate contiguous space in memory for number of channels in each pixel with the size of byte
			ptr[y][x] = (byte*)calloc(chan, sizeof(byte));
	}
	return ptr;
}

void free2d(byte** data, int rows)
{
	int y;
	for (y = 0; y < rows; y++)
		free(data[y]);
	free(data);
}

void free3d(byte*** data, int rows, int cols)
{
	int y, x;
	for (y = 0; y < rows; y++) {
		for (x = 0; x < cols; x++)
			free(data[y][x]);
		free(data[y]);
	}
	free(data);
}


float** malloc2d_float(int rows, int cols)
{
	int y;
	float** ptr = (float**)malloc(rows * sizeof(float*));
	for (y = 0; y < rows; y++)
		ptr[y] = (float*)calloc(cols, sizeof(float));
	return ptr;
}

float** free2d_float(float*** data, int rows)
{
	int y;
	for (y = 0; y < rows; y++)
		free(data[y]);
	free(data);
}




// Loading / Saving Images

void SaveRgbPng(byte*** in, const char* fname, int rows, int cols)
{
	printf("SaveRgbaPng %s %d %d\n", fname, rows, cols);

	int y, x, c, i = 0;
	byte* data = (byte*)malloc(cols * rows * 3 * sizeof(byte));
	for (y = 0; y < rows; y++) {
		//printf("y %d rows %d cols %d\n", y, rows, cols);
		for (x = 0; x < cols; x++) {
			for (c = 0; c < 3; c++) {
				data[i++] = in[rows - y - 1][x][c];
			}
		}
	}
	stbi_write_png(fname, cols, rows, 3, data, cols * 3);
	free(data);
}

void SaveGrayPngInt(int** in, const char* fname, int rows, int cols)
{
	byte** img = malloc2d(rows, cols);
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			int val = in[y][x];
			if (val > 255) val = 255;
			if (val < 0) val = 0;
			img[y][x] = val;
		}
	}
	SaveGrayPng(img, fname, rows, cols);

	free2d(img, rows);
}

void SaveGrayPng(byte** in, const char* fname, int rows, int cols)
{
	printf("SaveGrayPng %s %d %d\n", fname, rows, cols);

	int y, x, c, i = 0;
	byte* data = malloc(cols * rows * 3 * sizeof(byte));
	for (y = 0; y < rows; y++) {
		for (x = 0; x < cols; x++) {
			data[i++] = in[rows - y - 1][x];   // red
			data[i++] = in[rows - y - 1][x];   // green
			data[i++] = in[rows - y - 1][x];   // blue
//			data[i++] = 255;        // alpha
		}
	}
	stbi_write_png(fname, cols, rows, 3, data, cols * 3);
	free(data);
}

void DrawPoint(int x, int y, int size, byte*** img, int rows, int cols, int red, int green, int blue)
{
	// printf("%d \n", ABS(x-size/2));
	// printf("%d", ABS(x+size/2));
	// printf("%d \n", ABS(y-size/2));
	// printf("%d", ABS(y+size/2));
	for (int p = ABS(x - size / 2); p < (x + size / 2) - 1; p++)
	{
		for (int q = ABS(y - size / 2); q < (y + size / 2) - 1; q++)
		{
			img[p][q][0] = red;
			img[p][q][1] = green;
			img[p][q][2] = blue;
		}
	}
}

void DrawLine(int ax, int ay, int bx, int by, byte*** img, int rows, int cols, int size, int red, int green, int blue)
{


	if (ABS(bx - ax) <= size && ABS(by - ay) <= size)
	{
		//byte *data = stbi_load(cols, rows, chan, 3);
		img[ay][ax][0] = red;
		img[ay][ax][1] = green;
		img[ay][ax][2] = blue;
		img[by][bx][0] = red;
		img[by][bx][1] = green;
		img[by][bx][2] = blue;
	}
	else
	{
		int mid_x = (ax + bx) / 2;
		int mid_y = (ay + by) / 2;
		DrawLine(ax, ay, mid_x, mid_y, img, rows, cols, 1, 255, 0, 0);
		DrawLine(mid_x, mid_y, bx, by, img, rows, cols, 1, 255, 0, 0);
	}
}

byte*** LoadRgb(const char* fname, int* rows, int* cols, int* chan)
{
	printf("LoadRgba %s\n", fname);

	//Initializing variables to zero 
	int y, x, c, i = 0;
	// Use stbi_load function which returns an unsigned char * which points to pixel data
	// It takes arguments columns, rows, channels and how many components we want in each pixel
	// It returns image width in pixels
	// It returns image height in pixels
	// outputs number of channels in image file
	byte* data = stbi_load(fname, cols, rows, chan, 3);
	// printf("%d and %d and %d \n", *rows, *cols, *chan);
	/*
	// Convert rgb to rgba
	if (*chan==3) {
		int N = *rows * *cols;
		printf("N %d\n", N);
		byte *rgb = data;
		data = malloc(N * 4 * sizeof(byte));
		for (i=0; i<N; i++) {
			//printf("i %d\n", i);
			data[4*i+0] = rgb[3*i+0];
			data[4*i+1] = rgb[3*i+1];
			data[4*i+2] = rgb[3*i+2];
			data[4*i+3] = 255;
		}
		free(rgb);
		printf("done convert\n");
	}
	*chan = 4;
	*/
	printf("channels: %d", *chan);
	if (*chan < 3) {
		printf("error: expected 3 channels (red green blue)\n");
		exit(1);
	}

	i = 0;
	// Allocating 3-dimnesional space in memory for rows, cols and chan
	byte*** img = malloc3d(*rows, *cols, *chan);
	for (y = 0; y < *rows; y++)
		for (x = 0; x < *cols; x++)
			for (c = 0; c < *chan; c++)
				//filling the each block of the 3-dimensional array with the image data.
				img[*rows - y - 1][x][c] = data[i++];
	// free will deallocate the memory previosly allocated by a call to calloc and malloc.
	free(data);
	printf("done read\n");
	return img;
}

#ifdef __cplusplus
}
#endif
