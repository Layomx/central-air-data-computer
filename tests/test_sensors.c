/*
    Este archivo contiene pruebas para verificar el correcto funcionamiento de los sensores de altitud y velocidad (Mach) en diferentes condiciones de vuelo. Se simulan perfiles de vuelo típicos del F-14 Tomcat, incluyendo ascensos, cruceros y descensos, para asegurar que los sensores respondan adecuadamente a cambios en la altitud y velocidad.

    TEST 1: Modo constante sin ruido: verificacion de valor exactos
    TEST 2: Ecuacion de pitot sub/supersonica: verificacion fisica
    TEST 3: Perfil de crucero F-14: verificacion de interpolacion temporal
    TEST 4: Perfil de intercepcion con ruido: verificacion de Box-Muller y comportamiento con ruido
    TEST 5: Tabla comparativa Mach vs Pt/Ps: verificacion fisica completa
*/

#include <stdio.h>
#include <math.h>
#include "../src/sensors.h"
#ifdef _WIN32
#include <windows.h>
#endif

int main (void) 
{
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    printf("CADC F-14 - Prueba de sensores\n");

    SensorConfig config;
    SensorReading reading;

    // TEST 1: Modo constante, nivel del mar, en reposo
    printf("Primera prueba: Modo constante, nivel del mar, en reposo\n");

    sensors_config_init(&config);
    sensors_set_constant(&config, 0.0, 0.0); // Altitud 0m, Mach 0
    sensors_read(&config, 0.0, &reading);
    sensors_print(&reading);

    printf("Verificacion: Pt debe ser igual a Ps, Mach 0\n");
    double diff = reading.total_pressure_Pa - reading.static_pressure_Pa;
    printf("Pt - Ps = %.4f Pa (esperado: 0.0000)\n", diff);
    printf("%s\n", (fabs(diff) < 0.01) ? "CORRECTO" : "ERROR");

    // TEST 2: Ecuacion de Pitot sub/supersonica
    printf("\nSegunda prueba: Relacion Pt/Ps vs Mach\n");
    printf("Mach   Calc     Ref      Err(%%)\n");
    printf("---------------------------------\n");

    // Valores de referencia estandar para aerodinamica
    struct { double mach; double ref_ratio; } ref[] = {
        {0.00,  1.0000},
        {0.30,  1.0638},
        {0.50,  1.1862},
        {0.80,  1.5243},
        {1.00,  1.8929},
        {1.50,  3.4133},
        {2.00,  5.6404},
        {2.34,  8.0747},   // Mach maximo del F-14 segun datos historicos
    };
    int n_ref = 8;

    double Ps_ref = 101325.0; // Presion estandar al nivel del mar en Pa

    for (int i = 0; i < n_ref; i++) {
        double M = ref[i].mach;
        double Pt = sensors_calc_total_pressure(Ps_ref, M);
        double ratio = Pt / Ps_ref;
        double err_pct = (ratio - ref[i].ref_ratio) / ref[i].ref_ratio * 100.0;
        char   regime = (M < 1.0) ? 'S' : 'T';  // S = subsónico, T = supersónico

        printf("[%c] M=%-5.2f  %-12.4f %-12.4f %+.3f%%\n",
            regime, M, ratio, ref[i].ref_ratio, err_pct);
    }
    printf("\nS = Subsonico  T = Supersonico\n");

    // TEST 3: Perfil de crucero F-14
    printf("\nTercera prueba: Perfil de crucero F-14 (sin ruido)\n");

    sensors_config_init(&config);
    config.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_cruise(&config.profile);

    printf("t[s]   alt[m]   mach    Ps[Pa]      Pt[Pa]      q[Pa]\n");
    printf("-----------------------------------------------------\n");

    for (double t = 0.0; t <= 1400.0; t += 100.0) {
        sensors_read(&config, t, &reading);

        printf("%4.0f   %6.0f   %5.3f   %10.1f   %10.1f   %8.1f\n",
            t,
            reading.true_altitude_m,
            reading.true_mach,
            reading.static_pressure_Pa,
            reading.total_pressure_Pa,
            reading.dynamic_pressure_Pa);
    }

    // TEST 4: Perfil de intercepcion con ruido
    printf("\nCuarta prueba: Perfil de intercepcion con ruido\n");

    sensors_config_init(&config);
    config.mode = SENSOR_MODE_PROFILE;
    flight_profile_load_f14_intercept(&config.profile);
    sensors_enable_noise(&config, 42); // Semilla fija para reproducibilidad

    printf("Primeras 10 lecturas con ruido durante el ascenso:\n");

    for (int i = 0; i < 10; i++) {
        double t = (double)i * 30.0; // Cada 30 segundos
        sensors_read(&config, t, &reading);
        sensors_print(&reading);
        printf("\n");
    }

    // TEST 5: Analisis estadistico de ruido 
    sensors_config_init(&config);
    sensors_set_constant(&config, 9000.0, 0.85); // Crucero tipico
    sensors_enable_noise(&config, 123);

    double sum_Pt = 0.0, sum_Ps = 0.0, sum_T = 0.0;
    double sum_Pt2 = 0.0, sum_Ps2 = 0.0, sum_T2 = 0.0;
    int N = 1000;

    for (int i = 0; i < N; i++) {
        sensors_read(&config, 0.0, &reading);
        sum_Pt += reading.total_pressure_Pa;
        sum_Ps += reading.static_pressure_Pa;
        sum_T += reading.temperature_K;
        
        sum_Pt2 += reading.total_pressure_Pa * reading.total_pressure_Pa;
        sum_Ps2 += reading.static_pressure_Pa * reading.static_pressure_Pa;
        sum_T2 += reading.temperature_K * reading.temperature_K;
    }

    double mean_Pt = sum_Pt / N;
    double mean_Ps = sum_Ps / N;
    double mean_T = sum_T / N;
    double sigma_Pt = sqrt(sum_Pt2 / N - mean_Pt * mean_Pt);
    double sigma_Ps = sqrt(sum_Ps2 / N - mean_Ps * mean_Ps);
    double sigma_T = sqrt(sum_T2 / N - mean_T * mean_T);

    // Valores verdaderos para comparar
    sensors_disable_noise(&config);
    sensors_read(&config, 0.0, &reading);

    printf("Condición: h=9000m, M=0.85 — N=%d muestras\n\n", N);

    printf("%-6s %12s %12s %10s %10s\n",
        "Sensor", "True", "Mean", "Sigma", "Sigma_ref");
    printf("-------------------------------------------------------\n");

    printf("%-6s %12.2f %12.2f %10.2f %10.2f\n", "Pt", reading.true_total_pressure_Pa, mean_Pt, sigma_Pt, SENSOR_DEFAULT_PRESSURE_NOISE_PA);

    printf("%-6s %12.2f %12.2f %10.2f %10.2f\n", "Ps", reading.true_static_pressure_Pa, mean_Ps, sigma_Ps, SENSOR_DEFAULT_PRESSURE_NOISE_PA);

    printf("%-6s %12.2f %12.2f %10.2f %10.2f\n", "T", reading.true_temperature_K, mean_T, sigma_T, SENSOR_DEFAULT_TEMP_NOISE_K);

    printf("\nPruebas completadas.\n");

    return 0; 
}