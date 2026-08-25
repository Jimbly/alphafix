#include <napi.h>

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <vector>

using namespace Napi;

void alphafixImpl(int alpha_channel, uint32_t width, uint32_t height, unsigned char* data, size_t length) {

    uint32_t dim = width * height;

    unsigned char* is_solid = new unsigned char[dim]();
    unsigned char* queued = new unsigned char[dim]();
    uint32_t* todo_buf = new uint32_t[dim];
    uint32_t* solid_mark = new uint32_t[dim];
    uint32_t solid_mark_len = 0;

    uint32_t todo_start = 0;
    uint32_t todo_end = 0;

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
        for (int offs = 0; offs < 4; ++offs) {
            if (alpha_channel & (1 << offs)) {
                if (data[idx * 4 + offs]) {
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
        int r = 0, g = 0, b = 0, a = 0;

        if (x > 0 && is_solid[idx - 1]) {
            r += data[idx * 4 - 4];
            g += data[idx * 4 - 3];
            b += data[idx * 4 - 2];
            a += data[idx * 4 - 1];
            c++;
        }
        if (x < width - 1 && is_solid[idx + 1]) {
            r += data[idx * 4 + 4];
            g += data[idx * 4 + 5];
            b += data[idx * 4 + 6];
            a += data[idx * 4 + 7];
            c++;
        }
        if (y > 0 && is_solid[idx - width]) {
            r += data[(idx - width) * 4];
            g += data[(idx - width) * 4 + 1];
            b += data[(idx - width) * 4 + 2];
            a += data[(idx - width) * 4 + 3];
            c++;
        }
        if (y < height - 1 && is_solid[idx + width]) {
            r += data[(idx + width) * 4];
            g += data[(idx + width) * 4 + 1];
            b += data[(idx + width) * 4 + 2];
            a += data[(idx + width) * 4 + 3];
            c++;
        }

#define rdiv(num, denom) ((num + denom/2) / denom)
        r = rdiv(r, c);
        g = rdiv(g, c);
        b = rdiv(b, c);
        a = rdiv(a, c);

        if (!(alpha_channel & 1)) {
            data[idx * 4] = static_cast<unsigned char>(r);
        }
        if (!(alpha_channel & 2)) {
            data[idx * 4 + 1] = static_cast<unsigned char>(g);
        }
        if (!(alpha_channel & 4)) {
            data[idx * 4 + 2] = static_cast<unsigned char>(b);
        }
        if (!(alpha_channel & 8)) {
            data[idx * 4 + 3] = static_cast<unsigned char>(a);
        }

        addNeighbors(idx);
        solid_mark[solid_mark_len++] = idx;
    }

    delete is_solid;
    delete queued;
    delete todo_buf;
    delete solid_mark;
}

Value alphafix(const CallbackInfo &info) {
  Env env = info.Env();

  if (info.Length() != 4) {
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

  // --- Arg 1: Buffer or Uint8Array ---
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

  alphafixImpl(alpha_channel, width, height, data, length);

  return env.Null();
}

Object Init(Env env, Object exports) {
  exports.Set(String::New(env, "alphafix"), Function::New(env, alphafix));
  return exports;
}

NODE_API_MODULE(addon, Init)
