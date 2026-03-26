# Referencia técnica — F-14 Tomcat y su CADC

Contexto histórico y técnico del sistema que este proyecto simula.

<img src="https://www.airdatanews.com/wp-content/uploads/2020/12/f-14-vf-211.jpg" alt="F-14 Tomcat" width="800"/>


---

## El F-14 Tomcat

El **Grumman F-14 Tomcat** fue el caza de superioridad aérea de la Marina de los Estados Unidos desde 1974 hasta su retiro en 2006. Fue diseñado específicamente para interceptar bombarderos soviéticos a larga distancia, equipado con el radar AWG-9 y el misil AIM-54 Phoenix, capaz de derribar blancos a más de 160 km.

**Características de rendimiento:**

| Parámetro | Valor |
|-----------|-------|
| Velocidad máxima | Mach 2.34 (~2485 km/h a altitud) |
| Techo operativo | ~18,300 m (60,000 ft) |
| Alcance | ~2960 km (con tanques externos) |
| Propulsión | 2× Pratt & Whitney TF30 (F-14A) / GE F110 (F-14D) |
| Tripulación | Piloto + Oficial de Sistemas de Armas (RIO) |

Lo que distinguía al F-14 de sus contemporáneos era su sistema de **alas de geometría variable** — las primeras alas de barrido variable controladas por computador en un caza operacional.

---

## El sistema de alas variables (AWG)

Las alas del F-14 podían barrer entre **20° y 68°** en vuelo (75° para almacenamiento en portaaviones). A diferencia de aviones anteriores como el F-111 donde el piloto controlaba el barrido manualmente, en el F-14 el sistema **AWG (Automatic Wing Geometry)** controlaba el barrido automáticamente basándose en la información del CADC.

**Por qué el barrido variable mejora el rendimiento:**

- **Alas extendidas (20°):** Mayor superficie alar → mayor sustentación a baja velocidad → velocidad de aproximación más baja para aterrizajes en portaaviones. El F-14 aterrizaba a ~135 kt con alas extendidas.

- **Alas barridas (68°):** El borde de ataque barrido reduce el componente de velocidad normal al ala, retrasando la aparición de ondas de choque. La resistencia de onda a M = 2.0 con 68° de barrido es significativamente menor que con 20°.

- **Zona transónica (M 0.8–1.2):** La transición suave del barrido en esta zona crítica reduce la inestabilidad de control que aparece cuando las ondas de choque se forman en diferentes puntos del ala.

---

## El CADC: Central Air Data Computer

### Historia del hardware

El CADC original del F-14A era un sistema **analógico-digital híbrido** fabricado por Lear Siegler. A diferencia de los CADC de generaciones posteriores que ejecutaban código en microprocesadores, el CADC del F-14 implementaba las ecuaciones matemáticas directamente en circuitos electrónicos:

- **Transductores de presión:** Convertían Pt y Ps en señales de voltaje proporcionales.
- **Amplificadores operacionales:** Implementaban multiplicaciones y sumas analógicamente.
- **Multiplicadores analógicos de cuatro cuadrantes:** Calculaban M².
- **Potenciómetros de función:** Memorias físicas que almacenaban tablas de lookup como curvas de resistencia.
- **Circuitos elevadores:** Calculaban potencias (M²^3.5) mediante amplificadores en cascada.

La salida era señal analógica continua que iba directamente a los servos de los actuadores de alas y a los synchros de los indicadores de cabina. No había conversión digital intermedia en el pipeline de control.

### Ciclo de actualización

El sistema operaba de forma continua a frecuencias efectivas de **50 Hz o superiores** — mucho más rápido que cualquier sistema basado en microprocesador de la época. La latencia del hardware analógico era de microsegundos, no milisegundos.

### Parámetros calculados

El CADC del F-14 calculaba y distribuía:

- **Mach** → AWG (alas), sistema de armas (solución de tiro), indicador de Mach en cabina
- **CAS / IAS** → velocímetro de cabina, computer de límites de vuelo
- **Altitud barométrica** → altímetro, control de vuelo automático (ACLS)
- **VSI** → variometro de cabina
- **Temperatura exterior (OAT)** → gauge de temperatura, correcciones de combustible
- **Presión dinámica (q)** → ganancias del sistema de control de vuelo (mayor q → reducir autoridad de mandos)

### Redundancia

El F-14 llevaba dos sistemas de datos de aire parcialmente redundantes. El CADC principal alimentaba la mayoría de los sistemas; un ADC secundario más simple servía como respaldo para los indicadores críticos de cabina.

---

## Parámetros operativos relevantes para la simulación

### Velocidades de referencia

| Designación | Valor | Descripción |
|-------------|-------|-------------|
| VLO | 300 kt CAS | Velocidad máxima con tren de aterrizaje bajado |
| VFE (20°) | 225 kt CAS | Velocidad máxima con flaps en despegue |
| VA | 350 kt CAS | Velocidad de maniobra (diseño) |
| VMO | 777 kt CAS | Velocidad operativa máxima |
| MMO | 2.34 | Mach operativo máximo |

### Límites de altitud

| Límite | Valor |
|--------|-------|
| Techo operativo | 60,000 ft (18,288 m) |
| Altitud máxima de prueba | >60,000 ft (no publicada) |

### Ángulos de barrido

| Posición | Ángulo | Uso |
|----------|--------|-----|
| Extendido | 20° | Despegue, aterrizaje, maniobra de baja velocidad |
| Crucero subsónico | ~35° | Crucero M < 0.8 |
| Transónico | ~50° | M ≈ 1.0 |
| Supersónico | 50°–68° | M > 1.0, varía con Mach |
| Máximo | 68° | Velocidad máxima (M = 2.34) |
| Almacenamiento | 75° | En cubierta de portaaviones |

---

## Diferencias con el hardware real

Este proyecto es una simulación educativa. Las diferencias con el CADC real son:

**Implementación:**
- Real: circuitos analógicos, tablas ROM de ferritas, synchros
- Simulación: ecuaciones en punto flotante de 64 bits, C estándar

**Precisión:**
- Real: ~0.5% de error en Mach, ~100 ft en altitud (limitado por tolerancias de componentes y deriva térmica)
- Simulación: error < 0.001% en Mach, < 1 m en altitud

**Modelo de temperatura:**
- Real: TAT probe mide temperatura total (de remanso), el CADC corrige por compresión adiabática para obtener temperatura estática
- Simulación: usa temperatura estática ISA directamente (simplificación)

**Ruido y latencia:**
- Real: ruido correlacionado temporalmente (inercia mecánica y térmica del sensor), latencia fija del hardware
- Simulación: ruido gaussiano blanco (no correlacionado), sin latencia modelada

**Histéresis en alas:**
- Real: lógica de histéresis para evitar ciclos de barrido en torno a umbrales de Mach
- Simulación: sin histéresis (simplificación documentada)

**Redundancia:**
- Real: dos sistemas con lógica de fallo activo
- Simulación: sistema único con detección de fallos sin conmutación automática

---

## Fuentes de referencia

Las siguientes fuentes fueron las que basicamente cimentaron el proyecto, todo lo que se menciona y se calcula proviene de estos documentos:

- **NATOPS Flight Manual F-14A/B/D** — Manual de vuelo oficial de la US Navy, múltiples versiones desclasificadas disponibles en archivos históricos
- **Grumman Design Report** — Reportes técnicos del programa F-14 disponibles en el sistema DTIC (Defense Technical Information Center)
- **AIAA Journal of Aircraft** — Artículos técnicos de los años 1970–1985 sobre el sistema de control de alas variables
- **U.S. Standard Atmosphere 1976** — NOAA/NASA/USAF, documento público y de acceso libre

La física de atmósfera, presiones de pitot, y velocidades es completamente pública y está documentada en cualquier texto de aerodinámica universitaria. Las ecuaciones implementadas son las mismas que usaban los ingenieros de Grumman en el diseño original.