# Arquitectura del sistema

## Visión general

El proyecto está dividido en seis módulos con dependencias estrictamente unidireccionales. Ningún módulo inferior conoce la existencia de los módulos superiores.

```
main.c
  └── logger.h/.c
        └── cadc.h/.c
              └── airdata.h/.c
                    └── sensors.h/.c
                          └── atmosphere.h/.c
```

Esta jerarquía intenta replicar la arquitectura del CADC real: los subsistemas de bajo nivel (atmósfera, sensores) son completamente independientes del sistema integrador de alto nivel.

---

## Módulos y responsabilidades

### Módulo 1 — `atmosphere.h/.c`

**Responsabilidad:** Modelo de atmósfera estándar ISA.

**Entradas:** altitud en metros `[0, 20000]`

**Salidas:** `AtmosphereState` con temperatura, presión, densidad, velocidad del sonido

**Dependencias:** ninguna (módulo base)

**Por qué existe como módulo separado:** La atmósfera ISA es consultada tanto desde `sensors.c` (para generar presiones físicamente consistentes) como desde `airdata.c` (para invertir altitud a partir de presión). Separarlo evita duplicar las ecuaciones.

---

### Módulo 2 — `sensors.h/.c`

**Responsabilidad:** Simulación de los tres sensores físicos del CADC.

**Entradas:** `SensorConfig` (perfil de vuelo, configuración de ruido)

**Salidas:** `SensorReading` con `Pt`, `Ps`, `T` (con ruido gaussiano opcional)

**Dependencias:** `atmosphere.h`

**Decisión de diseño:** En lugar de generar valores de presión arbitrarios, el módulo recibe un perfil de vuelo (altitud + Mach en el tiempo) y deriva las presiones físicamente correctas mediante el modelo ISA. Esto garantiza que `Pt`, `Ps` y `T` siempre sean coherentes entre sí.

**Ruido gaussiano:** Implementado con Box-Muller usando solo `rand()` de C estándar.

---

### Módulo 3 — `airdata.h/.c`

**Responsabilidad:** Núcleo computacional — inversión de las ecuaciones de pitot.

**Entradas:** `SensorReading` (`Pt`, `Ps`, `T`)

**Salidas:** `AirDataState` con Mach, TAS, CAS, EAS, altitud barométrica, VSI, densidad

**Dependencias:** `sensors.h`, `atmosphere.h`

**Decisión de diseño crítica:** La inversión de Mach supersónico usa bisección numérica en lugar de Newton-Raphson. La razón es robustez: la bisección no puede diverger aunque la lectura de sensor sea ruidosa. El software de aviónica certificado prefiere algoritmos conservadores sobre algoritmos rápidos pero frágiles.

**Corrección del bug de CAS:** La versión inicial calculaba `qc = Pt - P0`, que daba `CAS = 0` en altitud. La corrección usa `qc = Pt - Ps`, que es lo que mide el instrumento físicamente.

---

### Módulo 4 — `cadc.h/.c`

**Responsabilidad:** Integrador central — conecta todos los módulos y alimenta subsistemas.

**Entradas:** `CadcConfig` (configuración), tiempo simulado

**Salidas:** `CadcState` completo (airdata + estado de alas + alertas)

**Dependencias:** `airdata.h`, `sensors.h`

**Ciclo de calculos:**
```
1. Calcular dt
2. Leer sensores
3. Calcular airdata
4. Actualizar ángulo de barrido de alas
5. Evaluar alertas
6. Logging opcional
7. Incrementar ciclo
```

**Modelo de alas:** Implementa la lógica del sistema AWG (Automatic Wing Geometry) del F-14. El ángulo se calcula en cuatro zonas de Mach con interpolación lineal. No implementa histéresis (por simplificacion).

---

### Módulo 5 — `logger.h/.c`

**Responsabilidad:** Instrumentación — CSV, validación física, diagnóstico de sensores.

**Entradas:** `CadcState` en cada ciclo

**Salidas:** archivo `.csv`, mensajes de diagnóstico, resumen estadístico

**Dependencias:** `cadc.h`

**Dos niveles de verificación distintos:**

| Nivel | Módulo | Pregunta |
|-------|--------|----------|
| Límites operativos | `cadc.c` | ¿Está el avión dentro de su sobre de vuelo? |
| Coherencia física | `logger.c` | ¿Son estos datos físicamente posibles? |

La validación del logger detecta condiciones como `CAS > TAS` (imposible por definición) que el sistema de alertas del CADC no cubriría.

**Detección de fallos:** Usa contadores de ciclos consecutivos anómalos por sensor. Un valor malo puntual es ruido gaussiano normal. Diez valores anómalos consecutivos indican un sensor fallando.

---

### Módulo 6 — `main.c`

**Responsabilidad:** Punto de entrada, argumentos CLI, bucle principal.

**Entradas:** argumentos de línea de comandos

**Salidas:** tabla de vuelo en terminal, resumen final

**Dependencias:** `logger.h`, `cadc.h`

**No contiene lógica de dominio.** Toda la física, todos los cálculos, toda la validación están en los módulos anteriores. `main.c` solo conecta los modulos.

---

## Convenciones de código

**Unidades:** Todo el sistema usa SI internamente (metros, Pascales, Kelvin, m/s). Las conversiones a unidades de aviación (pies, nudos, ft/min) solo ocurren en funciones de display, nunca en cálculos intermedios.

**Estructuras de salida:** Cada módulo tiene una estructura que agrupa sus resultados. Pasar un puntero a esa estructura es la interfaz pública. Esto permite añadir campos sin romper las firmas de funciones existentes.

**Funciones puras vs con estado:** Las funciones de cálculo (`airdata_calc_mach`, `cadc_calc_wing_sweep`) son puras — no modifican estado global, no tienen efectos secundarios. Las funciones con estado (`cadc_update`, `logger_log_cycle`) reciben y modifican estructuras explícitamente. No hay variables globales.

**Clamping defensivo:** Todos los módulos aplican clamping en sus entradas antes de cualquier cálculo. Esto previene que valores inválidos (por ruido extremo o error de programación) propaguen `NaN` o `Inf` por el pipeline.

**Headers como contrato:** El `.h` define la interfaz pública completa. El `.c` puede cambiar su implementación libremente sin afectar ningún módulo consumidor, siempre que respete las firmas del `.h`.