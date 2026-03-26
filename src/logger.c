/* 
    Implementacion del sistema de logging y diagnostico

    La validacion implementada opera en dos niveles distintos que no se deben confundir:

    Limites operativos y coherencia fisica

    Los limites operativos son limites fisicos impuestos durante el diseño original del F-14, este modelo de CADC no es valido para otras aeronaves debido a que esta planteado para un aeronave especifica, el F-14 Tomcat, y utiliza valores de referencia de su diseño

    El nivel de coherencia fisica, denominado nivel 2, protege contra errores de calculo, bugs en los modulos anteriores (atmosphere, airdata, cadc, sensors, etc) o corrupcion e datos, no contra los limites impuestos del avion.

    Se usara un contador de ciclos consecutivos anomalos por sensor pra detectar fallos sostenidos por sensores rotos e ignorar ruidos puntuales.
*/

#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

// Constantes de validacion fisica: rangos fuera de los cuales es fisicamente imposible que este en vuelo el F-14

static const double TEMP_MIN_K = 180.0; // Temperatura minima
static const double TEMP_MAX_K = 325.0; // Temperatura maxima
static const double MACH_MAX_VALID = 3.0; // Mach maximo con margen de seguridad para validacion
static const double SPEED_MARGIN_KT = 2.0; // Margen de tolerancia para comparacion CAS vs TAS
static const double DENSITY_MIN = 0.04;
static const double DENSITY_MAX = 1.10; // Rango valido de densidad relativa
static const double PS_MIN_PA = 4000.0; // > 20.000 m 
static const double PS_MAX_PA = 110000.0; // Presion de sensores, rangos validos
static const double PT_MIN_PA = 4000.0; 
static const double PT_MAX_PA = 200000.0; 
static const double TEMP_SENSOR_MIN_K = 150.0; 
static const double TEMP_SENSOR_MAX_K = 350.0; 


// Funcion privada
static void write_csv_header(FILE *f)
{
    fprintf(f, "time_s,"
        "mach,"
        "supersonic,"
        "altitude_ft,"
        "altitude_m,"
        "tas_kt,"
        "cas_kt,"
        "eas_kt,"
        "vsi_ftmin,"
        "static_temp_K,"
        "static_temp_C,"
        "density_ratio,"
        "dynamic_pressure_Pa,"
        "speed_of_sound_ms,"
        "sweep_deg,"
        "sweep_rate_dps,"
        "wing_auto,"
        "alert_any,"
        "alert_mach,"
        "alert_cas,"
        "alert_alt,"
        "alert_sensor,"
        "alert_low_speed,"
        "pt_Pa,"
        "ps_Pa,"
        "sensor_temp_K,"
        "airdata_valid,"
        "cycle\n"
    );
}

// Escribe una fila de datos del ciclo actual
static void write_csv_row(FILE *f, const CadcState *state)
{
    const AirDataState *ad = &state->airdata;
    const WingState    *wg = &state->wings;
    const AlertState   *al = &state->alerts;
    const SensorReading *sr = &state->sensor;

    fprintf(f,
        "%.3f,"         // time_s
        "%.6f,"         // mach
        "%d,"           // supersonic
        "%.2f,"         // altitude_ft         
        "%.2f,"         // altitude_m          
        "%.3f,"         // tas_kt             
        "%.3f,"         // cas_kt              
        "%.3f,"         // eas_kt              
        "%.1f,"         // vsi_ftmin           
        "%.3f,"         // static_temp_K       
        "%.3f,"         // static_temp_C       
        "%.6f,"         // density_ratio       
        "%.2f,"         // dynamic_pressure_Pa 
        "%.3f,"         // speed_of_sound_ms   
        "%.2f,"         // sweep_deg           
        "%.4f,"         // sweep_rate_dps      
        "%d,"           // wing_auto           
        "%d,"           // alert_any           
        "%d,"           // alert_mach          
        "%d,"           // alert_cas           
        "%d,"           // alert_alt           
        "%d,"           // alert_sensor        
        "%d,"           // alert_low_speed     
        "%.2f,"         // pt_Pa              
        "%.2f,"         // ps_Pa              
        "%.3f,"         // sensor_temp_K      
        "%d,"           // airdata_valid       
        "%d\n",         // cycle           
        state->time_s,
        ad->mach,
        ad->supersonic,
        ad->altitude_ft,
        ad->altitude_m,
        ad->tas_kt,
        ad->cas_kt,
        ad->eas_kt,
        ad->vsi_ftmin,
        ad->static_temp_K,
        ad->static_temp_C,
        ad->density_ratio,
        ad->dynamic_pressure_Pa,
        ad->speed_of_sound_ms,
        wg->sweep_deg,
        wg->sweep_rate_dps,
        wg->mode_auto,
        al->any_alert,
        al->overspeed_mach,
        al->overspeed_cas,
        al->altitude_max,
        al->sensor_invalid,
        al->low_speed,
        sr->total_pressure_Pa,
        sr->static_pressure_Pa,
        sr->temperature_K,
        ad->valid,
        state->cycle_count
    );
}

// Retorna el nombre del nivel como strin
static const char *level_name(LogLevel level)
{
    switch (level) {
        case LOG_INFO: return "INFO";
        case LOG_WARN: return "WARN";
        case LOG_ERROR: return "ERROR";
        default: return "????";
    }
}

// Implementaciones
int logger_init(LoggerState *logger, const char *filename, LogLevel level)
{
    assert(logger != NULL);

    memset(logger, 0, sizeof(LoggerState));

    logger->min_level = level;
    logger->initialized = 1;
    logger->csv_open = 0;
    logger->max_mach_seen = 0.0;
    logger->max_alt_ft_seen = 0.0;
    logger->max_tas_kt_seen = 0.0;
    logger->max_cas_kt_seen = 0.0;

    // Inicializar estado de fallos a cero
    memset(&logger->faults, 0, sizeof(SensorFaultState));
    
    if (filename == NULL) {
        return 0;
    }

    // Copiar nombre del archivo con proteccion de buffer
    strncpy(logger->csv_filename, filename, sizeof(logger->csv_filename) - 1);
    logger->csv_filename[sizeof(logger->csv_filename) - 1] = '\0';

    // Abrir archivo en modo escritura
    logger->csv_file = fopen(filename, "w");
    if (logger->csv_file == NULL) {
        fprintf(stderr, "[LOGGER] ERROR: no se pudo abrir '%s'\n", filename);
        return -1;
    }

    // Escribir cabecera del CSV
    write_csv_header(logger->csv_file);

    logger->csv_open = 1;

    printf("[LOGGER] Archivo CSV abierto: %s\n", filename);
    return 0;
}

void logger_close(LoggerState *logger)
{
    assert(logger != NULL);

    if (!logger->initialized) return;

    if (logger->csv_open && logger->csv_file != NULL) {
        fflush(logger->csv_file);
        fclose(logger->csv_file);
        logger->csv_file = NULL;
        logger->csv_open = 0;
        printf("[LOGGER] Archivo CSV cerrado: %s (%d filas)\n", logger->csv_filename, logger->csv_rows_written);
    }

    // Imprimir resumen final
    logger_print_summary(logger);

    logger->initialized = 0;
}

void logger_validate(const CadcState *state, ValidationReport *report)
{
    assert(state != NULL);
    assert(report != NULL);

    memset(report, 0, sizeof(ValidationReport));
    report->result = VALIDATION_OK;
    report->first_fault_desc = NULL;

    // Si el AirData o es valido, no se valida
    if (!state->airdata.valid) {
        report->result = VALIDATION_INVALID;
        report->first_fault_desc = "airdata marcado como invalido por el CADC";
        return;
    }

    const AirDataState *ad = &state->airdata;

    // Mach negativo 
    if (ad->mach < 0.0) {
        report->mach_negative = 1;
        report->result = VALIDATION_INVALID;
        report->first_fault_desc = "Mach negativo";
    }

    // Mach excesivo
    if (ad->mach > MACH_MAX_VALID && report->first_fault_desc == NULL) {
        report->mach_exceed = 1;
        report->result = VALIDATION_WARN;
        report->first_fault_desc = "Mach > 3.0";
    }

    // Temperatura demasiado baja
    if (ad ->static_temp_K < TEMP_MIN_K) {
        report->temp_too_low = 1;
        if (report->result < VALIDATION_WARN) report->result = VALIDATION_WARN;
        if (report->first_fault_desc == NULL) {
            report->first_fault_desc = "Temperatura < 180K";
        }
    }

    // Temperatura demasiado alta
    if (ad->static_temp_K > TEMP_MAX_K) {
        report->temp_too_high = 1;
        if (report->result < VALIDATION_WARN) report->result = VALIDATION_WARN;
        if (report->first_fault_desc == NULL) {
            report->first_fault_desc = "Temperatura > 325K";
        }
    }

    // CAS > TAS
    if (ad->cas_kt > ad->tas_kt + SPEED_MARGIN_KT) {
        report->cas_exceeds_tas = 1;
        report->result = VALIDATION_INVALID;
        if (report->first_fault_desc == NULL) {
            report->first_fault_desc = "CAS > TAS";
        }
    }

    // EAS > TAS
    if (ad->eas_kt > ad->tas_kt + SPEED_MARGIN_KT) {
        report->eas_exceeds_tas = 1;
        report->result = VALIDATION_INVALID;
        if (report->first_fault_desc == NULL) {
            report->first_fault_desc = "EAS > TAS";
        }
    }

    // Presion dinamica negativa
    if (ad->dynamic_pressure_Pa < 0.0) {
        report->negative_dynamics_pressure = 1;
        report->result = VALIDATION_INVALID;
        if (report->first_fault_desc == NULL) {
            report->first_fault_desc = "Presion dinamica negativa";
        }
    }

    // Altitud negativa mas alla del margen de ruido
    if (ad->altitude_m < -100.0) {
        report->altitude_negative = 1;
        if (report->result < VALIDATION_WARN) report->result = VALIDATION_WARN;
        if (report->first_fault_desc == NULL) {
            report->first_fault_desc = "Altitud < -100m";
        }
    }

    // Densidad relativa fuera de rango fisico
    if (ad->density_ratio < DENSITY_MIN || ad->density_ratio > DENSITY_MAX) {
        report->density_out_of_range = 1;
        if (report->result < VALIDATION_WARN) report->result = VALIDATION_WARN;
        if (report->first_fault_desc == NULL) {
            report->first_fault_desc = "Densidad relativa fuera de rango";
        }
    }
}

void logger_update_fault_detection(LoggerState *logger, const SensorReading *reading)
{
    assert(logger != NULL);
    assert(reading != NULL);

    SensorFaultState *f = &logger->faults;

    // Si la lectura esta fuera del rango fisico valido, se incrementa el contador para el sensor, si esta dentro se resetea a cero, si se supera el umbral se marca el sensor como allo

    // Pitot
    int pitot_bad = (reading->total_pressure_Pa < PT_MIN_PA ||
                     reading->total_pressure_Pa > PT_MAX_PA);
    if (pitot_bad) {
        f->pitot_fault_cycles++;
    } else {
        f->pitot_fault_cycles = 0;
    }
    if (f->pitot_fault_cycles >= LOGGER_FAULT_THRESHOLD) {
        f->pitot_failed = 1;
    }

    // Puerto estatico
    int static_bad = (reading->static_pressure_Pa < PS_MIN_PA ||
                      reading->static_pressure_Pa > PS_MAX_PA);
    if (static_bad) {
        f->static_fault_cycles++;
    } else {
        f->static_fault_cycles = 0;
    }
    if (f->static_fault_cycles >= LOGGER_FAULT_THRESHOLD) {
        f->static_failed = 1;
    }

    // Termometro
    int temp_bad = (reading->temperature_K < TEMP_SENSOR_MIN_K ||
                    reading->temperature_K > TEMP_SENSOR_MAX_K);
    if (temp_bad) {
        f->temp_fault_cycles++;
    } else {
        f->temp_fault_cycles = 0;
    }
    if (f->temp_fault_cycles >= LOGGER_FAULT_THRESHOLD) {
        f->temp_failed = 1;
    }

    // OR de todos los fallos
    f->any_failed = f->pitot_failed || f->static_failed || f->temp_failed;
}

void logger_print_message(const LoggerState *logger, LogLevel level, double time_s, const char *msg)
{
    assert(logger != NULL);
    assert(msg    != NULL);

    if (level < logger->min_level) return;

    printf("[%s] t=%8.1f  %s\n", level_name(level), time_s, msg);
}


// Implementacion de loger_log_cycle()
void logger_log_cycle(LoggerState *logger, const CadcState *state)
{
    assert(logger != NULL);
    assert(state != NULL);

    if (!logger->initialized) return;

    logger_update_fault_detection(logger, &state->sensor);

    ValidationReport report;
    logger_validate(state, &report);

    logger->total_cycles++;

    if (!state->airdata.valid) {
        logger->invalid_cycles++;
    } else {
        const AirDataState *ad = &state->airdata;

        if (ad->mach > logger->max_mach_seen) logger->max_mach_seen = ad->mach;
        if (ad->altitude_ft > logger->max_alt_ft_seen) logger->max_alt_ft_seen = ad->altitude_ft;
        if (ad->tas_kt > logger->max_tas_kt_seen) logger->max_tas_kt_seen = ad->tas_kt;
        if (ad->cas_kt > logger->max_cas_kt_seen) logger->max_cas_kt_seen = ad->cas_kt;

        if (ad->supersonic) logger->supersonic_cycles++;
    }

    if (state->alerts.any_alert) {
        logger->alert_cycles++;
    }

    if (logger->csv_open && logger->csv_file != NULL) {
        write_csv_row(logger->csv_file, state);
        logger->csv_rows_written++;

        // Flush cada 100 filas para asegurar que los datos llegan al disco aun interrumpiendose
        if (logger->csv_rows_written % 100 == 0) {
            fflush(logger->csv_file);
        }
    }

    // Fallo de sensor detectado
    if (logger->faults.any_failed) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Fallo de sensor - pitot: %d static: %d temp: %d", logger->faults.pitot_failed, logger->faults.static_failed, logger->faults.temp_failed);
        logger_print_message(logger, LOG_ERROR, state->time_s, msg);
    }

    // Validacion fallida 
    if (report.result == VALIDATION_WARN) {
        logger_print_message(logger, LOG_WARN, state->time_s, report.first_fault_desc);
    } else if (report.result == VALIDATION_INVALID) {
        logger_print_message(logger, LOG_ERROR, state->time_s, report.first_fault_desc);
    }

    // Alertas operativas del CADC
    if (state->alerts.any_alert && logger->min_level <= LOG_INFO) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Alerta CADC activa - mach: %d cas: %d alt: %d sensor: %d", state->alerts.overspeed_mach, state->alerts.overspeed_cas, state->alerts.altitude_max, state->alerts.sensor_invalid);
        logger_print_message(logger, LOG_INFO, state->time_s, msg);
    }
}

void logger_print_summary(const LoggerState *logger)
{
    assert(logger != NULL);

    printf("\n");
    printf("Resumen de vuelo\n");
    printf("Ciclos totales: %d\n", logger->total_cycles);
    printf("Ciclos invalidos: %d (%.1f%%)\n", logger->invalid_cycles, logger->total_cycles > 0 ? 100.0 * logger->invalid_cycles / logger->total_cycles : 0.0);
    printf("Ciclos con alerta: %d (%.1f%%)\n", logger->alert_cycles, logger->total_cycles > 0 ? 100.0 * logger->alert_cycles / logger->total_cycles : 0.0);
    printf("Ciclos supersonicos: %d (%.1f%%)\n", logger->supersonic_cycles, logger->total_cycles > 0 ? 100.0 * logger->supersonic_cycles / logger->total_cycles : 0.0);
    printf("\nMaximos alcanzados:\n");
    printf("Mach maximo: %.4f\n", logger->max_mach_seen);
    printf("Altitud maxima: %.0f ft\n", logger->max_alt_ft_seen);
    printf("TAS maximo: %.1f kt\n", logger->max_tas_kt_seen);
    printf("CAS maximo: %.1f kt\n", logger->max_cas_kt_seen);
    printf("\nEstado de sensores:\n");
    printf("Pitot: %s\n", logger->faults.pitot_failed ? "FALLO" : "OK");
    printf("Puerto estatico: %s\n", logger->faults.static_failed ? "FALLO" : "OK");
    printf("Termometro: %s\n", logger->faults.temp_failed ? "FALLO" : "OK");

    if (logger->csv_open || logger->csv_rows_written > 0) {
        printf("\nCSV: %s (%d filas)\n", logger->csv_filename, logger->csv_rows_written);
    }
    printf("\n");
}

void logger_print_fault_report(const LoggerState *logger)
{
    assert(logger != NULL);

    const SensorFaultState *f = &logger->faults;

    printf("\nDiagnostico de sensores:\n");
    printf("Pitot: ciclos fallo = %-3d declarado = %s\n", f->pitot_fault_cycles, f->pitot_failed ? "FALLO" : "OK");
    printf("Estatico: ciclos fallo = %-3d declarado = %s\n", f->static_fault_cycles, f->static_failed ? "FALLO" : "OK");
    printf("Temp: ciclos fallo = %-3d declarado = %s\n", f->temp_fault_cycles, f->temp_failed ? "FALLO" : "OK");
}