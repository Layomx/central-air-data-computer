# System architecture

Documentation language: English

## Overview

The project is split into six modules with mostly one-way dependencies. The logger is an observer of the CADC state; it is not a processing stage above the CADC.

```
main.c
  ├── cadc_update()
  │    ├── sensors_read()
  │    │    └── atmosphere_calculate()
  │    ├── airdata_compute()
  │    ├── wing-sweep scheduler
  │    └── alert evaluation
  └── logger_log_cycle()
       ├── validation and fault diagnosis
       └── CSV/statistics output
```

This is a software decomposition inspired by the functional role of an air-data computer. It is not a wiring diagram or a claim that the original F-14 CADC used these software modules.

---

## Modules and responsibilities

### Module 1 - atmosphere.h/.c

**Responsibility:** ISA standard atmosphere model.

**Inputs:** altitude in meters `[0, 20000]`

**Outputs:** `AtmosphereState` with temperature, pressure, density, speed of sound

**Dependencies:** none (base module)

**Why it is separate:** ISA is used both by the sensor model (to generate physically consistent pressures) and by airdata calculations (to invert pressure back to altitude). Separation avoids duplicated equations.

---

### Module 2 - sensors.h/.c

**Responsibility:** Simulation of the three CADC physical sensors.

**Inputs:** `SensorConfig` (flight profile and noise setup)

**Outputs:** `SensorReading` with `Pt`, `Ps`, `T` (optional Gaussian noise)

**Dependencies:** `atmosphere.h`

**Design decision:** Instead of producing arbitrary pressure values, this module takes altitude + Mach over time and derives a self-consistent synthetic measurement set through ISA-based equations. This is a test-data generator, not a model of the original probe electronics.

**Gaussian noise:** Implemented with Box-Muller using only standard C `rand()`.

---

### Module 3 - airdata.h/.c

**Responsibility:** Computational core - pitot equation inversion.

**Inputs:** `SensorReading` (`Pt`, `Ps`, `T`)

**Outputs:** `AirDataState` with Mach, TAS, CAS, EAS, barometric altitude, VSI, density

**Dependencies:** `sensors.h`, `atmosphere.h`

**Critical design decision:** Supersonic Mach inversion uses numeric bisection instead of Newton-Raphson. The reason is bounded, predictable iteration behavior within a validated bracket. This design choice alone does not make the software compliant with any avionics standard.

**CAS note:** The implementation uses `qc = Pt - Ps` as an educational impact-pressure approximation. The code and documentation must not present this as a complete supersonic CAS calibration model.

---

### Module 4 - cadc.h/.c

**Responsibility:** Central integrator - connects all modules and feeds downstream systems.

**Inputs:** `CadcConfig`, simulated time

**Outputs:** complete `CadcState` (airdata + wing state + alerts)

**Dependencies:** `airdata.h`, `sensors.h`

**Update cycle:**
```
1. Compute dt
2. Read sensors
3. Compute airdata
4. Update wing sweep angle
5. Evaluate alerts
6. Optional logging
7. Increment cycle counter
```

**Wing model:** Implements F-14 AWG (Automatic Wing Geometry) logic with four Mach regions and linear interpolation. Hysteresis is intentionally not modeled in this version.

---

### Module 5 - logger.h/.c

**Responsibility:** Instrumentation - CSV export, physical consistency checks, sensor diagnostics.

**Inputs:** `CadcState` every cycle

**Outputs:** `.csv` file, diagnostics, summary statistics

**Dependencies:** `cadc.h`; called by `main.c` after `cadc_update()`

**Two verification levels:**

| Level | Module | Question |
|-------|--------|----------|
| Operational envelope | `cadc.c` | Is the aircraft inside operational limits? |
| Physical consistency | `logger.c` | Are these values physically plausible? |

The logger catches conditions like `CAS > TAS` (physically impossible) that might not be part of CADC operational alert rules.

**Fault detection:** Uses counters of consecutive abnormal readings per sensor. A single outlier can be normal random noise; multiple consecutive outliers indicate probable sensor failure.

---

### Module 6 - main.c

**Responsibility:** Entry point, CLI arguments, main simulation loop.

**Inputs:** command-line arguments

**Outputs:** terminal flight table, final summary

**Dependencies:** `logger.h`, `cadc.h`

**No domain logic here.** Physics, core calculations, and validation live in previous modules. `main.c` only orchestrates module wiring.

---

## Code conventions

**Units:** Internal calculations use SI units only (m, Pa, K, m/s). Conversions to aviation units (ft, kt, ft/min) happen only at display/output boundaries.

**Output structures:** Each module exposes a result structure. Passing pointers to these structures defines the public interface and allows fields to grow without breaking function signatures.

**Pure vs stateful functions:** Calculation functions (`airdata_calc_mach`, `cadc_calc_wing_sweep`) are pure and side-effect free. Stateful functions (`cadc_update`, `logger_log_cycle`) explicitly receive and update state structures. No global mutable state is required.

**Defensive clamping:** Several modules clamp inputs before calculations, but this is not yet a complete numeric-safety policy. The refactoring roadmap will define explicit range, error, and non-finite-value requirements.

**Headers as contracts:** Each `.h` file defines the full public API. `.c` implementations can evolve internally as long as function signatures and documented behavior remain compatible.
