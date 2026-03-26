/*

	Modelo de Atmosfera Estandar Internacional (ISA) simplificado
	
	Esta cabecera implementa el modelo ISA que cubre capas relevantes de funcionamiento del F-14
	
	El CADC real utilizaba look-up e interpolacion. Esta implementacion usa ecuaciones cerradas de ISA 
	suficientes para realizar simulaciones con alta precision
	
	Unidades del sistema: SI
	
	- Altitud: metros (m)
	- Temperatura: Kelvin (K)
	- Presion: Pascales (Pa)
	- Densidad: kilogramo por metro cubico (kg/m^3)
	- Velocidad: metros por segundo (m/s)

*/

#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

// Constantes fisicas universales

#define AIR_GAS_CONSTANT_R 287.058		// Constante del gas especifica del aire seco [J/(kg*K)]
#define AIR_GAMMA 1.4					// Coeficiente adiabetico del aire [gamma, y]
#define GRAVITY_G 9.80665				// Aceleracion gravitacional [m/s^2]

// Constantes ISA al nivel del mar

#define ISA_T0_SEA_LEVEL_K 288.15		// Temperatura a nivel del mar [K]
#define ISA_P0_SEA_LEVEL_PA 101325.0 	// Presion en nivel del mar [Pa]
#define ISA_RHO0_SEA_LEVEL 1.225		// Densidad del aire a nivel del mar [kg/m^3]

// Constantes ISA troposfera (0 a 11,000 metros)

#define ISA_LAPSE_RATE 0.0065			// Gradiente de temperatura en troposfera [K/m]
#define ISA_TROPOPAUSE_ALT_M 11000.0	// Altitud maxima de troposfera [m]
#define ISA_T_TROPOPAUSE_K 216.65		// Temperatura en la troposfera [K]
#define ISA_P_TROPOPAUSE_PA 22632.1		// Presion en la troposfera [Pa] - calculado por ISA

// Constantes ISA estratosfera baja (11,000 a 20,000 metros)

#define ISA_MAX_ALT_M 20000.0

/* 
Estructura principal

Contiene todos los parametros atmosfericos para una altitud dadada
La estructura agrupa los resultados del calculo ISA. El CADC usaria
estos datos internamente para derivar parametros de vuelo
*/

typedef struct {
	double altitude_m;			// Altitud de entrada [m]
	double temperature_K;		// Temperatura estatica [K]
	double pressure_Pa;			// Presion estatica [Pa]
	double density_kg_m3;		// Densidad del aire [kg/m^3]
	double speed_of_sound_ms;	// Velocidad del sonido [m/s]
} AtmosphereState;

// Funciones

/* 
Funcion de calculo de la atmosfera. Selecciona automaticamente la capa atmosferica correcta (troposfera o estratosfera baja)
y aplica las ecuaciones ISA adecuadas.
*/
void atmosphere_calculate(double altitude_m, AtmosphereState *state);

/*
Funcion de calculo de la temperatura ISA de la atmosfera en base a altitud
*/
double atmosphere_temperature(double altitude_m);

/*
Funcion de calculo de la presion ISA para una altitud
*/

double atmosphere_pressure(double altitude_m);

/*
Funcion de calculo de la velocidad del sonido dado la temperatura
*/
double atmosphere_speed_of_sound(double temperature_K);

/*
Funciones de conversion de unidades
*/

double meter_to_feet(double meters);

double feet_to_meters(double feet);

double ms_to_knots(double ms);

/*
Funcion que imprime el estado atmosferico en forma legible (debug)
*/
void atmosphere_print_state(const AtmosphereState *state);

#endif