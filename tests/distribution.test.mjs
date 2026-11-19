import test from 'node:test';
import assert from 'node:assert/strict';
import {existsSync, readFileSync, readdirSync} from 'node:fs';
import {resolve, sep} from 'node:path';

const read = (...parts) => readFileSync(resolve(...parts));
const json = (...parts) => JSON.parse(read(...parts));

test('both routes and all static links work under a path prefix', () => {
  const root = new URL('https://example.test/experimentos/');
  for (const edition of ['', 'sem-passaros/']) {
    const page = new URL(edition, root);
    const html = read('dist', edition, 'index.html').toString();
    assert.match(html, new RegExp(`data-edition="${edition ? 'sem-passaros' : 'com-passaros'}"`));
    const links = [...html.matchAll(/(?:href|src)="([^"#]+)"/g)].map(([, path]) => new URL(path, page));
    assert.ok(links.some(url => url.href === root.href));
    assert.ok(links.some(url => url.href === new URL('sem-passaros/', root).href));
    for (const url of links) {
      assert.ok(url.href.startsWith(root.href), url.href);
      const path = url.pathname.slice(root.pathname.length);
      assert.ok(existsSync(resolve('dist', path.endsWith('/') || path === '' ? `${path}index.html` : path)), url.href);
    }
    assert.equal(/id="training-policy"/.test(html), edition === '');
  }
});

test('each edition packages its own original networks, font and referenced sprites', () => {
  for (const [edition, pkg] of [['', '.'], ['sem-passaros', 'Dino sem pássaros']]) {
    const target = resolve('dist', edition);
    const source = resolve(pkg, 'arquivos', 'redes', 'DINO');
    const models = json(target, 'models', 'manifest.json');
    assert.deepEqual([...models].sort(), readdirSync(source).filter(name => /^\d+$/.test(name)).sort());
    for (const name of models) assert.deepEqual(read(target, 'models', name), read(source, name));
    assert.equal(existsSync(resolve(target, 'models', 'redeDesviando')), !edition);
    assert.deepEqual(read(target, 'assets', 'arial.ttf'), read(pkg, 'fontes', 'arial.ttf'));
    const sprites = json(target, 'assets', 'manifest.json');
    assert.equal(sprites.length, new Set(sprites).size);
    for (const id of [...sprites, 68, 69, 70, 71]) {
      assert.ok(sprites.includes(id));
      assert.equal(read(target, 'assets', `${id}.png`).readUInt32BE(), 0x89504e47);
    }
    assert.deepEqual(read(target, 'assets', '68.png'), read(pkg, 'arquivos', 'imagens', 'desenharNeural',
      edition ? 'neuronio2.png' : 'neuronio20.png'));
    assert.ok(!readdirSync(target, {recursive:true}).some(file => /\.(exe|dll)$/i.test(file)), target + sep);
  }
});
