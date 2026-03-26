/*
    Este programa es de prueba y verifica que el modelo ISA produce valores correctos comparados con valores de tablas ya conocidos como:
    altura, temperatura, presion, densidad y velocidad del sonido a diferentes altitudes.

    Se usaran altitudes y referencias del F-14 ya que se simula su CADC (Central Air Data Computer) y el F-14 es un avion de combate que opera en altitudes altas, por lo tanto es importante verificar que el modelo ISA se comporte correctamente en esas condiciones.
    
    Crucero tipico: 10,000 metros (32,808 pies)
    Techo operativo: 18,300 metros (60,000 pies)
*/

#include <stdio.h>
#include "atmosphere.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(void)
{
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    printf("CADC F-14 - Prueba de verificacion de atmosfera ISA\n");

    // Primera prueba: verificacion en altitudes estandar

    printf("PRUEBA 1: Altitudes estandar\n");
    double test_altitudes[] = {0.0, 1000.0, 5000.0, 11000.0, 15000.0, 20000.0};
    int num_test = 6;

    AtmosphereState state;
    
    for (int i = 0; i < num_test; i++)
    {
        atmosphere_calculate(test_altitudes[i], &state);
        atmosphere_print_state(&state);
    }

    // Segunda prueba: altitudes operativas del F-14 en pies
    printf("\nPRUEBA 2: Altitudes operativas del F-14 Tomcat\n");

    printf("%-12s %-12s %-10s %-10s %-10s\n",
           "Alt [ft]", "Alt [m]", "T [K]", "P [Pa]", "a [kt]");
    printf("%-12s %-12s %-10s %-10s %-10s\n",
           "────────────", "────────────", "──────────",
           "──────────", "──────────");

    // Altitudes en pies tipicas del F-14: despegue (0 ft), crucero (32,000 ft), techo operativo (60,000 ft)
    double altitudes_ft[] = {
        0.0,
        10000.0,
        20000.0,
        30000.0,
        40000.0,
        50000.0,
        60000.0
    };
    int num_f14_tests = 7;

    for (int i = 0; i < num_f14_tests; i++)
    {
        double h_m = feet_to_meters(altitudes_ft[i]);
        atmosphere_calculate(h_m, &state);

        printf("%-12.0f %-12.1f %-10.2f %-10.1f %-10.1f\n",
               altitudes_ft[i],
               state.altitude_m,
               state.temperature_K,
               state.pressure_Pa,
               ms_to_knots(state.speed_of_sound_ms));
    }

    // Tercera prueba: variacion continua con perfil de ascenso
    printf("\nPRUEBA 3: Perfil de ascenso continuo 0 a 18,000 metros\n");
    printf("%-10s %-10s %-10s %-12s\n",
           "Alt [m]", "T [°C]", "P/P0", "a [m/s]");
    printf("%-10s %-10s %-10s %-12s\n",
           "──────────", "──────────", "──────────", "────────────");

    for (double h = 0.0; h <= 18000.0; h += 1000.0) {
        atmosphere_calculate(h, &state);
        printf("%-10.0f %-10.1f %-10.4f %-12.2f\n",
               h,
               state.temperature_K - 273.15,
               state.pressure_Pa / ISA_P0_SEA_LEVEL_PA,
               state.speed_of_sound_ms);
    }

    scanf("%*c");

    return 0;
}
