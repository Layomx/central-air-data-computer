// Punto de entrada y opciones principales del simulador CADC F-14.

#include "src/cadc.h"
#include "src/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Constantes del programa.

#define PROG_VERSION "1.0.0"
#define PROG_DEFAULT_DT 1.0
#define PROG_DEFAULT_OUTPUT "output/vuelo.csv"
#define PROG_DEFAULT_SEED 42

// Configuración de simulación leída desde la línea de comandos.

typedef struct {
    // Perfil de vuelo.
    enum { PROFILE_CRUISE, PROFILE_INTERCEPT, PROFILE_CUSTOM } profile;

    // Parámetros para perfil custom.
    double custom_alt_m;
    double custom_mach;

    // Ruido.
    int noise_enabled;
    int noise_seed;

    // Tiempo.
    double dt_s;

    // Salida.
    char output_file[256];
    int csv_enabled;
    int quiet;
    int show_panel;
} SimConfig;

// Funciones privadas.

static void print_help(const char *prog_name)
{
    printf("\n");
    printf("CADC F-14 Tomcat Simulador de Air Data Computer\n");
    printf("USO: %s [opciones]\n\n", prog_name);
    printf("PERFILES DE VUELO:\n");
    printf("  --profile cruise      Crucero completo (despegue→M1.4→aterrizaje)\n");
    printf("  --profile intercept   Intercepción agresiva (hasta M2.34)\n");
    printf("  --profile custom      Condición constante definida por usuario\n\n");
    printf("PERFIL CUSTOM:\n");
    printf("  --alt <m>             Altitud en metros  (ej: --alt 9000)\n");
    printf("  --mach <M>            Número de Mach     (ej: --mach 0.85)\n\n");
    printf("SENSORES:\n");
    printf("  --noise               Activar ruido gaussiano (σ_P=15Pa, σ_T=0.5K)\n");
    printf("  --seed <n>            Semilla aleatoria para reproducibilidad\n\n");
    printf("SIMULACIÓN:\n");
    printf("  --dt <s>              Paso de tiempo en segundos (defecto: 1.0)\n\n");
    printf("SALIDA:\n");
    printf("  --output <archivo>    Archivo CSV de telemetría\n");
    printf("  --no-csv              No generar archivo CSV\n");
    printf("  --quiet               Solo mostrar resumen final\n");
    printf("  --panel               Mostrar panel completo en puntos clave\n\n");
    printf("EJEMPLOS:\n");
    printf("  %s\n", prog_name);
    printf("  %s --profile intercept --noise --dt 0.5\n", prog_name);
    printf("  %s --profile custom --alt 9000 --mach 0.85\n", prog_name);
}

static void simconfig_defaults(SimConfig *cfg)
{
    memset(cfg, 0, sizeof(SimConfig));
    cfg->profile = PROFILE_CRUISE;
    cfg->custom_alt_m = 0.0;
    cfg->custom_mach = 0.0;
    cfg->noise_enabled = 0;
    cfg->noise_seed = PROG_DEFAULT_SEED;
    cfg->dt_s = PROG_DEFAULT_DT;
    cfg->csv_enabled = 1;
    cfg->quiet = 0;
    cfg->show_panel = 0;
    strncpy(cfg->output_file, PROG_DEFAULT_OUTPUT,
            sizeof(cfg->output_file) - 1);
}

// Procesa argumentos: 0 ok, 1 ayuda, -1 error.
static int parse_args(int argc, char *argv[], SimConfig *cfg)
{
    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--help") == 0 ||
            strcmp(argv[i], "-h") == 0) {
            return 1;
        }
        else if (strcmp(argv[i], "--profile") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: --profile requiere un argumento\n");
                return -1;
            }
            i++;
            if (strcmp(argv[i], "cruise") == 0) cfg->profile = PROFILE_CRUISE;
            else if (strcmp(argv[i], "intercept") == 0) cfg->profile = PROFILE_INTERCEPT;
            else if (strcmp(argv[i], "custom") == 0) cfg->profile = PROFILE_CUSTOM;
            else {
                fprintf(stderr, "ERROR: perfil desconocido '%s'\n", argv[i]);
                fprintf(stderr, "       Opciones: cruise, intercept, custom\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "--alt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: --alt requiere un valor en metros\n");
                return -1;
            }
            cfg->custom_alt_m = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--mach") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: --mach requiere un valor numérico\n");
                return -1;
            }
            cfg->custom_mach = atof(argv[++i]);
            if (cfg->custom_mach < 0.0 || cfg->custom_mach > 3.0) {
                fprintf(stderr, "WARN: Mach %.2f fuera del rango típico [0, 3]\n",
                        cfg->custom_mach);
            }
        }
        else if (strcmp(argv[i], "--noise") == 0) {
            cfg->noise_enabled = 1;
        }
        else if (strcmp(argv[i], "--seed") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: --seed requiere un valor entero\n");
                return -1;
            }
            cfg->noise_seed = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--dt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: --dt requiere un valor en segundos\n");
                return -1;
            }
            cfg->dt_s = atof(argv[++i]);
            if (cfg->dt_s <= 0.0 || cfg->dt_s > 10.0) {
                fprintf(stderr, "ERROR: --dt debe estar en (0, 10] segundos\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: --output requiere un nombre de archivo\n");
                return -1;
            }
            strncpy(cfg->output_file, argv[++i],
                    sizeof(cfg->output_file) - 1);
        }
        else if (strcmp(argv[i], "--no-csv") == 0) {
            cfg->csv_enabled = 0;
        }
        else if (strcmp(argv[i], "--quiet") == 0) {
            cfg->quiet = 1;
        }
        else if (strcmp(argv[i], "--panel") == 0) {
            cfg->show_panel = 1;
        }
        else {
            fprintf(stderr, "ERROR: argumento desconocido '%s'\n", argv[i]);
            fprintf(stderr, "       Use --help para ver opciones\n");
            return -1;
        }
    }
    return 0;
}

// Traduce SimConfig a CadcConfig.
static void setup_cadc_config(const SimConfig *sim, CadcConfig *cadc)
{
    cadc_config_init(cadc);
    cadc->wing_auto = 1;
    cadc->log_enabled = 0;  // El registro se hace con logger.
    cadc->update_rate_hz = 1.0 / sim->dt_s;

    // Configura el perfil de vuelo.
    switch (sim->profile) {
        case PROFILE_CRUISE:
            cadc->sensor_cfg.mode = SENSOR_MODE_PROFILE;
            flight_profile_load_f14_cruise(&cadc->sensor_cfg.profile);
            break;

        case PROFILE_INTERCEPT:
            cadc->sensor_cfg.mode = SENSOR_MODE_PROFILE;
            flight_profile_load_f14_intercept(&cadc->sensor_cfg.profile);
            break;

        case PROFILE_CUSTOM:
            sensors_set_constant(&cadc->sensor_cfg,
                                  sim->custom_alt_m,
                                  sim->custom_mach);
            break;
    }

    // Activa ruido si se solicita.
    if (sim->noise_enabled) {
        sensors_enable_noise(&cadc->sensor_cfg, sim->noise_seed);
        // En perfiles, mantener modo NOISY con el perfil cargado.
        if (sim->profile != PROFILE_CUSTOM) {
            cadc->sensor_cfg.mode = SENSOR_MODE_NOISY;
        }
    }
}

// Devuelve la duración del perfil en segundos.
static double get_profile_duration(const SimConfig *sim)
{
    switch (sim->profile) {
        case PROFILE_CRUISE: return 1400.0;
        case PROFILE_INTERCEPT: return 1080.0;
        case PROFILE_CUSTOM: return 60.0;
        default: return 1400.0;
    }
}

static const char *get_profile_name(const SimConfig *sim)
{
    switch (sim->profile) {
        case PROFILE_CRUISE: return "Crucero F-14";
        case PROFILE_INTERCEPT: return "Intercepción F-14";
        case PROFILE_CUSTOM: return "Condición custom";
        default: return "Desconocido";
    }
}

    // Imprime la cabecera de la simulación.
static void print_sim_header(const SimConfig *sim)
{
    printf("\n");
    printf("CADC F-14 Tomcat - Simulacion v%s\n", PROG_VERSION);
    printf("Perfil: %s\n", get_profile_name(sim));

    if (sim->profile == PROFILE_CUSTOM) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f m / Mach %.3f",
                 sim->custom_alt_m, sim->custom_mach);
        printf("Condicion: %s\n", buf);
    }

    printf("Ruido: %s\n",
           sim->noise_enabled ? "Activado (gaussiano)" : "Desactivado");
    printf("Paso dt: %.2f s\n", sim->dt_s);
    printf("CSV: %s\n",
           sim->csv_enabled ? sim->output_file : "(desactivado)");
    printf("\n");
}

// Imprime la cabecera de la tabla de telemetría.
static void print_flight_log_header(void)
{
    printf("t[s]  Mach   Alt[ft]  TAS[kt]  CAS[kt]  VSI[ft/min]  Sweep[deg]  Estado\n");
}

// Función principal.

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Paso 1: parsear argumentos.

    SimConfig sim;
    simconfig_defaults(&sim);

    int parse_result = parse_args(argc, argv, &sim);
    if (parse_result == 1) {
        print_help(argv[0]);
        return 0;
    }
    if (parse_result == -1) {
        fprintf(stderr, "Use --help para ver opciones de uso.\n");
        return 1;
    }

    // Paso 2: imprimir cabecera.

    if (!sim.quiet) {
        print_sim_header(&sim);
    }

    // Paso 3: inicializar subsistemas.

    CadcConfig cadc_cfg;
    CadcState cadc_state;
    LoggerState logger;

    setup_cadc_config(&sim, &cadc_cfg);
    cadc_state_init(&cadc_state);

    const char *csv_path = sim.csv_enabled ? sim.output_file : NULL;
    if (logger_init(&logger, csv_path, LOG_WARN) != 0) {
        fprintf(stderr, "ERROR: No se pudo inicializar el logger.\n");
        fprintf(stderr, "       Verifica que el directorio 'output/' existe.\n");
        return 1;
    }

    // Paso 4: mostrar cabecera de tabla.

    if (!sim.quiet) {
        print_flight_log_header();
    }

    // Paso 5: ejecutar bucle principal.

    double t_end = get_profile_duration(&sim);
    int show_step = (int)(t_end / (sim.dt_s * 30.0));
    if (show_step < 1) show_step = 1;

    // Tiempos para mostrar el panel cuando se usa --panel.
    double panel_times_cruise[] = {0, 300, 800, 1350, -1};
    double panel_times_intercept[] = {0, 180, 500, 1080, -1};
    double panel_times_custom[] = {0, 30, -1};
    double *panel_times = panel_times_cruise;
    if (sim.profile == PROFILE_INTERCEPT) panel_times = panel_times_intercept;
    if (sim.profile == PROFILE_CUSTOM) panel_times = panel_times_custom;

    int panel_idx = 0;  // Índice en panel_times.

    int cycle = 0;
    double t = 0.0;

    for (t = 0.0; t <= t_end + sim.dt_s * 0.5; t += sim.dt_s) {

        // Ejecuta un ciclo del CADC.
        cadc_update(&cadc_cfg, t, &cadc_state);

        // Registra el ciclo en logger.
        logger_log_cycle(&logger, &cadc_state);

        // Muestra telemetría en terminal.
        if (!sim.quiet) {
            // Limita impresión para evitar saturar terminal.
            if (cycle % show_step == 0 || cadc_state.alerts.any_alert) {
                const AirDataState *ad = &cadc_state.airdata;
                const WingState *wg = &cadc_state.wings;

                if (ad->valid) {
                    printf("%7.1f  %5.3f  %7.0f  %6.1f  %6.1f  %+8.0f  %5.1f  %s\n",
                           t,
                           ad->mach,
                           ad->altitude_ft,
                           ad->tas_kt,
                           ad->cas_kt,
                           ad->vsi_ftmin,
                           wg->sweep_deg,
                           cadc_state.alerts.any_alert ? "ALERTA" : "OK");
                } else {
                    printf("%7.1f  INVALID\n", t);
                }
            }
        }

        // Muestra panel en puntos clave si se usa --panel.
        if (sim.show_panel && panel_times[panel_idx] >= 0) {
            double pt = panel_times[panel_idx];
            if (t >= pt && t < pt + sim.dt_s * 1.5) {
                printf("\n");
                cadc_print_panel(&cadc_state);
                printf("\n");
                panel_idx++;
            }
        }

        cycle++;
    }

    // Paso 6: cerrar logger y terminar.

    printf("\n");
    logger_close(&logger);

    return 0;
}
