
# Focused tests

The historical main and recurring-worker native probes are in source/randgrid_probe.* and source/randgrid_worker_probe.*. They require x64 MSVC/MASM, Windows SDK and the shared header from src/. The historical receipts are stored under receipts/ and do not test the match path.

The match-specific validator is source/validate_randgrid_match.py, using Python, Capstone, Unicorn and pefile. It operates only on a private Unicorn emulation; it does not attach to, start or alter a game process.

The match validator must receive an authorized matching COFF object, the corresponding compiled DLL and a separate native reference image, none of which are included in this repository. Example arguments:

    python -m pip install capstone unicorn pefile
    python tests/source/validate_randgrid_match.py --source src/randgrid_match.asm --object PATH_TO_MATCH_OBJ --dll PATH_TO_DLL --native-image PATH_TO_REFERENCE_IMAGE --output randgrid_match_validation.json

The historical run reported 4,096 passing input/flag combinations and 16 consecutive eligible admissions. The Python validator's original machine-specific default paths were replaced with portable repository-relative defaults, so the historical validator hash is not a hash of this exact copy. Test a new build against its own source and object/binary identities rather than reusing the saved receipt as certification.

Native bookkeeping, concurrent invocation timing, other executable versions and real LAN peers are outside the isolated emulation test.
