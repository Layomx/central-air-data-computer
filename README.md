# Central Air Data Computer F-14 Tomcat

Language: English | [Español](README.es.md)

I have always loved aviation, especially military aircraft, and the F-14 Tomcat has always been my all-time favorite. After learning more about the history of its digital air data computer and seeing how much technical documentation is now public, I built this project as an educational simulation of the **Central Air Data Computer (CADC)** used by the F-14.

<img src="https://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/F-14_Tomcat_DF-SD-06-03497.jpg/1920px-F-14_Tomcat_DF-SD-06-03497.jpg" alt="F-14 Tomcat" width="800"/>

The project reproduces the functional CADC architecture: it takes simulated sensor inputs (pitot pressure, static pressure, and temperature), processes them with physics-based models, and produces aerodynamic parameters used by the cockpit instruments, flight control logic, and variable-sweep wing system.

## Documentation

- Architecture: [English](docs/architecture.md) | [Español](docs/architecture.es.md)
- F-14 technical reference: [English](docs/f14_references.md) | [Español](docs/f14_references.es.md)
- Implemented physics: [English](docs/physics.md) | [Español](docs/physics.es.md)

---

## Historical context

The **F-14 Tomcat** (Grumman, 1970) was one of the earliest naval fighters with computer-controlled variable geometry wings. The CADC made that automation possible: it received pressure and temperature measurements, computed Mach in real time, and sent that information to hydraulic systems that positioned the wings.

The original CADC was a hybrid analog-digital computer. Instead of running modern software, many equations were implemented with analog circuits, operational amplifiers, and physically programmed function components. This project reimplements the same core logic in modern C with 64-bit floating-point precision for educational use.

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
git clone https://github.com/tu-usuario/cadc-f14-simulation
cd cadc-f14-simulation

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

Future work may include additional CADC behavior that was not modeled yet due to complexity, more efficient numerical methods for selected routines, and a real-time graphical cockpit panel that moves beyond pure console simulation.

---

## References

- **NATOPS Flight Manual F-14A/B/D** - Grumman/US Navy (declassified)
- **U.S. Standard Atmosphere 1976** - NOAA/NASA/USAF
- *Introduction to Flight* - John D. Anderson (7th ed.)
- *Modern Compressible Flow* - John D. Anderson
- *Avionics Navigation Systems* - Kayton and Fried (2nd ed.)
- *Optimal State Estimation* - Dan Simon
- Reference source code: FlightGear `FGAtmosphere` module (GPL)
