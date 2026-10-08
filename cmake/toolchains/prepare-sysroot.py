#!/usr/bin/env python3
"""Extract pinned ARM64 development packages without installing them.

This sysroot has glibc 2.28. The extracted graphics and ALSA packages supply build headers and link probes.
The portable bundle uses device graphics drivers.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess

SOURCE=Path(__file__).with_name('handheld-packages.json')

def sha(path):
    with path.open('rb') as source:
        digest=hashlib.sha256()
        for block in iter(lambda:source.read(1024*1024),b''):digest.update(block)
        return digest.hexdigest()

def fetch(row,base,downloads,offline):
    path=downloads/Path(row['filename']).name
    if not path.exists():
        if offline:raise ValueError(f'Offline package missing: path={path}')
        temporary=path.with_suffix(path.suffix+'.part')
        subprocess.run(['curl','--fail','--location','--retry','2','--max-time','120',
                        '--output',str(temporary),base+row['filename']],check=True,timeout=180,
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        temporary.replace(path)
    actual=sha(path)
    if actual!=row['sha256']:
        raise ValueError(f'Package SHA256 differs: path={path}; actual={actual}; expected={row["sha256"]}')
    return path

def prepare(output,offline=False):
    manifest=json.loads(SOURCE.read_text());output=output.resolve()
    downloads=output/'downloads';downloads.mkdir(parents=True,exist_ok=True)
    root=output/'root';stamp=output/'sysroot.json'
    expected=sha(SOURCE)
    if stamp.exists():
        report=json.loads(stamp.read_text())
        if report.get('manifest_sha256')!=expected:
            raise ValueError(f'Sysroot manifest changed: actual={expected}; expected={report.get("manifest_sha256")}; use a new output directory')
        for name,value in report['critical_files'].items():
            path=root/name;actual=sha(path) if path.is_file() else 'missing'
            if actual!=value:raise ValueError(f'Sysroot file differs: path={path}; actual={actual}; expected={value}')
        return report
    if root.exists():raise ValueError(f'Incomplete sysroot exists: path={root}; preserve it and use a new output directory')
    with ThreadPoolExecutor(max_workers=4) as pool:
        packages=list(pool.map(lambda row:fetch(row,manifest['base_url'],downloads,offline),manifest['packages']))
    root.mkdir()
    for package in packages:
        subprocess.run(['dpkg-deb','-x',str(package),str(root)],check=True,timeout=60)
    # Keep package absolute symlinks inside this private root, not the host.
    for path in root.rglob('*'):
        if path.is_symlink() and path.readlink().is_absolute():
            target=root/str(path.readlink()).lstrip('/')
            path.unlink();path.symlink_to(os.path.relpath(target,path.parent))
    critical=('lib/aarch64-linux-gnu/libc.so.6','lib/aarch64-linux-gnu/libpthread.so.0',
              'lib/aarch64-linux-gnu/ld-linux-aarch64.so.1',
              'usr/lib/aarch64-linux-gnu/crt1.o','usr/lib/aarch64-linux-gnu/Scrt1.o',
              'usr/lib/aarch64-linux-gnu/crti.o','usr/lib/aarch64-linux-gnu/crtn.o',
              'usr/include/features.h','usr/include/aarch64-linux-gnu/bits/libc-header-start.h')
    report={'schema':'m8-arm64-sysroot/1','complete':True,'root':str(root),
            'glibc':manifest['glibc'],'maximum_glibc':manifest['maximum_glibc'],
            'manifest_sha256':expected,'packages':manifest['packages'],
            'critical_files':{name:sha(root/name) for name in critical},
            'device_graphics_not_bundled':True}
    stamp.write_text(json.dumps(report,indent=2)+'\n');return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--offline',action='store_true')
    args=parser.parse_args()
    try:print(json.dumps(prepare(args.output,args.offline),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as error:parser.exit(1,f'ARM64 sysroot: {error}\n')

if __name__=='__main__':main()
