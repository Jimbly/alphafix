#include <napi.h>

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <vector>

#include "pngtypes.h"

using namespace Napi;

bool alphafixImpl(int alpha_channel, int bpp, uint32_t width, uint32_t height, unsigned char* data) {

  uint32_t dim = width * height;

  unsigned char* is_solid = new unsigned char[dim]();
  unsigned char* queued = new unsigned char[dim]();
  uint32_t* todo_buf = new uint32_t[dim];
  uint32_t* solid_mark = new uint32_t[dim];
  uint32_t solid_mark_len = 0;

  uint32_t todo_start = 0;
  uint32_t todo_end = 0;
  bool diff = false;

  #define addNeighbors(idx) { \
    uint32_t x = idx % width; \
    if (x > 0 && !queued[idx - 1]) { \
      todo_buf[todo_end++] = idx - 1; \
      queued[idx - 1] = 1; \
    } \
    if (x + 1 < width && !queued[idx + 1]) { \
      todo_buf[todo_end++] = idx + 1; \
      queued[idx + 1] = 1; \
    } \
    if (idx > width && !queued[idx - width]) { \
      todo_buf[todo_end++] = idx - width; \
      queued[idx - width] = 1; \
    } \
    if (idx + width < dim && !queued[idx + width]) { \
      todo_buf[todo_end++] = idx + width; \
      queued[idx + width] = 1; \
    } \
  }

  for (uint32_t idx = 0; idx < dim; ++idx) {
    bool solid = false;
    for (int offs = 0; offs < bpp; ++offs) {
      if (alpha_channel & (1 << offs)) {
        if (data[idx * bpp + offs]) {
          solid = true;
          break;
        }
      }
    }
    if (solid) {
      queued[idx] = 1;
      is_solid[idx] = 1;
      addNeighbors(idx);
    }
  }

  size_t loop_end = todo_end;

  while (todo_start < todo_end) {
    if (todo_start == loop_end) {
      for (size_t ii = 0; ii < solid_mark_len; ++ii) {
        is_solid[solid_mark[ii]] = 1;
      }
      solid_mark_len = 0;
      loop_end = todo_end;
    }

    uint32_t idx = todo_buf[todo_start++];
    if (is_solid[idx]) {
      continue;
    }

    uint32_t y = idx / width;
    uint32_t x = idx % width;

    int c = 0;
    int rgba[4] = {0};
#define addColor(idx) \
      for (int offs=0; offs < bpp; ++offs) { \
        rgba[offs] += data[(idx) * bpp + offs]; \
      } \
      c++;

    if (x > 0 && is_solid[idx - 1]) {
      addColor(idx - 1);
    }
    if (x < width - 1 && is_solid[idx + 1]) {
      addColor(idx + 1);
    }
    if (y > 0 && is_solid[idx - width]) {
      addColor(idx - width);
    }
    if (y < height - 1 && is_solid[idx + width]) {
      addColor(idx + width);
    }

    for (int offs=0; offs < bpp; ++offs) {
      if (!(alpha_channel & (1<<offs))) {
        int effidx = idx * bpp + offs;
        unsigned char newv = (rgba[offs] + c/2) / c;
        if (diff) {
          data[effidx] = newv;
        } else if (data[effidx] != newv) {
          data[effidx] = newv;
          diff = true;
        }
      }
    }

    addNeighbors(idx);
    solid_mark[solid_mark_len++] = idx;
  }

  delete is_solid;
  delete queued;
  delete todo_buf;
  delete solid_mark;

  return diff;
}

Value alphafix(const CallbackInfo &info) {
  Env env = info.Env();

  if (info.Length() != 5) {
    TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsNumber()) {
    TypeError::New(env, "Argument 0 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int alpha_channel = info[0].As<Number>().Int32Value();

  if (!info[1].IsNumber()) {
    TypeError::New(env, "Argument 1 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int width = info[1].As<Number>().Int32Value();

  if (!info[2].IsNumber()) {
    TypeError::New(env, "Argument 2 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int height = info[2].As<Number>().Int32Value();

  if (!info[3].IsNumber()) {
    TypeError::New(env, "Argument 3 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int bpp = info[3].As<Number>().Int32Value();

  unsigned char* data = nullptr;
  size_t length = 0;
  if (info[4].IsBuffer()) {
    Buffer<unsigned char> buffer = info[4].As<Buffer<unsigned char>>();
    data = buffer.Data();
    length = buffer.Length();
  } else if (info[4].IsTypedArray()) {
    TypedArray typedArray = info[4].As<TypedArray>();
    if (typedArray.TypedArrayType() != napi_uint8_array) {
      TypeError::New(env, "TypedArray must be a Uint8Array").ThrowAsJavaScriptException();
      return env.Null();
    }
    Uint8Array uint8Array = typedArray.As<Uint8Array>();
    data = uint8Array.Data();
    length = uint8Array.ByteLength();
  } else {
    TypeError::New(env, "Argument must be a Buffer or Uint8Array").ThrowAsJavaScriptException();
    return env.Null();
  }

  if ((size_t)width * height * bpp > length) {
    TypeError::New(env, "Buffer is not large enough for stated dimensions").ThrowAsJavaScriptException();
    return env.Null();
  }

  bool changed = alphafixImpl(alpha_channel, bpp, width, height, data);
  return Napi::Boolean::New(env, changed);
}

Value pngRead(const CallbackInfo &info) {
  Env env = info.Env();

  if (info.Length() != 2) {
    TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  unsigned char* data = nullptr;
  size_t length = 0;
  if (info[0].IsBuffer()) {
    Buffer<unsigned char> buffer = info[0].As<Buffer<unsigned char>>();
    data = buffer.Data();
    length = buffer.Length();
  } else if (info[0].IsTypedArray()) {
    TypedArray typedArray = info[0].As<TypedArray>();
    if (typedArray.TypedArrayType() != napi_uint8_array) {
      TypeError::New(env, "TypedArray must be a Uint8Array").ThrowAsJavaScriptException();
      return env.Null();
    }
    Uint8Array uint8Array = typedArray.As<Uint8Array>();
    data = uint8Array.Data();
    length = uint8Array.ByteLength();
  } else {
    TypeError::New(env, "Argument must be a Buffer or Uint8Array").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[1].IsNumber()) {
    TypeError::New(env, "Argument 1 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int force_bpp = info[1].As<Number>().Int32Value();

  GlovImage *img = pngReadFromMem(data, length);
  if (!img) {
    Error::New(env, pngLastError()).ThrowAsJavaScriptException();
    return env.Null();
  }
  int numpixels = img->size[0] * img->size[1];
  int bpp = img->bytesPerPixel;

  if (force_bpp && bpp != force_bpp) {
    U8 *newdata = new U8[numpixels * force_bpp];
    for (int inidx=0, outidx=0; inidx < numpixels * bpp; ) {
      for (int ii=0; ii < force_bpp; ++ii) {
        if (ii < bpp) {
          newdata[outidx++] = img->data[inidx++];
        } else {
          newdata[outidx++] = ii == 3 ? 255 : 0;
        }
      }
    }
    delete img->data;
    img->data = newdata;
    bpp = force_bpp;
  }

  U8 *outdata = img->data;
  Napi::Buffer<unsigned char> buf = Napi::Buffer<unsigned char>::New(
    env,
    outdata,
    numpixels * bpp,
    [](Napi::Env /*env*/, unsigned char* outdata) {
      delete outdata;
    }
  );

  Napi::Object result = Napi::Object::New(env);
  result.Set("width", Napi::Number::New(env, img->size[0]));
  result.Set("height", Napi::Number::New(env, img->size[1]));
  result.Set("bpp", Napi::Number::New(env, bpp));
  result.Set("data", buf);
  delete img;
  return result;
}

Value pngWrite(const CallbackInfo &info) {
  Env env = info.Env();

  if (info.Length() != 4) {
    TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsNumber()) {
    TypeError::New(env, "Argument 0 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int width = info[0].As<Number>().Int32Value();

  if (!info[1].IsNumber()) {
    TypeError::New(env, "Argument 1 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int height = info[1].As<Number>().Int32Value();

  if (!info[2].IsNumber()) {
    TypeError::New(env, "Argument 2 must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  int bpp = info[2].As<Number>().Int32Value();

  unsigned char* data = nullptr;
  size_t length = 0;
  if (info[3].IsBuffer()) {
    Buffer<unsigned char> buffer = info[3].As<Buffer<unsigned char>>();
    data = buffer.Data();
    length = buffer.Length();
  } else if (info[3].IsTypedArray()) {
    TypedArray typedArray = info[3].As<TypedArray>();
    if (typedArray.TypedArrayType() != napi_uint8_array) {
      TypeError::New(env, "TypedArray must be a Uint8Array").ThrowAsJavaScriptException();
      return env.Null();
    }
    Uint8Array uint8Array = typedArray.As<Uint8Array>();
    data = uint8Array.Data();
    length = uint8Array.ByteLength();
  } else {
    TypeError::New(env, "Argument must be a Buffer or Uint8Array").ThrowAsJavaScriptException();
    return env.Null();
  }

  if ((size_t)width * height * bpp > length) {
    TypeError::New(env, "Buffer is not large enough for stated dimensions").ThrowAsJavaScriptException();
    return env.Null();
  }

  GlovImage img = {
    data,
    { width, height },
    bpp,
  };
  U8 *outdata;
  int outdata_size;
  bool ret = pngWriteToMem(outdata, outdata_size, &img);

  if (!ret) {
    TypeError::New(env, "pngWirtetoMem failed").ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::Buffer<unsigned char> result = Napi::Buffer<unsigned char>::New(
    env,
    outdata,
    outdata_size,
    [](Napi::Env /*env*/, unsigned char* outdata) {
      free(outdata);
    }
  );
  return result;
}

Object Init(Env env, Object exports) {
  exports.Set(String::New(env, "alphafix"), Function::New(env, alphafix));
  exports.Set(String::New(env, "pngRead"), Function::New(env, pngRead));
  exports.Set(String::New(env, "pngWrite"), Function::New(env, pngWrite));
  return exports;
}

NODE_API_MODULE(addon, Init)
