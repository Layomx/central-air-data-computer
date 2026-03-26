# Central Air Data Computer F-14 Tomcat

Me encanta la aviacion, sobretodo las aeronaves tambien llamadas Aircraft, pero en particular, mi favorito para siempre y por siempre sera el F-14 Tomcat, y recientemente vi la historia de la creacion de su computadora digital y como actualmente mucha documentacion de como se realizo es abierta, lo que me permite replicar el sistema del aeronave de forma bastante simplificada y obteniendo simulación educativa del **Central Air Data Computer (CADC)** del F-14 Tomcat.

<img src="https://upload.wikimedia.org/wikipedia/commons/thumb/3/3d/F-14_Tomcat_DF-SD-06-03497.jpg/1920px-F-14_Tomcat_DF-SD-06-03497.jpg" alt="F-14 Tomcat" width="800"/>


El proyecto replica la arquitectura funcional del CADC real: toma datos de sensores simulados (presión de pitot, presión estática, temperatura), procesa esos datos como lo haría el hardware original, y produce los parámetros aerodinámicos que alimentaban la cabina, el sistema de control de vuelo, y las alas de geometría variable del F-14.

---

## Contexto histórico

El **F-14 Tomcat** (Grumman, 1970) fue el primer caza naval con alas de geometría variable controladas por computador. El CADC era el sistema que hacía posible esa automatización: recibía las mediciones de los sensores de presión y temperatura, calculaba el número de Mach en tiempo real, y enviaba esa información a los actuadores hidráulicos que movían las alas. El CADC ademas de eso, contaba con lo que se considera hoy en dia el primer microprocesador de la historia, por supuesto, esto era informacion clasificada en su momento. 

El CADC real era un computador analógico-digital híbrido. Las ecuaciones matemáticas no se ejecutaban en software, sino que se implementaban en circuitos de amplificadores operacionales y potenciómetros programados físicamente. Este proyecto reimplementa esa misma lógica en C moderno, con precisión de punto flotante de 64 bits que supera la del hardware original, he cambiado personalmente "el enfoque" de como construi el CADC porque estaba utilizando francamente tecnologia moderna, en su momento, se utilizaban otros metodos que no utilice en mi codigo para calcular muchas de las variables y los parametros ya sea por complejidad, eficiencia u otros motivos documentados en codigo.

---

## Qué calcula este proyecto

A partir de tres mediciones de sensor (`Pt`, `Ps`, `T`), el sistema produce:

| Parámetro | Descripción | Unidades |
|-----------|-------------|----------|
| **Mach** | Inversión de la ecuación de pitot / Rayleigh | adimensional |
| **TAS** | True Airspeed | kt / m/s |
| **CAS** | Calibrated Airspeed | kt |
| **EAS** | Equivalent Airspeed | kt |
| **Altitud** | Altitud barométrica | ft / m |
| **VSI** | Variación vertical | ft/min |
| **Sweep** | Ángulo de barrido de alas | grados |
| **Alertas** | Overspeed, altitud máxima, sensor inválido | flags |

---

## Compilación

Requiere solo `gcc` y `make`. No hay dependencias externas.

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

**Plataformas probadas:** Linux (GCC 11+), macOS (Clang 14+), Windows (MinGW/GCC).

En Windows, compilar con el flag adicional para UTF-8 en terminal:

```bash
gcc -std=c11 -Wall -Wextra -I. \
    src/atmosphere.c src/sensors.c src/airdata.c \
    src/cadc.c src/logger.c main.c \
    -o cadc_f14 -lm
```

---

## Uso

```
./cadc_f14 [opciones]
```

### Opciones

| Opción | Descripción | Defecto |
|--------|-------------|---------|
| `--profile cruise` | Perfil de crucero completo (despegue → M1.4 → aterrizaje) | |
| `--profile intercept` | Intercepción agresiva hasta M2.34 | |
| `--profile custom` | Condición constante definida por `--alt` y `--mach` | |
| `--alt <m>` | Altitud en metros para perfil custom | `0` |
| `--mach <M>` | Mach para perfil custom | `0` |
| `--noise` | Activar ruido gaussiano en sensores (σ_P=15 Pa, σ_T=0.5 K) | off |
| `--seed <n>` | Semilla aleatoria para reproducibilidad | `42` |
| `--dt <s>` | Paso de tiempo de simulación | `1.0` |
| `--output <archivo>` | Ruta del archivo CSV de telemetría | `output/vuelo.csv` |
| `--no-csv` | No generar archivo CSV | |
| `--quiet` | Solo mostrar resumen final | |
| `--panel` | Mostrar panel completo en puntos clave del vuelo | |
| `--help` | Mostrar ayuda | |

### Ejemplos

```bash
# Crucero básico — genera output/vuelo.csv
./cadc_f14

# Intercepción con ruido gaussiano, paso de 0.5s
./cadc_f14 --profile intercept --noise --dt 0.5

# Condición fija de crucero típico del F-14
./cadc_f14 --profile custom --alt 9000 --mach 0.85

# Panel de instrumentos completo en momentos clave
./cadc_f14 --panel --dt 2.0

# Solo resumen estadístico, sin tabla de vuelo
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
─────────────────────────────────────────
Despegue             0        0    0.00
Ascenso inicial     30      500    0.30
Ascenso             120    3000    0.50
Crucero bajo        300    7000    0.72
Crucero subsónico   500    9500    0.85
Cruce del sonido    650   10500    1.05
Crucero supersónico 800   12000    1.40
Descenso           1000    8000    0.90
Aproximación       1200    1500    0.40
Aterrizaje         1350       0    0.20
```

### Intercepción (`--profile intercept`)

Simula una misión de intercepción de 1080 segundos con Mach máximo 2.34:

```
Fase              t[s]    Alt[m]    Mach
─────────────────────────────────────────
Despegue             0        0    0.00
Ascenso rápido      90     4000    0.70
M=1.0              180     9000    1.00
Aceleración        350    15000    2.10
Mach máximo        500    16000    2.34  ← alas a 68°
Post-combate       650    14000    1.80
Descenso           800     8000    0.90
Aterrizaje        1080        0    0.20
```

### Custom (`--profile custom`)

Condición estacionaria durante 60 segundos. Útil para verificar un punto de vuelo específico.

```bash
# Crucero típico F-14 a 30,000 ft
./cadc_f14 --profile custom --alt 9144 --mach 0.85

# Nivel del mar a baja velocidad
./cadc_f14 --profile custom --alt 0 --mach 0.3
```

---

## Salida CSV

El archivo CSV contiene 28 columnas por ciclo de  y se puede analizar con Python:

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
# Ejecuta todos los test
make tests

# Test individuales
./tests/test_atmosphere   
./tests/test_sensors     
./tests/test_airdata      
./tests/test_cadc         
./tests/test_logger       
```

Cada test incluye verificación automática de invariantes físicos:
- `EAS ≤ TAS` en todas las condiciones
- `TAS = Mach × a` en todas las condiciones  
- `sweep ∈ [20°, 68°]` en toda la envolvente
- `CAS ≥ 0` siempre

---

## Física implementada

### Modelo ISA (atmosphere.c)

Atmósfera Estándar Internacional para 0–20,000 m:

```
Troposfera (0–11,000 m):
  T(h) = 288.15 - 0.0065 × h
  P(h) = 101325 × (T/288.15)^5.2561

Estratosfera baja (11,000–20,000 m):
  T    = 216.65 K (isotermal)
  P(h) = 22632.1 × exp(-(h-11000)/6341.6)

Densidad:  ρ = P / (R × T)
Sonido:    a = sqrt(γ × R × T)
```

### Presión de pitot (sensors.c)

```
Subsónico (M < 1):
  Pt/Ps = (1 + 0.2 × M²)^3.5

Supersónico (M ≥ 1) — Ecuación de Rayleigh:
  Pt/Ps = [(γ+1)²M²/(4γM²-2(γ-1))]^(γ/(γ-1)) × (1-γ+2γM²)/(γ+1)
```

### Inversión de Mach (airdata.c)

```
Subsónico:   M = sqrt(5 × ((Pt/Ps)^(2/7) - 1))   [analítico]
Supersónico: bisección de Rayleigh                 [numérico, ~20 iter]
```

### Alas de geometría variable (cadc.c)

```
M < 0.40          → sweep = 20°
0.40 ≤ M < 0.80   → sweep = lerp(M, 0.40, 0.80, 20°, 35°)
0.80 ≤ M < 1.00   → sweep = lerp(M, 0.80, 1.00, 35°, 50°)
1.00 ≤ M ≤ 2.34   → sweep = lerp(M, 1.00, 2.34, 50°, 68°)
```

---

## Extensiones futuras

En el futuro, me gustaria añadir todas los aspectos tecnicos que ni utilice en el modelado debido a complejidad como tambien hacer metodos mas eficientes para ciertas funciones especificas del CADC. Una extension futura es el desarrollo de un panel grafico con la informacion a tiempo real, pasando de simular una computadora a quizas simular vuelo.

---

## Referencias

- **NATOPS Flight Manual F-14A/B/D** — Grumman/US Navy (desclasificado)
- **U.S. Standard Atmosphere 1976** — NOAA/NASA/USAF
- *Introduction to Flight* — John D. Anderson (7ª ed.)
- *Modern Compressible Flow* — John D. Anderson
- *Avionics Navigation Systems* — Kayton & Fried (2ª ed.)
- *Optimal State Estimation* — Dan Simon
- Código fuente de referencia: módulo `FGAtmosphere` de FlightGear (GPL)
