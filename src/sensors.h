/*
    Simulacion de un sistema de sensores aerodinamicos del F-14 Tomcat
    Vision a futuro: Implementar un sistema de sensores reales con ruido y latencia, para simular el comportamiento del CADC real y escribir sus drivers de software

    Este archivo de cabecera simula los tres sensores fisicos que alimentaban el CADC del F-14: el Pitot, el Estatica y el Termometro. Estos sensores se utilizan para medir la velocidad del aire, la presion atmosferica y la temperatura respectivamente.

    Se definen 3 modos de operaciones para cada sensor:
    SENSOR_MODE_CONSTANT: El sensor devuelve valor fijos. Ideal para pruebas.
    SENSOR_MODE_PROFILE: Sigue una tabla de tiempo vs valor, util para simular maniobras aerodinamicas.
    SENSOR_MODE_NOISY: Cualquier modo sumado con ruido aleatorio, para simular la realidad de los sensores fisicos.

    Arquiectura del codigo:
    En vez de modelar cada sensor por separado, se define una abstraccion mas util: el sensor recibe un "perfil de vuelo" (altitud + Mach en el tiempo) y deriva internamente los valores de la presion y temperaturas correctas utilizando el modelo atmosferico ISA. Esto permite simular el comportamiento de los sensores en cualquier maniobra aerodinamica, sin necesidad de modelar cada sensor por separado. Esto le da una capa de realismo al modelo, ya que no se inventan valores garantizando que Pt, Ps y T sean fisicamente consistentes entre si.

    Ruido Gaussiano (por mejorar, implementacion "basica"):
    Para el ruido Gaussiano se utilizara el algoritmo Box-Muller para convertir dos valores uniformes en un valor con distribucion normal. 
    
    Z = sqrt(-2 * ln(U1)) * cos(2 * pi * U2)

    Donde U1 y U2 son numeros aleatorios uniformes entre 0 y 1, luego: valor_ruidoso = valor_real + Z * desviacion_estandar
*/

#ifndef SENSORS_H
#define SENSORS_H

#include "atmosphere.h"

// Constantes para el modelo de sensores
#define SENSOR_MAX_PROFILE_POINTS 64            // Maximo numero de puntos en el perfil de vuelo (altitud + Mach) para cada sensor
#define SENSOR_DEFAULT_PRESSURE_NOISE_PA 15.0     // Ruido tipico en la medicion de presion estatica [Pa]
#define SENSOR_DEFAULT_TEMP_NOISE_K 0.5         // Ruido tipico en la medicion de temperatura [K]

// Enumeraciones

// Modos de operacion de los sensores, para simular diferentes escenarios de vuelo y condiciones de los sensores
typedef enum {
    SENSOR_MODE_CONSTANT = 0, // Valor fijo en el tiempo
    SENSOR_MODE_PROFILE  = 1, // Interpolacion sobre tabla temporal
    SENSOR_MODE_NOISY    = 2  // Perfil o constante + ruido aleatorio
} SensorMode;

// Estructuras

// Perfil de vuelo: (tiempo, altitud, mach) para simular maniobras aerodinamicas y condiciones de vuelo variables
typedef struct {
    double time_s;      // Tiempo en segundos desde que inicia el vuelo
    double altitude_m;  // Altitud en metros
    double mach;        // Numero de Mach
} FlightProfilePoint;

// Configuracion de ruido para sensores individuales
typedef struct {
    double sigma; // Desviacion estandar
    int seed; 
} NoiseConfig;

// Perfil de vuelo completo: secuencia temporal de las condiciones
// Contiene un array de puntos y su conteo para interpolar linealmente entre puntos consecutivos para obtener valores continuos
typedef struct {
    FlightProfilePoint points[SENSOR_MAX_PROFILE_POINTS];   // Tabla de puntos
    int count; // Numero de puntos en el perfil
} FlightProfile;

// Estructura principal del sensor y su estado completo en un instante
typedef struct {
    double time_s;          // Tiempo actual del sensor

    // Lectura de sensores (con ruido si esta activado)
    double total_pressure_Pa;   // Presion total medida por el Pitot [Pa]
    double static_pressure_Pa;  // Presion estatica medida por el Estatica [Pa]
    double temperature_K;       // Temperatura exterior medida por el Termometro [K]
    
    // Condiciones verdaderas (sin ruido) para referencia interna y calculos
    double true_altitude_m;    // Altitud verdadera en este instante [m]
    double true_mach;          // Mach verdadero en este instante
    double true_total_pressure_Pa;   // Presion total verdadera sin ruido [Pa]
    double true_static_pressure_Pa;  // Presion estatica verdadera sin ruido [Pa]
    double true_temperature_K;       // Temperatura verdadera sin ruido [K]

    // Presion dinamica derivada
    double dynamic_pressure_Pa; // Presion dinamica calculada a partir de las lecturas (Pt - Ps) [Pa]
} SensorReading; 

// Configuracion completa del sensor y sus configuraciones
typedef struct {
    SensorMode mode; // Modo de operacion global
    FlightProfile profile; // Perfil de vuelo para modos de perfil

    // Configuraciones de ruido para cada sensor, aplicables si el modo incluye ruido
    NoiseConfig pitot_noise;   // Configuracion de ruido para el Pitot (presion total)
    NoiseConfig static_noise; // Configuracion de ruido para presion estatica
    NoiseConfig temp_noise;     // Configuracion de ruido para temperatura

    // Para SENSOR_MODE_CONSTANT, valores fijos a retornar
    double const_altitude_m; // Altitud fija para modo constante
    double const_mach;       // Mach fijo para modo constante
} SensorConfig;

// Funciones

// Funcion que inicializa la configuracion del sensor con valores por defecto
void sensors_config_init(SensorConfig *config);

// Funcion de configuracion del modo constante
void sensors_set_constant(SensorConfig *config, double altitude_m, double mach);

// Funcion que activa el ruido en todos los sensores
void sensors_enable_noise(SensorConfig *config, int seed);

// Funcion que desactiva el ruido en todos los sensores
void sensors_disable_noise(SensorConfig *config);

// Funcion que inicializa un perfil de vuelo vacio
void flight_profile_init(FlightProfile *profile);

// Funcion que agrega un punto al perfil de vuelo, return 0 si se agrego correctamente, -1 si se alcanzo el maximo de puntos
int flight_profile_add_point(FlightProfile *profile, double time_s, double altitude_m, double mach);

// Funcion que carga el perfil de crucero tipico del F-14 Tomcat
void flight_profile_load_f14_cruise(FlightProfile *profile);

// Funcion que carga el perfil de intercepcion/combate del F-14 Tomcat
void flight_profile_load_f14_intercept(FlightProfile *profile);

/*
Lectura de sensores
Dado un tiempo simulado y una configuracion, se producen las lecturas de los tres sensores.
Internamente se interpola altitud y mach del perfil, calcula estado ISA en la altitud dada, deriva Pt a partir de Ps y Mach y luego se le agrega ruido si esta activado. El resultado es un SensorReading completo con las lecturas ruidosas y los valores verdaderos para referencia interna.
*/
void sensors_read(const SensorConfig *config, double time_s, SensorReading *out);

// Funcion de impresion del estado del sensor en formato diagnostico
void sensors_print(const SensorReading *reading);

// Funcion de calculo de la presion total dada la presion estatica y el Mach, utilizando las ecuaciones de la aerodinamica
/* 
Para M<1 (subsónico): ecuación de Bernoulli compresible
    Pt/Ps = (1 + (γ-1)/2 * M²)^(γ/(γ-1))
Para M≥1 (supersónico): ecuación de Rayleigh (onda de choque normal) 
    Pt/Ps = [(γ+1)²*M²/(4γ*M²-2(γ-1))]^(γ/(γ-1))*[(1-γ+2γ*M²)/(γ+1)]
*/
double sensors_calc_total_pressure(double static_pressure_Pa, double mach);

#endif