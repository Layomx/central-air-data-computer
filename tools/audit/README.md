# Phase 1 audit reproduction

Executable evidence for [`docs/audit-phase1.md`](../../docs/audit-phase1.md). It runs against the **immutable tag** `v1.0.0-phase1`, never against the working tree, so its results do not change when Phase 2 modifies `src/`.

These programs **reproduce findings on Phase 1 code**. They are not regression tests for Phase 2: a finding is closed by a test in the Phase 2 suite, not by this tool. Once a finding is fixed on `main`, this tool still reports it as REPRODUCED on the tag, which is the intended behavior.

## Run

```bash
git fetch --tags
tools/audit/run.sh          # about 30 s
```

Requirements: `bash`, `git`, `tar`, `gcc`, `make`, `timeout`, `mkfifo`; `gcov` is optional (coverage is skipped without it). Nothing in the repository is modified; all work happens under `AUDIT_OUT` (default `/tmp/cadc-audit`).

| Variable | Default | Meaning |
|----------|---------|---------|
| `AUDIT_TAG` | `v1.0.0-phase1` | tag to audit |
| `AUDIT_OUT` | `/tmp/cadc-audit` | scratch and results directory (deleted and recreated) |
| `CC` | `gcc` | compiler |

Outputs: `results.txt` (all `CHECK` lines) and `environment.txt` (tag, commit, compiler, make, libc, machine). Keep both when quoting results. Exit status: `0` all checks REPRODUCED, `1` at least one NOT-REPRODUCED, `2` setup error.

`CHECK <id> REPRODUCED` means the claim in the audit document holds on the audited tree. `NOT-REPRODUCED` means the documented number or behavior was not observed in this environment: investigate before trusting either the audit or the environment.

## What each check covers

| IDs | Program | Claim |
|-----|---------|-------|
| F-08, F-09, R-2, OK-01, OK-02, OK-03 | `exp_atmosphere.c` | constants, ISA reference table vs independent oracle, saturation, pitot relations, Mach round trip |
| F-03, F-04, F-05, R-1, OK-04 | `exp_airdata.c` | uninitialised outputs, pressure semantics, non-finite inputs, CAS in the supersonic regime, CAS inside the shipped profiles |
| F-06, F-07, F-10, F-11 | `exp_cadc.c` | VSI after invalid cycles, ground invalid fraction, unreachable validation check and fault latch, open-loop takeoff alert, no total-temperature effect |
| F-01, F-02, F-12, F-13, OK-05..OK-08 | `run.sh` | tests cannot fail (six hand-made mutants), blocking `scanf`, coverage, CLI parsing, repository hygiene, sanitizers, strict warnings, `-fanalyzer`, same-seed reproducibility |

`OK-*` identifiers are claims from "What was found to be correct" and the build/determinism confirmations. `audit.h` is a small output helper, not a test framework.

## Limits of this evidence

- The **oracles** (ISA, supersonic CAS) are independent implementations of the standard equations written for this audit. They are not copied from a published table. The tabulated pitot values in `OK-01` come from standard ideal-gas tables recalled during the audit; cite the exact source when requirements are formalised.
- Thresholds are tolerance bands around the documented numbers, not exact equality.
- The mutant check detects "visible" failures by counting failure words in the test output against an unmodified baseline. It is a heuristic that exists only to show that exit status never reflects a failure.
- Sanitizers, strict warnings and `-fanalyzer` report no issues on the executed paths. That does not prove absence of defects.
- `OK-08` covers one machine and toolchain. Cross-platform reproducibility (R-3) and the README claim of macOS and Windows testing are **not** verified here.
- Not executable and therefore reviewed manually: F-10 monitor placement (logger vs CADC core), F-11 boundary blur (`true_*` fields travelling inside `SensorReading`).
- The audited numbers were first obtained on Ubuntu 24.04 / GCC 13.3 / glibc 2.39. Results on other toolchains are recorded by running the tool there.
