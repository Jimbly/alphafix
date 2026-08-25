const assert = require('assert');
const alphafix = require('../');

console.log('Checking basic functionality...');
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

function check(expected) {
  for (let ii = 0; ii < expected.length; ++ii) {
    assert.equal(imgdata.readUInt32BE(ii*4), expected[ii]);
  }
}

check([
  0xFF000000, 0xFF000000, 0xFF000000,
  0xFF00007f, 0xFF0000ff, 0xFF000000,
  0xFF000000, 0xFF000000, 0xFF000000,
]);

console.log('Initializing speed test...');
const W = 8192;
imgdata = Buffer.alloc(W * W * 4);
let mid = (W/2 * 8192 + W/2) * 4;
imgdata[mid++] = 0xFF;
imgdata[mid++] = 0x00;
imgdata[mid++] = 0xFF;
imgdata[mid++] = 0xFF;
console.log('Performing speed test...');
let start = Date.now();
alphafix({
  image: {
    width: W,
    height: W,
    data: imgdata,
  }
});
let end = Date.now();
console.log(`Finished in ${end - start}ms`);
