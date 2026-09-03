#include <string.h>
#include <functional>

#include "pngtypes.h"
extern "C" {
#include "png/png.h"
}

static void PNGCBAPI pngWarningFn(png_structp, png_const_charp msg) {
  if (strcmp(msg, "libpng warning: iCCP: known incorrect sRGB profile")) {
    // ignore, happens on Linux only for some reason
    return;
  }
  fprintf(stderr, "%s\n", msg);
}

typedef struct PngReadStruct {
  const U8 *walk;
  int left;
} PngReadStruct;

static void PNGCBAPI pngReadFunc(png_structp pPng, png_bytep data, png_size_t data_size)
{
  PngReadStruct *io_ptr = (PngReadStruct *)png_get_io_ptr(pPng);
  if ((int)data_size > io_ptr->left)
    png_error(pPng, "EOF");
  memcpy(data, io_ptr->walk, data_size);
  io_ptr->walk += data_size;
  io_ptr->left -= (int)data_size;
}

char *png_last_err = NULL;

static void PNGCBAPI pngErrorFn(png_structp png_ptr, png_const_charp error_msg) {
  // fprintf(stderr, "%s\n", error_msg);
  if (png_last_err) {
    free(png_last_err);
  }
  png_last_err = strdup(error_msg);

  longjmp(png_jmpbuf(png_ptr), 1);
}

GlovImage *pngReadShared(std::function<void(png_structp pPng)> setup_io)
{
  png_structp pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!pPng)
    return NULL;

  setup_io(pPng);
  png_set_error_fn(pPng, NULL, pngErrorFn, pngWarningFn);

  png_infop pPngInfo = png_create_info_struct(pPng);
  if (!pPngInfo)
  {
    png_destroy_read_struct(&pPng, NULL, NULL);
    return NULL;
  }

  GlovImage *ret = new GlovImage;
  memset(ret, 0, sizeof(*ret));

  U8** pRowPointers = NULL;

  // it's a goto (in case png lib hits the bucket)
  if (setjmp(png_jmpbuf(pPng)))
  {
    png_destroy_read_struct(&pPng, &pPngInfo, NULL);
    if (ret->data)
      delete[] ret->data;
    delete ret;
    if (pRowPointers)
      delete[] pRowPointers;
    return NULL;
  }

  png_set_sig_bytes(pPng, 8);  // we already read the 8 signature bytes
  png_read_info(pPng, pPngInfo);  // read all PNG info up to image data

  int bitDepth, colorType;
  unsigned int width, height;
  png_get_IHDR(pPng, pPngInfo, &width, &height, &bitDepth, &colorType, NULL, NULL, NULL);

  ret->size[0] = width;
  ret->size[1] = height;

  // apply filters to image so we can get proper image data format
  if (colorType == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(pPng);
  if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
    png_set_expand_gray_1_2_4_to_8(pPng);
  if (png_get_valid(pPng, pPngInfo, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(pPng);

  if (bitDepth == 16)
  {
    if (1 /*m_bForce8BitDepth*/)
      png_set_strip_16(pPng); //make 8-bit
    else
      png_set_swap(pPng); //swap endian order (PNG is Big Endian)
  }

  png_read_update_info(pPng, pPngInfo);
  // re-get header, may have changed due to above instructions
  png_get_IHDR(pPng, pPngInfo, &width, &height, &bitDepth, &colorType, NULL, NULL, NULL);

  if (1 /*m_bForce8BitDepth*/) // force8bit depth per channel
    bitDepth = 8;

  int byteDepth = bitDepth / 8;

  switch (colorType)
  {
  case PNG_COLOR_TYPE_GRAY_ALPHA:
  case PNG_COLOR_TYPE_GRAY:
    // ret->format = TextureFormat_ALPHA;
    ret->bytesPerPixel = byteDepth;
    break;

  case PNG_COLOR_TYPE_RGB_ALPHA:
    // ret->format = TextureFormat_RGBA;
    ret->bytesPerPixel = byteDepth * 4;
    break;

  case PNG_COLOR_TYPE_PALETTE:
    // ret->format = TextureFormat_RGBA;
    ret->bytesPerPixel = byteDepth * 4;
    break;

  case PNG_COLOR_TYPE_RGB:
    // ret->format = TextureFormat_RGB;
    ret->bytesPerPixel = byteDepth * 3;
    break;

  default:
    // assert(!"Invalid PNG data type (grayscale?)");
    break;
  }

  size_t rowbytes = png_get_rowbytes(pPng, pPngInfo);

  ret->data = new U8[rowbytes*ret->size[1]];
  pRowPointers = new U8*[ret->size[1]];

  for (int i = 0; i < ret->size[1]; ++i)
    pRowPointers[i] = ret->data + i * rowbytes;

  png_read_image(pPng, pRowPointers);

  png_read_end(pPng, NULL);

  png_destroy_read_struct(&pPng, &pPngInfo, NULL);

  delete[] pRowPointers;

  // rowbytes should only mismatch with colorType == PNG_COLOR_TYPE_GRAY_ALPHA
  // assert((colorType == PNG_COLOR_TYPE_GRAY_ALPHA) ==
  //   (rowbytes != (size_t)(ret->size[0] * ret->bytesPerPixel)));
  if (colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
    // assert(rowbytes == (size_t)(ret->size[0] * ret->bytesPerPixel * 2));
    size_t total_pixels = ret->size[0] * ret->size[1];
    for (size_t ii = 0; ii < total_pixels; ii++) {
      ret->data[ii] = ret->data[ii * 2 + 1];
    }
  }

  return ret;
}

const char *pngLastError()
{
  return png_last_err;
}

GlovImage *pngReadFromMem(const U8 *data, int data_size)
{
  if (data_size < 8 || !png_check_sig(data, 8))
    return NULL;

  PngReadStruct read_struct = {
    data + 8,
    data_size - 8,
  };

  return pngReadShared([&read_struct](png_structp pPng) {
    png_set_read_fn(pPng, &read_struct, pngReadFunc);
  });
}

bool pngWriteShared(GlovImage *image, std::function<void(png_structp pPng)> setup_io)
{
  int m_bytesPerPixel;
  int colorType, bitDepthPerChannel;

  png_structp pPng = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!pPng)
  {
    return false;
  }

  png_infop pPngInfo = png_create_info_struct(pPng);
  if (!pPngInfo)
  {
    png_destroy_write_struct(&pPng, NULL);
    return false;
  }

#ifdef _WIN32
#pragma warning(disable:4611)
#endif
  // it's a goto (in case png lib hits the bucket)
  if (setjmp(png_jmpbuf(pPng)))
  {
    png_destroy_write_struct(&pPng, &pPngInfo);
    return false;
  }

  setup_io(pPng);

  // Z_NO_COMPRESSION         0
  // Z_BEST_SPEED             1
  // Z_BEST_COMPRESSION       9
  png_set_compression_level(pPng, 6); //compresion level 0(none)-9(best compression)
  png_set_compression_strategy(pPng, 3);

  if (image->bytesPerPixel == 1)
  {
    m_bytesPerPixel = 1;
    bitDepthPerChannel = m_bytesPerPixel * 8;
    colorType = PNG_COLOR_TYPE_GRAY;
  } else if (image->bytesPerPixel == 3)
  {
    m_bytesPerPixel = 3;
    bitDepthPerChannel = m_bytesPerPixel / 3 * 8;
    colorType = PNG_COLOR_TYPE_RGB;
  } else if (image->bytesPerPixel == 4)
  {
    m_bytesPerPixel = 4;
    bitDepthPerChannel = m_bytesPerPixel / 4 * 8;
    colorType = PNG_COLOR_TYPE_RGB_ALPHA;
  } else
  {
    png_destroy_write_struct(&pPng, &pPngInfo);
    return false;
  }


  png_set_IHDR(pPng, pPngInfo, (U32)image->size[0], (U32)image->size[1],
    bitDepthPerChannel, colorType, PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_write_info(pPng, pPngInfo);

  if (bitDepthPerChannel == 16) //reverse endian order (PNG is Big Endian)
    png_set_swap(pPng);

  int bytesPerRow = image->size[0] * m_bytesPerPixel;

  //write non-interlaced buffer

  for (int row = 0; row < image->size[1]; ++row)
  {
    png_write_row(pPng, image->data + row * bytesPerRow);
  }

  png_write_end(pPng, NULL);

  png_destroy_write_struct(&pPng, &pPngInfo);
  return true;
}

static void PNGCBAPI pngFlushFunc(png_structp pPng)
{
  // NOOP
}

struct ByteAccumulator {
  unsigned char* data = nullptr;
  size_t size = 0;
  size_t capacity = 0;

  void ensureCapacity(size_t additional) {
    if (size + additional <= capacity) return;

    size_t newCapacity = capacity ? capacity * 2 : 4096;
    while (newCapacity < size + additional) {
        newCapacity *= 2;
    }

    unsigned char* newData = static_cast<unsigned char*>(realloc(data, newCapacity));
    // In production, check newData != nullptr and handle OOM
    data = newData;
    capacity = newCapacity;
  }

  void append(const unsigned char* chunk, size_t chunkSize) {
    ensureCapacity(chunkSize);
    memcpy(data + size, chunk, chunkSize);
    size += chunkSize;
  }
};

static void PNGCBAPI pngWriteFunc(png_structp pPng, png_bytep data, png_size_t data_size)
{
  ByteAccumulator *ba = (ByteAccumulator *)png_get_io_ptr(pPng);
  ba->append(data, data_size);
}

bool pngWriteToMem(U8 *&data, int &data_size, GlovImage *image)
{
  ByteAccumulator ba;
  bool ret = pngWriteShared(image, [&ba](png_structp pPng) {
    png_set_write_fn(pPng, &ba, pngWriteFunc, pngFlushFunc);
  });
  if (ret) {
    data = ba.data;
    data_size = ba.size;
  }

  return ret;
}
