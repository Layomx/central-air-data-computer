/*
    El modulo CADC es el mas importante de todos, ya que conecta todos los anteriores en un sistema coherente que intenta replicar la arquitectura funcional del CADC real:
    Sensores -> Calculos -> Distribucion en el sistema

    Los subsistemas alimentados por el CADC son:
    - Indicadores de cabina
    - Sistema de alas de geometria variable
    - Sistema de control de vuelo
    - Sistema de alertas

    Arquitectura del ciclo de calculos: 
    El CADC opera en un bucle fijo, en cada ciclo ocurre:
    - Lectura de sensores
    - Validacion de lecturas
    - Calculos de airdata
    - Actualiza alas
    - Evalua alertas
    - Publica outpus 

    El F-14 real este ciclo corria a ~50 Hz o cada 20 ms
    En mi simulacion, el dt lo controla el caller
*/

#ifndef CADC_H
#define CADC_H

#include "airdata.h"
#include "sensors.h"

// Limites operativos del F-14, valores de referencia del NATOPS F-14 Flight Manual
#define F14_MACH_MAX 2.34  // Mach maximo estructural
#define F14_MACH_SUPERSONIC 1.00 // Umbral supersonico
#define F14_ALT_MAX_FT 60000.0 // Techo operativo en pies
#define F14_ALT_MAX_M 18288.0 // Techo operativo en metros
#define F14_CAS_MAX_KT 777.0 // Velocidd maxima CAS 
#define F14_CAS_LAND_KT 150.0 // Velocidad tipica de aproximacion

// Angulos de barrido de alas 
#define F14_WING_MIN_DEG 20.0 // Minimo barrido - despegue/aterrizaje
#define F14_WING_MAX_DEG 68.0 // Maximo barrido - velocidad maxima
#define F14_WING_CARRIER_DEG 75.0 // Posicion de almacenamientos en portaaviones

// Umbrales de Mach para transcion de barrido
#define F14_WING_M_LOW 0.40 // Por debajo: alas extendidas a 20 grados
#define F14_WING_M_MID 0.80 // Zona de transicion inicial
#define F14_WING_M_TRANS 1.00 // Cruce de la barrera del sonido
#define F14_WING_M_HIGH 2.34 // Mach maximo: alas a 68 grados

// Estructuras

/*
    Estado del sistema de alas de geometria variable.

    En el F-14 real, el sistema "Automatic Wing Geometry" usaba el Mach del CADC para interactuar con los actuadores hidraulicos de las alas. El piloto podia sobreescribir esto manualmente pero en modo automatico el CADC controlaba esto por completo.
*/
typedef struct {
    double sweep_deg; // Angulo de barrido actual
    double sweep_prev_deg; // Angulo de barrido ciclo anterior
    double sweep_rate_dps; // Tasa de cambio
    int mode_auto; // 1 para automatico, 0 para manual piloto
    int at_min; // 1 si esta en barrido minimo (20 grados) 
    int at_max; // 1 si esta en barrido maximo (68 grados) 
} WingState; 

/* 
    Alertas ctivas del CADC. Realmente, el CADC generaba señales discretas que encendian luces en la cabina, yo las represento como flags booleanos
*/
typedef struct {
    int overspeed_cas; // Cas supera VMO (777 kt)
    int overspeed_mach; // Mach supera M_max (2.34)
    int altitude_max; // Altitud supera el techo operativo
    int sensor_invalid; // Lectura de sensor invalida
    int low_speed; // CAS < velocidad minima de vuelo segura
    int any_alert; // OR de todas las alertas anteriores
} AlertState;

// Configuracion del sistema CADC completo
typedef struct {
    SensorConfig sensor_cfg; // Configuracion del sistema de sensores
    int wing_auto; // Alas en automatico
    int log_enabled; // Habilita loggin por stdout
    double update_rate_hz; // Frecuencia de actualizacion 
} CadcConfig;

// Estado completo del CADC en un instante, esta estructura representa el bus de datos del CADC, todo subsistema que necesite informacion en algun momento la tomara de esta estructura
typedef struct {
    // Tiempo de simulacion
    double time_s; // Tiempo actual 
    double dt_s; // Intervalo desde ciclo anterior

    // Datos crudos del sensor
    SensorReading sensor; // Lectura de sensores en este ciclo

    // Parametros aerodinamicos calculados
    AirDataState airdata; // Estado completo de datos aereos

    // Subsistemas
    WingState wings; // Estado de las alas de geometria variable
    AlertState alerts; // Alertas activas

    // Historial necesario para VSI y tasas de cambio
    double prev_altitude_m; // Altitud del ciclo anterior
    int cycle_count; // Numero de ciclos desde inicio
} CadcState;

// Funciones publicas

// Funcion que inicializa configuracion con valores por defecto. Siempre debe llamarse antes que CadcConfig
void cadc_config_init(CadcConfig *cfg);

// Fucion que inicializa el estado del CADC a cero. Siempre llamar antes del primer ciclo
void cadc_state_init(CadcState *state);

// Funcion que ejecuta un ciclo completo del CADC, replica el ciclo de calculos del CADC real, lee sensores, calcula airdata y actualiza subsistemas
void cadc_update(const CadcConfig *cfg, double time_s, CadcState *state);

// Funcion que calcula el angulo de barrido optimo para un Mach especifico
double cadc_calc_wing_sweep(double mach);

// Funcion que evalua las condiciones de alerta
void cadc_eval_alerts(const AirDataState *airdata, AlertState *alerts);

// Funcion que imprime panel completo de instrumentos
void cadc_print_panel(const CadcState *state);

// Funcion que imprime una linea de telemetria. Formato continu para logging continuo de vuelo
void cadc_print_log_line(const CadcState *state);

#endif