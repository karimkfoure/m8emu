#!/usr/bin/env python3
"""Isolate installed GCC from newer cross libc headers and startup files."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

def run(command):
    return subprocess.check_output(list(map(str,command)),text=True,stderr=subprocess.STDOUT,timeout=30)

def replace(path,data):
    # Each configure prepares the compiler again. Keep unchanged inputs and
    # their mtimes so Ninja does not relink frozen PGO stages.
    if path.is_file() and path.read_bytes()==data:return
    temporary=path.with_name(path.name+'.tmp');temporary.write_bytes(data);temporary.replace(path)

def prepare(compiler,sysroot,output):
    compiler=compiler.resolve();sysroot=sysroot.resolve();output=output.resolve()
    version=run([compiler,'--version']).splitlines()[0]
    if not run([compiler,'-dumpmachine']).strip().startswith('aarch64'):
        raise ValueError(f'Compiler target must be aarch64: compiler={compiler}')
    header=sysroot/'usr/include/features.h'
    major=re.search(r'^#define\s+__GLIBC__\s+(\d+)',header.read_text(),re.M)
    minor=re.search(r'^#define\s+__GLIBC_MINOR__\s+(\d+)',header.read_text(),re.M)
    if not major or not minor or (int(major[1]),int(minor[1]))>(2,28):
        raise ValueError(f'Sysroot glibc must be <=2.28: path={header}; actual={major[1] if major else None}.{minor[1] if minor else None}')
    for name in ('crt1.o','Scrt1.o','crti.o','crtn.o','libc.so','libpthread.so'):
        path=sysroot/'usr/lib/aarch64-linux-gnu'/name
        if not path.is_file():raise ValueError(f'Sysroot development input missing: path={path}')
    include=Path(run([compiler,'-print-file-name=include']).strip())
    output.mkdir(parents=True,exist_ok=True)
    gcov=Path(run([compiler,'-print-file-name=libgcov.a']).strip())
    gcov_imports=run(['aarch64-linux-gnu-nm','-A','-u',gcov])
    gcov_repair=None
    if '__isoc23_' in gcov_imports:
        sites=run(['aarch64-linux-gnu-objdump','-dr',gcov])
        calls=re.findall(r'((?:[^\n]*\n){0,4})[^\n]*bl\s+[^\n]*<__isoc23_strtol>\n[^\n]*R_AARCH64_CALL26\s+__isoc23_strtol',sites)
        if len(calls)!=1 or not re.search(r'mov\s+w2,\s*#0xa\s',calls[0]) or set(re.findall(r'U\s+(__isoc23_\w+)',gcov_imports))!={'__isoc23_strtol'}:
            raise ValueError(f'Unqualified libgcov C23 import: archive={gcov}; call_count={len(calls)}; only one base10 strtol call is supported')
        # GCC parses GCOV_PREFIX_STRIP in base10. C23 changes binary-prefix
        # recognition for bases0/2 only. Keep all profiling instructions and
        # schemas, and rename this single undefined import in a private copy.
        patched=output/'libgcov.a'
        staged=output/'libgcov.a.objcopy'
        subprocess.run(['aarch64-linux-gnu-objcopy','--redefine-sym','__isoc23_strtol=strtol',str(gcov),str(staged)],check=True,timeout=30)
        replace(patched,staged.read_bytes());staged.unlink()
        gcov_repair={'original_sha256':hashlib.sha256(gcov.read_bytes()).hexdigest(),
                     'patched_sha256':hashlib.sha256(patched.read_bytes()).hexdigest(),
                     'renamed_import':'__isoc23_strtol=strtol','qualified_base':10}
    flags=['--sysroot='+str(sysroot),'-nostdinc','-isystem',str(include),
           '-isystem',str(sysroot/'usr/include/aarch64-linux-gnu'),'-isystem',str(sysroot/'usr/include'),
           '-B'+str(output)+'/',
           '-B'+str(sysroot/'usr/lib/aarch64-linux-gnu')+'/',
           '-L'+str(sysroot/'usr/lib/aarch64-linux-gnu'),'-L'+str(sysroot/'lib/aarch64-linux-gnu'),
           '-static-libgcc']
    wrapper=output/'aarch64-linux-gnu-gcc'
    replace(wrapper,('#!/bin/sh\nexec '+shlex.join([str(compiler),*flags])+' "$@"\n').encode());wrapper.chmod(0o755)
    for name in ('gcov','gcov-dump','gcov-tool','readelf'):
        stem=re.fullmatch(r'(.*)gcc(-\d+)?',compiler.name)
        if not stem:raise ValueError(f'Unsupported GCC executable name: compiler={compiler}')
        target=compiler.with_name(stem[1]+name+(stem[2] or '') if name!='readelf' else stem[1]+name)
        if not target.is_file():raise ValueError(f'Matching GCC tool missing: path={target}')
        path=output/('aarch64-linux-gnu-'+name)
        if path.is_symlink():path.unlink()
        elif path.exists():raise ValueError(f'Compiler wrapper output exists: path={path}')
        path.symlink_to(target)
    actual=run([wrapper,'-print-file-name=Scrt1.o']).strip()
    expected=sysroot/'usr/lib/aarch64-linux-gnu/Scrt1.o'
    if Path(actual).resolve()!=expected.resolve():
        raise ValueError(f'Startup object leak: actual={actual}; expected={expected}')
    report={'schema':'m8-arm64-sysroot-compiler/1','complete':True,'compiler':str(compiler),
            'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'version':version,
            'sysroot':str(sysroot),'glibc':major[1]+'.'+minor[1],'flags':flags,'wrapper':str(wrapper),
            'wrapper_sha256':hashlib.sha256(wrapper.read_bytes()).hexdigest(),
            'startup_objects':{name:run([wrapper,'-print-file-name='+name]).strip() for name in ('crt1.o','Scrt1.o','crti.o','crtn.o')},
            'gcc_shared_runtime_used':False,'gcov_base10_import_repair':gcov_repair}
    (output/'sysroot-compiler.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--sysroot',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:print(json.dumps(prepare(args.compiler,args.sysroot,args.output),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as error:parser.exit(1,f'ARM64 compiler isolation: {error}\n')

if __name__=='__main__':main()
