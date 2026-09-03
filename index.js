const assert = require('assert');

const { floor, round } = Math;

function alphafixJS(alpha_channel, width, height, bpp, data) {
  if (alpha_channel === 8 && bpp < 4) {
    return false;
  }
  let alpha_channels = [];
  for (let ii = 0; ii < 4; ++ii) {
    if (alpha_channel & (1 << ii)) {
      assert(ii < bpp);
      alpha_channels.push(ii);
    }
  }
  assert.equal(width * height * bpp, data.length);
  let dim = width * height;
  let is_solid = new Uint8Array(dim);
  let queued = new Uint8Array(dim);
  let todo_buf = new Uint32Array(dim);
  let todo_start = 0;
  let todo_end = 0;
  function addNeighbors(idx) {
    let x = idx % width;
    if (x > 0 && !queued[idx - 1]) {
      todo_buf[todo_end++] = idx - 1;
      queued[idx - 1] = 1;
    }
    if (x + 1 < width && !queued[idx + 1]) {
      todo_buf[todo_end++] = idx + 1;
      queued[idx + 1] = 1;
    }
    if (idx >= width && !queued[idx - width]) {
      todo_buf[todo_end++] = idx - width;
      queued[idx - width] = 1;
    }
    if (idx + width < dim && !queued[idx + width]) {
      todo_buf[todo_end++] = idx + width;
      queued[idx + width] = 1;
    }
  }
  for (let idx = 0; idx < dim; ++idx) {
    let solid = false;
    for (let ii = 0; ii < alpha_channels.length; ++ii) {
      let offs = alpha_channels[ii];
      if (data[idx*bpp + offs]) {
        solid = true;
        break;
      }
    }
    if (solid) {
      queued[idx] = 1;
      is_solid[idx] = 1;
      addNeighbors(idx);
    }
  }
  if (!todo_end) {
    // completely solid
    return false;
  }
  let solid_mark = [];
  let loop_end = todo_end;
  let diff = false;
  while (todo_start < todo_end) {
    if (todo_start === loop_end) {
      for (let ii = 0; ii < solid_mark.length; ++ii) {
        is_solid[solid_mark[ii]] = 1;
      }
      solid_mark.length = 0;
      loop_end = todo_end;
    }
    let idx = todo_buf[todo_start++];
    if (is_solid[idx]) {
      continue;
    }
    let y = floor(idx / width);
    let x = idx - y * width;
    let rgba = [0, 0, 0, 0];
    let c = 0;
    function addColor(idx) {
      for (let offs=0; offs < bpp; ++offs) {
        rgba[offs] += data[idx * bpp + offs];
      }
      c++;
    }
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
    assert(c);

    for (let offs=0; offs < bpp; ++offs) {
      if (!(alpha_channel & (1<<offs))) {
        let effidx = idx * bpp + offs;
        let newv = round(rgba[offs] / c);
        if (diff) {
          data[effidx] = newv;
        } else if (data[effidx] !== newv) {
          data[effidx] = newv;
          diff = true;
        }
      }
    }
    addNeighbors(idx);
    solid_mark.push(idx);
  }
  return diff;
}

let PNG;
const PNG_GRAYSCALE = 0;
const PNG_RGB = 2;
const PNG_PALETTE = 3;
const PNG_RGBA = 6;

function pngReadJS(buf, force_bpp) {
  let img = PNG.sync.read(buf);
  let { width, height, data, bpp, colorType } = img;
  let numpix = width * height;
  assert.equal(numpix * 4, data.length);
  if (force_bpp === 4) {
    // we're good
    bpp = 4; // possibly was 1/3, but handled in reading
  } else if (force_bpp) {
    // 1-3
    let newdata = Buffer.alloc(force_bpp * numpix);
    let skip = 4 - bpp;
    for (let inidx = 0, outidx=0; inidx < data.length; ) {
      for (let ii = 0; ii < bpp; ++ii) {
        newdata[outidx++] = data[inidx++];
      }
      inidx += skip;
    }
    data = newdata;
    bpp = force_bpp;
  } else {
    // shrink down to a 3 byte-per-pixel, etc, if that's what the format is
    if (colorType === PNG_RGB || colorType === PNG_GRAYSCALE) {
      let newdata = Buffer.alloc(bpp * numpix);
      let skip = 4 - bpp;
      for (let inidx = 0, outidx=0; inidx < data.length; ) {
        for (let ii = 0; ii < bpp; ++ii) {
          newdata[outidx++] = data[inidx++];
        }
        inidx += skip;
      }
      data = newdata;
    } else if (colorType === PNG_PALETTE) {
      bpp = 4;
    } else if (colorType !== PNG_RGBA) {
      assert(false, `Unhandled PNG colorType ${colorType}`);
    }
  }
  return {
    width,
    height,
    data,
    bpp,
  };
}

function pngWriteJS(width, height, bpp, data) {
  let colorType = bpp === 4 ? PNG_RGBA : bpp === 3 ? PNG_RGB : PNG_GRAYSCALE;
  let img;
  if (bpp === 4) {
    img = new PNG({ width: 1, height: 1, colorType });
    img.width = width;
    img.height = height;
    img.data = data;
  } else {
    img = new PNG({ width, height, colorType });
    if (!img.data) {
      throw new Error(`Out of memory allocating ${width}x${height}x${bpp} PNG`);
    }
    let num_bytes = width * height * 4;
    assert.equal(img.data.length, num_bytes);
    let skip = 4 - bpp - 1;
    for (let inidx = 0, outidx=0; inidx < data.length; ) {
      for (let ii = 0; ii < bpp; ++ii) {
        img.data[outidx++] = data[inidx++];
      }
      outidx += skip;
      img.data[outidx++] = 255;
    }
  }

  return PNG.sync.write(img);
}

let alphafixImpl;
let pngReadImpl;
let pngWriteImpl;
let native = false;
function reinit(force_js) {
  try {
    if (force_js) {
      throw new Error('mocked native not found');
    }
    // eslint-disable-next-line global-require
    let nativemod = require('node-gyp-build')(__dirname);
    alphafixImpl = nativemod.alphafix;
    pngReadImpl = nativemod.pngRead;
    pngWriteImpl = nativemod.pngWrite;
    native = true;
  } catch (e) {
    // eslint-disable-next-line global-require
    PNG = require('pngjs').PNG;
    alphafixImpl = alphafixJS;
    pngReadImpl = pngReadJS;
    pngWriteImpl = pngWriteJS;
  }
}
reinit(false);

function isInteger(v) {
  return typeof v === 'number' && isFinite(v) && floor(v) === v;
}

function checkImageParam(img) {
  assert(img);
  let {
    width,
    height,
    data,
    bpp,
  } = img;
  assert(isInteger(width) && width > 0);
  assert(isInteger(height) && height > 0);
  assert(data && (data instanceof Buffer || data instanceof Uint8Array));
  if (!bpp) {
    bpp = img.bpp = data.length / (width * height);
  }
  assert(isInteger(bpp) && (bpp === 1 || bpp === 3 || bpp === 4));
}

function pngWrite(img) {
  checkImageParam(img);
  return pngWriteImpl(img.width, img.height, img.bpp, img.data);
}

function pngRead(buf, opts) {
  assert(buf && (buf instanceof Buffer || buf instanceof Uint8Array));
  let force_bpp = 0;
  if (opts) {
    assert.equal(typeof opts, 'object');
    if (opts.bpp) {
      assert(isInteger(opts.bpp));
      force_bpp = opts.bpp;
    }
  }
  let header = buf.toString('binary', 0, 4)
  if (header === '\x89PNG') {
    return pngReadImpl(buf, force_bpp);
  } else {
    if (buf.toString('binary', 6, 10) === 'JFIF') {
      throw new Error('Invalid header: this is a JPEG file');
    } else {
      throw new Error(`Invalid header: "${header}"`);
    }
  }
}

function alphafix(opts) {
  let { alpha_channel, image } = opts;
  if (!alpha_channel) {
    alpha_channel = 8;
  }
  checkImageParam(image);
  return alphafixImpl(alpha_channel, image.width, image.height, image.bpp, image.data);
}

module.exports = {
  native,
  alphafix,
  pngRead,
  pngWrite,
  reinit,
};
