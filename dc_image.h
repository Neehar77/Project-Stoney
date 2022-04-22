#ifdef __cplusplus
extern "C" {
#endif

#ifndef dc_image_h
#define dc_image_h



#define byte unsigned char

//-------------------------------------------------------
// A few image helper functions
//-------------------------------------------------------

// Memory Allocation

byte** malloc2d(int rows, int cols);

byte*** malloc3d(int rows, int cols, int chan);

void free2d(byte** data, int rows);

void free3d(byte*** data, int rows, int cols);


float** malloc2d_float(int rows, int cols);

float** free2d_float(float*** data, int rows);

void DrawLine(int ax, int ay, int bx, int by, byte*** img, int rows, int cols, int size, int red, int green, int blue);

void DrawPoint(int x, int y, int size, byte*** img, int rows, int cols, int red, int green, int blue);
// Loading / Saving Images

void SaveRgbPng(byte*** in, const char* fname, int cols, int rows);

void SaveGrayPng(byte** in, const char* fname, int cols, int rows);

void SaveGrayPngInt(int** in, const char* fname, int rows, int cols);

byte*** LoadRgb(const char* fname, int* rows, int* cols, int* chan);


#endif // dc_image_h

#ifdef __cplusplus
}
#endif
