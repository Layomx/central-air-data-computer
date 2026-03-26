/*
    Pruebas integradoras del CADC, ya se podran dar cuenta que este es mi debug

    TEST 1: Verificacion de las alas de geometria variable
    TEST 2: Verificacion del fix de CAS en todo el rango de vuelo
    TEST 3: Simulacion de vuelo completa con un perfil de crucero
    TEST 4: Simulacion de intercepcion para revisar alas y alertas
    TEST 5: Panel de instrumentos en momentos clave del vuelo
*/

#include <stdio.h>
#include <math.h>
#include "../src/cadc.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    printf("CADC F-14: Integrador CADC completo\n");
    printf("Pruebas de verificacion\n\n");

    // Prueba 1: Alas de geometria variable

    printf("Prueba 1: Alas de geometria variable F-14\n");
    printf("%-8s %-12s %-10s\n", "Mach", "Sweep [°]", "Zona");

    struct {
        double mach;
        double expected;
        const char *zona;
    } wing_tests[] = {
        {0.00, 20.0, "Extendidas"},
        {0.20, 20.0, "Extendidas"},
        {0.40, 20.0, "Trans 1 inicio"},
        {0.60, 27.5, "Trans 1 medio"},
        {0.80, 35.0, "Trans 2 inicio"},
        {0.90, 42.5, "Trans 2 medio"},
        {1.00, 50.0, "Supersonico inicio"},
        {1.50, 58.9, "Supersonico medio"},
        {2.00, 63.8, "Supersonico alto"},
        {2.34, 68.0, "Maximo"},
    };
    int n_wing = 10;

    for (int i = 0; i < n_wing; i++) {
        double sweep = cadc_calc_wing_sweep(wing_tests[i].mach);
        double err = fabs(sweep - wing_tests[i].expected);
        printf("M=%5.2f  sweep=%-8.2f  exp=%-6.1f  err=%.2f  [%s]  %s\n", wing_tests[i].mach, sweep, wing_tests[i].expected, err, wing_tests[i].zona, (err < 1.0) ? "CORRECTO" : "INCORRECTO"); 
    }

    // Prueba 2: Verificacion del fix de CAS
    printf("\nPrueba 2: Verificar del fix de CAS\n");
    printf("%-28s %8s %8s %8s %8s\n", "Condición", "Mach", "TAS[kt]", "CAS[kt]", "EAS[kt]");

    CadcConfig cfg;
    CadcState state;

    struct {
        double alt_ft;
        double mach;
        const char *label;
    } cas_tests[] = {
          {0.0, 0.30, "Nivel mar  M=0.30"},
        {  10000.0, 0.60, "10,000ft   M=0.60"},
        {  30000.0, 0.85, "30,000ft   M=0.85"},
        {  39370.0, 1.40, "39,370ft   M=1.40"},
        {  50000.0, 2.00, "50,000ft   M=2.00"},
    };
    int n_cas = 5;

    for (int i = 0; i < n_cas; i++) {
        cadc_config_init(&cfg);
        cadc_state_init(&state);

        double h_m = cas_tests[i].alt_ft * 0.3048;
        sensors_set_constant(&cfg.sensor_cfg, h_m, cas_tests[i].mach);

        cadc_update(&cfg, 0.0, &state);

        printf("%-28s %8.3f %8.1f %8.1f %8.1f  %s\n",
               cas_tests[i].label,
               state.airdata.mach,
               state.airdata.tas_kt,
               state.airdata.cas_kt,
               state.airdata.eas_kt,
               (state.airdata.cas_kt > 1.0) ? "CORRECTO" : "INCORRECTO CAS=0 BUG");
    }

    printf("\nA nivel del mar: TAS ~ CAS ~ EAS (densidad = p0)\n");
    printf("En altitud: TAS > CAS > EAS (deben divergir con la altitud)\n");

    // Prueba 3: Simulacion de vuelo completa con peril crucero
    printf("\nPrueba 3: Vuelo completo con perfil crucero\n");

    cadc_config_init(&cfg);
    cadc_state_init(&state);

    cfg.sensor_cfg.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_cruise(&cfg.sensor_cfg.profile);
    cfg.wing_auto = 1;

    printf("%-8s %-6s %-8s %-8s %-8s %-9s %-7s %-6s\n",
           "t[s]", "Mach", "Alt[ft]", "TAS[kt]",
           "CAS[kt]", "VSI[f/m]", "Sw[°]", "Alert");

    double dt_sim = 50.0;
    for (double t = 0.0; t <= 1400.0; t += dt_sim) {
        cadc_update(&cfg, t, &state);

        if (!state.airdata.valid) {
            printf("%8.1f | INVALIDO\n", t);
            continue;
        }

        printf("%8.1f  %5.3f  %7.0f  %7.1f  %7.1f  %+8.0f  %5.1f  %s\n",
               t,
               state.airdata.mach,
               state.airdata.altitude_ft,
               state.airdata.tas_kt,
               state.airdata.cas_kt,
               state.airdata.vsi_ftmin,
               state.wings.sweep_deg,
               state.alerts.any_alert ? "ALERT" : "OK");
    }

    // Prueba 4: Perfil de intercepcion
    printf("\nPrueba 4: Intercepcion, alas y alertas en maxima velocidad\n");

    cadc_config_init(&cfg);
    cadc_state_init(&state);

    cfg.sensor_cfg.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_intercept(&cfg.sensor_cfg.profile);
    cfg.wing_auto = 1;

    printf("Momentos clave de la interpcecion:\n\n");

    double key_times[] = {0, 90, 180, 350, 500, 650, 800, 1000, 1080};
    int n_keys = 9;

    for (int i = 0; i < n_keys; i++) {
        cadc_config_init(&cfg);
        cadc_state_init(&state);
        cfg.sensor_cfg.mode = SENSOR_MODE_PROFILE;
        flight_profile_load_f14_intercept(&cfg.sensor_cfg.profile);

        double prev_t = 0.0;
        for (double t = 0.0; t <= key_times[i]; t += 10.0) {
            cadc_update(&cfg, t, &state);
            prev_t = t;
        }
        (void)prev_t;

        printf("t=%4.0fs | M%.3f | %5.0fft | TAS%6.1f | CAS%6.1f"
               " | sw%4.1f° | %s\n",
               key_times[i],
               state.airdata.mach,
               state.airdata.altitude_ft,
               state.airdata.tas_kt,
               state.airdata.cas_kt,
               state.wings.sweep_deg,
               state.alerts.any_alert ? "ALERTA" : "OK");
    }

    // Prueba 5: Panel en momentos clave
    printf("\nPrueba 5: Panel de instrumentos en crucero supersonico\n");

    cadc_config_init(&cfg);
    cadc_state_init(&state);
    sensors_set_constant(&cfg.sensor_cfg, 12000.0, 1.4);
    cfg.wing_auto = 1;

    cadc_update(&cfg, 800.0, &state);
    cadc_print_panel(&state);

    printf("Prueba 5b: Aproximacion a portaaviones\n");

    cadc_config_init(&cfg);
    cadc_state_init(&state);
    sensors_set_constant(&cfg.sensor_cfg, 150.0, 0.28);
    cfg.wing_auto = 1;

    cadc_update(&cfg, 0.0, &state);
    cadc_print_panel(&state);

    // Prueba 6: Invariantes del sistem completo
    printf("\nPrueba 6: Invariantes del sistema CADC\n");

    cadc_config_init(&cfg);
    cadc_state_init(&state);
    cfg.sensor_cfg.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_intercept(&cfg.sensor_cfg.profile);

    int violations_sweep = 0;
    int violations_cas = 0;
    int total = 0;

    for (double t = 0.0; t <= 1080.0; t += 5.0) {
        cadc_update(&cfg, t, &state);
        if (!state.airdata.valid) continue;
        total++;

        // Sweep siepre en 20 a 68 grados
        if (state.wings.sweep_deg < F14_WING_MIN_DEG - 0.01 ||
            state.wings.sweep_deg > F14_WING_MAX_DEG + 0.01) {
            violations_sweep++;
        }

        // CAS nunca negativa
        if (state.airdata.cas_kt < 0.0) {
            violations_cas++;
        }
    }

    printf("Invariante sweep (20, 68): %d checks, %d violaciones %s\n", total, violations_sweep, violations_sweep == 0 ? "CORRECTO": "INCORRECTO");

    printf("Invariante CAS >= 0: %d checks, %d violaciones  %s\n", total, violations_cas, violations_cas == 0 ? "CORRECTO" : "INCORRECTO");

    printf("\nPruebas completas\n\n");

    return 0;
}