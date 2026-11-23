"""Stage only the resources referenced by the current cube-preview runtime.

The full source library remains in assets/. No recursive deletion is performed.
"""
from pathlib import Path
import shutil
import sys

def required_files(source):
    files = {'engine/blocks.tsv', 'engine/player_sounds.tsv',
             'textures/gui/sprites/hud/crosshair.png',
             'textures/block/grass_block_side_overlay.png'}
    files.update(f'textures/block/destroy_stage_{i}.png' for i in range(10))
    rows = (source / 'engine/blocks.tsv').read_text().splitlines()
    if rows[0] != 'engine-blocks-v3':
        raise ValueError('Unsupported block manifest')
    for row in rows[1:]:
        if not row:
            continue
        fields = row.split('\t')
        if len(fields) != 19:
            raise ValueError('Invalid block manifest row')
        files.update(f'textures/{name}.png' for name in fields[1:7])
        for column in (13, 14, 15, 16, 18):
            files.update(name for name in fields[column].split(',') if name)
    for row in (source / 'engine/player_sounds.tsv').read_text().splitlines()[1:]:
        if row:
            files.update(row.split('\t', 1)[1].split(','))
    return files

def stage(source, destination):
    source, destination = source.resolve(), destination.resolve()
    copied = 0
    files = required_files(source)
    for relative in sorted(files):
        original, target = (source / relative).resolve(), (destination / relative).resolve()
        if not original.is_relative_to(source) or not target.is_relative_to(destination):
            raise ValueError(f'Asset path escapes root: {relative}')
        if not original.is_file():
            raise FileNotFoundError(original)
        if target.exists():
            a, b = original.stat(), target.stat()
            if a.st_size == b.st_size and a.st_mtime_ns == b.st_mtime_ns:
                continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
        copied += 1
    print(f'Runtime assets: {len(files)} referenced, {copied} copied.')

if __name__ == '__main__':
    stage(Path(sys.argv[1]), Path(sys.argv[2]))
