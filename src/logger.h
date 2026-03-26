/* 
    Sistema de validacion y diagnostico del CADC

    Los archivos logger cumplen 3 funcionalidades separadas

    1. Logging a CSV
    Se escribira cada ciclo del CADC en un archivo .csv con todos los parametros de vuelo. El formato se espera que sea compatible con Excel, Python y cualquier herramienta que cumpla para el analisis de datos

    2. Validacion de rangos y coherencia fisica
    Se realizaran verificaciones fisicas para determinar si los parametros tienen coherencia o no, esto es muy distinto a las alertas que son limites operativos definidos para el F-14, lo que se verifica aqui son valores fisicamente imposibles, como el numero Mach negativo

    3. Diagnostico de sensores
    Se detectaran patrones de fallo en las lecturas, un valor malo puntual es puro ruido, diez ciclos consecutivos con malas lecturas indicaran que un sensor ha fallado o se ha desconectado.
*/

#ifndef LOGGER_H
#define LOGGER_H

#include "cadc.h"
#include <stdio.h>

// Constantes

// Ciclos consecutivos fuera de rango para declarar un fallo en el sensor
#define LOGGER_FAULT_THRESHOLD 10

//  Numero maximo de entradas en el buffer de log en memoria
#define LOGGER_BUFFER_SIZE 2048

// Enumeraciones

// Enumeracion que replica el esquema tipico de sistemas embebidos aeronauticos: INFO para operacion normal, WARN para anomalias, ERROR para fallos. Son niveles de severidad en un mensaje diagnostico
typedef enum {
    LOG_INFO = 0, // Operacion normal, datos de telemetria
    LOG_WARN = 1, // Anomalia detectda, sistema sigue operativo
    LOG_ERROR = 2 // Fallo confirmado, dato no confiable
} LogLevel;

// Resultado de validar un CadcState completo
typedef enum {
    VALIDATION_OK = 0, // Todos los parametros dentro de rango
    VALIDATION_WARN = 1, // Anomalia menor, dato probablemente util
    VALIDATION_INVALID = 2 // Dato invalido, no se debe usar
} ValidationResult;

// Estructuras

// Estado de diagnostico de los sensores, mantiene contadores consecutivos con lecturas anomalas cuando un contador supera LOGGER_FAULT_THRESHOLD se declara un fallo
typedef struct {
    // Ciclos consecutivos de los sensores
    int pitot_fault_cycles;
    int static_fault_cycles;
    int temp_fault_cycles;

    // 1 si se declara en fallo
    int pitot_failed;
    int static_failed;
    int temp_failed;

    int any_failed; // OR de los tres flags anteriores
} SensorFaultState;

// Resultado detallado de una validacion, flags que indican que parametro ha allado y por que
typedef struct {
    ValidationResult result; // Resultado global

    // Flags de que ha fallado
    int mach_negative;
    int mach_exceed;
    int temp_too_low;
    int temp_too_high;
    int cas_exceeds_tas;
    int eas_exceeds_tas;
    int negative_dynamics_pressure;
    int altitude_negative;
    int density_out_of_range;

    // Descripcion textual del primer fallo encontrado
    const char *first_fault_desc;
} ValidationReport;

// Estado completo del sistema de logging
typedef struct {
    // Archivo CSV
    FILE *csv_file; // Handle del archivo de salida
    int csv_open; // 1 si el archivo esa abierto
    char csv_filename[256]; // Nombre del archivo
    int csv_rows_written; // Filas escritas hasta ahora

    // Diagnostico de sensores
    SensorFaultState faults; // Estado de fallos por sensor

    // Estadisticas de vuelo
    double max_mach_seen; // Mach maximo registrado
    double max_alt_ft_seen; // Altitud maxima registrada
    double max_tas_kt_seen; // TAS maxima registrada
    double max_cas_kt_seen; // CAS maxima registrada
    int supersonic_cycles; // Ciclos en regimen supersonico
    int total_cycles; // Total de ciclos
    int invalid_cycles; // Ciclos con datos invalidos
    int alert_cycles; // Ciclos con alertas activas

    // Estado del logger
    int initialized; // 1 si el logger esta listo
    LogLevel min_level; // Nivel minimo para imprimir por stdout
} LoggerState;

// Funciones del ciclo de vida del logger

// Inicializacion y apertura del archivo CSV, escribe cabecera del CSV con los nombres de columna.
int logger_init(LoggerState *logger, const char *filename, LogLevel level);

// Cierra el archivo CSV e imprime
void logger_close(LoggerState *logger);

// Procesamiento y registro de un ciclo del CADC, diagnostica sensores, actualiza estadisticas, escriba filas en el CSV e imprime mensajes
void logger_log_cycle(LoggerState *logger, const CadcState *state);

// Impresion de un mensaje de diagnostico
void logger_print_message(const LoggerState *logger, LogLevel level, double time_s, const char *msg);

// Funcion de validacion de un CadcState compelto
void logger_validate(const CadcState *state, ValidationReport *report);

// Funcion que actualiza el diagnostico de sensores
void logger_update_fault_detection(LoggerState *logger, const SensorReading *reading);

// Impresion del resumen estadistico de vuelo
void logger_print_summary(const LoggerState *logger);

// Impresion del estado de diagnostico de sensores
void logger_print_fault_report(const LoggerState *logger);

#endif