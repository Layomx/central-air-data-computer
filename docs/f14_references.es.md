# Referencia tecnica - F-14 Tomcat y su CADC

Idioma: Español | [English](f14_references.md)

Contexto historico y tecnico del sistema que este proyecto simula.

<img src="https://www.airdatanews.com/wp-content/uploads/2020/12/f-14-vf-211.jpg" alt="F-14 Tomcat" width="800"/>

---

## El F-14 Tomcat

El **Grumman F-14 Tomcat** fue el caza de superioridad aerea de la Marina de los Estados Unidos desde 1974 hasta su retiro en 2006. Fue diseniado para interceptar amenazas de largo alcance, equipado con radar AWG-9 y misiles AIM-54 Phoenix, con capacidad de engagement a mas de 160 km.

**Rendimiento destacado:**

| Parametro | Valor |
|-----------|-------|
| Velocidad maxima | Mach 2.34 (~2485 km/h a altitud) |
| Techo operativo | ~18,300 m (60,000 ft) |
| Alcance | ~2960 km (con tanques externos) |
| Propulsion | 2x Pratt and Whitney TF30 (F-14A) / GE F110 (F-14D) |
| Tripulacion | Piloto + Oficial de Intercepcion por Radar (RIO) |

Lo que distinguia al F-14 era su sistema de **alas de geometria variable**, de los primeros barridos variables controlados por computador en un caza operacional.

---

## Sistema de alas variables (AWG)

Las alas del F-14 pueden barrer entre **20 deg y 68 deg** en vuelo (75 deg para almacenamiento en cubierta). A diferencia de aviones anteriores como el F-111, donde el control podia ser manual, el F-14 usaba el sistema **AWG (Automatic Wing Geometry)** para posicionar alas automaticamente desde datos del CADC.

**Por que mejora el rendimiento:**

- **Alas extendidas (20 deg):** mayor area efectiva, mejor sustentacion a baja velocidad y menor velocidad de aproximacion para aterrizajes en portaaviones.
- **Alas barridas (68 deg):** menor componente normal de velocidad en borde de ataque, retrasando efectos de compresibilidad y reduciendo resistencia de onda.
- **Zona transonica (M 0.8-1.2):** transicion suave de barrido para mejorar controlabilidad en una region critica.

---

## CADC: Central Air Data Computer

### Historia del hardware

El CADC original del F-14A era un sistema **hibrido analogico-digital** fabricado por Lear Siegler. A diferencia de ADC modernos, muchas ecuaciones se implementaban en hardware:

- **Transductores de presion:** convertian Pt y Ps a voltajes proporcionales.
- **Amplificadores operacionales:** sumas y escalados analogicos.
- **Multiplicadores analogicos de cuatro cuadrantes:** terminos como M^2.
- **Potenciometros de funcion:** comportamiento de lookup no lineal.
- **Circuitos elevadores:** leyes de potencia con etapas analogicas en cascada.

La salida era analogica continua hacia servos de actuadores y panel de cabina, sin etapa software intermedia en el camino de control.

### Tasa de actualizacion

El hardware original operaba de forma continua con tasas efectivas de **50 Hz o mas**, con latencia en microsegundos.

### Parametros calculados

El CADC calculaba y distribuia:

- **Mach** -> AWG, referencias de armas e indicador de Mach
- **CAS / IAS** -> indicacion de velocidad y logica de limites
- **Altitud barometrica** -> altimetro y funciones automaticas
- **VSI** -> variometro
- **Temperatura exterior (OAT)** -> indicador y correcciones asociadas
- **Presion dinamica (q)** -> programacion de ganancias de control de vuelo

### Redundancia

La arquitectura F-14 tenia rutas de datos de aire parcialmente redundantes. El CADC primario alimentaba la mayoria de sistemas; una ruta secundaria respaldaba indicadores criticos.

---

## Parametros operativos relevantes para la simulacion

### Velocidades de referencia

| Designacion | Valor | Descripcion |
|-------------|-------|-------------|
| VLO | 300 kt CAS | Velocidad maxima con tren abajo |
| VFE (20 deg) | 225 kt CAS | Velocidad maxima con flap de despegue |
| VA | 350 kt CAS | Velocidad de maniobra de diseno |
| VMO | 777 kt CAS | Velocidad operativa maxima |
| MMO | 2.34 | Mach operativo maximo |

### Limites de altitud

| Limite | Valor |
|--------|-------|
| Techo operativo | 60,000 ft (18,288 m) |
| Altitud maxima de prueba | >60,000 ft (valor publico no especificado) |

### Posiciones de barrido

| Posicion | Angulo | Uso |
|----------|--------|-----|
| Extendidas | 20 deg | Despegue, aterrizaje, baja velocidad |
| Crucero subsonico | ~35 deg | Crucero bajo M 0.8 |
| Transonico | ~50 deg | Alrededor de M 1.0 |
| Supersonico | 50-68 deg | Sobre M 1.0 |
| Maximo | 68 deg | Velocidad maxima (M 2.34) |
| Almacenamiento | 75 deg | Cubierta de portaaviones |

---

## Diferencias frente al hardware real

Esta es una simulacion educativa. Diferencias principales:

**Implementacion:**
- Real: circuiteria analogica y elementos hardware de funcion
- Simulacion: ecuaciones en punto flotante de 64 bits en C estandar

**Precision:**
- Real: alrededor de 0.5% en Mach y ~100 ft en altitud
- Simulacion: menor a 0.001% en Mach y menor a 1 m en altitud bajo condiciones ideales

**Modelo de temperatura:**
- Real: sonda TAT y correccion de recuperacion
- Simulacion: temperatura estatica ISA directa

**Ruido y latencia:**
- Real: ruido correlacionado y dinamica de hardware
- Simulacion: ruido gaussiano no correlacionado, sin latencia explicita

**Histeresis de alas:**
- Real: logica de histeresis para evitar oscilaciones
- Simulacion: no modelada

**Redundancia:**
- Real: dual-path con logica de fallo
- Simulacion: camino unico con deteccion de fallos

---

## Fuentes de referencia

El proyecto se basa en fuentes abiertas e historicamente documentadas:

- **NATOPS Flight Manual F-14A/B/D** - manuales oficiales de la U.S. Navy (versiones desclasificadas)
- **Grumman Design Reports** - reportes tecnicos disponibles en archivos DTIC
- **AIAA Journal of Aircraft** - papers tecnicos 1970-1985 sobre alas variables y aerodinamica de alta velocidad
- **U.S. Standard Atmosphere 1976** - documento publico NOAA/NASA/USAF

La fisica atmosferica, las relaciones pitot y las ecuaciones de velocidad son fundamentos publicos de aerodinamica.
