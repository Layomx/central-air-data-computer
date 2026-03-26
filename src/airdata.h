/*
    Calculos de Airdata Data Computer (CADC) del F-14 Tomcat

    Este archivo define el nucleo computacional del CADC. Toma lecturas crudas de sensores y produce parametros aerodinamicos que el sistema distribuira a:
    - Instrumentos de cabina
    - Sistemas de control de vuelo
    - Sistemas de armas
    - Sistema de geometria variable de las alas (mi favorita personal, amo el F-14 por eso)

    Se calcularan parametros como: 
    - Mach, numero de Mach - velocidad del aire relativa a la velocidad del sonido
    - TAS (Velocidad Aerea Verdadera) - velocidad real respecto al aire 
    - CAS (Velocidad Aerea Indicada) - velocidad calibrada
    - EAS (Velocidad Aerea Equivalente) - velocidad equivalente
    - Altitud - altitud barometrica
    - Densidad - densidad del aire derivada
    - Q_c - presion de impacto comprensible
*/

#ifndef AIRDATA_H
#define AIRDATA_H

#include "sensors.h"
#include "atmosphere.h"


// Constantes 

// Velocidad del sonido a nivel del mar ISA
#define AIRDATA_A0_MS 340.294

// Presion dinamica de referencia a nivel del mar
#define AIRDATA_P0_PA ISA_P0_SEA_LEVEL_PA

// Densidad de ferencia a nivel del mar
#define AIRDATA_RHO0 ISA_RHO0_SEA_LEVEL

// Tolerancia de convergencia para biseccion supersonica
#define AIRDATA_MACH_TOL 1e-6

// Maxima de iteracion en biseccion
#define AIRDATA_MAX_ITER 100

// Mach minimo considerado en vuelo (por debajo se considera "en tierra")
#define AIRDATA_MACH_MIN 0.001

// Estructuras

// AirDataState: el resultado completo de un ciclo de calculos del CADC. Esta estructura es lo que el CADC manda al resto del avion en cada ciclo de actualiacion, todos los sistemas que de alguna forma necesitan datos o consumen informacion leen de aqui, como los sistemas de control de vuelo, los indicadores, las alas variables, etc.

typedef struct {
    
    // Parametro central, el Mach
    double mach; // Numero de Mach

    // Velocidades
    double tas_ms; // True Airspeed
    double tas_kt; // True Airspeed en nudos
    double cas_ms; // Calibrated Airspeed
    double cas_kt; // Calibrated Airspeed en nudos
    double eas_ms; // Equivalent Airspeed
    double eas_kt; // Equivalent Airspeed en nudos

    // Altitud
    double altitude_m; // Altitud barometrica
    double altitude_ft; // Altitud barometrica en pies

    // asa de cambio de altitud
    double vsi_ms; // Variacion de altitud
    double vsi_ftmin; // Variacion de altitud en pies por minuto

    // Propiedades del aire
    double static_temp_K ; // Temperatura estatica en Kelvin
    double static_temp_C; // Temperatura estatica en grados Celsius
    double density_kg_m3; // Densidad del aire en kg/m^3
    double density_ratio; // Densidad relativa rho/rho0
    double speed_of_sound_ms; // Velocidad del sonido local

    // Presiones derivadas
    double dynamic_pressure_Pa; // Presion dinamica
    double impact_pressure_Pa; // Presion de impacto compresible

    // Estado de validaz
    int valid; // 1 si los calculos son validos, 0 si no
    int supersonic; // 1 si M >= 1
} AirDataState;

// Funciones publicas encargadas de calculos

/*
    airdata_compute()

    Toma una lectura de los sensores y produce el estado completo del CADC. Esta funcion se llama en cada ciclo del bucle principal.
*/
void airdata_compute(const SensorReading *reading, double prev_alt_m, double dt_s, AirDataState *state);

// Calculo de Mach a partir de la relacion Pt/Ps usando biseccion para el caso supersonico e inversion analitica para el caso subsónico
double airdata_calc_mach(double total_pressure_Pa, double static_pressure_Pa);

// Calculo de la altitud barometrica a partir de presion estatica
double airdata_calc_altitude(double static_pressure_Pa);

// Calculo de True Airspeed
double airdata_calc_tas(double mach, double temperatura_K);

// Calculo de Calibrated Airspeed
double airdata_calc_cas(double total_pressure_Pa, double static_pressure_Pa);

// Calculo de Equivalente Airspeed
double airdata_calc_eas(double tas_ms, double density_kg_m3);

// Funcion de display
void airdata_print(const AirDataState *state);

// Funcion de impresion de una lineaa compacta del estado
void airdata_print_compact(double time_s, const AirDataState *state);

#endif