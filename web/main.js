const $ = id => document.getElementById(id);
const birdless = document.documentElement.dataset.edition === 'sem-passaros';
const ctx = $('scene').getContext('2d', {alpha:false});
const logicalWidth = 1200, logicalHeight = 675, unit = ctx.canvas.width/logicalWidth;
const decoder = new TextDecoder();
const color = rgb => `#${rgb.toString(16).padStart(6, '0')}`;
const tintCache = new Map();
let sprites, api, db;
const message = text => { ($('preferences').open ? $('settings-message') : $('message')).textContent = text; };

function tinted(id, tint) {
  const key = `${id}:${tint}`;
  if (!tintCache.has(key)) {
    const src = sprites[id], canvas = document.createElement('canvas');
    canvas.width = src.naturalWidth;
    canvas.height = src.naturalHeight;
    const c = canvas.getContext('2d');
    c.drawImage(src, 0, 0);
    c.globalCompositeOperation = 'multiply';
    c.fillStyle = color(tint);
    c.fillRect(0, 0, canvas.width, canvas.height);
    c.globalCompositeOperation = 'destination-in';
    c.drawImage(src, 0, 0);
    tintCache.set(key, canvas);
  }
  return tintCache.get(key);
}

const browser = {
  surface() {
    ctx.setTransform(unit, 0, 0, unit, 0, 0);
    ctx.imageSmoothingEnabled = false;
  },
  camera(x, y, z) {
    ctx.setTransform(unit*z, 0, 0, unit*z, unit*((1-z)*logicalWidth/2 - x*z), unit*((1-z)*logicalHeight/2 - y*z));
  },
  rect(x, y, w, h, rgb, a) {
    ctx.fillStyle = color(rgb); ctx.globalAlpha = a; ctx.fillRect(x, y, w, h);
  },
  line(x, y, x2, y2, rgb, width, a) {
    ctx.strokeStyle = color(rgb); ctx.globalAlpha = a; ctx.lineWidth = width;
    ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x2, y2); ctx.stroke();
  },
  sprite(id, x, y, w, h, rotation, a, tint) {
    if (!sprites[id]) return;
    const src = tint ? tinted(id, tint) : sprites[id];
    ctx.globalAlpha = a;
    if (!rotation) ctx.drawImage(src, x, y, w, h);
    else {
      ctx.save(); ctx.translate(x + w/2, y + h/2); ctx.rotate(rotation);
      ctx.drawImage(src, -w/2, -h/2, w, h); ctx.restore();
    }
  },
  text(ptr, len, x, y, size, rgb) {
    ctx.fillStyle = color(rgb); ctx.globalAlpha = 1; ctx.font = `${size}px DinoArial, Arial, sans-serif`;
    ctx.fillText(decoder.decode(view(ptr, len)), x, y);
  },
  mini(x, y) {
    const fixed = api.status(5) && !birdless;
    const z = fixed ? 1 : api.status(19);
    if (!fixed) {
      x = (x - api.status(20))*z + (1-z)*logicalWidth/2;
      y = (y - api.status(21))*z + (1-z)*logicalHeight/2;
    }
    const side = 126;
    const left = Math.max(0, Math.min(logicalWidth-side, x-side/2));
    const top = Math.max(0, Math.min(logicalHeight-side, y-side*.74));
    ctx.globalAlpha = 1;
    ctx.fillStyle = '#ff8700';
    ctx.fillRect(1060, 505, 128, 128);
    ctx.drawImage(ctx.canvas, left*unit, top*unit, side*unit, side*unit, 1061, 506, side, side);
  },
  record(score, ptr, size, type) {
    const name = modelName(score, type);
    addModel({id: `record:${name}`, name, score, type,
      kind: `recorde${modelLabel(type)}`, bytes: view(ptr, size).slice()});
  },
};

const view = (ptr, size) => new Uint8Array(api.memory.buffer, ptr, size);
async function fetchFile(path) {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`${path}: HTTP ${response.status}`);
  return response;
}
const fetchBytes = async path => new Uint8Array(await (await fetchFile(path)).arrayBuffer());
function image(id) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error(`Não foi possível carregar o sprite ${id}.`));
    img.src = `assets/${id}.png`;
  });
}

function openDB() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(birdless ? 'dino-hardmode-sem-passaros' : 'dino-hardmode', 1);
    req.onupgradeneeded = () => req.result.createObjectStore('networks', {keyPath: 'id'});
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

function readSaved() {
  return new Promise((resolve, reject) => {
    const req = db.transaction('networks').objectStore('networks').getAll();
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

const models = new Map();
const modelLabel = type => birdless ? '' : type === 2 ? ' · completo' : type === 3 ? ' · movimento' : ' · disparo';
const modelName = (score, type) => `${score}${birdless ? '' : type === 2 ? '.dino' : type === 3 ? '.mov' : ''}`;
let dirtyModels = true;
let activeMovement = 'default';
function addModel(entry) {
  models.set(entry.id, entry);
  dirtyModels = true;
  if (db) {
    try {
      const txn = db.transaction('networks', 'readwrite');
      txn.onabort = () => message('Não foi possível salvar a rede neste navegador. Você ainda pode baixá-la.');
      txn.objectStore('networks').put(entry);
    } catch {
      message('Não foi possível salvar a rede neste navegador. Você ainda pode baixá-la.');
    }
  }
}

function refreshModels() {
  if (!dirtyModels) return;
  const select = $('models'), previous = select.value;
  const movementSelect = $('firing-dodge');
  if ((document.activeElement === select && select.options.length && models.has(previous)) ||
      (movementSelect && document.activeElement === movementSelect && movementSelect.options.length)) return;
  dirtyModels = false;
  const ordered = Array.from(models.values()).sort((a,b) => b.score - a.score || a.name.localeCompare(b.name));
  select.replaceChildren(...ordered.map(entry => new Option(`${entry.name} · ${entry.kind}`, entry.id)));
  if (models.has(previous)) select.value = previous;
  if (movementSelect) {
    const selected = movementSelect.value || activeMovement;
    movementSelect.replaceChildren(new Option('Original (redeDesviando)', 'default'),
      ...ordered.filter(entry => entry.type === 3).map(entry => new Option(entry.name,entry.id)));
    if (models.get(selected)?.type === 3) movementSelect.value = selected;
  }
  const focusedModel = document.activeElement?.dataset?.model;
  const buttons = ordered.map(entry => {
    const button = document.createElement('button');
    button.type = 'button'; button.textContent = entry.name; button.dataset.model = entry.id;
    return button;
  });
  $('test-models').replaceChildren(...buttons);
  if (focusedModel) buttons.find(b => b.dataset.model === focusedModel)?.focus({preventScroll:true});
}

function loadModel(bytes, role) {
  if (bytes.length > api.model_capacity()) return false;
  view(api.model_buffer(), bytes.length).set(bytes);
  return api.model_load(bytes.length, role);
}

function download(name, bytes) {
  const url = URL.createObjectURL(new Blob([bytes], {type: 'application/octet-stream'}));
  const link = document.createElement('a');
  link.href = url; link.download = name; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function selectTest(id) {
  if (id) {
    const entry = models.get(id);
    if (!entry || !loadModel(entry.bytes, 2)) return message('Escolha uma rede válida para o teste.');
  } else {
    const size = api.export_leader();
    if (!size || !api.model_load(size, 2)) return message('Ainda não há uma rede de treino para testar.');
  }
  $('test-menu').hidden = true;
  api.mode(1);
  held.clear();
  $('stage').focus({preventScroll:true});
  sync();
}

const set = (id, text) => { if ($(id).textContent !== text) $(id).textContent = text; };
function sync() {
  const s = api.status, test = !!s(5), choosing = !$('test-menu').hidden, drawing = !!s(7), paused = !!s(11);
  $('training-stats').hidden = test || choosing;
  $('test-stats').hidden = !test || choosing;
  $('average').setAttribute('aria-pressed', !!s(10));
  $('toggle-view').setAttribute('aria-pressed', drawing);
  $('export-best').disabled = !s(36);
  if (!birdless) $('save-movement').hidden = s(27) !== 2;
  set('toggle-mode', test || choosing ? 'Voltar ao treino ↵' : 'Testar ↵');
  set('pause', paused ? 'Continuar P' : 'Pausar P');
  set('mode-label', choosing ? 'Selecione a rede' : paused ? 'Pausado' : test ? 'Teste manual' : 'Treinando');
  set('speed', `${s(6)}×`);
  const goal = birdless || s(27) === 2 ? 'Movimento' : s(27) === 0 ? 'Disparo' : 'Movimento e disparo';
  set('training-stats', `${goal}; geração ${s(0)-1}; vivos ${s(1)} de ${s(2)}; recorde ${s(4)}`);
  set('test-stats', `Jogador ${Math.floor(s(14))}; IA ${Math.floor(s(15))}`);
}

// Mirrors the input bitfields and command IDs in wasm/game.h and wasm/game.c.
const playKeys = {ArrowUp:1, ArrowDown:2, KeyZ:4, ArrowLeft:8, ArrowRight:16, KeyX:32, KeyC:64};
const panKeys = {KeyW:1, KeyS:2, KeyA:4, KeyD:8, KeyQ:16, KeyE:32};
const commands = {Space:0, Escape:1, KeyR:2, KeyF:3, KeyG:4, KeyH:5, KeyN:6, KeyM:7, KeyU:8, KeyP:9, KeyT:10};
if (!birdless) {
  Object.assign(playKeys, {KeyA:128, KeyD:256, KeyS:512});
  commands.KeyV = 11;
}
const held = new Set();
const mask = map => Array.from(held).reduce((bits,key) => bits | (map[key] ?? 0), 0);

function bind() {
  const menu = $('test-menu'), preferences = $('preferences');
  const toggleMode = () => {
    held.clear();
    if (api.status(5) || !menu.hidden) {
      menu.hidden = true;
      api.mode(0);
      $('stage').focus({preventScroll:true});
    } else {
      menu.hidden = false;
      refreshModels();
      $('test-leader').focus({preventScroll:true});
    }
    sync();
  };
  $('toggle-mode').onclick = toggleMode;
  $('test-leader').onclick = () => selectTest();
  $('test-models').onclick = event => {
    const id = event.target.closest('button[data-model]')?.dataset.model;
    if (id) selectTest(id);
  };
  $('open-settings').onclick = () => { held.clear(); refreshModels(); preferences.showModal(); };
  if (!birdless) {
    const policy = $('training-policy'), field = $('firing-dodge-field');
    policy.onchange = () => {field.hidden = policy.value !== '0';};
    $('save-movement').onclick = () => {
      if (api.status(27) !== 2) return;
      const best = api.export_best(), size = best || api.export_leader();
      if (!size) return;
      const score = best ? api.status(4) : 0;
      const name = best ? modelName(score,3) : `movimento-${Date.now()}.mov`;
      const id = best ? `record:${name}` : `saved:${name}`;
      addModel({id,name,score,type:3,kind:'salva · movimento',bytes:view(api.model_buffer(),size).slice()});
      refreshModels();
      $('firing-dodge').value = id;
      message(`Rede de movimento salva: ${name}. Selecione “Só disparo” para usá-la.`);
    };
  }
  $('close-settings').onclick = () => preferences.close();
  preferences.addEventListener('close', () => { held.clear(); if (menu.hidden) $('stage').focus({preventScroll:true}); });
  document.querySelectorAll('[data-cmd]').forEach(button => {
    button.onclick = () => {
      const action = Number(button.dataset.cmd);
      api.command(action); sync();
      if ([1,2,4,5,8,11].includes(action)) preferences.close();
    };
  });
  window.addEventListener('keydown', event => {
    if (preferences.open || event.target.closest?.('.site-header') || event.ctrlKey || event.altKey || event.metaKey || event.isComposing) return;
    const code = event.code;
    if (!menu.hidden) {
      if (code === 'Escape') { event.preventDefault(); toggleMode(); }
      if (code === 'ArrowUp' || code === 'ArrowDown') {
        event.preventDefault();
        const buttons = Array.from(menu.querySelectorAll('button'));
        const i = buttons.indexOf(document.activeElement);
        buttons[Math.max(0,Math.min(buttons.length-1,i+(code === 'ArrowDown'?1:-1)))].focus();
      }
      return;
    }
    if (event.target instanceof HTMLElement && event.target.matches('input, select, textarea, [contenteditable]')) return;
    if ((code === 'Enter' || code === 'Space') && event.target instanceof HTMLElement &&
         event.target.matches('button, summary, a')) return;
    if (code === 'Enter') { event.preventDefault(); if (!event.repeat) toggleMode(); }
    if (code in commands) { event.preventDefault(); if (!event.repeat) api.command(commands[code]); }
    const testing = api.status(5);
    if ((testing && code in playKeys && (!api.status(33) || playKeys[code] <= 64)) || ((!testing || birdless) && code in panKeys)) {
      event.preventDefault(); held.add(code);
    }
  });
  window.addEventListener('keyup', event => held.delete(event.code));
  window.addEventListener('blur', () => held.clear());
  $('settings').onsubmit = event => {
    event.preventDefault();
    const pop = Number($('population-setting').value);
    const parents = Math.min(pop, Number($('parents').value));
    $('parents').value = parents;
    const policy = Number($('training-policy')?.value), selected = $('firing-dodge')?.value || 'default';
    const changed = !birdless && (policy !== api.status(27) || policy === 0 && selected !== activeMovement);
    if (changed && !window.confirm('Trocar a etapa ou a rede fixa reinicia a população e o gráfico. As redes salvas serão mantidas.')) {
      $('training-policy').value = api.status(27);
      $('firing-dodge').value = activeMovement;
      $('training-policy').onchange();
      return;
    }
    api.configure(pop, parents, Number($('mutation').value), Number($('limit').value));
    if (changed) {
      if (policy === 0) {
        const bytes = models.get(selected)?.bytes;
        if (selected !== 'default' && !bytes) return message('Selecione uma rede de movimento válida.');
        if (bytes) view(api.model_buffer(),bytes.length).set(bytes);
        if (!api.start_firing(bytes?.length ?? 0,seed())) return message('A rede de movimento escolhida é incompatível.');
        activeMovement = selected;
      } else api.set_training(policy, seed());
      api.mode(0);
      menu.hidden = true;
      message(policy === 0 ? 'Treinando só o disparo. A rede de movimento está fixa.' : 'Treinamento reiniciado com o novo objetivo.');
    } else message(api.status(18) ? 'Ajustes agendados para a próxima geração.' : 'Ajustes já em uso.');
  };
  $('reset').onclick = () => {
    if (!window.confirm('Reiniciar o treino e limpar o gráfico? As redes salvas serão mantidas.')) return;
    api.mode(0);
    menu.hidden = true;
    api.reset_training(seed());
    message('Treinamento reiniciado.');
  };
  $('import').onclick = () => $('model-file').click();
  $('model-file').onchange = async event => {
    const [file] = event.target.files;
    event.target.value = '';
    if (!file) return;
    try {
      if (file.size > api.model_capacity()) throw new Error('arquivo maior que o formato suportado');
      const bytes = new Uint8Array(await file.arrayBuffer());
      const type = loadModel(bytes, 0);
      if (!type) throw new Error('formato ou arquitetura da rede inválidos');
      const entry = {id: `import:${Date.now()}:${file.name}`, name: file.name, score: 0,
        type,kind:`importada${modelLabel(type)}`,bytes};
      addModel(entry);
      refreshModels();
      $('models').value = entry.id;
      message(`Rede importada: ${file.name}`);
    } catch (error) { message(`Não foi possível importar: ${error.message}.`); }
  };
  $('export').onclick = () => {
    const entry = models.get($('models').value);
    if (entry) download(entry.name, entry.bytes);
  };
  $('export-best').onclick = () => {
    const size = api.export_best();
    if (size) download(modelName(api.status(4),api.status(32)),view(api.model_buffer(),size).slice());
  };
  $('scene').addEventListener('wheel', event => {
    if (!menu.hidden || api.status(9) !== 3) return;
    event.preventDefault();
    api.graph_scroll(Math.sign(event.deltaX || event.deltaY)*12);
  }, {passive:false});
  /* The native network view picks the neuron under the cursor. */
  const point = event => {
    const rect = $('scene').getBoundingClientRect();
    api.pointer(1, (event.clientX-rect.left)/rect.width*logicalWidth,
      (event.clientY-rect.top)/rect.height*logicalHeight);
  };
  $('scene').addEventListener('pointermove', point);
  $('scene').addEventListener('pointerdown', point);
  $('scene').addEventListener('pointerleave', () => api.pointer(0,0,0));
}

const seed = () => crypto.getRandomValues(new Uint32Array(1))[0];
let previousTime, lastAux = -Infinity, lastModels = -Infinity;
function tick(now) {
  const elapsed = now - (previousTime ?? now);
  previousTime = now;
  if ($('test-menu').hidden && !$('preferences').open) {
    api.frame(elapsed, mask(playKeys), mask(panKeys));
    api.paint();
  }
  if (now - lastAux >= 100) { sync(); lastAux = now; }
  if (now - lastModels >= 500) { refreshModels(); lastModels = now; }
  requestAnimationFrame(tick);
}

async function boot() {
  const [wasm, assets, dodge, manifest, storage] = await Promise.all([
    fetchBytes(new URL('dino.wasm', import.meta.url)).then(bytes => WebAssembly.instantiate(bytes, {browser})),
    fetchFile('assets/manifest.json').then(r => r.json()).then(ids =>
      Promise.all(ids.map(async id => [id, await image(id)]))).then(Object.fromEntries),
    birdless ? null : fetchBytes('models/redeDesviando'), fetchFile('models/manifest.json').then(r => r.json()),
    openDB().catch(() => null),
  ]);
  api = wasm.instance.exports; sprites = assets; db = storage;
  if (birdless) api.init_birdless(seed());
  else {
    api.init(seed());
    if (!loadModel(dodge, 1)) throw new Error('redeDesviando incompatível');
  }
  const entries = await Promise.all(manifest.map(async name => ({id:`original:${name}`, name, score:Number(name),
    kind:'original', bytes:await fetchBytes(`models/${encodeURIComponent(name)}`)})));
  for (const entry of entries) {
    const type=loadModel(entry.bytes,0);
    if (type) models.set(entry.id,{...entry,type});
  }
  if (db) {
    try { for (const entry of await readSaved()) {
      const type=loadModel(entry.bytes,0);
      if (type) models.set(entry.id,{...entry,type});
    } }
    catch { db = null; }
  }
  refreshModels();
  bind();
  sync();
  $('app').hidden = false;
  if (document.activeElement === document.body) $('stage').focus({preventScroll:true});
  message(db ? '' : 'O armazenamento local não está disponível. Baixe as redes que quiser guardar.');
  requestAnimationFrame(tick);
}

boot().catch(error => {
  message(`Não foi possível iniciar: ${error.message}. Sirva a pasta dist por HTTP.`);
  set('mode-label', 'Falha ao iniciar');
  console.error(error);
});
