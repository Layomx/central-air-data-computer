# Phase 2 test assurance record

## Purpose and status

This document records the Phase 2 verification work performed against the
legacy simulator. It is an engineering record, not a certification artifact
and not a claim of DO-178C compliance.

The immediate objective is to replace demonstration-style tests with
automated, deterministic verification that can fail the build and that leaves
reviewable evidence. The production modules are being changed separately from
their tests; this record therefore distinguishes test-framework changes from
algorithm or interface changes.

## Baseline problems from the Phase 1 audit

The relevant audit finding is:

| Finding | Problem | Phase 1 evidence | Phase 2 response |
|---|---|---|---|
| F-01 | Test programs printed results but returned success unconditionally. | Mutated implementations could pass `make tests`; the textual verdict was not machine-enforced. | Shared assertion context, failure accounting, non-zero exit status, and a final verdict. |
| F-02 | `test_atmosphere.c` waited for `scanf`, blocking automation. | Non-interactive test execution could hang. | The atmosphere test has no interactive input and runs from a finite test vector. |

The complete finding definitions remain in [`audit-phase1.md`](audit-phase1.md).

## Decisions recorded in Phase 2

### Shared test framework

Tests use `tests/test_framework.h` and `tests/test_framework.c` rather than
printing ad-hoc `CORRECTO`/`ERROR` messages. The framework provides:

- a `TestContext` with total, passed, and failed assertion counts;
- boolean, near-equality, finite-value, integer-equality, unsigned-equality,
  and inclusive-range checks;
- requirement identifiers on every assertion;
- file and line information for failed assertions;
- a summary that returns `0` only when assertions exist, all assertions pass,
  and the accounting invariant is satisfied;
- non-zero return status for failed verification or an invalid context.

The framework is intentionally small and has no dependency on the production
code. It is test support, not part of the simulated CADC.

### Test execution

Each executable test owns its `main`, initializes a context, executes its
verification procedures, and returns `test_summary(&ctx)`. The Makefile links
the shared framework into each migrated test target. A test that fails an
assertion therefore fails both its process and the enclosing `make tests`
recipe.

### Requirement identifiers

The current identifiers are provisional test-level identifiers, for example
`ATM-REF-*` and `SENS-REQ-*`. They identify the behavior checked by an
assertion; they are not yet approved system or software requirements. They
must later be mapped to reviewed requirements and design elements.

### Independent reference data

Atmosphere and Pitot tests use explicit reference values and documented
equations instead of obtaining the expected value by calling the same
production function under test. This reduces the risk of reproducing the
same implementation defect in the oracle. Reference provenance and tolerance
decisions remain subject to review before being treated as formal evidence.

### Determinism

Interactive input has been removed from the migrated tests. Sensor noise tests
use a fixed seed when checking reproducibility and statistics. This documents
the current behavior of the legacy global random-number implementation; it
does not yet establish cross-platform bit-for-bit reproducibility (that is a
separate Phase 2 design issue related to R-3).

## Current migrated tests

### `test_atmosphere.c`

The atmosphere test now uses the shared framework and returns a meaningful
process status. It covers independent ISA reference points, public API
results, altitude limits, derived-property consistency, monotonicity,
tropopause continuity, and unit conversions. It runs without `scanf` or
operator interaction.

### `test_sensors.c`

The sensor test now uses the same framework and returns a meaningful process
status. Its current scope includes:

- default configuration;
- constant, profile, interpolation, endpoint, and capacity behavior;
- subsonic and supersonic Pitot pressure ratios;
- built-in F-14 profile samples;
- noiseless behavior;
- noise activation and deactivation;
- fixed-seed sequence reproduction;
- statistical mean and standard-deviation checks;
- finite output checks and the measured dynamic-pressure identity.

The test does not claim that the sensor model is physically complete. In
particular, sensor-interface semantics, validation of invalid profile input,
temperature-probe modeling, and random-number portability remain candidates
for later requirements and production changes.

## F-01 closure assessment

F-01 is not closed for the entire repository yet. It is addressed for the
migrated atmosphere and sensor test targets when all of the following are
true:

1. the target is linked with the shared framework;
2. assertions are recorded in `TestContext`;
3. a failed assertion returns a non-zero process status;
4. the test contains at least one assertion and reports a passing summary;
5. the test can run non-interactively to completion.

The remaining test programs must still be migrated and verified against the
same criteria. A complete F-01 closure requires all test targets in the
repository, a clean build, execution of the complete suite, and at least one
negative-control or mutation check demonstrating that an introduced defect
causes a non-zero result.

## Evidence expected for this increment

For the migrated tests, retain the following review evidence with the branch
or its integration record:

```text
make clean
make test_atmosphere test_sensors
./tests/test_atmosphere
./tests/test_sensors
git diff --check
```

The expected result is a zero exit status for the unmodified implementation,
with a non-zero exit status if an asserted result is deliberately changed.
The exact assertion totals are output by the executables and should be
recorded from the verified toolchain rather than hard-coded in this document.

## Deferred work

This document does not authorize changes to `src/atmosphere.*` or
`src/sensors.*`. Production-module findings will be recorded in the separate
implementation issue/decision notes proposed for Phase 2. Future work should
add traceability, reviewed numerical references, invalid-input behavior,
coverage evidence, and configuration-controlled build evidence before any
claim of a complete assurance baseline.
