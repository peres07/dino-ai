import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, readdirSync} from 'node:fs';

const module = new WebAssembly.Module(readFileSync('dist/dino.wasm'));
const originals = readdirSync('arquivos/redes/DINO');
const dodge = readFileSync('arquivos/redeDesviando');
const original = name => readFileSync(`arquivos/redes/DINO/${name}`);
const movementDir = 'Dino sem pássaros/arquivos/redes/DINO';
const movementModels = readdirSync(movementDir);
const movement = name => readFileSync(`${movementDir}/${name}`);

function engine(birdless = false) {
  const records = [], draws = [], dinos = [], sprites = [], texts = [];
  const browser = Object.fromEntries(WebAssembly.Module.imports(module).map(({name}) => [name, () => {}]));
  browser.record = (score, ptr, len) => records.push([score, Buffer.from(view(ptr, len))]);
  browser.surface = id => draws.push(id);
  browser.sprite = (id, x, y, width) => { sprites.push(id); if (id < 12) dinos.push({x, y, width}); };
  browser.text = (p, len) => texts.push(Buffer.from(view(p, len)).toString('utf8'));
  const e = new WebAssembly.Instance(module, {browser}).exports;
  const view = (p, len) => new Uint8Array(e.memory.buffer, p, len);
  const load = (bytes, type) => {
    assert.ok(bytes.length <= e.model_capacity());
    view(e.model_buffer(), bytes.length).set(bytes);
    return e.model_load(bytes.length, type);
  };
  if (birdless) e.init_birdless(1);
  else e.init(1);
  return {e, view, records, draws, dinos, sprites, texts, load};
}

test('loads each legacy network and rejects invalid or incompatible data', () => {
  const {e, load} = engine();
  assert.equal(e.mode(1), 0);
  assert.equal(load(dodge, 1), 1);
  assert.equal(load(dodge, 0), 3);
  for (const name of originals) assert.equal(load(original(name), 2), 1, name);
  assert.equal(load(original('6611').subarray(0, 147), 2), 0);
  const bad = original('6611');
  bad.writeDoubleLE(Infinity, 28);
  assert.equal(load(bad, 2), 0);
  bad.writeDoubleLE(1e90, 28);
  assert.equal(load(bad, 2), 0);
  assert.equal(e.model_load(e.model_capacity()+1, 2), 0);
  assert.equal(e.mode(1), 1);
});

test('movement edition starts independently and accepts all 19 original movement models only', () => {
  const {e, load, view} = engine(true);
  assert.equal(e.status(31), 1);
  assert.equal(e.status(2), 1000);
  e.frame(50, 0, 0);
  assert.ok(e.status(12) > 0, 'training must run without redeDesviando');
  assert.equal(e.mode(1), 0);
  assert.equal(movementModels.length, 19);
  for (const name of movementModels) assert.equal(load(movement(name), 2), 1, name);
  assert.equal(load(original('6611'), 2), 0);
  assert.equal(load(movement('12343').subarray(0, -1), 2), 0);
  assert.equal(e.mode(1), 1, 'invalid imports do not destroy a loaded model');
  const len = e.export_leader();
  assert.equal(len, 4164);
  const bytes = Buffer.from(view(e.model_buffer(), len));
  assert.deepEqual(Array.from({length: 7}, (_, i) => bytes.readUInt32LE(i*4)), [10,15,7,2,0,0,1]);
  assert.equal(load(bytes, 2), 1);
  const other = engine();
  assert.equal(other.e.status(31), 0);
  assert.equal(other.load(bytes, 2), 3);
});

test('bird edition trains movement alone and freezes the selected weights for firing', () => {
  const {e, load, view, records, sprites, texts} = engine();
  load(dodge,1);
  e.configure(40,8,.4,10);
  e.set_training(2,7);
  assert.equal(e.status(27),2);
  assert.equal(e.status(32),3);
  for (let i=0;i<5;i++) e.command(6);
  for (let i=0;i<95 && (e.status(0)<2 || !e.status(36));i++) e.frame(50,0,0);
  assert.ok(e.status(0)>=2);
  const len=e.export_best();
  assert.equal(len,dodge.length);
  const chosen=Buffer.from(view(e.model_buffer(),len));
  assert.deepEqual(chosen,records.at(-1)[1]);
  e.paint();
  assert.ok(!sprites.includes(39)&&!sprites.includes(40));
  assert.ok(texts.includes('Pular')&&!texts.includes('Atirar'));
  assert.equal(load(chosen,2),3);
  assert.equal(e.mode(1),1);
  assert.equal(e.status(33),1);
  e.mode(0);

  view(e.model_buffer(),len).set(chosen);
  assert.equal(e.start_firing(len-1,8),0);
  assert.equal(e.status(27),2,'rejected model does not reset training');
  assert.equal(e.start_firing(len,8),1);
  assert.equal(e.status(27),0);
  assert.equal(e.status(32),2);
  const first=Buffer.from(view(e.model_buffer(),e.export_leader()));
  assert.equal(first.toString('ascii',0,4),'DNN1');
  const offset=12+first.readUInt32LE(4);
  assert.deepEqual(first.subarray(offset),chosen);
  for (let i=0;i<120 && e.status(0)<3;i++) e.frame(50,0,0);
  assert.ok(e.status(0)>=3);
  const evolved=Buffer.from(view(e.model_buffer(),e.export_leader()));
  assert.notDeepEqual(evolved.subarray(12,offset),first.subarray(12,offset));
  assert.deepEqual(evolved.subarray(offset),chosen,'movement stays byte-identical after evolution');
  assert.equal(load(evolved,2),2);
  e.mode(1);
  assert.equal(e.status(33),0);
  assert.equal(e.start_firing(0,7),1);
  assert.equal(e.export_leader(),308);
});

test('movement edition uses only movement and exports replayable legacy training records', () => {
  const {e, load, records, sprites, texts, view} = engine(true);
  e.configure(30, 6, .3, 10);
  e.reset_training(4);
  for (let i = 0; i < 5; i++) e.command(6);
  for (let i = 0; i < 140 && (e.status(0) <= 1 || !records.length); i++) e.frame(50, 0, 0);
  assert.ok(e.status(0) > 1, 'at least one generation completed');
  assert.ok(records.length > 0, 'a record was emitted');
  const len = e.export_best();
  assert.equal(len, 4164);
  const bytes = Buffer.from(view(e.model_buffer(), len));
  assert.deepEqual(bytes, records.at(-1)[1]);
  assert.equal(load(bytes, 2), 1);
  e.mode(1);
  const angle = e.status(24);
  e.frame(50, 1 | 128 | 256 | 512, 0);
  assert.ok(e.status(22) > 0, 'human can jump');
  assert.equal(e.status(24), angle);
  e.paint();
  assert.ok(!sprites.some(id => (id >= 21 && id <= 24) || id === 39 || id === 40), 'no bird, weapon or projectile sprites');
  assert.ok(texts.includes('Pular') && !texts.includes('Atirar'));
});

test('test-mode camera is allowed in movement edition and remains fixed in birds edition', () => {
  for (const birdless of [true, false]) {
    const {e, load} = engine(birdless);
    if (!birdless) load(dodge, 1);
    load(birdless ? movement('12343') : original('6611'), 2);
    assert.equal(e.mode(1), 1);
    e.frame(40, 0, 8 | 16);
    assert.equal(e.status(20) > 0, birdless);
    assert.equal(e.status(19) > 1, birdless);
    e.command(0);
    assert.equal(e.status(20), 0);
    assert.equal(e.status(19), 1);
  }
});

test('evolves generations, applies settings, and exports a compatible record', () => {
  const {e, records, load, view} = engine();
  assert.equal(load(dodge, 1), 1);
  e.set_training(0, 1);
  e.configure(48, 6, .3, 30);
  e.reset_training(1);
  assert.equal(e.status(2), 48);
  e.configure(32, 5, .25, 20);
  assert.equal(e.status(18), 1);
  for (let i = 0; i < 5; i++) e.command(6);
  for (let i = 0; i < 500 && (e.status(0) <= 10 || e.status(4)<2); i++) e.frame(50, 0, 0);
  assert.ok(e.status(0) > 10);
  assert.equal(e.status(2), 32);
  assert.equal(e.status(18), 0);
  assert.equal(e.status(13), e.status(0)-1);
  assert.ok(records.length >= 2);
  assert.ok(e.status(4) >= 2, 'fitness is now measured in hits, not distance or coins');
  for (let i = 1; i < records.length; i++) assert.ok(records[i][0] >= records[i-1][0]);
  const len = e.export_best();
  const bytes = Buffer.from(view(e.model_buffer(), len));
  assert.equal(len, 308);
  assert.deepEqual(bytes, records.at(-1)[1]);
  assert.equal(load(bytes, 2), 1);
});

test('complete training evolves movement and aim and saves both for testing', () => {
  const {e, records, load, view, dinos} = engine();
  assert.equal(e.status(27), 0, 'the native training mode starts with a fixed movement network');
  assert.equal(load(dodge, 1), 1);
  e.set_training(1, 1);
  e.configure(50, 8, .3, 30);
  e.reset_training(1);
  e.frame(50, 0, 0);
  e.paint();
  assert.ok(new Set(dinos.map(d => d.y)).size > 1, 'movement decisions differ, not just on-screen placement');

  for (let i = 0; i < 5; i++) e.command(6);
  for (let i = 0; i < 500 && (e.status(0) <= 1 || !e.status(4)); i++) e.frame(50, 0, 0);
  assert.ok(e.status(0) > 1);
  assert.ok(records.length);
  const len = e.export_best();
  const bytes = Buffer.from(view(e.model_buffer(), len));
  assert.equal(bytes.toString('ascii', 0, 4), 'DNN1');
  assert.equal(bytes.readUInt32LE(4), 308);
  assert.equal(bytes.readUInt32LE(8), dodge.length);
  assert.equal(len, 12 + 308 + dodge.length);
  assert.deepEqual(bytes, records.at(-1)[1]);
  assert.notDeepEqual(bytes.subarray(12 + 308), dodge);
  assert.equal(load(bytes.subarray(0, bytes.length-1), 2), 0);
  assert.equal(load(bytes, 2), 2);
  assert.equal(e.mode(1), 1);
  assert.equal(e.status(28), 1);
  assert.equal(load(original('6611'), 2), 1);
  assert.equal(e.status(28), 0);
});

test('test mode uses the movement network packed with the aiming network', () => {
  const {e, load, dinos} = engine();
  load(dodge, 1);
  const aim = original('6611'), still = Buffer.from(dodge);
  still.fill(0, 28);
  const header = Buffer.alloc(12);
  header.write('DNN1');
  header.writeUInt32LE(aim.length, 4);
  header.writeUInt32LE(still.length, 8);
  assert.equal(load(Buffer.concat([header, aim, still]), 2), 2);
  e.mode(1);
  e.frame(50, 0, 0);
  e.paint();
  assert.equal(dinos.length, 2);
  assert.equal(dinos[0].y, dinos[1].y, 'AI and idle player stay on the ground');
  load(aim, 2);
  e.mode(1);
  e.frame(50, 0, 0);
  dinos.length = 0;
  e.paint();
  assert.notEqual(dinos[0].y, dinos[1].y, 'legacy files fall back to the original dodge network');
});

test('evaluates two training rounds before evolving or saving records', () => {
  const {e, load, records} = engine();
  load(dodge, 1);
  e.configure(4, 2, .3, 10);
  e.set_training(0, 1);
  let second = false;
  for (let i = 0; i < 500 && e.status(0) === 1; i++) {
    e.frame(50, 0, 0);
    if (e.status(0) === 1) assert.equal(records.length,0);
    if (e.status(29) === 2) second = true;
  }
  assert.equal(second, true);
  assert.equal(e.status(0), 2);
  assert.equal(e.status(29), 1);
  load(original('6611'), 2);
  e.mode(1);
  for (let i=0;i<5;i++) e.command(6);
  for (let i = 0; i < 2400 && e.status(30) < 2; i++) e.frame(50, 0, 0);
  assert.ok(e.status(30) >= 2);
  assert.equal(e.status(0), 2);
});

test('manual mode pauses training, accepts movement, and can restart', () => {
  const {e, load} = engine();
  load(dodge, 1);
  load(original('6611'), 2);
  e.frame(50, 0, 0);
  const tick = e.status(12);
  assert.equal(e.mode(1), 1);
  const startX = e.status(25), startAngle = e.status(24);
  e.frame(50, 1 | 16 | 128 | 512, 0);
  assert.ok(e.status(22) > 0);
  assert.ok(e.status(25) > startX);
  assert.ok(e.status(24) > startAngle, 'A aims up');
  e.command(10);
  e.frame(20, 32 | 64, 0);
  assert.ok(Math.abs(e.status(22) - 9*1.8*(1200/1920)) < 1e-9);
  assert.ok(e.status(25) > startX);
  assert.ok(e.status(23) < 100);
  e.command(10);
  assert.equal(e.status(12), 0);
  e.mode(0);
  assert.equal(e.status(12), tick);
});

test('camera, pause, speed, overlay and headless update are independent', () => {
  const {e, load, draws, texts, sprites} = engine();
  load(dodge, 1);
  e.frame(40, 0, 8|16);
  assert.ok(e.status(19) > 1 && e.status(20) > 0);
  e.command(0);
  assert.equal(e.status(19), 1);
  assert.equal(e.status(20), 0);
  e.paint();
  assert.deepEqual(draws.splice(0), [0]);
  const hud = texts.splice(0);
  assert.ok(hud.includes('Indivíduos Vivos:'));
  assert.ok(hud.includes('1.000x'), 'TA uses the native %.3fx format');
  assert.ok(hud.some(t => /^\d+h:\d+m:\d+s$/.test(t)), 'training time uses %dh:%dm:%ds');
  e.command(2);
  e.paint();
  assert.deepEqual(draws.splice(0), [0]);
  assert.ok(sprites.splice(0).includes(69), 'expanded overlay draws the original neuron sprite');
  assert.ok(!texts.splice(0).includes('Indivíduos Vivos:'));
  e.command(8);
  e.paint();
  assert.deepEqual(draws.splice(0), [0]);
  assert.ok(!sprites.splice(0).includes(69));
  e.command(3);
  assert.equal(e.status(10), 1);
  e.command(9);
  const tick = e.status(12);
  e.frame(50, 0, 0);
  assert.equal(e.status(12), tick);
  e.command(9);
  e.command(1);
  e.frame(50, 0, 0);
  assert.ok(e.status(12) > tick+3);
  e.command(6);
  assert.equal(e.status(6), 2);
  e.command(7);
  e.command(7);
  assert.equal(e.status(6), .5);
  e.command(6);
  assert.equal(e.status(6), 1);
});

test('tree layers follow the native two passes', () => {
  for (const birdless of [false, true]) {
    const {e, load, sprites} = engine(birdless);
    if (birdless) e.configure(30, 6, 1, 30);
    else { load(dodge, 1); e.configure(30, 6, 1, 30); }
    e.reset_training(7);
    e.command(6); // fast-forward until a tree is visible
    let frame = null;
    for (let i = 0; i < 2000 && !frame; i++) {
      e.frame(50, 0, 0);
      sprites.length = 0;
      e.paint();
      if (sprites.includes(62)) frame = [...sprites];
    }
    assert.ok(frame, `${birdless ? 'sem' : 'com'} pássaros: a tree is on screen`);
    const base = frame.indexOf(36), deep = frame.indexOf(63), deeper = frame.indexOf(64), front = frame.indexOf(62);
    assert.ok(base >= 0 && front > base, 'arvore.png comes from the back pass and arvore2 from the front pass');
    if (birdless) {
      assert.equal(deep, -1); assert.equal(deeper, -1);
    } else {
      /* arvore3/arvore4 sit on manager+0x7c/+0x80 and are drawn in pass 1. */
      assert.ok(deep > base && deep < front, 'arvore3 is drawn behind the runner');
      assert.ok(deeper > deep && deeper < front, 'arvore4 is drawn behind the runner');
    }
  }
});

test('population sprites are painted at real locations, not in an artificial row', () => {
  const {e, load, dinos} = engine();
  load(dodge, 1);
  e.configure(4, 2, .3, 30);
  e.reset_training(1);
  const startX = e.status(25);
  e.paint();
  assert.equal(dinos.length, 4);
  assert.ok(dinos.every(d => d.y === dinos[0].y));
  assert.ok(dinos.every(d => d.x === startX));
  assert.equal(e.status(25), startX);
  assert.equal(e.status(1), 4);

  dinos.length = 0;
  e.configure(500, 50, .3, 30);
  e.reset_training(1);
  e.paint();
  assert.equal(dinos.length, 500);
  assert.equal(new Set(dinos.map(d => d.x)).size, 1);
  assert.equal(e.status(1), 500);
});
