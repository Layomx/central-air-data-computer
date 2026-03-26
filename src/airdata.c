/* 
    Implementacion del nucleo computacional del CADC (la parte mas interesante)

    La fisica implementada estara detallada en un archivo README.md aparte debido a que es bastante extansa y merece una revision por separado mas alla de comentarios en codigo.

    Lo  que se puede decir, que se utilizara lo que viene siendo la inversion de Mach para el caso subsonico ya que la ecuacion de Bernoulli compresible tiene inversion directa, mientras que en el caso supersonico la ecuacion de Rayleigh no tiene una inversion analitica por lo que debe utilizar una biseccion. De igual forma puede resolverse por medio de Newton-Raphson antes que por biseccion por cuestiones de velocidad pero he optado que no por codigo de seguridad (no se puede divergir).

    Altitud barometricas, TAS, CAS y EAS seran calculados respectivamente con sus formulas especificas a traves de los datos que iremos obteniendo de los sensores.
*/

#include "airdata.h"

#include <math.h>
#include <stdio.h>
#include <assert.h>

// Constantes internas

static const double GAMMA = 1.4;
static const double GM1 = 0.4; // gamma - 1
static const double G_GM1 = 3.5; // gamma/( gamma - 1)
static const double GM1_G = 0.285714286; // (gamma - 1)/ gamma = 2/7
static const double TWO_GM1 = 5.0; // 2/(gamma - 1)

// Exponente ISA troposfera 
static const double ISA_EXPO = 5.25587611; // g/(R*L) exacto

// Funcion privadas

// Funcion que calcula Pt/Ps para Mach >=1 usando Rayleigh 
static double rayleigh_ratio(double mach)
{
    /* Esta es exactamente la misma fisica aplicada en los "drivers" de sensores, pero aqui se usa al reves para encontrar M*/

    double M2 = mach * mach;
    double gp1 = GAMMA + 1.0;
    double gm1 = GM1;
    double two_g = 2.0 * GAMMA;

    double denomA = 4.0 * GAMMA * M2 - 2.0 * gm1;
    double factor_A = pow((gp1 * gp1 * M2) / denomA, G_GM1);
    double factor_B = (1.0 - GAMMA + two_g * M2) / gp1;

    return factor_A * factor_B;
}

// Funcion que encuentr Mach supersonico por biseccion
static double mach_bisection(double target_ratio, double M_lo, double M_hi)
{
    /* Esta funcion resuelve raileight_ratio(M) = target_ratio en el intervalo [M_lo, M_hi]*/
    double M_mid = 1.0;
    int iter = 0;

    while (iter < AIRDATA_MAX_ITER)
    {
        M_mid = 0.5 * (M_lo + M_hi);

        double ratio_mid = rayleigh_ratio(M_mid);
        double error = ratio_mid - target_ratio;

        // Convergencia alcanzada
        if (fabs(error) < AIRDATA_MACH_TOL) {
            break; 
        }

        /* 
        La funcion rayleight_Ratio es creciente con M
        Si ratio_mid < target entonces M real esta en [M_mid, M_hi]
        Si ratio_mid > target entonces M real esta en [M_lo, M_mid]
        */
       if (ratio_mid < target_ratio) {
        M_lo = M_mid;
       } else {
        M_hi = M_mid;
       }

       iter++;
    }

    return M_mid;
}

// Implementacion de funciones definidas en .h

double airdata_calc_mach(double total_pressure_Pa, double static_pressure_Pa)
{
    // Validacion de entrada: Pt debe ser >= Ps
    if (total_pressure_Pa <= static_pressure_Pa) {
        // Esto es clave en lecturas de sensores ruidosas,  el CADC real hace clamp a M = 0, asi que aqui retornamos 0 y el caller puede verificar con "valid"
        return 0.0;
    }

    double ratio = total_pressure_Pa / static_pressure_Pa;

    // Se determina el regimen comparando Pt/Ps con el valor critico en M = 1. En M = 1 la ecuacion subsonica y de Rayleigh deberian dar el mismo resultado
    static const double RATIO_AT_MACH1 = 1.8929; 

    if (ratio < RATIO_AT_MACH1) {
        // Regimen subsonico, se realiza inversion analitica directa
        double term = pow(ratio, GM1_G) - 1.0;
        return sqrt(TWO_GM1 * term); 
    }
    else {
        // Regimen supersonico, se realiza biseccion numerica para la ecuacion de Rayleigh en el intervalo [1.0, 5.0], el F-14 tiene Mach maximo de 2.4, bien dento del rango. Un mach 5.0 cubre hasta misiles hipersonicos si se quisiera.
        return mach_bisection(ratio, 1.0, 5.0);
    }
}

double airdata_calc_altitude(double static_pressure_Pa)
{
    // Revision de presiones invalidas
    if (static_pressure_Pa <= 0.0) {
        return ISA_MAX_ALT_M;
    }

    // No se puede calcular altitud fuera del modelo
    if (static_pressure_Pa >= ISA_P0_SEA_LEVEL_PA) {
        return 0.0; 
    }

    if (static_pressure_Pa > ISA_P_TROPOPAUSE_PA) {
        // h = (T0/L) * (1 - (Ps/P0)^(1/expo)) troposfera
        double p_ratio = static_pressure_Pa / ISA_P0_SEA_LEVEL_PA;
        double T = ISA_T0_SEA_LEVEL_K * pow(p_ratio, 1.0 / ISA_EXPO);
        return (ISA_T0_SEA_LEVEL_K - T) / ISA_LAPSE_RATE;
    }
    else {
        // Hs = R*T_tp/g = 6341.6 m estratosfera baja
        const double Hs = (AIR_GAS_CONSTANT_R *  ISA_T_TROPOPAUSE_K) / GRAVITY_G;
        double delta_h = -Hs *log(static_pressure_Pa / ISA_P_TROPOPAUSE_PA);
        double h = ISA_TROPOPAUSE_ALT_M + delta_h;

        if (h > ISA_MAX_ALT_M) h = ISA_MAX_ALT_M;
        return h;
    }
}

double airdata_calc_tas(double mach, double temperature_K)
{
    if (mach < 0.0) mach = 0.0;
    if (temperature_K < 1.0) temperature_K = 1.0;

    double a = sqrt(GAMMA * AIR_GAS_CONSTANT_R * temperature_K);
    return mach * a;
}

double airdata_calc_cas(double total_pressure_Pa, double static_pressure_Pa)
{
    if (total_pressure_Pa <= static_pressure_Pa) return 0.0;

    // Presion de impacto compresible: qc = Pt - P0
    double qc = total_pressure_Pa - static_pressure_Pa;
    double term = pow(qc / AIRDATA_P0_PA + 1.0, GM1_G) - 1.0;
 
    if (term <= 0.0) return 0.0;
 
    return AIRDATA_A0_MS * sqrt(TWO_GM1 * term);
}

double airdata_calc_eas(double tas_ms, double density_kg_m3)
{
    if (tas_ms <= 0.0) return 0.0;
    if (density_kg_m3 <= 0.0) return 0.0;

    double sigma = density_kg_m3 / AIRDATA_RHO0;
    return tas_ms * sqrt(sigma);
}

void airdata_compute(const SensorReading *reading, double prev_alt_m, double dt_s, AirDataState *state)
{
    assert(reading != NULL);
    assert(state != NULL);

    // Iniciailizacion todo a cero
    state->valid = 0;
    state->supersonic = 0;
    state->mach = 0.0;
    state->tas_ms = 0.0;
    state->tas_kt = 0.0;
    state->cas_ms = 0.0;
    state->cas_kt = 0.0;
    state->eas_ms = 0.0;
    state->eas_kt = 0.0;
    state->altitude_m = 0.0;
    state->altitude_ft = 0.0;
    state->vsi_ms = 0.0;
    state->vsi_ftmin = 0.0;

    // Validacion de entradas
    double Pt = reading->total_pressure_Pa;
    double Ps = reading->static_pressure_Pa;
    double T = reading->temperature_K;

    // Validaciones fisics basicas
    if (Pt < Ps || Ps <= 0.0 || T <= 0.0) {
        // Lectura invalida quizas ocurrida por ruido
        return;
    }

    // Calculo de Mach
    state->mach = airdata_calc_mach(Pt, Ps);

    if (state->mach >= 1.0) {
        state->supersonic = 1;
    }

    // Calculo de altitud barometrica
    state->altitude_m = airdata_calc_altitude(Ps);
    state->altitude_ft = state->altitude_m * 3.28084;

    // Calculo VSI (tasa de ascenso y descenso)
    if (dt_s > 0.001) {
        state->vsi_ms = (state->altitude_m - prev_alt_m) / dt_s;
        state->vsi_ftmin = state->vsi_ms * 196.850;
    }

    // Propiedades del aire en condiciones actuales
    state->static_temp_K = T;
    state->static_temp_C = T - 273.15;
    state->speed_of_sound_ms = sqrt(GAMMA * AIR_GAS_CONSTANT_R * T);
    state->density_kg_m3 = Ps / (AIR_GAS_CONSTANT_R * T);
    state->density_ratio = state->density_kg_m3 / AIRDATA_RHO0;

    // Calculo de velocidades
    state->tas_ms = airdata_calc_tas(state->mach, T);
    state->tas_kt = state->tas_ms / 0.514444;

    state->cas_ms = airdata_calc_cas(Pt, Ps);
    state->cas_kt = state->cas_ms / 0.514444;

    state->eas_ms = airdata_calc_eas(state->tas_ms, state->density_kg_m3);
    state->eas_kt = state->eas_ms / 0.514444;

    // Presiones derivadas
    state->dynamic_pressure_Pa = Pt - Ps;
    state->impact_pressure_Pa = Pt - AIRDATA_P0_PA;

    // Validar datos
    state->valid = 1;
}

void airdata_print(const AirDataState *state)
{
    assert(state != NULL);

    if (!state->valid) {
        printf("Lecturas invalidas, sensores fuera de rango");
        return;
    }

    printf("F-14 CADC - Panel de instrumentos\n");
    printf("\n");
    printf("Mach:  %f | Alt:  %f ft\n", state->mach, state->altitude_ft);
    printf("TAS:  %f kt | ALT:  %f m\n", state->tas_kt, state->altitude_m);
    printf("CAS:  %f kt | VSI:  %f ft/min\n", state->cas_kt, state->vsi_ftmin);
    printf("EAS:  %f kt | VSI:  %f m/s\n\n", state->eas_kt, state->vsi_ms);
    printf("T estatica:  %f K | Densidad:  %f kg/m^3\n", state->static_temp_K, state->density_kg_m3);
    printf("T estatica:  %f C | Ratio densidad:  %f\n", state->static_temp_C, state->density_ratio);
    printf("a local:  %f m/s | q = Pt - Ps: %f Pa\n", state->speed_of_sound_ms, state->dynamic_pressure_Pa);
}

void airdata_print_compact(double time_s, const AirDataState *state)
{
    assert(state != NULL); 

    if (!state->valid) {
        printf("%f | INVALID\n", time_s);
        return;
    }

    printf("%f | M %f | %f ft | TAS %f | CAS %f | VSI %f\n", time_s, state->mach, state->altitude_ft, state->tas_kt, state->cas_kt, state->vsi_ftmin);
}