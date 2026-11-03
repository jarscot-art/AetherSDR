"""Read documented AMD64 CONTEXT records from preserved Windows minidumps.

No execution, modification, disassembly or dependency on a debugger package.
The x64 fields follow the Windows SDK CONTEXT definition (winnt.h).
"""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parent
results = []
for path in sorted((ROOT / 'artifact-original/feature-original').glob('*/capture/process.dmp')):
    data = path.read_bytes()
    signature, version, count, directory = struct.unpack_from('<4I', data)
    assert signature == 0x504D444D, 'Not a Windows minidump'
    streams = {}
    for index in range(count):
        kind, size, rva = struct.unpack_from('<III', data, directory + 12 * index)
        assert rva + size <= len(data)
        streams[kind] = (size, rva)
    architecture = struct.unpack_from('<H', data, streams[7][1])[0]
    assert architecture == 9, 'AMD64 contexts required'
    thread_list = streams[3][1]
    thread_count = struct.unpack_from('<I', data, thread_list)[0]
    threads = []
    for index in range(thread_count):
        position = thread_list + 4 + 48 * index
        thread_id = struct.unpack_from('<I', data, position)[0]
        context_size, context_rva = struct.unpack_from('<II', data, position + 40)
        assert context_size >= 256 and context_rva + context_size <= len(data)
        registers = {name: struct.unpack_from('<Q', data, context_rva + offset)[0]
                     for name, offset in {'rcx': 128, 'rdx': 136, 'r8': 184,
                                          'r9': 192, 'r10': 200, 'rip': 248}.items()}
        assert registers['rcx'] == 0xC0000135 and registers['r10'] == 0xC0000135
        threads.append({'threadId': thread_id,
                        'registers': {name: hex(value) for name, value in registers.items()},
                        'status': '0xC0000135', 'statusName': 'STATUS_DLL_NOT_FOUND'})
    results.append({'test': path.parent.parent.name,
                    'dump': path.relative_to(ROOT).as_posix(),
                    'dumpBytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                    'architecture': 'AMD64', 'threadCount': thread_count, 'threads': threads})
assert len(results) == 4
(ROOT / 'minidump-register-analysis.json').write_text(json.dumps(results, indent=2) + '\n')
for row in results:
    print(f"{row['test']}: {row['threadCount']} threads, NtRaiseHardError argument "
          "RCX=R10=0xC0000135 (STATUS_DLL_NOT_FOUND)")
