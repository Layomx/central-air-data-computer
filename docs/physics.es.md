# Fisica implementada

Idioma: Espanol | [English](fisica.md)

Todas las ecuaciones implementadas en el proyecto, con su derivacion y contexto fisico.

---

## 1. Atmosfera estandar ISA

### Troposfera (0-11,000 m)

La temperatura decrece linealmente con la altitud por expansion adiabatica:

```
T(h) = T0 - L*h

T0 = 288.15 K
L  = 0.0065 K/m
```

La presion sigue el equilibrio hidrostatico junto con ley de gas ideal:

```
P(h) = P0 * (T(h)/T0)^(g/(R*L))

P0 = 101325 Pa
g  = 9.80665 m/s^2
R  = 287.058 J/(kg*K)

Exponente = g/(R*L) ~= 5.2561
```

### Estratosfera baja (11,000-20,000 m)

Temperatura constante (zona isotermal):

```
T = 216.65 K
```

Presion con decaimiento exponencial:

```
P(h) = P_tp * exp(-(h - h_tp)/Hs)

h_tp = 11000 m
P_tp = 22632.1 Pa
Hs   = R*T_tp/g ~= 6341.6 m
```

### Propiedades derivadas

```
Densidad:          rho = P / (R*T)      [kg/m^3]
Velocidad sonido:  a = sqrt(gamma*R*T)  [m/s], gamma = 1.4
```

### Inversion ISA (altitud desde presion)

Para recuperar altitud desde presion estatica Ps:

**Troposfera** (Ps > P_tp):
```
T = T0 * (Ps/P0)^(1/exponente)
h = (T0 - T)/L
```

**Estratosfera** (Ps <= P_tp):
```
h = h_tp - Hs * ln(Ps/P_tp)
```

---

## 2. Presion total de tubo pitot

### Regimen subsonico (M < 1)

Flujo isentropico hasta remanso:

```
Pt/Ps = (1 + (gamma-1)/2 * M^2)^(gamma/(gamma-1))
      = (1 + 0.2 * M^2)^3.5
```

Verificacion de borde:
- M = 0: Pt/Ps = 1.0
- M = 1: Pt/Ps = 1.893

### Regimen supersonico (M >= 1): relacion pitot de Rayleigh

A velocidad supersonica se forma una onda de choque normal frente al pitot. El flujo pasa a subsonico y pierde presion total de forma irreversible.

```
Pt/Ps = [(gamma+1)^2*M^2 / (4*gamma*M^2 - 2*(gamma-1))]^(gamma/(gamma-1)) * (1-gamma+2*gamma*M^2)/(gamma+1)
```

Hay continuidad en M = 1 (ambas expresiones dan 1.893).

---

## 3. Inversion de Mach

El CADC no recibe Mach directo. Lo calcula invirtiendo relaciones pitot desde presiones medidas.

### Caso subsonico (Pt/Ps < 1.893)

Inversion analitica directa:

```
Pt/Ps = (1 + 0.2*M^2)^3.5

=> (Pt/Ps)^(2/7) = 1 + 0.2*M^2
=> M = sqrt(5 * ((Pt/Ps)^(2/7) - 1))
```

### Caso supersonico (Pt/Ps >= 1.893)

Rayleigh no tiene inversion analitica cerrada. Se resuelve con **biseccion** en [1.0, 5.0]:

```
Encontrar M tal que rayleigh_ratio(M) = Pt/Ps medido
```

La biseccion converge en ~20 iteraciones para tolerancia 1e-6 y es robusta frente a ruido.

---

## 4. Velocidades aeronauticas

### True Airspeed (TAS)

Velocidad del avion respecto a la masa de aire:

```
TAS = Mach * a(T)
    = Mach * sqrt(gamma*R*T)
```

Ejemplo: M = 1.4, T = 216.65 K -> a = 295.07 m/s -> TAS = 413.1 m/s = 803 kt

### Calibrated Airspeed (CAS)

CAS es velocidad equivalente de instrumento referida a ISA de nivel del mar. Usa `qc = Pt - Ps` normalizada por P0:

```
CAS = a0 * sqrt(5 * ((qc/P0 + 1)^(2/7) - 1))

a0 = 340.294 m/s
P0 = 101325 Pa
```

A nivel del mar, CAS se aproxima a TAS. En altitud, sigue representando el efecto de presion calibrado a nivel del mar.

### Equivalent Airspeed (EAS)

Velocidad que produce la misma carga aerodinamica a densidad de referencia:

```
EAS = TAS * sqrt(rho/rho0)

rho0 = 1.225 kg/m^3
```

Los limites estructurales suelen expresarse en EAS porque las cargas escalan con 0.5*rho*V^2.

### Relacion entre velocidades

En condiciones tipicas de altitud: **EAS <= CAS <= TAS**.

Cerca de nivel del mar: EAS ~= CAS ~= TAS.

---

## 5. Variacion vertical (VSI)

Tasa de ascenso/descenso por diferencias finitas entre ciclos:

```
VSI = (h_actual - h_anterior) / dt

Positivo: ascenso
Negativo: descenso
```

Conversion: `VSI [ft/min] = VSI [m/s] * 196.850`

---

## 6. Modelo F-14 de alas de barrido variable

El angulo objetivo se calcula con cuatro zonas de Mach e interpolacion lineal:

```
M < 0.40:            sweep = 20 deg
0.40 <= M < 0.80:    sweep = lerp(M, 0.40, 0.80, 20 deg, 35 deg)
0.80 <= M < 1.00:    sweep = lerp(M, 0.80, 1.00, 35 deg, 50 deg)
1.00 <= M <= 2.34:   sweep = lerp(M, 1.00, 2.34, 50 deg, 68 deg)
```

Mayores barridos reducen la componente normal de velocidad en el borde de ataque y disminuyen la resistencia de onda.

Rango: 20 deg (baja velocidad/operaciones de portaaviones) a 68 deg (maxima velocidad). Posicion de almacenamiento: 75 deg.

---

## 7. Ruido gaussiano de sensores (Box-Muller)

C estandar ofrece `rand()` uniforme. Para obtener ruido normal:

```
Dados U1, U2 ~ Uniforme(0, 1):
Z = sqrt(-2 * ln(U1)) * cos(2*pi*U2)

Entonces Z ~ Normal(0, 1)
```

Para desviacion estandar sigma:
```
valor_ruidoso = valor_real + Z * sigma
```

Parametros usados:
- Sensor de presion: sigma = 15.0 Pa
- Sensor de temperatura: sigma = 0.5 K

---

## Constantes fisicas usadas

| Simbolo | Valor | Descripcion |
|---------|-------|-------------|
| R | 287.058 J/(kg*K) | Constante especifica del aire seco |
| gamma | 1.4 | Relacion de calores especificos |
| g | 9.80665 m/s^2 | Gravedad estandar |
| T0 | 288.15 K | Temperatura ISA a nivel del mar |
| P0 | 101325 Pa | Presion ISA a nivel del mar |
| rho0 | 1.225 kg/m^3 | Densidad ISA a nivel del mar |
| a0 | 340.294 m/s | Velocidad del sonido a nivel del mar |
| L | 0.0065 K/m | Lapse rate de troposfera |

---

## Precision y limitaciones

**Precision de inversion ISA:** menor a 0.2 m en 0-20,000 m bajo condiciones ideales.

**Precision de inversion de Mach:** menor a 1e-6 en M = 0 a M = 2.34.

**Limitaciones del modelo:**
- La atmosfera real difiere de ISA por clima, humedad y variaciones locales de presion.
- El ruido gaussiano blanco no modela correlacion temporal ni dinamicas termicas/mecanicas reales.
- El scheduling de alas es lineal por tramos y no incluye histeresis.
