# Technical reference - F-14 Tomcat and its CADC

Documentation language: English

Historical and technical context for the system modeled in this project. Each statement should be read as either a cited fact, an engineering assumption, or an implementation approximation; those categories are not interchangeable.

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

The F-14 is documented as a variable-sweep aircraft. The exact historical priority claim (“one of the first”) is not needed by the simulator and is intentionally not used as a technical requirement.

---

## Variable wing geometry (AWG)

The F-14 wings sweep from **20 deg to 68 deg** in flight (75 deg for carrier deck storage). Unlike earlier aircraft such as the F-111, where sweep control was often manual, the F-14 used the **AWG (Automatic Wing Geometry)** system to command wing position automatically from CADC data.

**Why variable sweep improves performance:**

- **Extended wings (20 deg):** Higher effective wing area gives better low-speed lift and lower approach speed for carrier landings.
- **Swept wings (68 deg):** A larger sweep angle reduces normal flow component at the leading edge, delaying compressibility effects and reducing wave drag at high Mach.
- **Transonic region (M 0.8-1.2):** Smooth sweep transition improves controllability through a regime where shock behavior changes quickly.

---

## CADC: Central Air Data Computer

### Hardware history and evidence boundary

Public descriptions identify the F-14 CADC as a hybrid analog/digital air-data system. The repository does not have sufficient primary evidence to assert every component, manufacturer detail, internal signal path, or calibration circuit listed in older versions of this document. The following items are therefore treated as historical hypotheses until tied to a specific manual, drawing, or technical report:

- pressure transducers and signal-conditioning stages;
- analog computation and function-generation stages;
- digital processing elements in variants that used them;
- continuous outputs to aircraft indication and control systems.

This project models functional relationships, not the original circuit implementation. The claim that a particular output path or component existed must be supported by a variant-specific primary source before it becomes a project requirement.

### Timing statement

The current repository does not establish a verified original CADC update rate or latency budget. The simulator uses a caller-selected `dt`; the `50 Hz` value previously stated in this document is removed until supported by a primary source.

### Computed outputs

The project models the following candidate air-data outputs:

- **Mach** -> AWG, weapon system references, cockpit Mach indicator
- **CAS / IAS** -> cockpit airspeed indication and envelope limiting logic
- **Barometric altitude** -> altimeter and automatic flight functions
- **VSI** -> vertical speed indication
- **Outside air temperature (OAT)** -> temperature gauge and fuel-related corrections
- **Dynamic pressure (q)** -> flight-control gain scheduling

### Redundancy and variant differences

Redundancy, backup paths, and variant differences require source-specific verification. They are not implemented by this repository and must not be inferred from the single-path simulator.

---

## Operating parameters currently used by this simulation

The following values are model inputs currently used by the simulator. They are not automatically certified aircraft limits. Each value must be traced to a specific F-14 variant, manual revision, and operating context before being promoted to a software requirement.

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

This project is an educational simulation. Main differences from any original aircraft implementation:

**Implementation:**
- Real: analog circuitry, hardware function elements, synchro outputs
- Simulation: 64-bit floating-point equations in standard C

**Accuracy:**
- Aircraft accuracy figures are not claimed without a source-specific calibration document.
- Simulation: approximately 1e-6 Mach inversion tolerance and sub-meter ideal-model altitude inversion in the tested range; this is numerical model behavior, not aircraft accuracy.

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

- [F-14A NATOPS manual catalog record](https://sova.si.edu/record/nasm.2018.0021) - Smithsonian Institution record for a historical F-14A NATOPS manual.
- [NAVAIR F-14 manual records](https://www.secnav.navy.mil/foia/readingroom/CaseFiles/FOIA%20Logs/DO%20FOIA%20Logs%20FY%202014%20_Redacted.pdf) - Navy FOIA log identifying F-14A, F-14B, and F-14D NATOPS manual identifiers.
- **Grumman Design Reports** - F-14 technical reports available through DTIC archives
- **AIAA Journal of Aircraft** - papers from the 1970-1985 period on variable wing control and high-speed aerodynamics
- [U.S. Standard Atmosphere 1976](https://ntrs.nasa.gov/archive/nasa%20/casi.ntrs.nasa.gov/19770009539.pdf) - NOAA/NASA/USAF public technical document.
- [FAA AC 20-115D](https://www.faa.gov/regulations_policies/advisory_circulars/index.cfm/go/document.information/documentID/1032046) - software-assurance context for the future refactoring; it is not an F-14 technical source.

Atmospheric physics, pitot relations, and airspeed equations are public-domain aerodynamics fundamentals and can be found in standard university-level references.
