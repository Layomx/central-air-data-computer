# Implemented physics

Documentation language: English

Equations currently used by the project, with derivation context, implementation scope, and physical interpretation. These equations describe the simulator; they are not a complete reconstruction of the original CADC calibration hardware.

---

## 1. ISA standard atmosphere

### Troposphere (0-11,000 m)

Temperature decreases linearly with altitude because of adiabatic expansion:

```
T(h) = T0 - L*h

T0 = 288.15 K    (sea-level temperature)
L  = 0.0065 K/m  (lapse rate)
```

Pressure follows hydrostatic balance with the ideal gas law:

```
P(h) = P0 * (T(h)/T0)^(g/(R*L))

P0 = 101325 Pa
g  = 9.80665 m/s^2
R  = 287.058 J/(kg*K)

Exponent = g/(R*L) ~= 5.2561
```

### Lower stratosphere (11,000-20,000 m)

Temperature is constant (isothermal layer):

```
T = 216.65 K
```

Pressure decays exponentially:

```
P(h) = P_tp * exp(-(h - h_tp)/Hs)

h_tp = 11000 m
P_tp = 22632.1 Pa
Hs   = R*T_tp/g ~= 6341.6 m
```

### Derived properties

```
Density:        rho = P / (R*T)          [kg/m^3]
Speed of sound: a = sqrt(gamma*R*T)      [m/s], gamma = 1.4
```

### ISA inversion (altitude from pressure)

To recover altitude from static pressure Ps:

**Troposphere** (Ps > P_tp):
```
T = T0 * (Ps/P0)^(1/exponent)
h = (T0 - T)/L
```

**Stratosphere** (Ps <= P_tp):
```
h = h_tp - Hs * ln(Ps/P_tp)
```

---

## 2. Total pressure from pitot tube

### Subsonic regime (M < 1)

Flow remains isentropic up to stagnation:

```
Pt/Ps = (1 + (gamma-1)/2 * M^2)^(gamma/(gamma-1))
      = (1 + 0.2 * M^2)^3.5
```

Boundary checks:
- M = 0: Pt/Ps = 1.0
- M = 1: Pt/Ps = 1.893

### Supersonic regime (M >= 1): Rayleigh pitot relation

At supersonic speeds, a normal shock forms ahead of the pitot opening. The flow becomes subsonic after the shock and stagnates with irreversible total-pressure loss.

```
Pt/Ps = [(gamma+1)^2*M^2 / (4*gamma*M^2 - 2*(gamma-1))]^(gamma/(gamma-1)) * (1-gamma+2*gamma*M^2)/(gamma+1)
```

Continuity holds at M = 1 (both formulas give 1.893).

---

## 3. Mach inversion

CADC does not receive Mach directly. It computes Mach by inverting pitot relations from measured pressures.

### Subsonic case (Pt/Ps < 1.893)

Direct analytic inversion:

```
Pt/Ps = (1 + 0.2*M^2)^3.5

=> (Pt/Ps)^(2/7) = 1 + 0.2*M^2
=> M = sqrt(5 * ((Pt/Ps)^(2/7) - 1))
```

### Supersonic case (Pt/Ps >= 1.893)

Rayleigh inversion has no closed-form analytic solution. Solve numerically with **bisection** on [1.0, 5.0]:

```
Find M such that rayleigh_ratio(M) = measured Pt/Ps
```

Bisection converges in about 20 iterations for tolerance 1e-6. It is preferred here over Newton-Raphson because it is always convergent and robust to noise.

---

## 4. Airspeed definitions

### True Airspeed (TAS)

Aircraft speed relative to the air mass:

```
TAS = Mach * a(T)
    = Mach * sqrt(gamma*R*T)
```

Example: M = 1.4, T = 216.65 K -> a = 295.07 m/s -> TAS = 413.1 m/s = 803 kt

### Calibrated Airspeed (CAS) — current approximation

CAS is instrument-equivalent speed referenced to sea-level ISA conditions. It uses impact pressure `qc = Pt - Ps` normalized by P0:

```
CAS = a0 * sqrt(5 * ((qc/P0 + 1)^(2/7) - 1))

a0 = 340.294 m/s
P0 = 101325 Pa
```

At sea level, this approximation approaches TAS. At altitude, it is a sea-level-referenced pressure-speed estimate. Its supersonic behavior is not claimed to reproduce the complete aircraft instrument calibration.

### Equivalent Airspeed (EAS)

Speed that yields the same aerodynamic load at reference density:

```
EAS = TAS * sqrt(rho/rho0)

rho0 = 1.225 kg/m^3
```

Structural loads are related to dynamic pressure, which in the incompressible approximation scales with `0.5*rho*V^2`. The current implementation computes EAS from density scaling, but its `dynamic_pressure_Pa` field currently stores `Pt - Ps` (impact pressure), not the incompressible formula. This distinction is scheduled for correction in the refactoring.

### Relationship among speeds

In typical subsonic altitude conditions: **EAS <= CAS <= TAS**. This relationship is used as a diagnostic expectation, not as a universal validation rule for every compressible-flow regime.

Near sea level: EAS ~= CAS ~= TAS.

---

## 5. Vertical speed (VSI)

Rate of climb/descent from finite differences between simulation cycles:

```
VSI = (h_current - h_previous) / dt

Positive: climb
Negative: descent
```

Conversion: `VSI [ft/min] = VSI [m/s] * 196.850`

---

## 6. F-14 variable-sweep wing model

Target wing angle is computed with four Mach zones and linear interpolation:

```
M < 0.40:            sweep = 20 deg
0.40 <= M < 0.80:    sweep = lerp(M, 0.40, 0.80, 20 deg, 35 deg)
0.80 <= M < 1.00:    sweep = lerp(M, 0.80, 1.00, 35 deg, 50 deg)
1.00 <= M <= 2.34:   sweep = lerp(M, 1.00, 2.34, 50 deg, 68 deg)
```

Higher sweep angles reduce the normal velocity component at the leading edge, delaying compressibility effects and reducing wave drag.

Range: 20 deg (low speed/carrier ops) to 68 deg (max speed). Carrier storage position: 75 deg.

---

## 7. Gaussian sensor noise (Box-Muller)

Standard C provides `rand()` as a uniform generator. To obtain normal-distributed noise, use Box-Muller:

```
Given U1, U2 ~ Uniform(0, 1):
Z = sqrt(-2 * ln(U1)) * cos(2*pi*U2)

Then Z ~ Normal(0, 1)
```

For standard deviation sigma:
```
noisy_value = true_value + Z * sigma
```

Parameters used:
- Pressure sensor: sigma = 15.0 Pa
- Temperature sensor: sigma = 0.5 K

---

## Physical constants used

| Symbol | Value | Description |
|--------|-------|-------------|
| R | 287.058 J/(kg*K) | Specific gas constant for dry air |
| gamma | 1.4 | Ratio of specific heats |
| g | 9.80665 m/s^2 | Standard gravity |
| T0 | 288.15 K | ISA sea-level temperature |
| P0 | 101325 Pa | ISA sea-level pressure |
| rho0 | 1.225 kg/m^3 | ISA sea-level density |
| a0 | 340.294 m/s | Speed of sound at sea level |
| L | 0.0065 K/m | Tropospheric lapse rate |

---

## Accuracy and limitations

**ISA inversion accuracy:** below 0.2 m over 0-20,000 m in ideal model conditions.

**Mach inversion accuracy:** approximately 1e-6 in the ideal, bracketed model over the tested range M = 0 to M = 2.34.

**Model limitations:**
- Real atmosphere deviates from ISA due to weather, humidity, and regional pressure variation.
- White Gaussian noise is statistically useful but does not model real temporal correlation or sensor thermal/mechanical dynamics.
- Wing schedule is piecewise linear and does not include hysteresis logic found in real systems.
- The profile driver is open-loop: wing sweep does not feed back into aircraft acceleration, altitude, or aerodynamic forces.

## Source basis

The standard-atmosphere constants and layer model are based on the public [U.S. Standard Atmosphere 1976 technical report](https://ntrs.nasa.gov/archive/nasa%20/casi.ntrs.nasa.gov/19770009539.pdf). The project uses only a limited 0–20 km subset and geometric-altitude simplifications. The pitot relations are standard compressible-flow equations; aircraft-specific calibration and CADC implementation details are not fully established in this repository and must be treated as assumptions unless a variant-specific source is cited.
