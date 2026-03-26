/*
    test_logger.c — Pruebas del módulo logger (Módulo 5)

    TEST 1: Validación física
    TEST 2: Detección de fallos de sensor
    TEST 3: Logging a CSV 
    TEST 4: Logging con sensores ruidosos
    TEST 5: Resumen de vuelo completo
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/logger.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Construye un CADC artificial con los parámetros dados, para facilitar la creación de casos de prueba
static void build_test_state(CadcState *state,
                              double mach,
                              double alt_m,
                              double tas_kt,
                              double cas_kt,
                              double temp_K,
                              double density_ratio,
                              double q_Pa,
                              int valid)
{
    memset(state, 0, sizeof(CadcState));
    state->airdata.valid          = valid;
    state->airdata.mach           = mach;
    state->airdata.altitude_m     = alt_m;
    state->airdata.altitude_ft    = alt_m * 3.28084;
    state->airdata.tas_kt         = tas_kt;
    state->airdata.cas_kt         = cas_kt;
    state->airdata.eas_kt         = cas_kt * 0.9;  // EAS < CAS siempre
    state->airdata.static_temp_K  = temp_K;
    state->airdata.static_temp_C  = temp_K - 273.15;
    state->airdata.density_ratio  = density_ratio;
    state->airdata.dynamic_pressure_Pa = q_Pa;
    state->wings.sweep_deg        = 20.0;
    state->wings.mode_auto        = 1;
}

int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    printf("\n");
    printf("CADC Logger Tests\n");

    LoggerState   logger;
    CadcState     state;
    ValidationReport report;

    // TEST 1: Validación física de parámetros
    printf("TEST 1: Validación física de parámetros\n");

    struct {
        const char *label;
        double mach;
        double alt_m;
        double tas_kt;
        double cas_kt;
        double temp_K;
        double density_ratio;
        double q_Pa;
        int    valid;
        ValidationResult expected;
    } val_tests[] = {
        // Datos buenos
        {"Crucero M=0.85 30,000ft",
          0.85, 9144, 500, 320, 229, 0.37, 17000, 1, VALIDATION_OK},
        {"Supersónico M=1.4 39,000ft",
          1.40, 11887, 803, 465, 217, 0.25, 39000, 1, VALIDATION_OK},
        {"En tierra M=0.0",
          0.00,    0,   0,   0, 288, 1.00,     0, 1, VALIDATION_OK},

        // Datos irreales e imposibles
        {"Mach negativo",
         -0.10, 5000, 300, 200, 255, 0.60, 10000, 1, VALIDATION_INVALID},
        {"CAS > TAS (imposible)",
          0.85, 9144, 400, 500, 229, 0.37, 17000, 1, VALIDATION_INVALID},
        {"Temperatura < 180K",
          1.00, 11000, 590, 390, 150, 0.36, 27000, 1, VALIDATION_WARN},
        {"Presión dinámica negativa",
          0.50, 5000, 320, 280, 255, 0.60, -100, 1, VALIDATION_INVALID},
        {"airdata marcado inválido",
          0.00,    0,   0,   0,   0, 0.00,    0, 0, VALIDATION_INVALID},
    };
    int n_val = 8;

    printf("%-35s %-12s %-12s %s\n",
           "Condición", "Esperado", "Obtenido", "");
    printf("%-35s %-12s %-12s %s\n",
           "───────────────────────────────────",
           "────────────", "────────────", "──");

    const char *result_names[] = {"OK", "WARN", "INVALID"};

    for (int i = 0; i < n_val; i++) {
        build_test_state(&state,
                         val_tests[i].mach,
                         val_tests[i].alt_m,
                         val_tests[i].tas_kt,
                         val_tests[i].cas_kt,
                         val_tests[i].temp_K,
                         val_tests[i].density_ratio,
                         val_tests[i].q_Pa,
                         val_tests[i].valid);

        logger_validate(&state, &report);

        int pass = (report.result == val_tests[i].expected);
        printf("%-35s %-12s %-12s %s\n",
               val_tests[i].label,
               result_names[val_tests[i].expected],
               result_names[report.result],
               pass ? "VALIDO" : "ERROR");

        if (!pass) {
            printf("  → fault_desc: %s\n",
                   report.first_fault_desc ? report.first_fault_desc : "(null)");
        }
    }

    // TEST 2: Detección de fallos de sensor
    printf("TEST 2: Detección de fallos de sensor\n");

    logger_init(&logger, NULL, LOG_WARN);

    SensorReading reading;
    memset(&reading, 0, sizeof(SensorReading));

    printf("Simulando fallo progresivo del pitot (Pt -> 0 Pa):\n\n");
    printf("%-8s %-12s %-8s %-12s\n",
           "Ciclo", "Pt [Pa]", "Conteo", "Estado");
    printf("%-8s %-12s %-8s %-12s\n",
           "────────", "────────────", "────────", "────────────");

    // Primeros 5 ciclos: sensor normal
    reading.static_pressure_Pa = 30000.0;
    reading.temperature_K      = 229.0;

    for (int i = 0; i < 5; i++) {
        reading.total_pressure_Pa = 45000.0; 
        logger_update_fault_detection(&logger, &reading);
        printf("%-8d %-12.0f %-8d %-12s\n",
               i + 1,
               reading.total_pressure_Pa,
               logger.faults.pitot_fault_cycles,
               logger.faults.pitot_failed ? "FALLO" : "OK");
    }

    // Ciclos 6-20: sensor empieza a fallar
    for (int i = 5; i < 20; i++) {
        reading.total_pressure_Pa = 0.0;  /* fallo — fuera de rango */
        logger_update_fault_detection(&logger, &reading);
        printf("%-8d %-12.0f %-8d %-12s\n",
               i + 1,
               reading.total_pressure_Pa,
               logger.faults.pitot_fault_cycles,
               logger.faults.pitot_failed ? "FALLO" : "OK");
    }

    printf("\nUmbral de declaración: %d ciclos\n", LOGGER_FAULT_THRESHOLD);
    printf("Resultado: pitot_failed = %d  %s\n",
           logger.faults.pitot_failed,
           logger.faults.pitot_failed ? "Detectado correctamente)" : "ERROR");

    printf("TEST 3: Logging de vuelo completo a CSV\n");

    logger_init(&logger, "output/vuelo_crucero.csv", LOG_WARN);

    CadcConfig cfg;
    cadc_config_init(&cfg);
    cadc_state_init(&state);
    cfg.sensor_cfg.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_cruise(&cfg.sensor_cfg.profile);
    cfg.wing_auto = 1;

    printf("Ejecutando vuelo completo (perfil crucero F-14)...\n");

    for (double t = 0.0; t <= 1400.0; t += 1.0) {
        cadc_update(&cfg, t, &state);
        logger_log_cycle(&logger, &state);
    }

    printf("Ciclos procesados: %d\n", logger.total_cycles);
    printf("Filas CSV escritas: %d\n", logger.csv_rows_written);
    printf("Archivo: output/vuelo_crucero.csv\n");

    logger_close(&logger);

    printf("TEST 4: Vuelo con sensores ruidosos\n");

    logger_init(&logger, "output/vuelo_ruidoso.csv", LOG_ERROR);

    cadc_config_init(&cfg);
    cadc_state_init(&state);

    cfg.sensor_cfg.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_intercept(&cfg.sensor_cfg.profile);
    sensors_enable_noise(&cfg.sensor_cfg, 99);
    cfg.wing_auto = 1;

    printf("Perfil de intercepción con ruido (σ_P=15Pa, σ_T=0.5K)...\n");
    printf("Solo se muestran mensajes de nivel ERROR:\n\n");

    for (double t = 0.0; t <= 1080.0; t += 0.5) {
        cadc_update(&cfg, t, &state);
        logger_log_cycle(&logger, &state);
    }

    printf("\n");
    logger_print_fault_report(&logger);
    logger_close(&logger);

    printf("TEST 5: Resumen estadístico — perfil de intercepción\n");

    logger_init(&logger, "output/vuelo_intercepcion.csv", LOG_WARN);

    cadc_config_init(&cfg);
    cadc_state_init(&state);
    cfg.sensor_cfg.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_intercept(&cfg.sensor_cfg.profile);
    cfg.wing_auto = 1;

    for (double t = 0.0; t <= 1080.0; t += 0.2) {
        cadc_update(&cfg, t, &state);
        logger_log_cycle(&logger, &state);
    }

    logger_close(&logger);

    printf("\nPruebas completadas correctamente.\n\n");

    return 0;
}