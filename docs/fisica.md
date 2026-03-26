# Física implementada

Todas las ecuaciones implementadas en el proyecto, con su derivación y contexto físico.

---

## 1. Atmósfera Estándar ISA

### Troposfera (0 – 11,000 m)

La temperatura decrece linealmente con la altitud debido a la expansión adiabática del aire:

```
T(h) = T₀ - L·h

T₀ = 288.15 K   (temperatura al nivel del mar)
L  = 0.0065 K/m  (lapse rate — gradiente adiabático)
```

La presión se obtiene del equilibrio hidrostático con la ley del gas ideal:

```
P(h) = P₀ · (T(h)/T₀)^(g/R·L)

P₀  = 101325 Pa
g   = 9.80665 m/s²
R   = 287.058 J/(kg·K)

Exponente = g/(R·L) ≈ 5.2561
```

### Estratosfera baja (11,000 – 20,000 m)

La temperatura es constante (zona isotermal):

```
T = 216.65 K
```

La presión decae exponencialmente (ecuación barométrica):

```
P(h) = P_tp · exp(-(h - h_tp)/Hs)

h_tp = 11000 m       (altitud de la tropopausa)
P_tp = 22632.1 Pa    (presión en la tropopausa)
Hs   = R·T_tp/g ≈ 6341.6 m  (escala de altura)
```

### Propiedades derivadas

```
Densidad:          ρ = P / (R·T)           [kg/m³]
Velocidad sonido:  a = sqrt(γ·R·T)         [m/s]   γ = 1.4
```

### Inversión ISA (altitud a partir de presión)

Para obtener la altitud dado Ps, se invierten las ecuaciones anteriores:

**Troposfera** (Ps > P_tp):
```
T   = T₀ · (Ps/P₀)^(1/expo)
h   = (T₀ - T) / L
```

**Estratosfera** (Ps ≤ P_tp):
```
h = h_tp - Hs · ln(Ps/P_tp)
```

---

## 2. Presión total: Tubo Pitot

### Régimen subsónico (M < 1)

El flujo es isentrópico hasta el punto de remanso del pitot. La relación de presiones sigue la ecuación de Bernoulli compresible:

```
Pt/Ps = (1 + (γ-1)/2 · M²)^(γ/(γ-1))
      = (1 + 0.2 · M²)^3.5
```

Verificación de límites:
- M = 0: Pt/Ps = 1.0 (en reposo, sin presión dinámica) ✓
- M = 1: Pt/Ps = 1.893 (transición al régimen supersónico) ✓

### Régimen supersónico (M ≥ 1): Ecuación de Rayleigh

A velocidades supersónicas se forma una **onda de choque normal** delante del pitot. El flujo pasa de supersónico a subsónico a través de la onda, con una pérdida irreversible de presión de remanso.

La relación de Rayleigh combina:
1. Relaciones de salto a través de la onda de choque normal
2. Flujo isentrópico post-choque hasta el punto de remanso

```
Pt/Ps = [(γ+1)²·M² / (4γ·M² - 2(γ-1))]^(γ/(γ-1)) × (1-γ+2γ·M²)/(γ+1)
```

Continuidad: en M = 1, las dos ecuaciones producen el mismo resultado (1.893).

---

## 3. Inversión de Mach

El CADC no recibe Mach directamente — lo calcula invirtiendo la ecuación de pitot a partir de las presiones medidas.

### Caso subsónico (Pt/Ps < 1.893)

La ecuación de Bernoulli compresible tiene inversión analítica directa:

```
Pt/Ps = (1 + 0.2·M²)^3.5

→ (Pt/Ps)^(2/7) = 1 + 0.2·M²
→ M = sqrt(5 · ((Pt/Ps)^(2/7) - 1))
```

### Caso supersónico (Pt/Ps ≥ 1.893)

La ecuación de Rayleigh no tiene inversión analítica cerrada. Se resuelve numéricamente con **bisección** en el intervalo [1.0, 5.0]:

```
Encontrar M tal que: rayleigh_ratio(M) = Pt/Ps_medido
```

La bisección converge en ~20 iteraciones para tolerancia 1×10⁻⁶. Se eligió bisección sobre Newton-Raphson porque:
- No puede diverger (siempre convergente)
- No requiere la derivada de rayleigh_ratio
- Comportamiento predecible ante lecturas ruidosas

---

## 4. Velocidades de aviación

### True Airspeed (TAS)

Velocidad real del avión respecto a la masa de aire:

```
TAS = Mach × a(T)
    = Mach × sqrt(γ·R·T)
```

Ejemplo: M = 1.4, T = 216.65 K → a = 295.07 m/s → TAS = 413.1 m/s = 803 kt

### Calibrated Airspeed (CAS)

Velocidad que marcaría el instrumento en condiciones de nivel del mar ISA. Usa la presión de impacto `qc = Pt - Ps` normalizada con P₀:

```
CAS = a₀ × sqrt(5 × ((qc/P₀ + 1)^(2/7) - 1))

a₀ = 340.294 m/s  (velocidad del sonido al nivel del mar ISA)
P₀ = 101325 Pa
```

A nivel del mar: Ps = P₀, luego qc = Pt - P₀ = Pt - Ps → CAS = TAS.
En altitud: qc = Pt - Ps (presión dinámica real), normalizada con P₀.

### Equivalent Airspeed (EAS)

Velocidad que produciría la misma fuerza aerodinámica en aire de densidad de referencia:

```
EAS = TAS × sqrt(ρ/ρ₀)

ρ₀ = 1.225 kg/m³  (densidad al nivel del mar ISA)
```

Los límites estructurales del avión se expresan en EAS porque la carga aerodinámica (sustentación, resistencia) depende de `½·ρ·V²`, y a densidad constante ρ₀, EAS es proporcional a esa carga.

### Relación entre velocidades

En cualquier condición: **EAS ≤ CAS ≤ TAS** (en altitud, ρ < ρ₀)

A nivel del mar: EAS ≈ CAS ≈ TAS (ρ ≈ ρ₀)

---

## 5. Variación vertical (VSI)

La tasa de ascenso o descenso se calcula por diferencia finita entre ciclos:

```
VSI = (h_actual - h_anterior) / dt

Positivo: ascendiendo
Negativo: descendiendo
```

Conversión: `VSI [ft/min] = VSI [m/s] × 196.850`

---

## 6. Alas de geometría variable: F-14

El ángulo de barrido óptimo se calcula en cuatro zonas con interpolación lineal:

```
M < 0.40:             sweep = 20°  (baja velocidad, máxima sustentación)
0.40 ≤ M < 0.80:      sweep = lerp(M, 0.40, 0.80, 20°, 35°)
0.80 ≤ M < 1.00:      sweep = lerp(M, 0.80, 1.00, 35°, 50°)
1.00 ≤ M ≤ 2.34:      sweep = lerp(M, 1.00, 2.34, 50°, 68°)
```

**Física del barrido:** A mayor ángulo de barrido, el borde de ataque del ala forma un ángulo más pequeño con el frente de la onda de choque oblicua. Esto reduce la componente normal de la velocidad relativa al ala, retrasando la aparición de efectos de compresibilidad y reduciendo la resistencia de onda.

**Rango:** 20° (extendidas, portaaviones/baja velocidad) → 68° (máximo barrido, velocidad máxima). Posición de almacenamiento en portaaviones: 75°.

---

## 7. Ruido gaussiano: Algoritmo Box-Muller

C estándar solo provee `rand()` con distribución uniforme. Para simular ruido de sensor real (distribución normal) use la transformación Box-Muller:

```
Dado U₁, U₂ ~ Uniforme(0, 1):
Z = sqrt(-2 · ln(U₁)) · cos(2π · U₂)

Donde Z ~ Normal(0, 1)
```

Para obtener ruido con desviación estándar σ:
```
valor_ruidoso = valor_real + Z · σ
```

Parámetros usados:
- Sensor de presión: σ = 15.0 Pa
- Sensor de temperatura: σ = 0.5 K

---

## Constantes físicas utilizadas

| Símbolo | Valor | Descripción |
|---------|-------|-------------|
| R | 287.058 J/(kg·K) | Constante del gas específica del aire seco |
| γ | 1.4 | Coeficiente adiabático del aire |
| g | 9.80665 m/s² | Aceleración gravitacional estándar |
| T₀ | 288.15 K | Temperatura ISA al nivel del mar |
| P₀ | 101325 Pa | Presión ISA al nivel del mar |
| ρ₀ | 1.225 kg/m³ | Densidad ISA al nivel del mar |
| a₀ | 340.294 m/s | Velocidad del sonido al nivel del mar |
| L | 0.0065 K/m | Lapse rate en troposfera |

---

## Precisión y limitaciones

**Precisión del modelo ISA:** Error < 0.2 m en la inversión de altitud para toda la envolvente 0–20,000 m. Los altímetros barométricos reales tienen ±30 m — el modelo es ~150 veces más preciso.

**Precisión de la inversión de Mach:** Error < 1×10⁻⁶ en todo el rango M = 0 a M = 2.34.

**Limitaciones del modelo:**
- La atmósfera real difiere de ISA por condiciones meteorológicas (temperatura, humedad, presión local). El modelo no incorpora estas variaciones.
- El ruido gaussiano es estadísticamente correcto pero no captura la correlación temporal de sensores reales (inercia mecánica y térmica). Un filtro paso bajo IIR mejoraría el realismo.
- El modelo de alas es lineal en cada zona. El sistema real tenía lógica de histéresis para evitar oscilaciones.