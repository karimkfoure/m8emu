"""Collect isolated GCC training runs and merge them in a fixed order."""
from concurrent.futures import ThreadPoolExecutor, as_completed
import os
from pathlib import Path
import signal
import subprocess
import threading
import time


def profile_environment(embedded_prefix, case_directory):
    prefix = Path(embedded_prefix)
    if not prefix.is_absolute():
        raise ValueError('The embedded GCC profile prefix must be absolute: ' + str(prefix))
    return {'GCOV_PREFIX': str(Path(case_directory).resolve()),
            'GCOV_PREFIX_STRIP': str(len(prefix.parts) - 1)}


class CaseFailure(RuntimeError):
    def __init__(self, record, job):
        self.record = record
        detail = ''.join(Path(job[key]).read_text(errors='replace')[-1200:]
                         for key in ('stdout', 'stderr'))
        reason = 'deadline exceeded' if record['timed_out'] else f'exit {record["returncode"]}'
        super().__init__(f'Training case {record["name"]}: {reason}: {detail}')


def run_cases(jobs, workers=1, timeout=3600, completed=None):
    """Run unique jobs; only the controller calls completed(index, record)."""
    if workers < 1 or timeout < 1:
        raise ValueError('Training workers and timeout must be positive')
    if len({job['name'] for job in jobs}) != len(jobs):
        raise ValueError('Training case names must be unique')
    outputs = [Path(job[key]).resolve() for job in jobs for key in ('stdout', 'stderr')]
    if len(set(outputs)) != len(outputs) or len({Path(job['directory']).resolve() for job in jobs}) != len(jobs):
        raise ValueError('Training case directories and output files must be unique')
    processes, lock, stopping = set(), threading.Lock(), False

    def execute(job):
        directory = Path(job['directory'])
        directory.mkdir(parents=True, exist_ok=True)
        for key in ('stdout', 'stderr'):
            Path(job[key]).parent.mkdir(parents=True, exist_ok=True)
        command = list(map(str, job['command']))
        record = dict(name=job['name'], command=command, cwd=str(directory), deadline=timeout,
                      timed_out=False)
        started = time.monotonic()
        process = None
        try:
            with Path(job['stdout']).open('wb') as out, Path(job['stderr']).open('wb') as err:
                with lock:
                    if stopping:
                        raise RuntimeError('Training was cancelled before starting ' + job['name'])
                    process = subprocess.Popen(command, cwd=directory, env=job['environment'],
                                               stdout=out, stderr=err, start_new_session=True)
                    processes.add(process)
                try:
                    record['returncode'] = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    record['timed_out'] = True
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    record['returncode'] = process.wait(timeout=5)
            record['seconds'] = time.monotonic() - started
            return record
        finally:
            if process is not None:
                with lock:
                    processes.discard(process)

    executor = ThreadPoolExecutor(max_workers=workers)
    futures = {executor.submit(execute, job): index for index, job in enumerate(jobs)}
    records = [None] * len(jobs)
    try:
        for future in as_completed(futures):
            index = futures[future]
            record = future.result()
            records[index] = record
            if completed:
                completed(index, record)
            if record['returncode'] or record['timed_out']:
                raise CaseFailure(record, jobs[index])
        return records
    except BaseException:
        with lock:
            stopping = True
            for process in processes:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        for future in futures:
            future.cancel()
        raise
    finally:
        executor.shutdown(wait=True, cancel_futures=True)


def merge_profiles(directories, tool, scratch, timeout=60):
    """Fold validated cases with equal weights in caller-supplied order."""
    directories = [Path(path).resolve() for path in directories]
    if not directories or len(set(directories)) != len(directories):
        raise ValueError('Profile merge needs a nonempty unique ordered case list')
    if any(not path.is_dir() or not any(path.rglob('*.gcda')) for path in directories):
        raise ValueError('Profile merge input is missing GCC data')
    scratch = Path(scratch)
    scratch.mkdir(parents=True, exist_ok=True)
    merged = directories[0]
    with (scratch / 'merge.log').open('wb') as log:
        for index, right in enumerate(directories[1:], 1):
            destination = scratch / f'merged-{index:04d}'
            if destination.exists():
                raise ValueError('Profile merge output already exists: ' + str(destination))
            subprocess.run([str(tool), 'merge', str(merged), str(right), '-o', str(destination)],
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=timeout)
            merged = destination
    return merged
