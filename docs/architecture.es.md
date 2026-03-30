# Arquitectura del sistema

Idioma: Español | [English](arquitectura.md)

## Vision general

El proyecto esta dividido en seis modulos con dependencias estrictamente unidireccionales. Ningun modulo inferior conoce la existencia de los modulos superiores.

```
main.c
  -> logger.h/.c
       -> cadc.h/.c
            -> airdata.h/.c
                 -> sensors.h/.c
                      -> atmosphere.h/.c
```

Esta jerarquia intenta replicar la arquitectura del CADC real: los subsistemas de bajo nivel (atmosfera, sensores) son completamente independientes del sistema integrador de alto nivel.

---

## Modulos y responsabilidades

### Modulo 1 - atmosphere.h/.c

**Responsabilidad:** Modelo de atmosfera estandar ISA.

**Entradas:** altitud en metros `[0, 20000]`

**Salidas:** `AtmosphereState` con temperatura, presion, densidad, velocidad del sonido

**Dependencias:** ninguna (modulo base)

**Por que existe como modulo separado:** La atmosfera ISA se consulta tanto desde `sensors.c` (para generar presiones fisicamente consistentes) como desde `airdata.c` (para invertir altitud a partir de presion). Separarlo evita duplicar ecuaciones.

---

### Modulo 2 - sensors.h/.c

**Responsabilidad:** Simulacion de los tres sensores fisicos del CADC.

**Entradas:** `SensorConfig` (perfil de vuelo y configuracion de ruido)

**Salidas:** `SensorReading` con `Pt`, `Ps`, `T` (con ruido gaussiano opcional)

**Dependencias:** `atmosphere.h`

**Decision de diseno:** En lugar de generar valores de presion arbitrarios, el modulo recibe un perfil de vuelo (altitud + Mach en el tiempo) y deriva presiones fisicamente correctas mediante ISA. Esto garantiza coherencia entre `Pt`, `Ps` y `T`.

**Ruido gaussiano:** Implementado con Box-Muller usando solo `rand()` de C estandar.

---

### Modulo 3 - airdata.h/.c

**Responsabilidad:** Nucleo computacional - inversion de las ecuaciones de pitot.

**Entradas:** `SensorReading` (`Pt`, `Ps`, `T`)

**Salidas:** `AirDataState` con Mach, TAS, CAS, EAS, altitud barometrica, VSI, densidad

**Dependencias:** `sensors.h`, `atmosphere.h`

**Decision de diseno critica:** La inversion supersónica de Mach usa biseccion numerica en lugar de Newton-Raphson por robustez: la biseccion no diverge con ruido.

**Nota del bug de CAS:** Una version anterior usaba `qc = Pt - P0`, que producia `CAS = 0` en altitud. La correccion es `qc = Pt - Ps`, que coincide con la definicion fisica.

---

### Modulo 4 - cadc.h/.c

**Responsabilidad:** Integrador central - conecta todos los modulos y alimenta subsistemas.

**Entradas:** `CadcConfig`, tiempo simulado

**Salidas:** `CadcState` completo (airdata + estado de alas + alertas)

**Dependencias:** `airdata.h`, `sensors.h`

**Ciclo de calculo:**
```
1. Calcular dt
2. Leer sensores
3. Calcular airdata
4. Actualizar angulo de barrido
5. Evaluar alertas
6. Logging opcional
7. Incrementar ciclo
```

**Modelo de alas:** Implementa la logica AWG (Automatic Wing Geometry) del F-14 con cuatro zonas de Mach e interpolacion lineal. Esta version no modela histeresis.

---

### Modulo 5 - logger.h/.c

**Responsabilidad:** Instrumentacion - exportacion CSV, validacion fisica y diagnostico de sensores.

**Entradas:** `CadcState` en cada ciclo

**Salidas:** archivo `.csv`, diagnosticos y resumen estadistico

**Dependencias:** `cadc.h`

**Dos niveles de verificacion:**

| Nivel | Modulo | Pregunta |
|-------|--------|----------|
| Limites operativos | `cadc.c` | Esta el avion dentro de su envolvente operativa? |
| Coherencia fisica | `logger.c` | Son fisicamente plausibles estos datos? |

El logger detecta condiciones como `CAS > TAS` (imposible fisicamente) que no siempre forman parte de las alertas operativas del CADC.

**Deteccion de fallos:** Usa contadores de lecturas anormales consecutivas por sensor. Un outlier puntual puede ser ruido; multiples consecutivos indican fallo probable.

---

### Modulo 6 - main.c

**Responsabilidad:** Punto de entrada, argumentos CLI y bucle principal.

**Entradas:** argumentos de linea de comandos

**Salidas:** tabla de vuelo en terminal y resumen final

**Dependencias:** `logger.h`, `cadc.h`

**Sin logica de dominio.** La fisica, los calculos y la validacion viven en los modulos anteriores. `main.c` solo orquesta.

---

## Convenciones de codigo

**Unidades:** Todo el calculo interno usa SI (m, Pa, K, m/s). Las conversiones a unidades aeronauticas (ft, kt, ft/min) se hacen solo en la capa de salida.

**Estructuras de salida:** Cada modulo expone una estructura de resultados. Esto facilita extender campos sin romper firmas existentes.

**Funciones puras vs con estado:** Las funciones de calculo (`airdata_calc_mach`, `cadc_calc_wing_sweep`) son puras. Las funciones con estado (`cadc_update`, `logger_log_cycle`) reciben y modifican estructuras explicitamente.

**Clamping defensivo:** Todos los modulos limitan entradas antes de calcular para evitar propagacion de `NaN` o `Inf`.

**Headers como contrato:** Cada `.h` define la API publica. Los `.c` pueden evolucionar internamente sin romper consumidores, mientras respeten firmas y comportamiento documentado.
