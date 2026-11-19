"""Build the static web tree without copying any Windows executables or DLLs."""
import json
from pathlib import Path
import re
import shutil
from string import Template

from PIL import Image

root = Path(__file__).resolve().parents[1]
dst = root / 'dist'
web = root / 'web'

# The order forms the sprite ABI used by wasm/render.c.
files = [*(f'dino{i}.bmp' for i in range(12)),
         *(f'cacto{i}.bmp' for i in range(7)), 'superCacto1.bmp', 'superCacto2.bmp',
         *(f'passaro{i}.bmp' for i in range(1, 5)),
         *(f'chao{i}.bmp' for i in range(6)), 'fundo0.bmp', 'fundo1.bmp', 'nuvem.bmp',
         'sol.png', 'lua.png', 'arvore.png', 'caverna.png', 'espinho.png', 'm4.png', 'projetil.png',
         *(f'moeda/{i}.png' for i in range(1, 13)), *(f'fumaca/fumaca{i}.png' for i in range(1, 10))]
files += ['arvore2.png', 'arvore3.png', 'arvore4.png', 'caverna2.png', 'caverna3.png', 'caverna4.png']


def copy_sprite(src, out):
    if src.suffix == '.bmp':
        with Image.open(src) as original:
            rgb = original.convert('RGBA')
            pixels = rgb.get_flattened_data() if hasattr(rgb, 'get_flattened_data') else rgb.getdata()
            rgb.putdata([(*p[:3], 0 if p[:3] == (255, 174, 201) else 255) for p in pixels])
            rgb.save(out)
    else:
        shutil.copyfile(src, out)


def manifest(directory, entries):
    (directory / 'manifest.json').write_text(json.dumps(entries) + '\n', encoding='utf-8')


def build_edition(package, page, birdless):
    source = package / 'arquivos'
    sprites = source / 'imagens/DINO_PLUS'
    assets, models = page / 'assets', page / 'models'
    assets.mkdir(parents=True, exist_ok=True)
    models.mkdir(parents=True, exist_ok=True)
    # IDs 68–71: literal texture paths in each edition's executable.
    cores = ('neuronio2', 'neuronio4') if birdless else ('neuronio20', 'neuroniobb3')
    names = files + [f'../desenharNeural/{n}.png' for n in (*cores, 'luz', 'entrada')] + ['chrome.png']
    ids = []
    for i, name in enumerate(names):
        src = sprites / name
        if birdless and not src.is_file():
            continue  # Keep gaps (projectile, smoke, extra trees); never shift the ABI.
        copy_sprite(src, assets / f'{i}.png')
        ids.append(i)
    manifest(assets, ids)
    shutil.copyfile(sprites / 'chrome.png', assets / 'icon.png')
    shutil.copyfile(package / 'fontes/arial.ttf', assets / 'arial.ttf')
    if not birdless:
        shutil.copyfile(source / 'redeDesviando', models / 'redeDesviando')
    scores = sorted((p for p in (source / 'redes/DINO').iterdir() if p.is_file() and p.name.isascii()
                     and p.name.isdecimal()), key=lambda p: -int(p.name))
    for item in scores:
        shutil.copyfile(item, models / item.name)
    manifest(models, [p.name for p in scores])

    html = (web / 'index.html').read_text(encoding='utf-8')
    html = re.sub(r'<!--birds-->(.*?)<!--/birds-->', '' if birdless else r'\1', html, flags=re.S)
    current = 'aria-current="page"'
    html = Template(html).substitute(shared='../' if birdless else './',
        edition='sem-passaros' if birdless else 'com-passaros',
         label='Sem pássaros' if birdless else 'Com pássaros',
         population=1000 if birdless else 500, parents=100 if birdless else 50,
         mutation='1.00' if birdless else '0.30',
         camera_scope='No treino e no teste' if birdless else 'No treino',
         birds_current='' if birdless else current, birdless_current=current if birdless else '',
        network='Rede neural' if birdless else 'Rede de mira')
    (page / 'index.html').write_text(html, encoding='utf-8')


build_edition(root, dst, birdless=False)
build_edition(root / 'Dino sem pássaros', dst / 'sem-passaros', birdless=True)
for name in ('main.js', 'style.css'):
    shutil.copyfile(web / name, dst / name)
