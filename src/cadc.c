/*
    Implementacion del nucleo integrador del CADC

    Modelo de alas de geometria variable

    La logica implementada aqui es una aproximacion simplificada basada en la relacion Mach -> sweep documentadas en fuentes tecnicas del programa F-14:

    Zona 1: M < 0.40 -> sweep 20 grados
    Zona 2: 0.40 - 0.80 -> sweep 20 a 35 grados
    Zona 3: 0.80 - 1.00 -> sweep 35 a 50 grados
    Zona 4: 1.00 - 2.34 -> sweep 50 a 68 grados

    La interpolacion sera lineal en cada zona. El sistema real habia histersis que evitaban oscilaciones de alas alas, no lo implemento por cuestiones de simplicidad, y es esta la razon que lo hace un modelo simplificado
*/

#include "cadc.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

// Funciones auxiliar: interpolacion lineal entre dos valores
static double cadc_lerp(double x, double x0, double x1, double y0, double y1)
{
    if (x1 - x0 < 1e-9) return y0;
    double t = (x - x0) / (x1 - x0);
    // Clamp t a [0,1] para no salir del rango
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return y0 + t * (y1 - y0);
}

// Funcion que calcula el angulo de barrido optimo de las alas dado el Mach actual
double cadc_calc_wing_sweep(double mach)
{
    if (mach < 0.0) mach = 0.0;

    if (mach < F14_WING_M_LOW) {
        // Zona 1: alas completamente extendidad, baja velocidad, maxima sustentacion, minima resistencia inductiva. Necesario para dpesegue y aterrizaje en portavviones.
        return F14_WING_MIN_DEG; // 20 grados
    }
    else if (mach < F14_WING_M_MID) {
        // Zona 2: transicion inicial 20 a 35 grados
        return cadc_lerp(mach,  F14_WING_M_LOW, F14_WING_M_MID, 20.0, 35.0);
    }
    else if (mach < F14_WING_M_TRANS) {
        // Zona 3: transicion 35 a 50 grados transonico
        return cadc_lerp(mach, F14_WING_M_MID, F14_WING_M_TRANS, 35.0, 50.0);
    }
    else {
        // Zona 4: regimen supersonico
        double sweep = cadc_lerp(mach, F14_WING_M_TRANS, F14_WING_M_HIGH, 50.0, F14_WING_MAX_DEG);
        return sweep;
    }
}

void cadc_eval_alerts(const AirDataState *airdata, AlertState *alerts) 
{ 
    assert(airdata != NULL);
    assert(alerts != NULL);

    // Limpiar todas las alertas
    memset(alerts, 0, sizeof(AlertState));

    // Si el airdata no es valido, alerta inmediata
    if (!airdata->valid) {
        alerts->sensor_invalid = 1;
        alerts->any_alert = 1;
        return;
    }

    // Overspeed CAS
    if (airdata->cas_kt > F14_CAS_MAX_KT) {
        alerts->overspeed_cas = 1;
    }

    // Overspeed Mach
    if (airdata->mach > F14_MACH_MAX) {
        alerts->overspeed_mach = 1;
    }

    // Altitud maxima superada
    if (airdata->altitude_ft > F14_ALT_MAX_FT) {
        alerts->altitude_max = 1;
    }

    // Baja velocidad en vuelo (relevante con sustentacion)
    if (airdata->cas_kt < 100.0 && airdata->altitude_ft > 500.0) {
        alerts->low_speed = 1;
    }

    // OR de todas las alertas
    alerts->any_alert = alerts->overspeed_cas || alerts->overspeed_mach || alerts->altitude_max || alerts->sensor_invalid || alerts->low_speed;
}

void cadc_config_init(CadcConfig *cfg)
{
    assert(cfg != NULL);
    memset(cfg, 0, sizeof(CadcConfig));

    sensors_config_init(&cfg->sensor_cfg);
    
    cfg->wing_auto = 1; // Alas en automatico por defecto
    cfg->log_enabled = 0; // Logging desactivado por defecto
    cfg->update_rate_hz = 10.0; // 10 hz es razonable para una simulacion
}

void cadc_state_init(CadcState *state)
{
    assert(state != NULL);
    memset(state, 0, sizeof(CadcState));

    // Alas comienzan extendidas (en tierra, Mach 0)
    state->wings.sweep_deg = F14_WING_MIN_DEG;
    state->wings.sweep_prev_deg = F14_WING_MIN_DEG;
    state->wings.mode_auto = 1;
    state->wings.at_min = 1;
    state->wings.at_max = 0;
}

void cadc_update(const CadcConfig *cfg, double time_s, CadcState *state)
{
    assert(cfg != NULL);
    assert(state != NULL);

    // Calculo de dt, el primer ciclo no tiene historial, dt = 0. A partir del segundo ciclo, dt = diferencia de tiempo real.
    double dt = (state->cycle_count == 0) ? 0.0 : time_s - state->time_s;
    
    // Actualizacion de tiempo
    state->time_s = time_s;
    state->dt_s = dt;

    // Lectura de sensores
    sensors_read(&cfg->sensor_cfg, time_s, &state->sensor);

    // Calculo de airdata completo
    airdata_compute(&state->sensor, state->prev_altitude_m, dt, &state->airdata);

    // Guardado de la altitud actual para el proximo ciclo
    if (state->airdata.valid) {
        state->prev_altitude_m = state->airdata.altitude_m;
    }

    // Actualizacion de las alas por geometria variable
    if (cfg->wing_auto && state->airdata.valid) {
        // Modo automatico: la computadora calcula el angulo optimo.
        state->wings.sweep_prev_deg = state->wings.sweep_deg;
        state->wings.sweep_deg = cadc_calc_wing_sweep(state->airdata.mach);

        // Tasa de cambio, se guardo el angulo anterior para calcular esta tasa
        if (dt > 0.001) {
            state->wings.sweep_rate_dps = (state->wings.sweep_deg - state->wings.sweep_prev_deg) / dt; 
        } else {
            state->wings.sweep_rate_dps = 0.0;
        }

        // Flags de posicion extrema
        state->wings.at_min = (state->wings.sweep_deg <= F14_WING_MIN_DEG + 0.1);
        state->wings.at_max = (state->wings.sweep_deg >= F14_WING_MAX_DEG - 0.01);
        state->wings.mode_auto = 1;
    }

    // Evaluacion de alertas
    cadc_eval_alerts(&state->airdata, &state->alerts);

    // Logging opcional

    if (cfg->log_enabled) {
        cadc_print_log_line(state);
    }

    // Incremento de contador
    state->cycle_count++;
}

void cadc_print_panel(const CadcState *state)
{
    assert(state != NULL);

    const AirDataState *ad = &state->airdata;
    const WingState    *wg = &state->wings;
    const AlertState   *al = &state->alerts;

    printf("\n");
    printf("CADC STATUS\n");
    printf("TIME: %8.2f s | CYCLE: %6d | DT: %.3f s\n",
           state->time_s, state->cycle_count, state->dt_s);

    if (!ad->valid) {
        printf("WARNING: SENSOR INVALIDO\n");
        return;
    }

    // Airdata
    printf("\n-- AIRDATA --\n");
    printf("MACH: %5.3f (%s)\n",
           ad->mach,
           ad->supersonic ? "SUPERSONICO" : "SUBSONICO");

    printf("ALT : %8.0f ft | %7.0f m\n",
           ad->altitude_ft, ad->altitude_m);

    printf("TAS : %6.1f kt | CAS: %6.1f kt | EAS: %6.1f kt\n",
           ad->tas_kt, ad->cas_kt, ad->eas_kt);

    printf("VSI : %+7.0f ft/min | %+6.2f m/s\n",
           ad->vsi_ftmin, ad->vsi_ms);

    printf("Q   : %8.1f Pa | TEMP: %7.2f K | A: %6.2f m/s\n",
           ad->dynamic_pressure_Pa,
           ad->static_temp_K,
           ad->speed_of_sound_ms);

    printf("RHO : %6.4f (rho/rho0)\n",
           ad->density_ratio);

    // Wings
    printf("\n-- WINGS --\n");
    printf("SWEEP: %6.2f deg | RATE: %+6.2f deg/s | MODE: %s\n",
           wg->sweep_deg,
           wg->sweep_rate_dps,
           wg->mode_auto ? "AUTO" : "MAN");

    printf("LIMITS: MIN=%d MAX=%d\n",
           wg->at_min,
           wg->at_max);

    // Alertas
    printf("\n-- ALERTS --\n");

    if (al->any_alert) {
        printf("ACTIVE: ");

        if (al->sensor_invalid) printf("SENSOR ");
        if (al->overspeed_mach) printf("MACH_LIMIT ");
        if (al->overspeed_cas)  printf("OVERSPEED ");
        if (al->altitude_max)   printf("ALT_MAX ");
        if (al->low_speed)      printf("LOW_SPEED ");

        printf("\n");
    } else {
        printf("NONE\n");
    }

    printf("\n");
}

void cadc_print_log_line(const CadcState *state)
{
    assert(state != NULL);

    const AirDataState *ad = &state->airdata;
    const WingState    *wg = &state->wings;
    const AlertState   *al = &state->alerts;

    // Caso invalido
    if (!ad->valid) {
        printf("t=%8.2f  status=INVALID  sweep=%5.1f deg\n",
               state->time_s,
               wg->sweep_deg);
        return;
    }

    // Línea principal de telemetría
    printf(
        "t=%8.2f  mach=%5.3f  alt_ft=%7.0f  tas=%6.1f  cas=%6.1f  vsi=%+7.0f  sweep=%5.1f  mode=%s  alert=%d\n",
        state->time_s,
        ad->mach,
        ad->altitude_ft,
        ad->tas_kt,
        ad->cas_kt,
        ad->vsi_ftmin,
        wg->sweep_deg,
        wg->mode_auto ? "AUTO" : "MAN",
        al->any_alert
    );
}