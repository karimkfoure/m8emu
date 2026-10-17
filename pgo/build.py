#!/usr/bin/env python3
"""Build, train and qualify the generated libraries in one CMake build."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from training import llvm_environment, merge_llvm_profiles, merge_profiles, profile_environment, run_cases

COMPONENTS = {'m8aot': 'aot', 'm8native-regions': 'regions', 'm8native-step': 'handlers',
              'm8rt': 'runtime', 'unicorn': 'unicorn', 'unicorn-common': 'unicorn',
              'arm-softmmu': 'unicorn'}
# One exported, instrumented function identifies each LLVM profile image.
LLVM_ANCHORS = {'m8rt_create_aot': 'runtime', 'uc_open': 'unicorn', 'm8_aot_entries': 'aot',
                'm8_native_region_step': 'regions', 'm8_session_hot_m8_native_region_step': 'regions',
                'm8_native_step': 'handlers'}
EXACT = ('pcm_bytes', 'pcm_sha256', 'end_iters', 'end_cycles', 'end_pc', 'non_silent')
SOURCE_SUFFIXES = {'.c', '.h', '.inc', '.s', '.py', '.cmake', '.txt', '.json', '.in', '.patch'}


def sha(path):
    with Path(path).open('rb') as source:
        digest = hashlib.sha256()
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
        return digest.hexdigest()


def atomic(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def cache(path):
    rows = {}
    for line in path.read_text().splitlines():
        match = re.match(r'([^:#]+):[^=]+=(.*)', line)
        if match:
            rows[match[1]] = match[2]
    return rows


def source_identity(root):
    digest = hashlib.sha256()
    paths = [root / 'CMakeLists.txt', root / 'CMakePresets.json']
    for directory in ('core', 'aot', 'hosts', 'cmake', 'pgo'):
        paths.extend(path for path in (root / directory).rglob('*') if path.is_file()
                     and '__pycache__' not in path.parts and path.suffix.lower() in SOURCE_SUFFIXES)
    for path in sorted(paths):
        digest.update(str(path.relative_to(root)).encode() + b'\0' + bytes.fromhex(sha(path)))
    for name in ('unicorn', 'm8c', 'M8HeadlessFirmware'):
        pin = subprocess.check_output(['git', '-C', str(root / 'third_party' / name),
                                       'rev-parse', 'HEAD'], text=True, timeout=10).strip()
        digest.update(name.encode() + b'\0' + pin.encode())
    return digest.hexdigest()


def gcov_header(text):
    version = re.search(r"version `([^']+)'", text)
    stamp = re.search(r':stamp (\d+)', text)
    functions = {int(ident): (int(line, 16), int(cfg, 16)) for ident, line, cfg in
                 re.findall(r'FUNCTION ident=(\d+), lineno_checksum=(0x[0-9a-f]+), cfg_checksum=(0x[0-9a-f]+)', text)}
    if not version or not stamp:
        raise ValueError('Profile header has no GCC version or object stamp')
    return version[1], int(stamp[1]), functions


def validate_profiles(objects, profiles, dump):
    """Match all loaded library objects by stamp and function checksums."""
    notes = []
    for target in COMPONENTS:
        found = sorted(path for directory in objects.rglob(target + '.dir')
                       if directory.parent.name == 'CMakeFiles' for path in directory.rglob('*.gcno'))
        if not found:
            raise ValueError('No profile notes for library: ' + target)
        for path in found:
            text = subprocess.check_output([str(dump), '-l', str(path)], text=True, timeout=60)
            notes.append((target, path, gcov_header(text)))
    data = sorted(profiles.rglob('*.gcda'))
    if not data:
        raise ValueError('Training wrote no GCC profile data')
    matched = set()
    rows = []
    for path in data:
        text = subprocess.check_output([str(dump), '-l', str(path)], text=True, timeout=60)
        version, stamp, functions = gcov_header(text)
        candidates = [(i, target, note) for i, (target, note, header) in enumerate(notes)
                      if (path.name.endswith(str(note.relative_to(objects).with_suffix('.gcda')).replace('/', '#'))
                          or path.as_posix().endswith('/' + note.relative_to(objects).with_suffix('.gcda').as_posix()))
                      and header[:2] == (version, stamp)
                      and all(header[2].get(ident) == checksums for ident, checksums in functions.items())]
        if len(candidates) != 1:
            raise ValueError(f'Profile object has {len(candidates)} matching notes: {path}')
        index, target, note = candidates[0]
        if index in matched:
            raise ValueError('Duplicate profile data for object: ' + str(note))
        matched.add(index)
        rows.append(dict(component=COMPONENTS[target], note=str(note.relative_to(objects)),
                         note_sha256=sha(note), data=str(path.relative_to(profiles)),
                         data_sha256=sha(path), stamp=stamp, functions=len(functions)))
    missing = [str(note.relative_to(objects)) for i, (_, note, _) in enumerate(notes) if i not in matched]
    if missing:
        raise ValueError('Training missed profile objects: ' + ', '.join(missing))
    return rows


def validate_llvm_profiles(profiles, tool):
    """Each instrumented image contributes exactly one raw or merged profile."""
    rows, seen = [], set()
    for path in sorted([*profiles.rglob('*.profraw'), *profiles.rglob('*.profdata')]):
        text = subprocess.check_output([str(tool), 'show', '--all-functions', str(path)], text=True, timeout=300)
        names = set(re.findall(r'^  (\S.*):$', text, re.M))
        components = sorted({LLVM_ANCHORS[name] for name in names & LLVM_ANCHORS.keys()})
        if not components or seen & set(components):
            raise ValueError(f'Profile has no unique instrumented image: {path}: {components}')
        seen.update(components)
        rows.append(dict(component=','.join(components), data=str(path.relative_to(profiles)),
                         data_sha256=sha(path), functions=len(names)))
    missing = sorted(set(LLVM_ANCHORS.values()) - seen)
    if missing:
        raise ValueError('Training missed profile images: ' + ', '.join(missing))
    return rows


def llvm_profdata(targets):
    """Raw profiles are bound to one LLVM release; use its own merge tool."""
    compiler = Path(shutil.which(targets['compiler']) or targets['compiler']).resolve()
    if sys.platform == 'darwin':
        found = subprocess.check_output(['xcrun', '--find', 'llvm-profdata'], text=True, timeout=30).strip()
    else:
        sibling = compiler.with_name('llvm-profdata')
        found = str(sibling) if sibling.is_file() else shutil.which(
            'llvm-profdata-' + targets['compiler_version'].split('.')[0]) or shutil.which('llvm-profdata')
    if not found:
        raise ValueError('llvm-profdata for the profile compiler is missing')
    version = subprocess.check_output([found, '--version'], text=True, timeout=10)
    release = '.'.join(targets['compiler_version'].split('.')[:3])
    if not re.search(r'LLVM version ' + re.escape(release) + r'\b', version):
        raise ValueError('llvm-profdata version differs from the compiler: ' + found)
    return Path(found)


def tool_for(compiler, name):
    path = Path(compiler)
    match = re.fullmatch(r'(.*)gcc(-\d+)?', path.name)
    if not match:
        raise ValueError('Profile compiler must be a GCC executable or GCC wrapper: ' + compiler)
    candidate = path.with_name(match[1] + name + (match[2] or ''))
    if candidate.is_file():
        return candidate
    found = shutil.which(candidate.name)
    if not found:
        raise ValueError('Matching GCC tool is missing: ' + candidate.name)
    return Path(found)


class Build:
    def __init__(self, args):
        self.args = args
        self.root, self.output = args.source.resolve(), args.build.resolve()
        self.work = self.output / 'pgo'
        self.objects, self.profiles = self.work / 'objects', self.work / 'profiles'
        self.report_path = self.work / 'result.json'
        outer_cache = cache(self.output / 'CMakeCache.txt')
        compiler_name = outer_cache['CMAKE_C_COMPILER']
        compiler = Path(shutil.which(compiler_name) or compiler_name).resolve()
        version = subprocess.check_output([str(compiler), '--version'], text=True, timeout=10)
        self.identity = dict(source_sha256=source_identity(self.root), cache_sha256=sha(args.cache),
                             compiler_sha256=sha(compiler), compiler_version=version)
        self.report = dict(schema='m8emu-build-pgo/1', complete=False, inputs=self.identity,
                           first_failure=None, commands=[], training=[], verification=[])
        self.start = time.monotonic()

    def save(self):
        self.report['seconds'] = time.monotonic() - self.start
        atomic(self.report_path, self.report)

    def run(self, argv, stem, cwd=None, env=None):
        command = list(map(str, argv))
        directory = cwd or self.work
        directory.mkdir(parents=True, exist_ok=True)
        row = dict(command=command, cwd=str(directory), deadline=self.args.timeout)
        self.report['commands'].append(row)
        self.save()
        started = time.monotonic()
        with (self.work / (stem + '.stdout')).open('wb') as out, (self.work / (stem + '.stderr')).open('wb') as err:
            result = subprocess.run(command, cwd=directory, env=env, stdout=out, stderr=err,
                                    timeout=self.args.timeout)
        row.update(returncode=result.returncode, seconds=time.monotonic() - started)
        self.save()
        if result.returncode:
            detail = ((self.work / (stem + '.stdout')).read_text(errors='replace')[-1200:] +
                      (self.work / (stem + '.stderr')).read_text(errors='replace')[-1200:])
            raise RuntimeError(f'{stem} failed ({result.returncode}): {detail}')
        print(f'{stem}: {row["seconds"]:.1f} s', flush=True)
        return row

    def configure(self, phase):
        self.run([self.args.cmake, '-S', self.root, '-B', self.objects, '-G', self.args.generator,
                  '-C', self.args.cache, '-DM8_PGO=OFF', '-DM8_PGO_PHASE=' + phase,
                  '-DM8_PGO_PROFILE_DIR=' + str(self.profiles),
                  '-DM8_RT_PGO=' + ('generate' if phase == 'GENERATE' else 'use'),
                  '-DM8_UNICORN_PGO=' + ('generate' if phase == 'GENERATE' else 'use'),
                  '-DM8_PGO_DIR=' + str(self.profiles), '-DBUILD_TESTING=OFF',
                  '-DM8_BUILD_BENCH=OFF'], 'configure-' + phase.lower())

    def build(self, phase):
        self.run([self.args.cmake, '--build', self.objects, '--parallel', self.args.jobs],
                 'build-' + phase.lower())

    def capture(self, phase, targets, rows):
        emulator = targets['emulator'].split(';') if targets.get('cross') == 'TRUE' else []
        if targets.get('cross') == 'TRUE' and (not emulator or not emulator[0]):
            raise ValueError('Cross PGO needs CMAKE_CROSSCOMPILING_EMULATOR and the old-glibc loader')
        captures, commands, proofs, jobs = {}, {}, {}, []
        command_start = len(self.report['commands'])
        key = 'training' if phase == 'generate' else 'verification'
        if len({row['name'] for row in rows}) != len(rows):
            raise ValueError('Synthetic fixture names must be unique')
        case_profiles, merge_scratch = self.work / 'case-profiles', self.work / 'merge-profiles'
        if phase == 'generate':
            for path in (case_profiles, merge_scratch):
                if path.exists():
                    shutil.rmtree(path)
            if self.llvm:
                dump = merge = llvm_profdata(targets)
            else:
                dump, merge = tool_for(targets['compiler'], 'gcov-dump'), tool_for(targets['compiler'], 'gcov-tool')
                for tool in (dump, merge):
                    version = subprocess.check_output([tool, '--version'], text=True, timeout=10)
                    if not re.search(r'\b' + re.escape(targets['compiler_version']) + r'\b', version):
                        raise ValueError('GCC profile tool version differs from the compiler: ' + str(tool))
        for fixture in rows:
            name = fixture['name']
            if not re.fullmatch(r'[A-Za-z0-9_-]+', name):
                raise ValueError('Unsafe synthetic fixture name: ' + name)
            directory = self.work / phase / name
            directory.mkdir(parents=True, exist_ok=True)
            image = (self.work / 'training' / fixture['image']).resolve()
            if not image.is_relative_to((self.work / 'training').resolve()) or sha(image) != fixture['image_sha256']:
                raise ValueError('Synthetic card path or SHA differs: ' + name)
            env = os.environ.copy()
            # The runner explicitly selects all model controls; inherited cold
            # owner settings must not select a different experiment.
            for variable in tuple(env):
                if variable.startswith('M8RT_') or variable in ('GCOV_PREFIX', 'GCOV_PREFIX_STRIP', 'LLVM_PROFILE_FILE'):
                    env.pop(variable)
            directories = list(dict.fromkeys(str(Path(targets[key]).parent) for key in
                               ('runtime', 'unicorn', 'aot', 'regions', 'handlers', 'sdl')))
            env['DYLD_LIBRARY_PATH' if sys.platform == 'darwin' else 'LD_LIBRARY_PATH'] = ':'.join(directories)
            if phase == 'generate':
                env.update(llvm_environment(case_profiles / name) if self.llvm else
                           profile_environment(self.profiles, case_profiles / name))
            jobs.append(dict(name=name, directory=directory, environment=env,
                             stdout=self.work / (phase + '-' + name + '.stdout'),
                             stderr=self.work / (phase + '-' + name + '.stderr'),
                             command=[*emulator, targets['runner'], targets['firmware'], image,
                                      targets['aot'], targets['regions'], targets['handlers'],
                                      str(fixture.get('seconds', 2)), directory / 'pcm.bin']))

        def completed(index, command):
            fixture, job = rows[index], jobs[index]
            name, directory = fixture['name'], Path(job['directory'])
            commands[index] = command
            self.report['commands'] = self.report['commands'][:command_start] + [commands[i] for i in sorted(commands)]
            self.save()
            if command['returncode'] or command['timed_out']:
                return
            try:
                result = json.loads(Path(job['stdout']).read_text())
            except ValueError as error:
                raise ValueError(f'Training report malformed: fixture={name}: {error}') from error
            if result.get('checks_passed') is not True or not result.get('native_enters'):
                raise ValueError('Native training contract failed: ' + name)
            pcm = directory / 'pcm.bin'
            if pcm.stat().st_size != result['pcm_bytes'] or sha(pcm) != result['pcm_sha256']:
                raise ValueError('Complete PCM output differs from runner report: ' + name)
            if fixture.get('non_silent') and not result['non_silent']:
                raise ValueError('Synthetic audible fixture produced silence: ' + name)
            if phase == 'generate':
                matched = (validate_llvm_profiles(case_profiles / name, dump) if self.llvm else
                           validate_profiles(self.objects, case_profiles / name, dump))
                proofs[index] = dict(name=name, objects=len(matched), data_sha256=hashlib.sha256(
                    json.dumps(matched, sort_keys=True).encode()).hexdigest())
                self.report['training_profiles'] = [proofs[i] for i in sorted(proofs)]
            captures[index] = dict(name=name, result=result)
            self.report[key] = [captures[i] for i in sorted(captures)]
            self.save()
            print(f'{phase}-{name}: {command["seconds"]:.1f} s', flush=True)

        run_cases(jobs, workers=self.args.train_jobs, timeout=self.args.timeout, completed=completed)
        if phase == 'generate':
            if self.llvm:
                merged = merge_llvm_profiles([case_profiles / row['name'] for row in rows], merge, merge_scratch)
                validate_llvm_profiles(merged, dump)
            else:
                merged = merge_profiles([case_profiles / row['name'] for row in rows], merge, merge_scratch)
                validate_profiles(self.objects, merged, dump)
            if self.profiles.exists():
                if any(self.profiles.rglob('*.profraw' if self.llvm else '*.gcda')):
                    raise ValueError('Unexpected shared profile data outside the isolated training cases')
                shutil.rmtree(self.profiles)
            shutil.copytree(merged, self.profiles)
            self.report['profile_merge'] = dict(tool=str(merge), weights='1:1',
                                                order=[row['name'] for row in rows])
            self.save()
        return [captures[i] for i in range(len(rows))]

    def snapshot_generate(self, targets):
        """Retain exact GEN inputs for an independent training comparison."""
        destination = self.work / 'gen-snapshot'
        if destination.exists():
            shutil.rmtree(destination)
        copied = {}
        snapshot_targets = dict(targets)
        for key in ('runner', 'runtime', 'aot', 'regions', 'handlers', 'unicorn', 'sdl', 'firmware'):
            source = Path(targets[key])
            category = 'bin' if key == 'runner' else 'firmware' if key == 'firmware' else 'lib'
            target = destination / category / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            snapshot_targets[key] = str(target)
            copied[str(target.relative_to(destination))] = sha(target)
        for row in self.report['profile_objects']:
            for category, origin, field in (('notes', self.objects, 'note'), ('profiles', self.profiles, 'data')):
                if field not in row:
                    continue
                source = origin / row[field]
                target = destination / category / row[field]
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
                copied[str(target.relative_to(destination))] = sha(target)
        shutil.copy2(self.objects / 'CMakeCache.txt', destination / 'CMakeCache.txt')
        copied['CMakeCache.txt'] = sha(destination / 'CMakeCache.txt')
        atomic(destination / 'manifest.json', dict(inputs=self.identity, objects=str(self.objects),
               embedded_profile_prefix=str(self.profiles), targets=snapshot_targets,
               original_targets=targets, profile_objects=self.report['profile_objects'], files=copied))
        self.report['generate_snapshot_sha256'] = sha(destination / 'manifest.json')
        self.save()

    @staticmethod
    def dynamic(path, readelf):
        """Runtime search paths and the library's own load name (SONAME or install id)."""
        if sys.platform == 'darwin':
            text = subprocess.check_output([readelf, '-l', str(path)], text=True, timeout=30)
            identity = re.search(r'cmd LC_ID_DYLIB\n.*\n\s+name (.+) \(offset \d+\)', text)
            return (re.findall(r'cmd LC_RPATH\n.*\n\s+path (.+) \(offset \d+\)', text),
                    identity and identity[1])
        text = subprocess.check_output([readelf, '-d', str(path)], text=True, timeout=30)
        match = re.search(r'\((?:RUNPATH|RPATH)\).*\[([^\]]+)\]', text)
        soname = re.search(r'\(SONAME\).*\[([^\]]+)\]', text)
        return [match[1]] if match else [], soname and soname[1]

    @classmethod
    def portable_rpath(cls, path, desired, readelf, patch, allow_empty=False):
        paths = cls.dynamic(path, readelf)[0]
        if paths == [desired]:
            return
        if not paths and allow_empty:
            return
        if not patch:
            raise ValueError(f'Bundle path requires RPATH {desired}: {path}; configure BUILD_WITH_INSTALL_RPATH or install patchelf')
        if sys.platform != 'darwin':
            subprocess.run([patch, '--set-rpath', desired, str(path)], check=True, timeout=30)
            return
        # Load-command edits invalidate the arm64 signature; sign ad hoc again.
        for old in paths:
            subprocess.run([patch, '-delete_rpath', old, str(path)], check=True, timeout=30)
        subprocess.run([patch, '-add_rpath', desired, str(path)], check=True, timeout=30)
        subprocess.run(['codesign', '--force', '--sign', '-', str(path)], check=True, timeout=30)

    def bundle(self, targets):
        macho = sys.platform == 'darwin'
        readelf = shutil.which('otool' if macho else 'readelf')
        patch = shutil.which('install_name_tool' if macho else 'patchelf')
        origin = '@loader_path' if macho else '$ORIGIN'
        if not readelf:
            raise ValueError('The portable bundle requires ' + ('otool' if macho else 'readelf'))
        libraries = self.output / 'lib'
        libraries.mkdir(exist_ok=True)
        keys = ['runtime', 'aot', 'regions', 'handlers', 'unicorn', 'sdl']
        if macho and not (self.dynamic(targets['sdl'], readelf)[1] or '').startswith('@rpath/'):
            # The app records an installed SDL3 by its absolute install name.
            keys.remove('sdl')
        paths = [Path(targets[key]) for key in keys]
        files = []
        for path in paths:
            destination = libraries / path.name
            shutil.copy2(path, destination)
            self.portable_rpath(destination, origin, readelf, patch,
                                allow_empty=path == Path(targets['sdl']))
            soname = self.dynamic(destination, readelf)[1]
            soname = soname and Path(soname).name
            if soname and soname != destination.name:
                alias = libraries / soname
                if alias.is_symlink():
                    alias.unlink()
                if alias.exists():
                    raise ValueError('Bundle SONAME collision: ' + str(alias))
                alias.symlink_to(destination.name)
            files.append(destination)
        app = self.output / 'm8emu'
        shutil.copy2(targets['app'], app)
        self.portable_rpath(app, origin + '/lib', readelf, patch)
        files.append(app)
        controllers = self.output / 'gamecontrollerdb.txt'
        shutil.copy2(Path(targets['app']).parent / controllers.name, controllers)
        files.append(controllers)
        firmware = self.output / 'firmware'
        firmware.mkdir(exist_ok=True)
        destination = firmware / Path(targets['firmware']).name
        shutil.copy2(targets['firmware'], destination)
        files.append(destination)
        if targets.get('cross') == 'TRUE':
            for path in files:
                if path.suffix in ('.bin', '.txt'):
                    continue
                versions = subprocess.check_output([readelf, '--version-info', str(path)], text=True, timeout=30)
                requirements = [tuple(map(int, version.split('.'))) for version in re.findall(r'\bGLIBC_([0-9.]+)', versions)]
                if requirements and max(requirements) > (2, 27):
                    raise ValueError(f'Bundle GLIBC ceiling exceeds 2.27: {path}: {max(requirements)}')
        return {str(path.relative_to(self.output)): sha(path) for path in files}

    def execute(self):
        if self.report_path.exists():
            previous = json.loads(self.report_path.read_text())
            if previous.get('complete') and previous.get('inputs') == self.identity and all(
                    (self.output / name).is_file() and sha(self.output / name) == digest
                    for name, digest in previous.get('outputs', {}).items()):
                print('PGO: reuse exact completed build', flush=True)
                return
        self.work.mkdir(parents=True, exist_ok=True)
        self.save()
        try:
            # A failed or changed build trains again. It cannot reuse partial
            # profiles or change their object stamps to make a merge succeed.
            if self.profiles.exists():
                shutil.rmtree(self.profiles)
            self.configure('GENERATE')
            self.build('GENERATE')
            targets = json.loads((self.objects / 'pgo-targets.json').read_text())
            self.llvm = targets.get('compiler_id', 'GNU') != 'GNU'
            values = cache(self.objects / 'CMakeCache.txt')
            command = [sys.executable, self.root / 'pgo/generate.py', '--output', self.work / 'training']
            if values.get('M8_PGO_FIXTURES'):
                command.extend(['--fixtures', *values['M8_PGO_FIXTURES'].split(';')])
            self.run(command, 'generate-projects')
            manifest = json.loads((self.work / 'training/manifest.json').read_text())
            rows = manifest['fixtures']
            for row in rows:
                row['seconds'] = float(values.get('M8_PGO_TRAINING_SECONDS', row.get('seconds', 2)))
            if not rows:
                raise ValueError('Synthetic training manifest is empty')
            self.report['training_manifest_sha256'] = sha(self.work / 'training/manifest.json')
            generated = self.capture('generate', targets, rows)
            if self.llvm:
                self.report['profile_objects'] = validate_llvm_profiles(self.profiles, llvm_profdata(targets))
            else:
                dump = tool_for(targets['compiler'], 'gcov-dump')
                version = subprocess.check_output([dump, '--version'], text=True, timeout=10)
                if not re.search(r'\b' + re.escape(targets['compiler_version']) + r'\b', version):
                    raise ValueError('gcov-dump version differs from the training compiler')
                self.report['profile_objects'] = validate_profiles(self.objects, self.profiles, dump)
            self.snapshot_generate(targets)
            # Configure the same object directory so GCC's profile names stay
            # identical. Missing-profile and coverage or LLVM hash mismatches stay fatal.
            self.configure('USE')
            self.build('USE')
            used = self.capture('use', targets, rows)
            for before, after in zip(generated, used):
                if before['name'] != after['name'] or any(before['result'][key] != after['result'][key] for key in EXACT):
                    raise ValueError('GEN/USE PCM or schedule differs: ' + before['name'])
            self.report['outputs'] = self.bundle(targets)
            self.report['complete'] = True
            self.save()
            # Captures are exact and hashed. Keep the compact reports, not
            # large synthetic cards and redundant complete PCM after success.
            for name in ('generate', 'use', 'training', 'case-profiles', 'merge-profiles'):
                shutil.rmtree(self.work / name)
            print(f'PGO complete: {len(rows)} exact synthetic projects; {self.report["seconds"]:.1f} s', flush=True)
        except BaseException as error:
            self.report['first_failure'] = dict(type=type(error).__name__, message=str(error))
            self.save()
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'cache'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--generator', default='Ninja')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--train-jobs', type=int, default=1)
    parser.add_argument('--timeout', type=int, default=3600)
    args = parser.parse_args()
    if args.jobs < 1 or args.train_jobs < 1 or args.timeout < 1:
        parser.error('Build jobs, training jobs and command timeout must be positive')
    try:
        Build(args).execute()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Build-time PGO: {error}\n')


if __name__ == '__main__':
    main()
