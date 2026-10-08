"""Profile guest basic blocks during m8_demo playback via a C UC_HOOK_BLOCK callback."""
import ctypes, sys, time
from pathlib import Path
import unicorn
from unicorn import Uc
from unicorn.unicorn_py3.unicorn import uclib

S = Path(__file__).parent
ROOT = S.resolve().parents[1]
sys.path.insert(0, str(ROOT / 'reference'))
sys.path.insert(0, str(ROOT / 'tools'))
bp = ctypes.CDLL(str(S / 'blockprof.so'))
visit = ctypes.cast(bp.bp_visit, ctypes.c_void_p)

import m8_demo
orig_buffered = m8_demo.Demo.buffered_run
def buffered_run(self):
    uc = self.machine.uc
    handle = ctypes.c_size_t()
    uc.ctl_flush_tb()
    err = uclib.uc_hook_add(uc._uch, ctypes.byref(handle), unicorn.UC_HOOK_BLOCK, visit, None,
                            ctypes.c_uint64(1), ctypes.c_uint64(0))
    assert err == 0, err
    f0 = len(self.machine.pcm) // 4; it0 = self.machine.main_iters; cyc0 = self.machine.mmio.cyccnt
    bp.bp_enable(1); t0 = time.monotonic()
    try:
        return orig_buffered(self)
    finally:
        bp.bp_enable(0)
        frames = len(self.machine.pcm) // 4 - f0
        print(f"PROFILE audio={frames/44100:.3f}s wall={time.monotonic()-t0:.1f}s main_iters={self.machine.main_iters-it0} "
              f"cyc_delta={(self.machine.mmio.cyccnt-cyc0)&0xffffffff}", flush=True)
        bp.bp_dump(str(S / f'blocks-{self.project}.txt').encode())
m8_demo.Demo.buffered_run = buffered_run
sys.argv = ['m8_demo.py'] + sys.argv[1:]
m8_demo.main()
