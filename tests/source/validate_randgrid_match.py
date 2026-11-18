"""Isolated x64 state-contract test for the actual assembled Randgrid match hook.

No game process is launched, opened, attached to, or modified. The only writes
are this validator's JSON report and Unicorn's private emulated memory.
Requires Python, capstone, unicorn, pefile. Re-run after each assembly change.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import platform
import struct
import sys
import time
from collections import Counter
from pathlib import Path

import capstone
import pefile
import unicorn
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn import UC_PROT_READ, UC_PROT_WRITE, UC_PROT_EXEC
from unicorn import x86_const as xc

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src' / 'randgrid_match.asm'
NATIVE_IMAGE = ROOT / 'reference' / 'live_image.bin'
MASK64 = (1 << 64) - 1
CODE = 0x10000000
STATE = 0x10010000
REFERENCE = 0x10020000
POSITIVE = 0x10030000
FAILURE = 0x10031000
BOOKKEEPING = 0x10032000
STACK = 0x70000000
STACK_SIZE = 0x10000
RSP = STACK + 0x1000
RBP = RSP + 0x100
FRAME_HANDLE = RBP + 0x438
TARGETS = {POSITIVE: 'positive', FAILURE: 'failure', BOOKKEEPING: 'bookkeeping'}
GPRS = ['RAX', 'RBX', 'RCX', 'RDX', 'RSI', 'RDI', 'RBP', 'RSP'] + [f'R{i}' for i in range(8, 16)]
GPR_IDS = {name: getattr(xc, 'UC_X86_REG_' + name) for name in GPRS}
YMM_IDS = [getattr(xc, f'UC_X86_REG_YMM{i}') for i in range(16)]
ARITH_BITS = (0, 2, 4, 6, 7, 11)
FLAGS_CASES = [0x202 | (df << 10) | sum(((bits >> i) & 1) << bit for i, bit in enumerate(ARITH_BITS))
               for df in (0, 1) for bits in range(64)]
# AF is architecturally undefined after TEST. All other returned flag bits must
# equal the native reference, including preserved DF and IF.
FLAGS_CONTRACT_MASK = 0xFFFFFFFF & ~(1 << 4)
NATIVE_RVA = 0x213C2DD9
NATIVE_TEMPLATE = bytes.fromhex(
    '85 D2 41 50 49 B8 00 00 00 00 00 00 00 00 FF 34 24 4C 89 44 24 08 '
    '41 58 52 53 48 8B 54 24 10 48 BB 00 00 00 00 00 00 00 00 '
    '48 0F 48 D3 48 89 54 24 10 5B 5A C3')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read_cstr(data, offset):
    return data[offset:data.index(b'\0', offset)].decode('ascii')


def load_coff(path):
    blob = path.read_bytes()
    machine, nsec, timestamp, symptr, nsym, opt, characteristics = struct.unpack_from('<HHIIIHH', blob)
    require(machine == 0x8664, f'Object is not AMD64: {machine:#x}')
    strings = blob[symptr + nsym * 18:]
    sections = {}
    for index in range(nsec):
        item = struct.unpack_from('<8sIIIIIIHHI', blob, 20 + opt + index * 40)
        name = item[0].split(b'\0')[0].decode('ascii')
        if name.startswith('/'):
            name = read_cstr(strings, int(name[1:]))
        sections[index + 1] = dict(name=name, size=item[3], raw=item[4], reloc=item[5], nreloc=item[7])
    symbols = {}
    index = 0
    while index < nsym:
        off = symptr + index * 18
        rawname, value, section, typ, storage, naux = struct.unpack_from('<8sIhHBB', blob, off)
        name = read_cstr(strings, struct.unpack_from('<I', rawname, 4)[0]) if rawname[:4] == b'\0' * 4 else rawname.split(b'\0')[0].decode('ascii')
        symbols[index] = dict(name=name, value=value, section=section, storage=storage, type=typ)
        index += 1 + naux
    funcs = [v for v in symbols.values() if v['name'] == 'RandgridMatchAdmission' and v['section'] > 0]
    require(len(funcs) == 1, 'Expected one defined RandgridMatchAdmission symbol')
    function = funcs[0]
    section = sections[function['section']]
    entry_offset = function['value']
    code = blob[section['raw'] + entry_offset:section['raw'] + section['size']]
    relocations = []
    for i in range(section['nreloc']):
        offset, symidx, typ = struct.unpack_from('<IIH', blob, section['reloc'] + i * 10)
        if offset < entry_offset:
            continue
        offset -= entry_offset
        symbol = symbols[symidx]
        require(4 <= typ <= 9, f'Unexpected code relocation type {typ}')
        require(symbol['name'] == 'g_randgrid_match' and symbol['section'] == 0,
                f'Unexpected external relocation {symbol}')
        addend = struct.unpack_from('<i', code, offset)[0]
        relocations.append(dict(offset=offset, type=typ, extra=typ - 4, symbol=symbol['name'], addend=addend))
    require(len(relocations) == 6, f'Expected six state references, got {len(relocations)}')
    return dict(path=str(path), sha256=sha(blob), byte_size=len(blob), section=section['name'],
                section_entry_offset=entry_offset, code=code, relocations=relocations)


def relocate(code, relocations, code_address, state_address):
    result = bytearray(code)
    for r in relocations:
        value = state_address + r['addend'] - (code_address + r['offset'] + 4 + r['extra'])
        require(-(1 << 31) <= value < (1 << 31), 'REL32 target out of range')
        struct.pack_into('<i', result, r['offset'], value)
    return bytes(result)


def inspect_linked_dll(path, obj, expected_sha256=None):
    blob = path.read_bytes()
    digest = sha(blob)
    if expected_sha256:
        require(digest == expected_sha256.lower(), 'Final DLL hash does not match parent-supplied artifact')
    pe = pefile.PE(data=blob, fast_load=True)
    require(pe.FILE_HEADER.Machine == 0x8664, 'DLL is not AMD64')
    code = obj['code']
    wildcard = set()
    for r in obj['relocations']:
        wildcard.update(range(r['offset'], r['offset'] + 4))
    invariant = [i for i in range(len(code)) if i not in wildcard]
    runs = []
    for _, group in itertools.groupby(enumerate(invariant), lambda item: item[1] - item[0]):
        indices = [item[1] for item in group]
        runs.append((indices[0], indices[-1] + 1))
    anchor_start, anchor_end = max(runs, key=lambda item: item[1] - item[0])
    anchor = code[anchor_start:anchor_end]
    matches = []
    states = []
    for section in pe.sections:
        data = section.get_data()
        if section.Characteristics & 0x20000000:
            cursor = 0
            while True:
                hit = data.find(anchor, cursor)
                if hit < 0:
                    break
                begin = hit - anchor_start
                if begin >= 0 and begin + len(code) <= len(data):
                    candidate = data[begin:begin + len(code)]
                    if all(candidate[i] == code[i] for i in invariant):
                        matches.append((section.VirtualAddress + begin, candidate))
                cursor = hit + 1
        if section.Characteristics & 0x80000000:
            cursor = 0
            while True:
                hit = data.find(b'RGMCH001', cursor)
                if hit < 0:
                    break
                states.append(section.VirtualAddress + hit)
                cursor = hit + 1
    require(len(matches) == 1, f'Expected unique linked function, found {len(matches)}')
    require(len(states) == 1, f'Expected unique writable RGMCH001, found {len(states)}')
    rva, linked_code = matches[0]
    state_rva = states[0]
    imagebase = pe.OPTIONAL_HEADER.ImageBase
    references = []
    for r in obj['relocations']:
        disp = struct.unpack_from('<i', linked_code, r['offset'])[0]
        target = imagebase + rva + r['offset'] + 4 + r['extra'] + disp
        resolved_state = target - r['addend']
        require(resolved_state == imagebase + state_rva, 'Linked relocation does not resolve to RGMCH001 state')
        references.append(dict(offset=r['offset'], state_field_offset=r['addend'], resolved_target=hex(target)))
    normalized = relocate(linked_code, obj['relocations'], CODE, STATE)
    require(normalized == relocate(code, obj['relocations'], CODE, STATE), 'Linked code differs after relocation normalization')
    return dict(path=str(path), sha256=digest, byte_size=len(blob), imagebase=hex(imagebase),
                function_rva=hex(rva), state_rva=hex(state_rva), invariant_byte_count=len(invariant),
                unique_function_match=True, linked_state_references=references,
                normalized_linked_code_equals_tested_object=True)


def load_native_reference(path):
    with path.open('rb') as f:
        f.seek(NATIVE_RVA)
        actual = f.read(53)
    excluded = set(range(6, 14)) | set(range(33, 41))
    require(len(actual) == 53, 'Native reference read is short')
    require(all(actual[i] == NATIVE_TEMPLATE[i] for i in range(53) if i not in excluded),
            'Captured native reference does not match the independently verified 53-byte bridge')
    oldpositive = struct.unpack_from('<Q', actual, 6)[0]
    oldfailure = struct.unpack_from('<Q', actual, 33)[0]
    oldimage = oldpositive - 0x15E8A23
    require(oldfailure - 0x34BC0CF == oldimage, 'Captured native bridge endpoints imply different image bases')
    relocated = bytearray(actual)
    struct.pack_into('<Q', relocated, 6, POSITIVE)
    struct.pack_into('<Q', relocated, 33, FAILURE)
    meta = dict(path=str(path), rva=hex(NATIVE_RVA), length=53, sha256=sha(actual),
                bytes=actual.hex(), original_imagebase=hex(oldimage),
                original_positive=hex(oldpositive), original_failure=hex(oldfailure),
                invariant_bytes_verified=True, isolated_reference_bytes=bytes(relocated).hex())
    return bytes(relocated), meta


def make_cases():
    cases = []
    def add(name, **kw):
        case = dict(name=name, enabled=1, status=0xC0000034, r12=0, handle=0,
                    counter=0x13579BDF02468ACE, invalid_rbp=False)
        case.update(kw)
        cases.append(case)
    add('eligible_missing_device')
    add('eligible_full_rdx_high32_preserved', status=0xDEADBEEFC0000034)
    for enabled in (0, 2, 0xFFFFFFFF):
        add(f'enabled_{enabled:08x}', enabled=enabled)
    for status in (0, 1, 0x40000000, 0x7FFFFFFF, 0x100000000, 0x80000000,
                   0xC0000005, 0xC0000022, 0xC000000F, 0xFFFFFFFF, 0xDEADBEEFC0000005):
        add(f'other_status_{status:016x}', status=status)
    for r12 in (1, 0x100000000, 0xFFFFFFFF00000000, 0x8000000000000000, MASK64):
        add(f'nonzero_full_r12_{r12:016x}', r12=r12)
    for handle in (1, 0x100000000, 0x8000000000000000, MASK64):
        add(f'nonzero_handle_{handle:016x}', handle=handle)
    for counter in (0, 0x7FFFFFFFFFFFFFFF, MASK64):
        add(f'admission_counter_{counter:016x}', counter=counter)
    add('disabled_short_circuits_invalid_frame', enabled=0, invalid_rbp=True)
    add('other_status_short_circuits_invalid_frame', status=0xC0000005, invalid_rbp=True)
    add('r12_high32_short_circuits_invalid_frame', r12=0x100000000, invalid_rbp=True)
    add('multiple_rejection_conditions', enabled=2, status=0xC0000005, r12=0x100000000, handle=MASK64)
    return cases


def is_admitted(case):
    return case['enabled'] == 1 and (case['status'] & 0xFFFFFFFF) == 0xC0000034 and case['r12'] == 0 and case['handle'] == 0


def initial_state(case):
    return struct.pack('<8sIIQQQQQII', b'RGMCH001', case['enabled'], 0x11223344,
                       case['counter'], 0x7FF700000000, POSITIVE, FAILURE,
                       BOOKKEEPING, 0xA55A5AA5, 0x7E31C2D4)


def context_for(case, index):
    regs = {name: ((0x9E3779B97F4A7C15 * (i + 1)) ^ (0xD1B54A32D192ED03 * (index + 1))) & MASK64
            for i, name in enumerate(GPRS)}
    regs.update(RSP=RSP, RBP=0x60000000 if case['invalid_rbp'] else RBP, R12=case['r12'], RDX=case['status'])
    regs['R13'] |= 0xE173000000000000
    regs['R14'] |= 0xB842000000000000
    vectors = [sum((((0xA5F0E19D72634B08 * (i + j + 1)) ^ (0x325796ABED0148CF * (index + 1))) & MASK64) << (64 * j)
                   for j in range(4)) for i in range(16)]
    stack = bytearray(bytes(((i * 67 + index * 13 + 19) & 255) for i in range(256)) * (STACK_SIZE // 256))
    struct.pack_into('<Q', stack, FRAME_HANDLE - STACK, case['handle'])
    # All exception masks remain set. Rounding and status bits vary, so a
    # gratuitous SIMD control-state change will also be detected.
    mxcsr = 0x1F80 | ((index & 3) << 13) | (index & 0x3F)
    return dict(gprs=regs, ymm=vectors, mxcsr=mxcsr, stack=bytes(stack))


class Engine:
    def __init__(self, hook, reference):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        for address in (CODE, REFERENCE, POSITIVE, FAILURE, BOOKKEEPING):
            self.uc.mem_map(address, 0x1000, UC_PROT_READ | UC_PROT_EXEC)
        self.uc.mem_map(STATE, 0x1000, UC_PROT_READ | UC_PROT_WRITE)
        self.uc.mem_map(STACK, STACK_SIZE, UC_PROT_READ | UC_PROT_WRITE)
        self.uc.mem_write(CODE, hook)
        self.uc.mem_write(REFERENCE, reference)
        self.uc.hook_add(UC_HOOK_CODE, self.on_code)
        self.coverage = set()
        self.runs = 0
        self.trace = []
        self.endpoint = None
        self.active_start = None
        self.state_guard = bytes((i * 17 + 3) & 255 for i in range(0x1000 - 64))
        # Verify that Unicorn actually accepts/returns the entire 256-bit
        # register before claiming the upper SIMD halves were checked.
        probe = int.from_bytes(bytes(range(32)), 'little')
        for reg in YMM_IDS:
            self.uc.reg_write(reg, probe)
            require(self.uc.reg_read(reg) == probe, 'Unicorn YMM round-trip failed')

    def on_code(self, uc, address, size, _):
        if address in TARGETS:
            self.endpoint = address
            uc.emu_stop()
            return
        self.trace.append(address)
        if self.active_start == CODE:
            self.coverage.add(address - CODE)

    def run(self, start, ctx, flags, state):
        self.uc.mem_write(STATE, state + self.state_guard)
        self.uc.mem_write(STACK, ctx['stack'])
        for name, value in ctx['gprs'].items():
            self.uc.reg_write(GPR_IDS[name], value)
        for reg, value in zip(YMM_IDS, ctx['ymm']):
            self.uc.reg_write(reg, value)
        self.uc.reg_write(xc.UC_X86_REG_MXCSR, ctx['mxcsr'])
        self.uc.reg_write(xc.UC_X86_REG_EFLAGS, flags)
        self.trace = []
        self.endpoint = None
        self.active_start = start
        try:
            self.uc.emu_start(start, 0, count=128)
        except UcError as exc:
            raise AssertionError(f'Unicorn fault at {self.uc.reg_read(xc.UC_X86_REG_RIP):#x}: {exc}; trace={list(map(hex, self.trace))}') from exc
        require(self.endpoint is not None, 'No expected continuation reached within instruction bound')
        self.runs += 1
        return dict(endpoint=self.endpoint, gprs={name: self.uc.reg_read(reg) for name, reg in GPR_IDS.items()},
                    flags=self.uc.reg_read(xc.UC_X86_REG_EFLAGS),
                    ymm=[self.uc.reg_read(reg) for reg in YMM_IDS], mxcsr=self.uc.reg_read(xc.UC_X86_REG_MXCSR),
                    stack=bytes(self.uc.mem_read(STACK, STACK_SIZE)), state=bytes(self.uc.mem_read(STATE, 0x1000)),
                    instruction_count=len(self.trace))


def validate_case(engine, case, ctx, flags, counts):
    state = initial_state(case)
    native = engine.run(REFERENCE, ctx, flags, state)
    hook = engine.run(CODE, ctx, flags, state)
    native_target = FAILURE if case['status'] & 0x80000000 else POSITIVE
    require(native['endpoint'] == native_target, 'Native bridge took an unexpected endpoint')
    require(native['gprs'] == ctx['gprs'], 'Native reference failed GPR preservation')
    require(native['ymm'] == ctx['ymm'] and native['mxcsr'] == ctx['mxcsr'], 'Native reference changed SIMD state')
    require(native['state'] == state + engine.state_guard, 'Native reference changed policy state')
    require(native['stack'][RSP - STACK:] == ctx['stack'][RSP - STACK:], 'Native reference changed caller frame')
    require(hook['stack'] == ctx['stack'], 'Hook changed stack bytes')
    require(hook['ymm'] == ctx['ymm'] and hook['mxcsr'] == ctx['mxcsr'], 'Hook changed SIMD or MXCSR state')
    require((hook['flags'] & FLAGS_CONTRACT_MASK) == (native['flags'] & FLAGS_CONTRACT_MASK),
            f'Hook/native flag mismatch: hook={hook["flags"]:#x}, native={native["flags"]:#x}, input={flags:#x}')
    require((hook['flags'] & (1 << 10)) == (flags & (1 << 10)), 'Hook changed DF')
    if hook['flags'] == native['flags']:
        counts['full_flags_equal_including_undefined_AF'] += 1
    expected_regs = dict(ctx['gprs'])
    expected_state = bytearray(state)
    admitted = is_admitted(case)
    if admitted:
        expected_regs['R13'] = 1
        struct.pack_into('<Q', expected_state, 16, (case['counter'] + 1) & MASK64)
        struct.pack_into('<I', expected_state, 56, 0xC0000034)
        require(hook['endpoint'] == BOOKKEEPING, 'Eligible case missed bookkeeping')
        counts['admission'] += 1
    else:
        require(hook['endpoint'] == native['endpoint'], 'Fallback endpoint differs from native bridge')
        counts['fallback_' + TARGETS[hook['endpoint']]] += 1
    require(hook['gprs'] == expected_regs,
            'Unexpected GPR changes: ' + repr({k: (hex(expected_regs[k]), hex(hook['gprs'][k])) for k in GPRS if hook['gprs'][k] != expected_regs[k]}))
    require(hook['state'] == bytes(expected_state) + engine.state_guard, 'Unexpected state or guard byte changes')
    counts['scenarios_passed'] += 1
    counts['hook_instructions'] += hook['instruction_count']
    counts['native_instructions'] += native['instruction_count']


def persistent_admissions(engine):
    case = make_cases()[0]
    case.update(counter=0)
    ctx = context_for(case, 71)
    state = initial_state(case)
    flags_seen = []
    for i in range(16):
        flags = FLAGS_CASES[(i * 9) % len(FLAGS_CASES)]
        before = state
        out = engine.run(CODE, ctx, flags, before)
        reference = engine.run(REFERENCE, ctx, flags, before)
        expected = bytearray(before)
        struct.pack_into('<Q', expected, 16, i + 1)
        struct.pack_into('<I', expected, 56, 0xC0000034)
        regs = dict(ctx['gprs'], R13=1)
        require(out['endpoint'] == BOOKKEEPING, 'Persistent admission stopped reaching bookkeeping')
        require(out['state'] == bytes(expected) + engine.state_guard, 'Persistent counter/state mismatch')
        require(out['gprs'] == regs and out['stack'] == ctx['stack'], 'Persistent admission damaged registers or stack')
        require(out['ymm'] == ctx['ymm'] and out['mxcsr'] == ctx['mxcsr'], 'Persistent admission damaged SIMD state')
        require((out['flags'] & FLAGS_CONTRACT_MASK) == (reference['flags'] & FLAGS_CONTRACT_MASK), 'Persistent flags mismatch')
        flags_seen.append(hex(flags))
        state = out['state'][:64]
    return dict(passed=True, consecutive_admissions=16, initial_counter=0, final_counter=16,
                final_enabled=struct.unpack_from('<I', state, 8)[0], last_status=hex(struct.unpack_from('<I', state, 56)[0]),
                input_flags=flags_seen)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--object', type=Path, default=ROOT / 'match_obj' / 'randgrid_match.obj')
    parser.add_argument('--dll', type=Path, default=ROOT / 'match_build' / 'DLL' / 'XInput9_1_0.dll')
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--native-image', type=Path, default=NATIVE_IMAGE)
    parser.add_argument('--output', type=Path, default=ROOT / 'randgrid_match_validation.json')
    parser.add_argument('--expected-dll-sha256')
    parser.add_argument('--expected-source-sha256')
    args = parser.parse_args()
    started = time.monotonic()
    report = dict(schema=1, passed=False, timestamp_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                  validator_path=str(Path(__file__).resolve()), validator_sha256=sha(Path(__file__).read_bytes()),
                  environment=dict(python=platform.python_version(), platform=platform.platform(),
                                   capstone=capstone.__version__, unicorn=unicorn.__version__, pefile=pefile.__version__),
                  scope='Actual COFF instructions and final DLL identity, executed only in isolated Unicorn memory; no game process interaction.',
                  limitations=['No gameplay or native Windows execution is performed.',
                               'Atomic increment semantics are executed single-threaded; concurrency is not stress-tested.',
                               'TEST leaves AF undefined; AF is excluded from the architectural contract, with full equality separately counted.',
                               'Reference bridge is permitted its documented scratch writes below initial RSP; the hook must preserve every mapped stack byte.',
                               'Continuation addresses are nonexecuting stop sentinels. Native bookkeeping and resource lifecycle are outside this harness.'])
    try:
        source = args.source.read_bytes()
        if args.expected_source_sha256:
            require(sha(source) == args.expected_source_sha256.lower(), 'Assembly source hash differs from parent-supplied final source')
        report['source'] = dict(path=str(args.source), sha256=sha(source))
        obj = load_coff(args.object)
        hook = relocate(obj['code'], obj['relocations'], CODE, STATE)
        report['object'] = {k: v for k, v in obj.items() if k != 'code'}
        report['object']['unrelocated_code_hex'] = obj['code'].hex()
        report['object']['isolated_code_hex'] = hook.hex()
        report['dll'] = inspect_linked_dll(args.dll, obj, args.expected_dll_sha256)
        disasm = list(Cs(CS_ARCH_X86, CS_MODE_64).disasm(hook, CODE))
        require(sum(i.size for i in disasm) == len(hook), 'Capstone did not decode every assembled hook byte')
        report['disassembly'] = [dict(offset=hex(i.address - CODE), bytes=i.bytes.hex(), mnemonic=i.mnemonic, operands=i.op_str) for i in disasm]
        normalize = [index for index, i in enumerate(disasm) if i.mnemonic == 'mov' and i.op_str == 'r13d, 1']
        require(len(normalize) == 1, 'Missing or duplicate R13D normalization')
        after = disasm[normalize[0] + 1]
        require(after.mnemonic == 'test' and after.op_str == 'edx, edx', 'Stale object: admission TEST EDX, EDX is absent')
        require(not any(i.mnemonic.startswith('call') for i in disasm), 'Unexpected call in hook')
        report['admission_test_after_r13d_verified'] = True
        native, report['native_reference'] = load_native_reference(args.native_image)
        engine = Engine(hook, native)
        counts = Counter()
        case_reports = []
        for index, case in enumerate(make_cases()):
            ctx = context_for(case, index)
            case_report = dict(case)
            case_report['admitted_expected'] = is_admitted(case)
            case_report['flags_variants'] = len(FLAGS_CASES)
            case_report['passed'] = False
            case_reports.append(case_report)
            report['case_results'] = case_reports
            for flags in FLAGS_CASES:
                try:
                    validate_case(engine, case, ctx, flags, counts)
                except Exception as exc:
                    raise AssertionError(f'Case {case["name"]}, input flags {flags:#x}: {exc}') from exc
            case_report['passed'] = True
        report['persistent_admission'] = persistent_admissions(engine)
        instruction_offsets = {i.address - CODE for i in disasm}
        require(engine.coverage == instruction_offsets,
                f'Assembled instructions not covered: {sorted(instruction_offsets - engine.coverage)}')
        report['coverage'] = dict(base_cases=len(case_reports), flag_variants_per_case=len(FLAGS_CASES),
                                  flag_dimensions='All 64 combinations of CF/PF/AF/ZF/SF/OF times DF=0/1; IF=1.',
                                  all_assembled_instructions_executed=True,
                                  assembled_instruction_count=len(disasm), executed_instruction_offsets=list(map(hex, sorted(engine.coverage))),
                                  emulation_runs=engine.runs, **dict(counts))
        report['preservation_checks'] = dict(all_16_GPRs=True, full_64bit_R12_gate=True,
                                             full_64bit_handle_gate=True, R13D_zero_extension=True,
                                             full_256bit_YMM0_to_YMM15=True, MXCSR=True,
                                             entire_64KiB_hook_stack_unchanged=True, entire_state_page_checked=True,
                                             DF_and_defined_TEST_flags_match_native=True,
                                             exact_enabled_value_one=True, exact_low32_status=True,
                                             counter_64bit_increment_and_wrap=True,
                                             short_circuit_before_unreadable_frame=True)
        report['passed'] = True
    except Exception as exc:
        report['error'] = repr(exc)
    report['elapsed_seconds'] = round(time.monotonic() - started, 6)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(passed=report['passed'], output=str(args.output), elapsed_seconds=report['elapsed_seconds'],
                          error=report.get('error'), coverage=report.get('coverage'),
                          object_sha256=report.get('object', {}).get('sha256'), dll=report.get('dll')), indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
