/*
    Modelo simplificado de la atmósfera estándar ISA
    (International Standard Atmosphere) para altitudes
    desde 0 hasta 20,000 metros.

    Este modelo se utiliza para calcular las propiedades
    atmosféricas necesarias para simulaciones aeronáuticas
    y sistemas como un Air Data Computer (ADC/CADC).

    CAPAS ATMOSFÉRICAS MODELADAS

    Troposfera (0 m – 11,000 m)

    En esta capa la temperatura decrece linealmente con la altitud
    debido a la expansión adiabática del aire.

        T(h) = T0 - L * h

    donde:
        T0 = 288.15 K      (temperatura a nivel del mar)
        L  = 0.0065 K/m    (gradiente de temperatura o lapse rate)
        h  = altitud [m]

    La presión se calcula usando la ecuación barométrica
    derivada del equilibrio hidrostático y la ley del gas ideal:

        P(h) = P0 * (T(h) / T0)^(g / (R * L))

    donde:
        P0 = 101325 Pa     (presión a nivel del mar)
        g  = 9.80665 m/s²  (gravedad estándar)
        R  = 287.05 J/(kg·K) (constante específica del aire)

    El exponente (g / (R * L)) ≈ 5.2561 para aire estándar.

    Estratosfera inferior (11,000 m – 20,000 m)

    En la tropopausa la temperatura deja de disminuir y se
    mantiene aproximadamente constante:

        T = 216.65 K

    En esta región la presión decrece exponencialmente:

        P(h) = P_tp * exp(-(g / (R * T_tp)) * (h - h_tp))

    donde:
        h_tp = 11,000 m     (altitud de la tropopausa)
        T_tp = 216.65 K     (temperatura en la tropopausa)
        P_tp = presión calculada a 11,000 m

    PROPIEDADES DERIVADAS

    Densidad del aire:

        ρ = P / (R * T)

    derivada de la ley del gas ideal.

    Velocidad del sonido:

        a = sqrt(γ * R * T)

    USO EN SIMULACIÓN

    Estas ecuaciones permiten calcular:
        - Temperatura atmosférica
        - Presión estática
        - Densidad del aire
        - Velocidad del sonido

    Estos parámetros son fundamentales para sistemas como
    un Air Data Computer (ADC/CADC), que los utiliza para
    derivar variables de vuelo como:
        - Mach
        - True Airspeed (TAS)
        - Calibrated Airspeed (CAS)
        - Altitud barométrica
*/

#include "atmosphere.h"

#include <math.h>
#include <stdio.h>
#include <assert.h>

// Exponente de la ecuación de presion en la troposfera  Expo = g / (R * L)
static const double PRESSURE_EXPONENT = GRAVITY_G / (AIR_GAS_CONSTANT_R * ISA_LAPSE_RATE);

// Escala de altura en estratosfera Hs = R * T_tp / g
static const double STRATOSPHERE_SCALE_HEIGHT = (AIR_GAS_CONSTANT_R * ISA_T_TROPOPAUSE_K) / GRAVITY_G;

// Calculo de la temperatura ISA en función de la altitud. Esta funcion se llamara internamente en atmosphere_calculte()
double atmosphere_temperature(double altitude_m) 
{
    if (altitude_m < 0.0)
    {
        altitude_m = 0.0; // Limite a nivel del mar, no existe altitud negativa en este modelo
    }

    // Troposfera (0 a 11,000 metros) la temperatura decrece linealmente con la altitud
    if (altitude_m <= ISA_TROPOPAUSE_ALT_M) 
    {
        // Troposfera: T(h) = T0 - L * h
        return ISA_T0_SEA_LEVEL_K - (ISA_LAPSE_RATE * altitude_m);
    } 
    else 
    {
        // Estratosfera baja la temperatura es constante a 216.65 K
        if (altitude_m > ISA_MAX_ALT_M)
        {
            altitude_m = ISA_MAX_ALT_M; // Limite a 20,000 metros, no se modela mas alla de esta altitud
        }

        // Estratosfera baja: T = constante
        return ISA_T_TROPOPAUSE_K;
    }
}

// Calculo de la presion ISA en funcion de la altitud 
double atmosphere_pressure(double altitude_m)
{
    if (altitude_m < 0.0)
    {
        altitude_m = 0.0;
    }

    // Troposfera (0 a 11,000 metros) la presion se calcula usando la ecuacion barometrica
    if (altitude_m <= ISA_TROPOPAUSE_ALT_M)
    {
        double T = ISA_T0_SEA_LEVEL_K - (ISA_LAPSE_RATE * altitude_m);
        double T_ratio = T / ISA_T0_SEA_LEVEL_K;
        return ISA_P0_SEA_LEVEL_PA * pow(T_ratio, PRESSURE_EXPONENT);
    }
    else {
        // Estratosfera baja la presion decrece exponencialmente
        if (altitude_m > ISA_MAX_ALT_M)
        {
            altitude_m = ISA_MAX_ALT_M;
        }

        double delta_h = altitude_m - ISA_TROPOPAUSE_ALT_M;
        return ISA_P_TROPOPAUSE_PA * exp(-delta_h / STRATOSPHERE_SCALE_HEIGHT);
    }
}

// Calculo de la velocidad del sonido en funcion de la temperatura
// Segun lo que lei, la velocidad del sonido en un gas ideal se calcula con la formula:
// a = sqrt(gamma * R * T) por tanto la velocidad del sonido de un gas ideal depende de la temperatura y de las constantes del gas
double atmosphere_speed_of_sound(double temperature_K)
{
    if (temperature_K < 0.0)
    {
        temperature_K = 1.0; // Evitar temperatura no fisica. la temperatura en Kelvin no puede ser negativa ni cero, el limite inferior es 0 K (cero absoluto)
    }
    return sqrt(AIR_GAMMA * AIR_GAS_CONSTANT_R * temperature_K);
}

void atmosphere_calculate(double altitude_m, AtmosphereState *state)
{
    assert(state != NULL); // Asegura que el punto no sea nulo

    // Clamps de altitud para evitar valores fuera del rango modelado 
    if (altitude_m < 0.0)
    {
        altitude_m = 0.0; // Limite a nivel del mar
    }
    else if (altitude_m > ISA_MAX_ALT_M)
    {
        altitude_m = ISA_MAX_ALT_M; // Limite a 20,000 metros
    }

    // Altitud corregida para el calculo
    state->altitude_m = altitude_m;

    // Calculo de la temperatura ISA
    state->temperature_K = atmosphere_temperature(altitude_m);

    // Calculo de la presion estatica ISA 
    state->pressure_Pa = atmosphere_pressure(altitude_m);

    // Calculo de la densidad del aire usando la ley del gas ideal: rho = P / (R * T)
    state->density_kg_m3 = state->pressure_Pa / (AIR_GAS_CONSTANT_R * state->temperature_K);

    // Calculo de la velocidad del sonido usando la formula: a = sqrt(gamma * R * T)
    state->speed_of_sound_ms = atmosphere_speed_of_sound(state->temperature_K);
}

// Apartado de funciones triviales pero mejoran la legibilidad del codigo. Conversiones de unidades
double meter_to_feet(double meters)
{
    return meters * 3.28084; 
}

double feet_to_meters(double feet)
{
    return feet * 0.3048;
}

double ms_to_knots(double ms)
{
    return ms / 0.514444;
}

// Funcion de debug para imprimir el estado atmosferico
void atmosphere_print_state(const AtmosphereState *state)
{
    assert(state != NULL);

    printf("ESTADO ATMOSFERICO ISA\n");
    printf("atm: alt = %.1f m T = %.2f K P = %.1f Pa rho = %.4f a = %.2f m/s\n",
       state->altitude_m,
       state->temperature_K,
       state->pressure_Pa,
       state->density_kg_m3,
       state->speed_of_sound_ms);
}

