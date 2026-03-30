# Technical reference - F-14 Tomcat and its CADC

Language: English | [Espanol](f14_referencias.es.md)

Historical and technical context for the system modeled in this project.

<img src="https://www.airdatanews.com/wp-content/uploads/2020/12/f-14-vf-211.jpg" alt="F-14 Tomcat" width="800"/>

---

## The F-14 Tomcat

The **Grumman F-14 Tomcat** served as the U.S. Navy air-superiority fighter from 1974 until retirement in 2006. It was designed to intercept long-range threats and was equipped with the AWG-9 radar and AIM-54 Phoenix missile system, with engagement capability beyond 160 km.

**Performance highlights:**

| Parameter | Value |
|-----------|-------|
| Maximum speed | Mach 2.34 (~2485 km/h at altitude) |
| Service ceiling | ~18,300 m (60,000 ft) |
| Range | ~2960 km (with external tanks) |
| Propulsion | 2x Pratt and Whitney TF30 (F-14A) / GE F110 (F-14D) |
| Crew | Pilot + Radar Intercept Officer (RIO) |

What made the F-14 stand out was its **variable-sweep wing** system, one of the first computer-controlled variable sweep implementations on an operational fighter.

---

## Variable wing geometry (AWG)

The F-14 wings sweep from **20 deg to 68 deg** in flight (75 deg for carrier deck storage). Unlike earlier aircraft such as the F-111, where sweep control was often manual, the F-14 used the **AWG (Automatic Wing Geometry)** system to command wing position automatically from CADC data.

**Why variable sweep improves performance:**

- **Extended wings (20 deg):** Higher effective wing area gives better low-speed lift and lower approach speed for carrier landings.
- **Swept wings (68 deg):** A larger sweep angle reduces normal flow component at the leading edge, delaying compressibility effects and reducing wave drag at high Mach.
- **Transonic region (M 0.8-1.2):** Smooth sweep transition improves controllability through a regime where shock behavior changes quickly.

---

## CADC: Central Air Data Computer

### Hardware history

The original F-14A CADC was a **hybrid analog-digital** system manufactured by Lear Siegler. Unlike modern ADC computers that run software on digital processors, many CADC equations were physically realized in hardware:

- **Pressure transducers:** Converted Pt and Ps to proportional voltage signals.
- **Operational amplifiers:** Performed analog summation and scaling.
- **Four-quadrant analog multipliers:** Implemented terms such as M^2.
- **Function potentiometers:** Hardware lookup behavior for nonlinear mappings.
- **Exponentiation circuits:** Implemented power-law relations using cascaded analog stages.

The output was continuous analog data feeding wing actuator servos and cockpit indicators, with no software processing stage in the core control path.

### Update rate

The original hardware operated continuously at effective rates of **50 Hz or higher**, with analog response latency in microseconds rather than milliseconds.

### Computed outputs

The CADC computed and distributed:

- **Mach** -> AWG, weapon system references, cockpit Mach indicator
- **CAS / IAS** -> cockpit airspeed indication and envelope limiting logic
- **Barometric altitude** -> altimeter and automatic flight functions
- **VSI** -> vertical speed indication
- **Outside air temperature (OAT)** -> temperature gauge and fuel-related corrections
- **Dynamic pressure (q)** -> flight-control gain scheduling

### Redundancy

The F-14 architecture included partially redundant air data paths. The primary CADC fed most systems, while a secondary and simpler ADC path supported critical cockpit indications.

---

## Operating parameters relevant to this simulation

### Reference speeds

| Designation | Value | Description |
|-------------|-------|-------------|
| VLO | 300 kt CAS | Max speed with landing gear down |
| VFE (20 deg) | 225 kt CAS | Max speed with takeoff flap setting |
| VA | 350 kt CAS | Design maneuvering speed |
| VMO | 777 kt CAS | Maximum operating speed |
| MMO | 2.34 | Maximum operating Mach |

### Altitude limits

| Limit | Value |
|------|-------|
| Service ceiling | 60,000 ft (18,288 m) |
| Maximum test altitude | >60,000 ft (public value not specified) |

### Wing sweep positions

| Position | Angle | Use |
|----------|-------|-----|
| Extended | 20 deg | Takeoff, landing, low-speed maneuvering |
| Subsonic cruise | ~35 deg | Cruise below M 0.8 |
| Transonic | ~50 deg | Around M 1.0 |
| Supersonic | 50-68 deg | Above M 1.0, increases with Mach |
| Maximum | 68 deg | Maximum speed (M 2.34) |
| Storage | 75 deg | Carrier deck storage |

---

## Differences versus real hardware

This project is an educational simulation. Main differences from the original CADC:

**Implementation:**
- Real: analog circuitry, hardware function elements, synchro outputs
- Simulation: 64-bit floating-point equations in standard C

**Accuracy:**
- Real: approximately 0.5% Mach error and around 100 ft altitude uncertainty
- Simulation: below 0.001% Mach error and below 1 m altitude error (ideal-model conditions)

**Temperature model:**
- Real: TAT probes measure total temperature, CADC applies recovery correction
- Simulation: uses ISA static temperature directly (intentional simplification)

**Noise and latency:**
- Real: correlated sensor noise and hardware dynamics
- Simulation: uncorrelated Gaussian noise, no explicit latency model

**Wing hysteresis:**
- Real: hysteresis logic to prevent sweep cycling near thresholds
- Simulation: no hysteresis model

**Redundancy behavior:**
- Real: dual-path logic with failure handling
- Simulation: single-path model with fault detection only

---

## Reference sources

The project is based on open and historically documented sources:

- **NATOPS Flight Manual F-14A/B/D** - official U.S. Navy flight manuals (declassified versions)
- **Grumman Design Reports** - F-14 technical reports available through DTIC archives
- **AIAA Journal of Aircraft** - papers from the 1970-1985 period on variable wing control and high-speed aerodynamics
- **U.S. Standard Atmosphere 1976** - NOAA/NASA/USAF public document

Atmospheric physics, pitot relations, and airspeed equations are public-domain aerodynamics fundamentals and can be found in standard university-level references.