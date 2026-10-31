# Development assurance and refactoring roadmap

## Purpose

This document establishes the planned software-engineering direction for the project. The goal is to evolve the legacy educational simulator into a complete, testable software model that can later run behind virtual hardware interfaces and, if appropriate, on a physical target.

The project will use RTCA DO-178C / EUROCAE ED-12C as a process and assurance reference. The FAA identifies AC 20-115D as an acceptable means, but not the only means, of showing compliance for applicable airborne software. This project is not an airborne product and does not claim compliance, certification, or an assigned software level.

References:

- [FAA AC 20-115D](https://www.faa.gov/regulations_policies/advisory_circulars/index.cfm/go/document.information/documentID/1032046)
- [RTCA DO-178C](https://www.rtca.org/do-178/)
- [FAA software and airborne electronic hardware guidance](https://www.faa.gov/aircraft/air_cert/design_approvals/air_software/software_regs)

## What “DO-178C-oriented” means here

The refactoring will adopt the discipline of requirements, design data, implementation control, verification, configuration management, quality assurance, and objective evidence. It will not copy certification artifacts mechanically or imply that passing local tests is equivalent to certification.

No Design Assurance Level is assigned at this stage. A DAL is a system-level safety-assessment result, not a property that can be selected from the source code alone. If the project later targets airborne use, the system function, installation, failure conditions, independence, tools, and certification authority would have to be assessed separately.

## Project phases

### Phase 1 — Original simulation (complete, frozen)

- Preserved as tag `v1.0.0-phase1` (commit `5751cdb784c66dc92849b1ac327f168573e1948b`) and a GitHub Release.
- Not modified after the tag. Its known limitations are recorded in [audit-phase1.md](audit-phase1.md).

### Phase 2 — Refactoring and engineering hardening (current)

- Freeze and document the legacy behavior before changing algorithms.
- Define the system boundary and distinguish simulated aircraft behavior from CADC behavior.
- Establish terminology, units, sign conventions, assumptions, source references, and known limitations.
- Create high-level and low-level requirements for each public interface, with acceptance criteria.
- Replace print-only tests with automated pass/fail tests and reproducible test data; keep verification evidence reproducible.
- Define deterministic execution, numeric ranges, error handling, data-validity rules, and startup behavior.
- Separate the atmospheric environment, sensor model, air-data algorithms, aircraft and wing logic, fault management, a platform-independent core, and output adapters.
- Replace the open-loop profile driver with an explicit simulation model.
- Correctly distinguish static, total, impact, and dynamic pressure, and define the temperature-probe model (including total-temperature recovery if it is retained).
- Add robustness tests for range limits, discontinuities, invalid data, timing, and numerical convergence.
- Add traceability from requirement to design, implementation, and verification; add configuration identification and a change history.

The current test-framework decisions, migrated test scope, and F-01 closure
criteria are recorded in [Phase 2 test assurance record](phase2-test-assurance.md).

### Phase 3 — Virtual hardware

- Define virtual sensor, actuator, clock, and bus interfaces, registers, and timing where relevant.
- Run the same functional core in software-in-the-loop configurations and check that the hardware model reproduces the specified behavior.
- Add timing budgets, scheduling behavior, interface monitoring, and fault injection.

### Phase 4 — Physical hardware (possible future extension, not planned in detail)

- Evaluate a target platform such as a microcontroller, processor board, or FPGA, and hardware-in-the-loop configurations.
- If airborne electronic hardware becomes an actual project objective, assess the relationship to DO-254 and the applicable system-development and certification processes before claiming suitability.
- Earlier phases must avoid architectural decisions that make this phase needlessly difficult.

## Planned assurance artifacts

The project is expected to introduce lightweight versions of the following artifacts as the baseline matures:

- system and software requirements;
- software architecture and detailed design;
- development and verification plans;
- configuration-management and quality-assurance plans;
- requirements-to-test traceability;
- verification procedures and results;
- known-problem and limitations register;
- release baseline and reproducibility record.

The names and content of these artifacts will be refined as the target use and assurance objectives become clearer.

## Current status

The repository is in Phase 2. It contains a useful functional prototype, not a certified or production-ready airborne software baseline. Existing physics, historical statements, and aircraft limits must be classified as one of:

- **documented fact:** supported by a cited primary or authoritative source;
- **engineering assumption:** selected to make the model executable;
- **implementation approximation:** a deliberate simplification of the real system;
- **unverified claim:** retained only until evidence is found or the claim is removed.
