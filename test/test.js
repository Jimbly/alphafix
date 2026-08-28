const assert = require('assert');
const fs = require('fs');
const alphafix = require('../');

console.log(`Using ${alphafix.native ? 'native' : 'JS'} implementation`);

console.log('Checking basic functionality...');
let imgdata = Buffer.from(new Uint32Array([
  0x00000000, 0x00000000, 0x00000000,
  0xFF00007f, 0xFF0000ff, 0x00000000,
  0x00000000, 0x00000000, 0x00000000,
]).buffer);
imgdata.swap32(); // expect byte-order of R, G, B, A

alphafix.alphafix({
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


function testPNG() {
  let pngdata = fs.readFileSync(`${__dirname}/testRGB.png`);
  let img = alphafix.pngRead(pngdata);
  assert.equal(img.bpp, 3);
  assert.equal(img.data.length, img.width * img.height * img.bpp);
  img = alphafix.pngRead(pngdata, { bpp: 4 });
  assert.equal(img.bpp, 4);
  assert.equal(img.data.length, img.width * img.height * img.bpp);
}
console.log('Checking PNG functionality (JS)...');
alphafix.reinit(true);
testPNG();
console.log('Checking PNG functionality (native)...');
alphafix.reinit(false);
testPNG();


function testFile(filename) {
  console.log(`\nTesting "${filename}"...`);
  let pngdata = fs.readFileSync(`${__dirname}/${filename}`);
  function dotest(name) {
    let start = Date.now();
    let png = alphafix.pngRead(pngdata);
    // console.log('  read finished')
    let time1 = Date.now();
    let time_read = time1 - start;
    let ret = alphafix.alphafix({
      image: png
    });
    // console.log('  fix finished')
    let time2 = Date.now();
    let time_fix = time2 - time1;
    let res;
    if (ret) {
      res = alphafix.pngWrite(png);
    } else {
      res = pngdata;
    }
    let time3 = Date.now();
    let time_write = time3 - time2;
    console.log(`Total ${time3 - start}ms, ${time_read}ms read, ${time_fix}ms fix, ${time_write}ms write`);
    console.log(`  result size = ${res.length} ${ret ? '' : '(no change)'}`);
    fs.writeFileSync(`${__dirname}/test-out-${name}.png`, res);
  }
  console.log('Performing speed test (js)...');
  alphafix.reinit(true);
  dotest('js');
  console.log('Performing speed test (native)...');
  alphafix.reinit(false);
  dotest('native');
}
testFile('testRGB.png');
testFile('testRGBA.png');
testFile('testPalettedRGB.png');
testFile('testPalettedRGBA.png');
testFile('testGrayscale.png');
if (fs.existsSync(`${__dirname}/testLarge.png`)) {
  testFile('testLarge.png');
  testFile('testLargeNoChange.png');
}
