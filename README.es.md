# Central Air Data Computer F-14 Tomcat

Idioma: Español | [English](README.md)

Me encanta la aviacion, sobretodo las aeronaves tambien llamadas Aircraft, pero en particular, mi favorito para siempre y por siempre sera el F-14 Tomcat, y recientemente vi la historia de la creacion de su computadora digital y como actualmente mucha documentacion de como se realizo es abierta, lo que me permite replicar el sistema del aeronave de forma bastante simplificada y obteniendo simulacion educativa del **Central Air Data Computer (CADC)** del F-14 Tomcat.

<img src="https://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/F-14_Tomcat_DF-SD-06-03497.jpg/1920px-F-14_Tomcat_DF-SD-06-03497.jpg" alt="F-14 Tomcat" width="800"/>

El proyecto replica la arquitectura funcional del CADC real: toma datos de sensores simulados (presion de pitot, presion estatica, temperatura), procesa esos datos como lo haria el hardware original, y produce los parametros aerodinamicos que alimentaban la cabina, el sistema de control de vuelo, y las alas de geometria variable del F-14.

## Documentacion

- Arquitectura: [Español](docs/architecture.es.md) | [English](docs/architecture.md)
- Referencia tecnica F-14: [Español](docs/f14_references.es.md) | [English](docs/f14_references.md)
- Fisica implementada: [Español](docs/physics.es.md) | [English](docs/physics.md)

---

## Contexto historico

El **F-14 Tomcat** (Grumman, 1970) fue el primer caza naval con alas de geometria variable controladas por computador. El CADC era el sistema que hacia posible esa automatizacion: recibia las mediciones de los sensores de presion y temperatura, calculaba el numero de Mach en tiempo real, y enviaba esa informacion a los actuadores hidraulicos que movian las alas. El CADC ademas de eso, contaba con lo que se considera hoy en dia el primer microprocesador de la historia, por supuesto, esto era informacion clasificada en su momento.

El CADC real era un computador analogico-digital hibrido. Las ecuaciones matematicas no se ejecutaban en software, sino que se implementaban en circuitos de amplificadores operacionales y potenciometros programados fisicamente. Este proyecto reimplementa esa misma logica en C moderno, con precision de punto flotante de 64 bits que supera la del hardware original.

---

## Que calcula este proyecto

A partir de tres mediciones de sensor (`Pt`, `Ps`, `T`), el sistema produce:

| Parametro | Descripcion | Unidades |
|-----------|-------------|----------|
| **Mach** | Inversion de la ecuacion de pitot / Rayleigh | adimensional |
| **TAS** | True Airspeed | kt / m/s |
| **CAS** | Calibrated Airspeed | kt |
| **EAS** | Equivalent Airspeed | kt |
| **Altitud** | Altitud barometrica | ft / m |
| **VSI** | Variacion vertical | ft/min |
| **Sweep** | Angulo de barrido de alas | grados |
| **Alertas** | Overspeed, altitud maxima, sensor invalido | flags |

---

## Compilacion

Requiere solo `gcc` y `make`. No hay dependencias externas.

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

**Plataformas probadas:** Linux (GCC 11+), macOS (Clang 14+), Windows (MinGW/GCC).

En Windows:

```bash
gcc -std=c11 -Wall -Wextra -I. \
    src/atmosphere.c src/sensors.c src/airdata.c \
    src/cadc.c src/logger.c main.c \
    -o cadc_f14 -lm
```

---

## Uso

```bash
./cadc_f14 [opciones]
```

### Opciones

| Opcion | Descripcion | Defecto |
|--------|-------------|---------|
| `--profile cruise` | Perfil de crucero completo (despegue -> M1.4 -> aterrizaje) | |
| `--profile intercept` | Intercepcion agresiva hasta M2.34 | |
| `--profile custom` | Condicion constante definida por `--alt` y `--mach` | |
| `--alt <m>` | Altitud en metros para perfil custom | `0` |
| `--mach <M>` | Mach para perfil custom | `0` |
| `--noise` | Activar ruido gaussiano en sensores (sigma_P=15 Pa, sigma_T=0.5 K) | off |
| `--seed <n>` | Semilla aleatoria para reproducibilidad | `42` |
| `--dt <s>` | Paso de tiempo de simulacion | `1.0` |
| `--output <archivo>` | Ruta del archivo CSV de telemetria | `output/vuelo.csv` |
| `--no-csv` | No generar archivo CSV | |
| `--quiet` | Solo mostrar resumen final | |
| `--panel` | Mostrar panel completo en puntos clave del vuelo | |
| `--help` | Mostrar ayuda | |

### Ejemplos

```bash
# Crucero basico - genera output/vuelo.csv
./cadc_f14

# Intercepcion con ruido gaussiano, paso de 0.5s
./cadc_f14 --profile intercept --noise --dt 0.5

# Condicion fija de crucero tipico del F-14
./cadc_f14 --profile custom --alt 9000 --mach 0.85

# Panel de instrumentos completo en momentos clave
./cadc_f14 --panel --dt 2.0

# Solo resumen estadistico
./cadc_f14 --quiet --output mi_vuelo.csv

# Semilla fija para resultados reproducibles
./cadc_f14 --profile intercept --noise --seed 1234
```

---

## Perfiles de vuelo

### Crucero (`--profile cruise`)

Simula un vuelo completo de 1400 segundos:

```
Fase              t[s]    Alt[m]    Mach
-----------------------------------------
Despegue             0        0    0.00
Ascenso inicial     30      500    0.30
Ascenso            120     3000    0.50
Crucero bajo       300     7000    0.72
Crucero subsonico  500     9500    0.85
Cruce del sonido   650    10500    1.05
Crucero supersonico 800   12000    1.40
Descenso          1000     8000    0.90
Aproximacion      1200     1500    0.40
Aterrizaje        1350        0    0.20
```

### Intercepcion (`--profile intercept`)

Simula una mision de intercepcion de 1080 segundos con Mach maximo 2.34:

```
Fase              t[s]    Alt[m]    Mach
-----------------------------------------
Despegue             0        0    0.00
Ascenso rapido      90     4000    0.70
M=1.0              180     9000    1.00
Aceleracion        350    15000    2.10
Mach maximo        500    16000    2.34  <- alas a 68 deg
Post-combate       650    14000    1.80
Descenso           800     8000    0.90
Aterrizaje        1080        0    0.20
```

### Custom (`--profile custom`)

Condicion estacionaria durante 60 segundos. Util para verificar un punto de vuelo especifico.

```bash
# Crucero tipico F-14 a 30,000 ft
./cadc_f14 --profile custom --alt 9144 --mach 0.85

# Nivel del mar a baja velocidad
./cadc_f14 --profile custom --alt 0 --mach 0.3
```

---

## Salida CSV

El archivo CSV contiene 28 columnas por ciclo y se puede analizar con Python:

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
# Ejecuta todos los tests
make tests

# Tests individuales
./tests/test_atmosphere
./tests/test_sensors
./tests/test_airdata
./tests/test_cadc
./tests/test_logger
```

Cada test incluye verificacion automatica de invariantes fisicos:
- `EAS <= TAS` en todas las condiciones
- `TAS = Mach x a` en todas las condiciones
- `sweep en [20 deg, 68 deg]` en toda la envolvente
- `CAS >= 0` siempre

---

## Fisica implementada

### Modelo ISA (atmosphere.c)

Atmosfera Estandar Internacional para 0-20,000 m:

```
Troposfera (0-11,000 m):
  T(h) = 288.15 - 0.0065 x h
  P(h) = 101325 x (T/288.15)^5.2561

Estratosfera baja (11,000-20,000 m):
  T    = 216.65 K (isotermal)
  P(h) = 22632.1 x exp(-(h-11000)/6341.6)

Densidad:  rho = P / (R x T)
Sonido:    a = sqrt(gamma x R x T)
```

### Presion de pitot (sensors.c)

```
Subsonico (M < 1):
  Pt/Ps = (1 + 0.2 x M^2)^3.5

Supersonico (M >= 1) - Ecuacion de Rayleigh:
  Pt/Ps = [(gamma+1)^2 M^2/(4 gamma M^2 - 2(gamma-1))]^(gamma/(gamma-1)) x (1-gamma+2 gamma M^2)/(gamma+1)
```

### Inversion de Mach (airdata.c)

```
Subsonico:   M = sqrt(5 x ((Pt/Ps)^(2/7) - 1))   [analitico]
Supersonico: biseccion de Rayleigh               [numerico, ~20 iter]
```

### Alas de geometria variable (cadc.c)

```
M < 0.40          -> sweep = 20 deg
0.40 <= M < 0.80  -> sweep = lerp(M, 0.40, 0.80, 20 deg, 35 deg)
0.80 <= M < 1.00  -> sweep = lerp(M, 0.80, 1.00, 35 deg, 50 deg)
1.00 <= M <= 2.34 -> sweep = lerp(M, 1.00, 2.34, 50 deg, 68 deg)
```

---

## Extensiones futuras

En el futuro, me gustaria anadir aspectos tecnicos que no se modelaron por complejidad, hacer metodos mas eficientes para funciones del CADC, y desarrollar un panel grafico en tiempo real.

---

## Referencias

- **NATOPS Flight Manual F-14A/B/D** - Grumman/US Navy (desclasificado)
- **U.S. Standard Atmosphere 1976** - NOAA/NASA/USAF
- *Introduction to Flight* - John D. Anderson (7a ed.)
- *Modern Compressible Flow* - John D. Anderson
- *Avionics Navigation Systems* - Kayton and Fried (2a ed.)
- *Optimal State Estimation* - Dan Simon
- Codigo fuente de referencia: modulo `FGAtmosphere` de FlightGear (GPL)
