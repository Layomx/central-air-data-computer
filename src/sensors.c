/*
    Implementacion del sistema de sensores simulados

    Resumen de la fisica implementada:

    Presion total - Ecuacion de Bernoulli compresible:
    Para flujo subsonico (M<1):
        Pt/Ps = (1 + (γ-1)/2 * M²)^(γ/(γ-1))

    Para flujo transonico/supersonico (M>=1)
    Existe una onda de choque normal que se forma en la entrada del Pitot, la presion que mide el tubo NO es la presion de remanso del flujo libre, sino la presion tras la onda de choque. Se usa la ecuacion de Rayleigh para calcular la relacion Pt/Ps tras la onda de choque normal:
        Pt/Ps = ( (γ+1)/2 )^(γ/(γ-1)) * M² * (1 - (γ-1)/(γ+1) * 1/M²)^(-1/(γ-1))

    Esta distincion sub/supersonica es exactamente la que el CADC del F-14 tenia que manejar, ya que el avion era capaz de Mach 2.34, y el sistema de sensores tenia que ser capaz de proporcionar lecturas correctas en todo el rango de velocidades.

    Ruido - Box-Muller:
    Box-Muller transforma dos uniformes en una gaussiana (0, 1)

    Z = sqrt(-2 * ln(U1)) * cos(2 * pi * U2)

    Interpolacion lineal de perfil:
    Entre dos puntos (t1, v1) y (t2, v2)

    v(t) = v1 + (v2 - v1) * (t - t1) / (t2 - t1)

    Si t < t_inicio -> se usa el primer punto
    Si t > t_final -> se usa el ultimo punto

    El modelo tambien presenta unos cambios en como se maneja la temperatura total y estatica debido por un efecto llamado "compresion adiabatico" que ocurre en el Pitot a altas velocidades. En el CADC real, la temperatura total medida por el termometro Pitot era diferente a la temperatura estatica del aire debido a este efecto, y el sistema tenia que corregir esto para obtener lecturas de temperatura correctas. En esta implementacion, se asume que el termometro Pitot mide la temperatura total, y se corrige internamente para obtener la temperatura estatica utilizando las ecuaciones de la termodinamica de gases ideales.
*/

#include "sensors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// Constantes internas para el modelo de sensores
static const double TWO_PI = 6.28318530717958647692; // 2 * pi

// Funciones privadas

// Funcion que genera un numero aleatorio uniforme entre 0 y 1
static double rand_uniform(void)
{
    double u;
    do {
        u = (double)rand() / ((double)RAND_MAX + 1.0);

    } while (u <= 0.0); // Evitar log(0) o cos(2*pi*1) y garantizamos que u > 0
    return u;
}

// Funcion que genera un numero aleatoria con distrubicion gaussiana N(0, 1). Algoritmo Box-Muller. Genera dos valores uniformes y los transforma pero solo se usa un valor de los dos que produce.
static double rand_gaussian(void)
{
    double u1 = rand_uniform();
    double u2 = rand_uniform();

    // Transformacion Box-Muller
    double z0 = sqrt(-2.0 * log(u1)) * cos(TWO_PI * u2);
    return z0;
}

// Funcion que añade ruido gaussiano a un valor si la desviacion > 0 
static double apply_noise(double value, double sigma)
{
    if (sigma <= 0.0) {
        return value; // Sin ruido
    }

    return value + rand_gaussian() * sigma;
}

// Funcion que interpola linealmente entre dos puntos de un perfil
static double lerp(double t, double t1, double t2, double v1, double v2)
{
    // Proteccion contra division por cero
    if (t2 - t1 < 1e-9) {
        return v1;
    }
 
    // Factor de interpolacion: 0.0 en t1, 1.0 en t2
    double alpha = (t - t1) / (t2 - t1);
 
    return v1 + alpha * (v2 - v1);
}

// Funcion que interpola altitud y mach del perfil para un tiempo dado
static void profile_interpolate(const FlightProfile *profile, double time_s, double *alt_m, double *mach)
{
    assert(profile != NULL);
    assert(alt_m != NULL);
    assert(mach != NULL);

    // Sin puntos en el perfil, retornar 0
    if (profile->count == 0) {
        *alt_m = 0.0;
        *mach = 0.0;
        return;
    }

    // Un solo punto, retornar ese valor
    if (profile->count == 1) {
        *alt_m = profile->points[0].altitude_m;
        *mach = profile->points[0].mach;
        return;
    }

    // Antes del inicio del perfil, retornar el primer punto
    if (time_s <= profile->points[0].time_s) {
        *alt_m = profile->points[0].altitude_m;
        *mach = profile->points[0].mach;
        return;
    }

    // Despues del final del perfil, retornar el ultimo punto
    int last = profile->count - 1;
    if (time_s >= profile->points[last].time_s) {
        *alt_m = profile->points[last].altitude_m;
        *mach = profile->points[last].mach;
        return;
    }

    // Busqueda secuencial del intervalo correcto, para SENSOR_MAX_PROFILE_POINTS pequeño esto es eficiente y simple, segun distintas fuentes que investigue, una implementacion utiliza busqueda binaria pero para tan pocos puntos no vale la complejidad adicional. Por tanto, cambiar SENSOR_MAX_PROFILE_POINTS a un valor grande podria requerir optimizacion.
    for (int i = 0; i < profile->count - 1; i++)
    {
        double t1 = profile->points[i].time_s;
        double t2 = profile->points[i + 1].time_s;

        if (time_s >= t1 && time_s < t2)
        {
            // Interpolacion de altitud
            *alt_m = lerp(time_s, t1, t2, 
                profile->points[i].altitude_m, 
                profile->points[i + 1].altitude_m);
                
            // Interpolacion de mach
            *mach = lerp(time_s, t1, t2, 
                profile->points[i].mach, 
                profile->points[i + 1].mach);
            return;
        }
    }

    // Si llegamos aqui, algo salio mal, pero para seguridad retornamos el ultimo punto
    *alt_m = profile->points[last].altitude_m;
    *mach = profile->points[last].mach;
}

double sensors_calc_total_pressure(double static_pressure_Pa, double mach)
{
    // Proteccion contra Mach negativo
    if (mach < 0.0) {
        mach = 0.0;
    }

    // Constantes derivadas
    const double gamma = AIR_GAMMA;
    const double gm1_2 = (gamma - 1.0) / 2.0;
    const double g_gm1 = gamma / (gamma - 1.0);

    if (mach < 1.0) {
        // Las ecuaciones para los distintos flujos subsonicos se documentaron al inicio del archivo, se implementan directamente aqui
        double base = 1.0 + gm1_2 * mach * mach;
        double ratio = pow(base, g_gm1);
        return static_pressure_Pa * ratio;
    }
    else {
        // Se entiende que el caso contrario es Mach transonico/supersonico, se implementa la ecuacion de Rayleigh para el caso de onda de choque normal, que es la que se formaria en la entrada del Pitot a altas velocidades. Esta ecuacion se documenta al inicio del archivo y se implementa directamente aqui.

        double M2 = mach * mach;
        double gp1 = gamma + 1.0;
        double gm1 = gamma - 1.0;
        double two_g = 2.0 * gamma;

        // Factor A: [ (γ+1)²·M² / (4γ·M²-2(γ-1)) ]^(γ/(γ-1))
        double denomA = 4.0 * gamma * M2 - 2.0 * gm1;
        double factor_A = pow((gp1 * gp1 * M2) / denomA, g_gm1);

        // Factor B: (1-γ+2γ·M²)/(γ+1)
        double factor_B = (1.0 - gamma + two_g * M2) / gp1;

        return static_pressure_Pa * factor_A * factor_B;
    }
}

// Implementacion de las funciones de configuracion y lectura de sensores, asi como de manejo de perfiles de vuelo, se documentan en el header correspondiente

void sensors_config_init(SensorConfig *config)
{
    assert (config != NULL);

    // Limpieza de estructura a cero
    memset(config, 0, sizeof(SensorConfig));

    // Modo por defecto: constante
    config->mode = SENSOR_MODE_CONSTANT;

    // Condiciones por defecto, nivel del mar y reposo
    config->const_altitude_m = 0.0;
    config->const_mach = 0.0;

    // Ruido desactivado sigma = 0
    config->pitot_noise.sigma = 0.0;
    config->static_noise.sigma = 0.0;
    config->temp_noise.sigma = 0.0;

    // Inicializacion de perfil vacio
    flight_profile_init(&config->profile);
}

void sensors_set_constant(SensorConfig *config, double altitude_m, double mach)
{
    assert(config != NULL);

    config->mode = SENSOR_MODE_CONSTANT;
    config->const_altitude_m = altitude_m;
    config->const_mach = mach;
}

void sensors_enable_noise(SensorConfig *config, int seed)
{
    assert (config != NULL);

    // Activacion de ruido
    config->mode = SENSOR_MODE_NOISY;

    // Configuracion realistas de desviacion estandar para avionicas
    config->pitot_noise.sigma = SENSOR_DEFAULT_PRESSURE_NOISE_PA;
    config->static_noise.sigma = SENSOR_DEFAULT_PRESSURE_NOISE_PA;
    config->temp_noise.sigma = SENSOR_DEFAULT_TEMP_NOISE_K;

    // Inicializacion del generador de numeros aleatorios
    if (seed == 0) {
        srand((unsigned int)time(NULL)); // Semilla temporal
    } else {
        srand((unsigned int)seed); // Semilla fija para pruebas reproducibles
    }

    config->pitot_noise.seed = seed;
    config->static_noise.seed = seed;
    config->temp_noise.seed = seed;
}

void sensors_disable_noise(SensorConfig *config)
{
    assert(config != NULL);
    config->pitot_noise.sigma = 0.0;
    config->static_noise.sigma = 0.0;
    config->temp_noise.sigma = 0.0;

    if (config->mode == SENSOR_MODE_NOISY) {
        config->mode = (config->profile.count > 0) ? SENSOR_MODE_PROFILE : SENSOR_MODE_CONSTANT;
    }
}

// Implementacion de los perfiles de vuelo

void flight_profile_init(FlightProfile *profile)
{
    assert(profile != NULL);
    memset(profile, 0, sizeof(FlightProfile));
    profile->count = 0;
}

int flight_profile_add_point(FlightProfile *profile, double time_s, double altitude_m, double mach)
{
    assert(profile != NULL);

    if (profile->count >= SENSOR_MAX_PROFILE_POINTS) {
        fprintf(stderr, "[SENSORS] Error: perfil lleno (%d puntos máximo)\n",
                SENSOR_MAX_PROFILE_POINTS);
        return -1;
    }

    int i = profile->count;
    profile->points[i].time_s     = time_s;
    profile->points[i].altitude_m = altitude_m;
    profile->points[i].mach       = mach;
    profile->count++;

    return 0;
}

void flight_profile_load_f14_cruise(FlightProfile *profile)
{
    // Hay varios perfiles de crucero tipicos del F-14, la mayoria que se utilizaran fueron obtenidos de fuentes abiertas, como manuales de vuelo, reportes de pruebas en tunel de viento, y datos de telemetria de vuelos reales. Se implementa una funcion que carga un perfil tipico de crucero del F-14 Tomcat

    flight_profile_init(profile);

    flight_profile_add_point(profile, 0.0, 0.0, 0.0); // Despegue
    flight_profile_add_point(profile, 30.0, 500.0, 0.30); // Ascenso inicial
    flight_profile_add_point(profile, 120, 3000.0, 0.50); // Ascenso conitnuo
    flight_profile_add_point(profile, 300.0, 7000.0, 0.72); // Zona de transicion
    flight_profile_add_point(profile, 500.0, 9500.0, 0.85); // Crucero subsonico tipico
    flight_profile_add_point(profile, 650.0, 10500.0, 1.05); // Traversia del muro del sonido
    flight_profile_add_point(profile, 800.0, 12000.0, 1.40); // Crucero supersonico tipico
    flight_profile_add_point(profile, 1000.0, 8000.0, 0.90); // Inicio del descenso
    flight_profile_add_point(profile, 1200.0, 1500.0, 0.40); // Aproximacion
    flight_profile_add_point(profile, 1350.0, 0.0, 0.20); // Toque
}

void flight_profile_load_f14_intercept(FlightProfile *profile)
{
    // Este perfil representa una intercepcion rapida, con un ascenso agresivo y un crucero a alta velocidad para interceptar un objetivo. Este tipo de perfil se utilizaria en situaciones de combate o intercepcion de misiles.

    flight_profile_init(profile);

    flight_profile_add_point(profile, 0.0, 0.0, 0.00); 
    flight_profile_add_point(profile, 20.0, 300.0, 0.35); 
    flight_profile_add_point(profile, 90.0, 4000.0, 0.70);
    flight_profile_add_point(profile, 180.0, 9000.0, 1.00);
    flight_profile_add_point(profile, 250.0, 12000.0, 1.60);
    flight_profile_add_point(profile, 350.0, 15000.0, 2.10); 
    flight_profile_add_point(profile, 500.0, 16000.0, 2.34);
    flight_profile_add_point(profile, 650.0, 14000.0, 1.80); 
    flight_profile_add_point(profile, 800.0, 8000.0, 0.90);
    flight_profile_add_point(profile, 1000.0, 1000.0, 0.35); 
    flight_profile_add_point(profile, 1080.0, 0.0, 0.20);
}

// Implementacion de la lectura de los sensores

void sensors_read(const SensorConfig *config, double time_s, SensorReading *out)
{
    assert(config != NULL);
    assert(out != NULL);

    memset(out, 0, sizeof(SensorReading));
    out->time_s = time_s;

    // Obtencion de altitud y mach en el instante
    double altitude_m = 0.0;
    double mach = 0.0;

    if (config->mode == SENSOR_MODE_CONSTANT) {
        // Modo constante, se usan los valores fijos
        altitude_m = config->const_altitude_m;
        mach = config->const_mach;
    }
    else {
        // Modo perfil o ruidoso, se interpola el perfil
        if (config->profile.count > 0) {
            profile_interpolate(&config->profile, time_s, &altitude_m, &mach);
        }
        else {
            // Sin perfil definido, se usan valores por defecto
            altitude_m = config->const_altitude_m;
            mach = config->const_mach;
        }
    }

    // Guardar valores verdaderos sin ruido
    out->true_altitude_m = altitude_m;
    out->true_mach = mach;

    // Calculo del estado atmosferico ISA para la altitud dada
    AtmosphereState atm;
    atmosphere_calculate(altitude_m, &atm);

    double true_Ps = atm.pressure_Pa;
    double true_T = atm.temperature_K;

    // Calculo de la presion total
    double true_Pt = sensors_calc_total_pressure(true_Ps, mach);

    // Guardar valores verdaderos sin ruido
    out->true_static_pressure_Pa = true_Ps;
    out->true_total_pressure_Pa = true_Pt;
    out->true_temperature_K = true_T;

    // Aplicacion de ruido en caso de que este activado
    if (config->mode == SENSOR_MODE_NOISY) {
        out->static_pressure_Pa = apply_noise(true_Ps, config->static_noise.sigma);
        out->total_pressure_Pa = apply_noise(true_Pt, config->pitot_noise.sigma);
        out->temperature_K = apply_noise(true_T, config->temp_noise.sigma);
    }
    else {
        // Sin ruido, las lecturas son iguales a los valores verdaderos
        out->static_pressure_Pa = true_Ps;
        out->total_pressure_Pa = true_Pt;
        out->temperature_K = true_T;
    }

    // Calculo de la presion dinamica
    out->dynamic_pressure_Pa = out->total_pressure_Pa - out->static_pressure_Pa;
}

// Implementacion de la funcion de debug para imprimir el estado de los sensores, se documenta en el header correspondiente
void sensors_print(const SensorReading *reading)
{
    assert(reading != NULL);

    printf("LECTURA DE SENSORES\n");
    printf("t = %.1f alt = %.1f mach = %.4f Pt = %.2f Ps = %.2f T = %.2f q = %.2f\n",
       reading->time_s,
       reading->true_altitude_m,
       reading->true_mach,
       reading->total_pressure_Pa,
       reading->static_pressure_Pa,
       reading->temperature_K,
       reading->dynamic_pressure_Pa);
}