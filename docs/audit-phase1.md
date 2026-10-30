# Phase 1 technical audit

Documentation language: English

## Scope and status of this document

Audit of the Phase 1 code base (`v1.0.0-phase1`, commit `5751cdb784c66dc92849b1ac327f168573e1948b`; the C sources are byte-identical on `main`). It was performed before any Phase 2 code change.

- **Method:** source reading, execution of the existing tests and CLI, ad-hoc numerical experiments, AddressSanitizer/UBSan runs, strict compiler warnings, `-fanalyzer`, `gcov` coverage, and six hand-made mutants.
- **Environments:** analysis sandbox with Ubuntu 24.04 / GCC 13.3 / GNU Make 4.3. Independent confirmation of build and tests on Debian 13 / GCC 14.2.0 / GNU Make 4.4.1 (0 warnings, 5 tests exit 0).
- **Evidence limits:** the experiment programs are **not yet versioned in this repository**. Every finding below must be re-verified by a versioned, reproducible test before it is closed (Phase 2, milestones M1–M3). Reference values marked "oracle" were computed independently from the U.S. Standard Atmosphere 1976 equations and the standard pitot/CAS relations, not copied from a published table; they should be cross-checked against the published reference when requirements are formalized.
- **Not reviewed line by line:** the test sources, `logger_print_summary`/`logger_print_fault_report`, and `docs/f14_references.md`.
- Nothing here implies DO-178C compliance, certifiability, or flight suitability.

## What was found to be correct

- Rayleigh and isentropic pitot relations agree with tabulated ideal-gas (γ = 1.4) values to about 4e-5 (M = 1, 1.5, 2, 3).
- Mach inversion round trip: max error 3.8e-7 for M in [0.01, 3]; 4.3e-7 near M = 1.
- `atmosphere.c` agrees with the independent oracle to 2.8e-5 (relative, pressure) over 0–20 km.
- AddressSanitizer + UBSan: no reports in the 5 tests and 6 CLI invocations. Strict GCC warning set and `-fanalyzer`: no warnings in `src/` and `main.c`. (This does not prove absence of defects.)
- Same seed gives byte-identical CSV on the same machine and toolchain.

## Confirmed defects

Priority: P0 blocks verification; P1 correctness or data integrity; P2 consistency or architecture; P3 hygiene.

| ID | Finding | Evidence | Prio |
|----|---------|----------|------|
| F-01 | Tests cannot fail: every test `main()` returns 0; verdicts are printed text. | 0/6 mutants fail `make tests`; only 2/6 (M3 wing sweep, M4 input validation) add a visible error line; ISA (M1, M5) and gamma (M6) mutants pass silently. Line coverage 77–96 %, branches taken 58–73 %. | P0 |
| F-02 | `tests/test_atmosphere.c:90` calls `scanf("%*c")`, which blocks non-interactive runs (`make tests`). | Two hangs of more than 300 s. | P0 |
| F-03 | `airdata_compute()` leaves 7 fields uninitialized on invalid input: `static_temp_K`, `static_temp_C`, `density_kg_m3`, `density_ratio`, `speed_of_sound_ms`, `dynamic_pressure_Pa`, `impact_pressure_Pa`. | Struct pre-filled with 0xAA; all 7 keep the pattern. CSV and panel read them. | P1 |
| F-04 | Pressure semantics are wrong: `dynamic_pressure_Pa` holds impact pressure qc = Pt − Ps; `impact_pressure_Pa` = Pt − P0 (negative at altitude). | At 9000 m, M 0.85: field 18563 Pa vs true q = ½γ·Ps·M² = 15548 Pa; `impact_pressure_Pa` = −52019 Pa. | P1 |
| F-05 | `valid = 1` does not imply finite or in-range data. | Pt = NaN → valid 1, CAS NaN; Pt = Inf → M 5, valid 1; Ps = 1 Pa → altitude 20000 m, valid 1; CLI `--mach nan` → 100 % supersonic cycles, 0 invalid. | P1 |
| F-06 | VSI has no reliable history: `prev_altitude_m` starts at 0 and only advances on valid cycles while `dt` spans one cycle. | First cycle invalid → VSI +1,771,623 ft/min at constant altitude; after an invalid cycle VSI doubles (+20 vs +10 m/s true); 62 of 523 valid cycles with error > 3 m/s. | P1 |
| F-07 | 49.4 % of cycles are invalid on the ground (M = 0, default noise). | 988 of 2000 cycles (Pt ≈ Ps, noise makes Pt < Ps). `AIRDATA_MACH_MIN` is declared and never used. | P1 |
| F-08 | Duplicated, inconsistent constants. | R = 287.058 (standard 287.05287); ISA exponent literal in `airdata.c` vs computed (difference 9e-5); tropopause pressure constant 22632.1 vs formula 22632.65 (0.55 Pa jump; 0.16 m altitude round-trip error); Mach 1 threshold truncated to 1.8929 (exact 1.892929). Small physical effect; no single source of truth. | P2 |
| F-09 | `data/isa_reference.txt` contains transcription errors and is not used by any code. | Rows 5500–10500 m: pressure error up to 1.6e-3, density up to 3.9e-3, T = 220.01 K at 10500 m (formula 219.90 K). It labels altitude "geometric" while the model is geopotential (19 m difference at 11 km, 63 m at 20 km). | P2 |
| F-10 | Plausibility monitoring lives in the logger and does not feed back into the CADC outputs. | `sensor_invalid` depends only on `airdata.valid`; the check "dynamic pressure < 0" is dead after input validation; the sensor-fault latch never clears. | P2 |
| F-11 | Simulation/CADC boundary is blurred. | `SensorReading` carries `true_*` fields into the core; the thermometer returns static T although a comment describes total-temperature modelling; the open-loop profile climbs about 3300 ft/min at 7–98 kt CAS and triggers `low_speed` during takeoff (t = 10–15 s). | P2 |
| F-12 | CLI performs no numeric validation. | `atof`/`atoi`: `--alt abc` → 0; `--seed abc` → seed 0 → `time(NULL)` (not reproducible); `--dt nan` → 0 cycles, exit 0. `NoiseConfig.seed` and `update_rate_hz` are written and never read. | P3 |
| F-13 | Repository hygiene. | Test binaries not covered by `.gitignore`; CRLF in `src/`, `tests/`, `docs/`, `data/` and LF in `main.c`, `Makefile`, `.gitignore`, `development-assurance.md`, no `.gitattributes`; unused constants (`AIRDATA_MACH_MIN`, `F14_CAS_LAND_KT`, `F14_WING_CARRIER_DEG`, `F14_ALT_MAX_M`, `LOGGER_BUFFER_SIZE`); alert thresholds 100 kt / 500 ft hard-coded in `cadc.c`. | P3 |

## Latent and potential risks

| ID | Risk | Evidence | Status |
|----|------|----------|--------|
| R-1 | CAS uses the subsonic formula in all regimes. Correct while CAS < a0 (661 kt), which covers the shipped profiles. | Versus the standard supersonic CAS relation at sea level: −0.1 % (M 1.1), −1.2 % (M 1.3), −3.4 % (M 1.5, −33 kt), −7.6 % (M 1.8, −91 kt). The 777 kt overspeed threshold lies in the affected regime. `physics.md` already documents CAS as an approximation. | Latent; reproducible with `--alt 0 --mach 1.5` |
| R-2 | Silent saturation: Mach above 5 clamps to 5.0; out-of-range altitude and pressure are clamped without indication. | M = 6 returns 5.000000. | Confirmed behavior; requirement undefined |
| R-3 | Cross-platform numerical reproducibility. Global `rand()`/`srand()` makes noise sequences libc-dependent; the README claims macOS and Windows testing that has not been verified here. | Checked on one toolchain family only. | Unverified claim |

## Open questions that block requirements

1. Required behavior at Pt ≈ Ps or M ≈ 0 on the ground: Mach 0 and valid, or invalid?
2. System boundary: air-data algorithms only, or also wing-sweep scheduling and alerts?
3. Timing requirements: `update_rate_hz` is never used and a comment cites 50 Hz.
4. Provenance of aircraft limits (777 kt CAS, Mach 2.34, 60000 ft, wing-sweep thresholds): unverified claims until a variant-specific source is cited.
