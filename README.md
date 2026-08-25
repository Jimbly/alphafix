Alpha Fix
=========

Photoshop writes pixels with 0 alpha but a bright white color (or other random patterns), which causes
interpolation errors - instead, we want to spread the nearest non-alpha color.
In this extended version, continue spreading until all alpha pixels are full
to fix blending even at low mipmap levels.

See https://www.dashingstrike.com/ImageProcessor/ - "Photoshop has an alpha problem" for details and examples.

Works with [pngjs](https://www.npmjs.com/package/pngjs) images or any `width`+`height`+`rgba data`.

API usage:
```javascript
const alphafix = require('alphafix');

let imgdata = Buffer.from(new Uint32Array([
  0x00000000, 0x00000000, 0x00000000,
  0xFF00007f, 0xFF0000ff, 0x00000000,
  0x00000000, 0x00000000, 0x00000000,
]).buffer);
imgdata.swap32(); // expect byte-order of R, G, B, A

alphafix({
  alpha_channel: 8, // bitmask, 1=R, etc; default=A
  image: {
    width: 3,
    height: 3,
    data: imgdata,
  },
});

// imgdata now contains:
// 0xFF000000, 0xFF000000, 0xFF000000,
// 0xFF00007f, 0xFF0000ff, 0xFF000000,
// 0xFF000000, 0xFF000000, 0xFF000000,

```
