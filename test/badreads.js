const fs = require('fs');
const { pngRead } = require('../');

const DIR = `${__dirname}/corrupt`;
let files = fs.readdirSync(DIR);
console.log(`Found ${files.length} files`);
let good = [0, 0];
let bad = [0, 0];
for (let ii = 0; ii < files.length; ++ii) {
  let fn = files[ii];
  if (!fn.toLowerCase().endsWith('.png')) {
    continue;
  }
  let prefix = `${ii}/${files.length} ${fn} `;
  // process.stdout.write(`${prefix}\r`);
  let buf = fs.readFileSync(`${DIR}/${fn}`);
  let ok = 0;
  try {
    pngRead(buf);
    // console.log(`${prefix} OK`);
    ok = 1;
  } catch (e) {
    console.log(`${prefix} ${e}`);
  }
  if (fn.includes('bad')) {
    bad[ok]++;
  } else {
    good[ok]++;
  }
}
console.log(`${bad[1]}/${bad[0]+bad[1]} "bad" files read OK`);
console.log(`${good[1]}/${good[0]+good[1]} "recoverable" files read OK`);

let seed = 3425149915;
const m = Math.pow(2, 32);
function randInt(max) {
  seed = (1664525 * seed + 1013904223) % m;
  return Math.floor(seed / m * max);
}

console.log('Testing random corrupt to ensure no hard crash...');
let buf = fs.readFileSync(`${DIR}/bad10.png`);
let b2 = Buffer.alloc(buf.length);
let successes = 0;
for (let ii = 0; ii < 100; ++ii) {
  buf.copy(b2);
  let numr = ii + 1;
  for (let jj = 0; jj < numr; ++jj) {
    let offs = randInt(b2.length);
    let v = b2[offs];
    let bit = randInt(8);
    v ^= (1 << bit);
    b2[offs] = v;
  }
  try {
    pngRead(b2);
    ++successes;
  } catch (e) {
    // ignored
  }
}
console.log(`Done (${successes} read anyway).`);
