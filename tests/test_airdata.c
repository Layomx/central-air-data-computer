/*
    Este programa es para probar el modelo realizado para el calculo del airdata del CADC del F-14

    TEST 1: Verificacion de inversion de Mach
    TEST 2: Verificacion de inversion barometrica
    TEST 3: Verificacion de TAS, CAS, EAS en condiciones conocidas
    TEST 4: Ciclo del CADC completo
    TEST 5: Manejo de lecturas invalidas
*/

#include <stdio.h>
#include <math.h>
#include "../src/airdata.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Tolerancia para comparaciones de punto flotante
#define TOL_MACH   0.001
#define TOL_ALT_M  50.0    
#define TOL_KT     2.0      



// check() disponible para tests futuros de valores exactos*/
static void __attribute__((unused)) check(const char *label, double calc, double ref, double tol)
{
    double err = fabs(calc - ref);
    printf("  %-30s calc = %-10.4f  ref = %-10.4f  err = %-8.4f  %s\n", label, calc, ref, err, (err <= tol) ? "VALIDO" : "ERROR");
}

int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    printf("\n");
    printf("CADC F-14 — Pruebas del modelo de Airdata\n");

    printf("PRUEBA 1: Inversión de Mach\n");

    printf("%-8s %-12s %-12s %-10s\n",
           "M_true", "Pt/Ps calc", "M_recovered", "Error");
    printf("%-8s %-12s %-12s %-10s\n",
           "────────", "────────────", "────────────", "──────────");

    double test_machs[] = {0.0, 0.3, 0.5, 0.8, 0.9, 1.0, 1.2, 1.5, 2.0, 2.34};
    int n_machs = 10;
    double Ps_sea = 101325.0; 

    for (int i = 0; i < n_machs; i++) {
        double M_true = test_machs[i];
        double Pt = sensors_calc_total_pressure(Ps_sea, M_true);
        double M_rec = airdata_calc_mach(Pt, Ps_sea);
        double ratio = Pt / Ps_sea;
        char regime = (M_true < 1.0) ? 'S' : 'T';

        printf("[%c] M=%-5.2f  Pt/Ps=%-10.4f  M_rec=%-8.4f  err=%+.6f\n", regime, M_true, ratio, M_rec, M_rec - M_true);
    }

    printf("\nNota: un error < 1e-4 es excelente. Bisección converge en ~20 iter.\n");

    printf("PRUEBA 2: Altitud barométrica (inversión ISA)\n");

    printf("%-12s %-12s %-12s %-10s\n",
           "h_true [m]", "Ps [Pa]", "h_calc [m]", "Err [m]");
    printf("%-12s %-12s %-12s %-10s\n",
           "────────────", "────────────", "────────────", "──────────");

    double test_alts[] = {0, 500, 2000, 5000, 9144, 11000, 12000, 15000, 18288};
    int n_alts = 9;

    for (int i = 0; i < n_alts; i++) {
        double h_true = test_alts[i];
        double Ps = atmosphere_pressure(h_true);
        double h_calc = airdata_calc_altitude(Ps);
        double err = fabs(h_calc - h_true);
        char *layer = (h_true <= 11000.0) ? "TROPOSFERA" : "ESTRATOSFERA";

        printf("[%s] h=%-8.0f  Ps=%-12.2f  h_calc=%-10.1f  err=%.2f m\n", layer, h_true, Ps, h_calc, err);
    }


    printf("PRUEBA 3: TAS / CAS / EAS en condiciones del F-14\n");

    struct {
        double h_ft;
        double mach;
        const char *label;
    } cases[] = {
        { 0.0, 0.30, "Nivel mar  M=0.30"},
        { 10000.0, 0.60, "10,000ft   M=0.60"},
        { 30000.0, 0.85, "30,000ft   M=0.85 (crucero)"},
        { 40000.0, 1.40, "40,000ft   M=1.40 (supersónico)"},
        { 60000.0, 2.00, "60,000ft   M=2.00 (alta velocidad)"},
    };
    int n_cases = 5;

    printf("%-28s %8s %8s %8s %8s %8s\n",
           "Condición", "Mach", "TAS[kt]", "CAS[kt]", "EAS[kt]", "Alt[ft]");
    printf("%-28s %8s %8s %8s %8s %8s\n",
           "────────────────────────────",
           "────────", "────────", "────────", "────────", "────────");

    for (int i = 0; i < n_cases; i++) {
        double h_m  = cases[i].h_ft * 0.3048;
        double Ps = atmosphere_pressure(h_m);
        double T = atmosphere_temperature(h_m);
        double Pt = sensors_calc_total_pressure(Ps, cases[i].mach);
        double rho = Ps / (AIR_GAS_CONSTANT_R * T);

        double tas = airdata_calc_tas(cases[i].mach, T);
        double cas = airdata_calc_cas(Pt, Ps);
        double eas = airdata_calc_eas(tas, rho);

        printf("%-28s %8.3f %8.1f %8.1f %8.1f %8.0f\n",
               cases[i].label,
               cases[i].mach,
               tas / 0.514444,
               cas / 0.514444,
               eas / 0.514444,
               cases[i].h_ft);
    }

    printf("\nObservacion: A nivel del mar TAS, CAS, EAS suelen tener valores similares. A gran altitud divergen.\n");
    printf("EAS siempre < TAS en altitud. CAS > EAS para M > 1 en altura.\n");

    printf("PRUEBA 4: Ciclo CADC completo\n");

    SensorConfig  config;
    SensorReading reading;
    AirDataState  state;
    AirDataState  prev_state;

    sensors_config_init(&config);
    config.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_cruise(&config.profile);

    printf("Log de vuelo:\n");
    printf("%-7s  %-6s  %-8s  %-8s  %-8s  %-10s\n",
           "t[s]", "Mach", "Alt[ft]", "TAS[kt]", "CAS[kt]", "VSI[ft/m]");
    printf("%-7s  %-6s  %-8s  %-8s  %-8s  %-10s\n",
           "───────", "──────", "────────", "────────", "────────", "──────────");

    double dt = 50.0;
    double prev_alt = 0.0;
    int first = 1;

    for (double t = 0.0; t <= 1400.0; t += dt) {
        sensors_read(&config, t, &reading);

        double dt_used = first ? 0.0 : dt;
        airdata_compute(&reading, prev_alt, dt_used, &state);

        airdata_print_compact(t, &state);

        prev_alt = state.altitude_m;
        first    = 0;
        (void)prev_state;
    }

    printf("PRUEBA 4b: Panel completo en crucero supersónico (t = 800s)\n");
    sensors_read(&config, 800.0, &reading);
    airdata_compute(&reading, 0.0, 0.0, &state);
    airdata_print(&state);

    printf("PRUEBA 5: Robustez ante lecturas inválidas\n");

    SensorReading bad_reading;
    bad_reading.total_pressure_Pa = 30000.0;
    bad_reading.static_pressure_Pa = 35000.0;
    bad_reading.temperature_K = 220.0;

    airdata_compute(&bad_reading, 0.0, 0.0, &state);
    printf("Lectura inválida (Pt < Ps): valid=%d  (esperado: 0)\n", state.valid);
    printf("%s\n", (state.valid == 0) ? "CORRECTO: CADC rechaza lectura" : "ERROR");

    // Temperatura inválida
    bad_reading.total_pressure_Pa = 40000.0;
    bad_reading.static_pressure_Pa = 30000.0;
    bad_reading.temperature_K = -10.0;

    airdata_compute(&bad_reading, 0.0, 0.0, &state);
    printf("Lectura inválida (T < 0):   valid=%d  (esperado: 0)\n", state.valid);
    printf("%s\n", (state.valid == 0) ? "CORRECTO: CADC rechaza lectura" : "ERROR");

    printf("TEST 6: Invariantes físicos\n");

    sensors_config_init(&config);
    config.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_intercept(&config.profile);

    int eas_le_tas_violations = 0;
    int total_checks = 0;

    for (double t = 0.0; t <= 1080.0; t += 10.0) {
        sensors_read(&config, t, &reading);
        airdata_compute(&reading, 0.0, 0.0, &state);

        if (!state.valid) continue;
        total_checks++;

        if (state.eas_ms > state.tas_ms + 0.01) {
            eas_le_tas_violations++;
            printf("  VIOLACIÓN en t=%.0f: EAS=%.1f > TAS=%.1f\n",
                   t, state.eas_ms, state.tas_ms);
        }
    }

    printf("Invariante EAS <= TAS: %d checks, %d violaciones  %s\n",
           total_checks, eas_le_tas_violations,
           (eas_le_tas_violations == 0) ? "Correcto" : "Invalido");

    int tas_consistency_violations = 0;
    for (double t = 0.0; t <= 1080.0; t += 10.0) {
        sensors_read(&config, t, &reading);
        airdata_compute(&reading, 0.0, 0.0, &state);
        if (!state.valid) continue;

        double tas_expected = state.mach * state.speed_of_sound_ms;
        double err = fabs(state.tas_ms - tas_expected);
        if (err > 0.1) {  
            tas_consistency_violations++;
        }
    }

    printf("Invariante TAS = M*a:   %d checks, %d violaciones  %s\n",
           total_checks, tas_consistency_violations,
           (tas_consistency_violations == 0) ? "Correcto" : "Invalido");

    printf("\nPruba completada correctamente.\n\n");

    return 0;
}