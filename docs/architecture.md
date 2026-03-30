# System architecture

Language: English | [Espanol](arquitectura.es.md)

## Overview

The project is split into six modules with strictly one-way dependencies. Lower-level modules do not depend on higher-level modules.

```
main.c
  -> logger.h/.c
       -> cadc.h/.c
            -> airdata.h/.c
                 -> sensors.h/.c
                      -> atmosphere.h/.c
```

This hierarchy mirrors the real CADC design approach: low-level subsystems (atmosphere and sensors) remain independent from the high-level integrator.

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

**Design decision:** Instead of producing arbitrary pressure values, this module takes altitude + Mach over time and derives physically valid pressure and temperature values through ISA-based equations. This guarantees consistency among `Pt`, `Ps`, and `T`.

**Gaussian noise:** Implemented with Box-Muller using only standard C `rand()`.

---

### Module 3 - airdata.h/.c

**Responsibility:** Computational core - pitot equation inversion.

**Inputs:** `SensorReading` (`Pt`, `Ps`, `T`)

**Outputs:** `AirDataState` with Mach, TAS, CAS, EAS, barometric altitude, VSI, density

**Dependencies:** `sensors.h`, `atmosphere.h`

**Critical design decision:** Supersonic Mach inversion uses numeric bisection instead of Newton-Raphson. The reason is robustness: bisection cannot diverge even with noisy inputs. Certified avionics software often prioritizes conservative and predictable behavior over speed.

**CAS bug fix note:** An earlier version used `qc = Pt - P0`, which incorrectly produced `CAS = 0` at altitude. The corrected relation is `qc = Pt - Ps`, matching the physical instrument definition.

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

**Dependencies:** `cadc.h`

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

**Defensive clamping:** All modules clamp inputs before calculations to avoid propagating invalid values (`NaN`, `Inf`) due to noise spikes or programming mistakes.

**Headers as contracts:** Each `.h` file defines the full public API. `.c` implementations can evolve internally as long as function signatures and documented behavior remain compatible.