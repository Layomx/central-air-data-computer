# Central Air Data Computer F-14 Tomcat

Documentation language: English

I have always loved aviation, especially military aircraft, and the F-14 Tomcat has always been my all-time favorite. After learning more about the history of its digital air data computer and seeing how much technical documentation is now public, I built this project as an educational simulation of the **Central Air Data Computer (CADC)** used by the F-14.

<img src="https://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/F-14_Tomcat_DF-SD-06-03497.jpg/1920px-F-14_Tomcat_DF-SD-06-03497.jpg" alt="F-14 Tomcat" width="800"/>

The project is an educational, software-only reconstruction of selected CADC functions. It takes simulated sensor inputs (pitot pressure, static pressure, and temperature), processes them with documented aerodynamic models, and produces air-data parameters consumed by the simulated wing-sweep scheduler, alert logic, and telemetry logger. It does not claim to reproduce the original hardware, wiring, calibration data, or certified aircraft interfaces.

## Documentation

- Architecture: [System architecture](docs/architecture.md)
- F-14 technical reference: [Technical reference](docs/f14_references.md)
- Implemented physics: [Implemented physics](docs/physics.md)
- Development assurance roadmap: [DO-178C-oriented roadmap](docs/development-assurance.md)

---

## Historical context

The F-14 used a Central Air Data Computer as part of its air-data and automatic wing-sweep functions. This model uses a 20°–68° in-flight wing-sweep range and a 75° oversweep/storage position as documented model inputs; the exact source revision and variant applicability must be tracked before treating those values as requirements. The exact internal implementation, calibration schedules, signal paths, and configuration differences between F-14 variants are not fully represented here.

The historical CADC is commonly described as a hybrid analog/digital system. This repository does not reproduce that hardware. It reimplements a selected functional model in portable C using IEEE-754 `double` arithmetic for educational and research purposes.

## Project status and refactoring commitment

The current codebase is the legacy Phase 1 educational simulator. The project will be refactored toward a complete, reviewable software product in three phases:

1. **Software baseline:** establish requirements, interfaces, assumptions, deterministic behavior, verification, configuration control, and documented limitations.
2. **Complete software model:** separate the physical model, air-data algorithms, aircraft/wing dynamics, fault handling, I/O adapters, and test infrastructure; define stable interfaces and machine-checkable verification evidence.
3. **Virtual hardware and hardware transition:** execute the same functional core behind virtual sensor/actuator interfaces, then evaluate a target implementation such as an embedded processor, FPGA, or hardware-in-the-loop platform.

The refactoring will use DO-178C as a development-assurance reference. It will not claim DO-178C compliance, an assigned Design Assurance Level, airworthiness approval, or certification. DO-178C applies in the context of an aircraft system and its safety assessment; this project is currently a non-airborne research simulator. See [Development assurance and roadmap](docs/development-assurance.md).

---

## What this project computes

From three sensor values (`Pt`, `Ps`, `T`), the system computes:

| Parameter | Description | Units |
|-----------|-------------|-------|
| **Mach** | Inversion of pitot / Rayleigh relation | dimensionless |
| **TAS** | True Airspeed | kt / m/s |
| **CAS** | Calibrated Airspeed | kt |
| **EAS** | Equivalent Airspeed | kt |
| **Altitude** | Barometric altitude | ft / m |
| **VSI** | Vertical speed | ft/min |
| **Sweep** | Wing sweep angle | deg |
| **Alerts** | Overspeed, max altitude, invalid sensor | flags |

---

## Build

Requires only `gcc` and `make`. No external dependencies.

```bash
git clone https://github.com/Layomx/central-air-data-computer.git
cd central-air-data-computer

make

make all

gcc -std=c11 -Wall -Wextra -g -I. \
    src/atmosphere.c src/sensors.c src/airdata.c \
    src/cadc.c src/logger.c main.c \
    -o cadc_f14 -lm
```

**Tested platforms:** Linux (GCC 11+), macOS (Clang 14+), Windows (MinGW/GCC).

Windows build command:

```bash
gcc -std=c11 -Wall -Wextra -I. \
    src/atmosphere.c src/sensors.c src/airdata.c \
    src/cadc.c src/logger.c main.c \
    -o cadc_f14 -lm
```

---

## Usage

```bash
./cadc_f14 [options]
```

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `--profile cruise` | Full cruise profile (takeoff -> M1.4 -> landing) | |
| `--profile intercept` | Aggressive intercept profile up to M2.34 | |
| `--profile custom` | Constant condition from `--alt` and `--mach` | |
| `--alt <m>` | Altitude in meters for custom profile | `0` |
| `--mach <M>` | Mach number for custom profile | `0` |
| `--noise` | Enable Gaussian sensor noise (sigma_P=15 Pa, sigma_T=0.5 K) | off |
| `--seed <n>` | Random seed for reproducibility | `42` |
| `--dt <s>` | Simulation time step | `1.0` |
| `--output <file>` | Output telemetry CSV path | `output/vuelo.csv` |
| `--no-csv` | Do not write CSV output | |
| `--quiet` | Show final summary only | |
| `--panel` | Show full instrument panel at key points | |
| `--help` | Show help | |

### Examples

```bash
# Basic cruise run - generates output/vuelo.csv
./cadc_f14

# Intercept with Gaussian noise, 0.5s step
./cadc_f14 --profile intercept --noise --dt 0.5

# Fixed F-14 cruise condition
./cadc_f14 --profile custom --alt 9000 --mach 0.85

# Full panel output at key moments
./cadc_f14 --panel --dt 2.0

# Statistical summary only
./cadc_f14 --quiet --output my_flight.csv

# Fixed seed for reproducibility
./cadc_f14 --profile intercept --noise --seed 1234
```

---

## Flight profiles

### Cruise (`--profile cruise`)

Simulates a full 1400-second mission:

```
Phase             t[s]    Alt[m]    Mach
-----------------------------------------
Takeoff              0        0    0.00
Initial climb       30      500    0.30
Climb              120     3000    0.50
Low cruise         300     7000    0.72
Subsonic cruise    500     9500    0.85
Transonic cross    650    10500    1.05
Supersonic cruise  800    12000    1.40
Descent           1000     8000    0.90
Approach          1200     1500    0.40
Landing           1350        0    0.20
```

### Intercept (`--profile intercept`)

Simulates a 1080-second intercept mission with max Mach 2.34:

```
Phase             t[s]    Alt[m]    Mach
-----------------------------------------
Takeoff              0        0    0.00
Rapid climb         90     4000    0.70
M=1.0              180     9000    1.00
Acceleration       350    15000    2.10
Max Mach           500    16000    2.34  <- wings at 68 deg
Post-combat        650    14000    1.80
Descent            800     8000    0.90
Landing           1080        0    0.20
```

### Custom (`--profile custom`)

Steady condition for 60 seconds. Useful for checking a specific flight point.

```bash
# Typical F-14 cruise at 30,000 ft
./cadc_f14 --profile custom --alt 9144 --mach 0.85

# Sea level, low speed
./cadc_f14 --profile custom --alt 0 --mach 0.3
```

---

## CSV output

The CSV output contains 28 columns per cycle and can be analyzed with Python:

```
time_s, mach, supersonic, altitude_ft, altitude_m,
tas_kt, cas_kt, eas_kt, vsi_ftmin,
static_temp_K, static_temp_C, density_ratio,
dynamic_pressure_Pa, speed_of_sound_ms,
sweep_deg, sweep_rate_dps, wing_auto,
alert_any, alert_mach, alert_cas, alert_alt, alert_sensor, alert_low_speed,
pt_Pa, ps_Pa, sensor_temp_K, airdata_valid, cycle
```

---

## Tests

```bash
# Run all tests
make tests

# Individual test binaries
./tests/test_atmosphere
./tests/test_sensors
./tests/test_airdata
./tests/test_cadc
./tests/test_logger
```

Each test includes automatic validation of physical invariants:
- `EAS <= TAS` in all conditions
- `TAS = Mach x a` in all conditions
- `sweep in [20 deg, 68 deg]` across the full envelope
- `CAS >= 0` always

---

## Implemented physics

### ISA model (atmosphere.c)

International Standard Atmosphere for 0-20,000 m:

```
Troposphere (0-11,000 m):
  T(h) = 288.15 - 0.0065 x h
  P(h) = 101325 x (T/288.15)^5.2561

Lower stratosphere (11,000-20,000 m):
  T    = 216.65 K (isothermal)
  P(h) = 22632.1 x exp(-(h-11000)/6341.6)

Density:  rho = P / (R x T)
Sound:    a = sqrt(gamma x R x T)
```

### Pitot pressure (sensors.c)

```
Subsonic (M < 1):
  Pt/Ps = (1 + 0.2 x M^2)^3.5

Supersonic (M >= 1) - Rayleigh equation:
  Pt/Ps = [(gamma+1)^2 M^2/(4 gamma M^2 - 2(gamma-1))]^(gamma/(gamma-1)) x (1-gamma+2 gamma M^2)/(gamma+1)
```

### Mach inversion (airdata.c)

```
Subsonic:   M = sqrt(5 x ((Pt/Ps)^(2/7) - 1))   [analytic]
Supersonic: Rayleigh bisection                  [numeric, ~20 iters]
```

### Variable geometry wings (cadc.c)

```
M < 0.40          -> sweep = 20 deg
0.40 <= M < 0.80  -> sweep = lerp(M, 0.40, 0.80, 20 deg, 35 deg)
0.80 <= M < 1.00  -> sweep = lerp(M, 0.80, 1.00, 35 deg, 50 deg)
1.00 <= M <= 2.34 -> sweep = lerp(M, 1.00, 2.34, 50 deg, 68 deg)
```

---

## Future extensions

Future work is governed by the staged refactoring roadmap rather than by ad-hoc feature additions. Candidate capabilities include a closed-loop aircraft model, explicit actuator dynamics, virtual buses, fault injection, a graphical cockpit, and a hardware-in-the-loop adapter.

---

## References

- [FAA AC 20-115D](https://www.faa.gov/regulations_policies/advisory_circulars/index.cfm/go/document.information/documentID/1032046) - FAA guidance recognizing RTCA DO-178C/ED-12C as an acceptable means for airborne software assurance; not a regulation and not a certification of this project.
- [RTCA DO-178C overview](https://www.rtca.org/do-178/) - scope and relationship of DO-178C to airborne software assurance.
- [U.S. Standard Atmosphere 1976](https://ntrs.nasa.gov/archive/nasa%20/casi.ntrs.nasa.gov/19770009539.pdf) - NASA/NOAA/USAF technical reference for the standard-atmosphere model.
- [F-14A NATOPS catalog record](https://sova.si.edu/record/nasm.2018.0021) - Smithsonian catalog record for a historical F-14A NATOPS manual artifact.
- **F-14 NATOPS and technical manuals** - use variant-specific manuals for any claim about aircraft limits, CADC behavior, or wing-sweep schedules; values in this repository must be treated as model inputs until traced to a specific document and revision.
- *Introduction to Flight* - John D. Anderson (7th ed.)
- *Modern Compressible Flow* - John D. Anderson
- *Avionics Navigation Systems* - Kayton and Fried (2nd ed.)
- *Optimal State Estimation* - Dan Simon
- Reference source code: FlightGear `FGAtmosphere` module (GPL)
