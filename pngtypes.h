
typedef unsigned char U8;
typedef unsigned int U32;

typedef struct GlovImage
{
  unsigned char *data;
  int size[2];
  int bytesPerPixel;
} GlovImage;

GlovImage *pngReadFromMem(const U8 *data, int data_size);
bool pngWriteToMem(U8 *&data, int &data_size, GlovImage *image);

#ifdef _WIN32
#pragma comment(lib, "../libs/pngWin64.lib")
#pragma comment(lib, "../libs/zlibWin64.lib")

#endif
