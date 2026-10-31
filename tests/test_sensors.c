#include <math.h>
#include <stddef.h>

#include "../src/sensors.h"
#include "test_framework.h"

/*
 * Sensor verification tests.
 *
 * Scope:
 *   - Sensor configuration and nominal readings.
 *   - Subsonic and supersonic Pitot pressure calculations.
 *   - Flight profile interpolation and boundaries.
 *   - Built-in F-14 profiles.
 *   - Noise activation, reproducibility and statistics.
 *   - Flight profile capacity.
 *
 * Production code is not modified by these tests.
 */

/* ------------------------------------------------------------------------- */
/* Test identifiers                                                          */
/* ------------------------------------------------------------------------- */

#define REQ_INIT "SENS-REQ-001"
#define REQ_CONSTANT "SENS-REQ-002"
#define REQ_MACH_ZERO "SENS-REQ-003"
#define REQ_PITOT_SUBSONIC "SENS-REQ-004"
#define REQ_PITOT_SUPERSONIC "SENS-REQ-005"
#define REQ_PROFILE_EMPTY "SENS-REQ-006"
#define REQ_PROFILE_SINGLE "SENS-REQ-007"
#define REQ_INTERPOLATION "SENS-REQ-008"
#define REQ_PROFILE_LIMITS "SENS-REQ-009"
#define REQ_CRUISE "SENS-REQ-010"
#define REQ_INTERCEPT "SENS-REQ-011"
#define REQ_NOISELESS "SENS-REQ-012"
#define REQ_NOISE_ACTIVATION "SENS-REQ-013"
#define REQ_NOISE_DISABLE "SENS-REQ-014"
#define REQ_NOISE_REPRO "SENS-REQ-015"
#define REQ_NOISE_STATS "SENS-REQ-016"
#define REQ_READING_INVAR "SENS-REQ-017"
#define REQ_PROFILE_CAPACITY "SENS-REQ-018"

/* ------------------------------------------------------------------------- */
/* Statistical helpers                                                       */
/* ------------------------------------------------------------------------- */

/*
 * Welford's online algorithm.
 *
 * Avoids the numerical cancellation associated with:
 *
 *   variance = E[x^2] - E[x]^2
 *
 * This helper belongs to the test, not to the production implementation.
 */

typedef struct {
  unsigned long count;
  double mean;
  double m2;
} RunningStats;

static void stats_init(RunningStats *stats) {
  stats->count = 0UL;
  stats->mean = 0.0;
  stats->m2 = 0.0;
}

static void stats_add(RunningStats *stats, double value) {
  double delta;
  double delta2;

  stats->count++;

  delta = value - stats->mean;
  stats->mean += delta / (double)stats->count;
  delta2 = value - stats->mean;

  stats->m2 += delta * delta2;
}

static double stats_stddev(const RunningStats *stats) {
  if (stats->count < 2UL) {
    return 0.0;
  }

  return sqrt(stats->m2 / (double)(stats->count - 1UL));
}

/* ------------------------------------------------------------------------- */
/* General reading checks                                                    */
/* ------------------------------------------------------------------------- */

static void check_reading_finite(TestContext *ctx, const SensorReading *reading,
                                 const char *requirement_id) {
  TEST_CHECK_FINITE(ctx, requirement_id, reading->time_s,
                    "Reading time must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->true_altitude_m,
                    "True altitude must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->true_mach,
                    "True Mach must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->true_static_pressure_Pa,
                    "True static pressure must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->true_total_pressure_Pa,
                    "True total pressure must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->true_temperature_K,
                    "True temperature must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->static_pressure_Pa,
                    "Measured static pressure must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->total_pressure_Pa,
                    "Measured total pressure must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->temperature_K,
                    "Measured temperature must be finite");

  TEST_CHECK_FINITE(ctx, requirement_id, reading->dynamic_pressure_Pa,
                    "Dynamic pressure field must be finite");
}

/* ------------------------------------------------------------------------- */
/* Test 1: Configuration initialization                                      */
/* ------------------------------------------------------------------------- */

static void test_configuration_initialization(TestContext *ctx) {
  SensorConfig config;

  sensors_config_init(&config);

  TEST_CHECK_EQUAL_INT(ctx, REQ_INIT, config.mode, SENSOR_MODE_CONSTANT,
                       "Default sensor mode must be CONSTANT");

  TEST_CHECK_EQUAL_INT(ctx, REQ_INIT, config.profile.count, 0,
                       "Profile must initially be empty");

  TEST_CHECK_NEAR(ctx, REQ_INIT, config.const_altitude_m, 0.0, 0.0,
                  "Default altitude must be zero");

  TEST_CHECK_NEAR(ctx, REQ_INIT, config.const_mach, 0.0, 0.0,
                  "Default Mach must be zero");

  TEST_CHECK_NEAR(ctx, REQ_INIT, config.pitot_noise.sigma, 0.0, 0.0,
                  "Pitot noise must initially be disabled");

  TEST_CHECK_NEAR(ctx, REQ_INIT, config.static_noise.sigma, 0.0, 0.0,
                  "Static pressure noise must initially be disabled");

  TEST_CHECK_NEAR(ctx, REQ_INIT, config.temp_noise.sigma, 0.0, 0.0,
                  "Temperature noise must initially be disabled");
}

/* ------------------------------------------------------------------------- */
/* Test 2: Constant mode and sea-level conditions                            */
/* ------------------------------------------------------------------------- */

static void test_constant_sea_level(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  sensors_set_constant(&config, 0.0, 0.0);

  sensors_read(&config, 0.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_CONSTANT, reading.true_altitude_m, 0.0, 1e-12,
                  "True altitude at sea level");

  TEST_CHECK_NEAR(ctx, REQ_CONSTANT, reading.true_mach, 0.0, 1e-12,
                  "True Mach at rest");

  /*
   * ISA sea-level reference values.
   *
   * These are independent nominal reference values, not values obtained
   * by calling atmosphere_calculate() from the test.
   */

  TEST_CHECK_NEAR(ctx, REQ_CONSTANT, reading.true_static_pressure_Pa, 101325.0,
                  0.1, "ISA sea-level static pressure");

  TEST_CHECK_NEAR(ctx, REQ_CONSTANT, reading.true_temperature_K, 288.15, 0.01,
                  "ISA sea-level temperature");

  check_reading_finite(ctx, &reading, REQ_CONSTANT);
}

/* ------------------------------------------------------------------------- */
/* Test 3: Mach zero                                                         */
/* ------------------------------------------------------------------------- */

static void test_zero_mach(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  sensors_set_constant(&config, 0.0, 0.0);

  sensors_read(&config, 0.0, &reading);

  TEST_CHECK_NEAR(
      ctx, REQ_MACH_ZERO, reading.true_total_pressure_Pa,
      reading.true_static_pressure_Pa, 1e-9,
      "At Mach zero, true total pressure must equal true static pressure");

  TEST_CHECK_NEAR(
      ctx, REQ_MACH_ZERO, reading.total_pressure_Pa, reading.static_pressure_Pa,
      1e-9, "Without noise, measured total and static pressure must be equal");

  TEST_CHECK_NEAR(ctx, REQ_MACH_ZERO, reading.dynamic_pressure_Pa, 0.0, 1e-9,
                  "Pressure difference must be zero at Mach zero");
}

/* ------------------------------------------------------------------------- */
/* Test 4 and 5: Pitot pressure ratio                                         */
/* ------------------------------------------------------------------------- */

static void test_pitot_pressure_ratio(TestContext *ctx) {
  /*
   * Reference values for gamma = 1.4.
   *
   * Subsonic:
   *   Isentropic stagnation pressure relation.
   *
   * Supersonic:
   *   Rayleigh-Pitot relation for a normal shock ahead of the probe.
   *
   * Values are ratios Pt/Ps. They must be independently verified and
   * documented before being treated as formal certification evidence.
   */

  static const struct {
    double mach;
    double expected_ratio;
    const char *description;
    const char *requirement_id;
  } references[] = {{0.00, 1.0000000000, "Mach 0.00", REQ_PITOT_SUBSONIC},
                    {0.30, 1.0644302862, "Mach 0.30", REQ_PITOT_SUBSONIC},
                    {0.50, 1.1862126380, "Mach 0.50", REQ_PITOT_SUBSONIC},
                    {0.80, 1.5243400096, "Mach 0.80", REQ_PITOT_SUBSONIC},

                    {1.00, 1.8929291587, "Mach 1.00", REQ_PITOT_SUPERSONIC},
                    {1.20, 2.4075016207, "Mach 1.20", REQ_PITOT_SUPERSONIC},
                    {1.50, 3.4132747634, "Mach 1.50", REQ_PITOT_SUPERSONIC},
                    {2.00, 5.6404408128, "Mach 2.00", REQ_PITOT_SUPERSONIC},
                    {2.34, 7.5318520813, "Mach 2.34", REQ_PITOT_SUPERSONIC}};

  const double static_pressure = 100000.0;
  const double tolerance = 1e-8;

  size_t i;

  for (i = 0U; i < sizeof(references) / sizeof(references[0]); i++) {
    double total_pressure;
    double ratio;

    total_pressure =
        sensors_calc_total_pressure(static_pressure, references[i].mach);

    ratio = total_pressure / static_pressure;

    TEST_CHECK_FINITE(ctx, references[i].requirement_id, total_pressure,
                      "Calculated total pressure must be finite");

    TEST_CHECK_NEAR(ctx, references[i].requirement_id, ratio,
                    references[i].expected_ratio, tolerance,
                    references[i].description);
  }
}

/* ------------------------------------------------------------------------- */
/* Test 6: Empty profile                                                     */
/* ------------------------------------------------------------------------- */

static void test_empty_profile(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);

  sensors_set_constant(&config, 5000.0, 0.8);

  /*
   * An empty profile in PROFILE mode currently falls back to the
   * configured constant values.
   */

  config.mode = SENSOR_MODE_PROFILE;

  sensors_read(&config, 100.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_EMPTY, reading.true_altitude_m, 5000.0, 1e-9,
                  "Empty profile must use configured constant altitude");

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_EMPTY, reading.true_mach, 0.8, 1e-9,
                  "Empty profile must use configured constant Mach");
}

/* ------------------------------------------------------------------------- */
/* Test 7: Single-point profile                                              */
/* ------------------------------------------------------------------------- */

static void test_single_point_profile(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  flight_profile_init(&config.profile);

  TEST_CHECK_EQUAL_INT(
      ctx, REQ_PROFILE_SINGLE,
      flight_profile_add_point(&config.profile, 10.0, 2500.0, 0.75), 0,
      "Single profile point must be accepted");

  config.mode = SENSOR_MODE_PROFILE;

  sensors_read(&config, 0.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_SINGLE, reading.true_altitude_m, 2500.0,
                  1e-9, "Single-point profile altitude before its timestamp");

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_SINGLE, reading.true_mach, 0.75, 1e-9,
                  "Single-point profile Mach before its timestamp");

  sensors_read(&config, 10.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_SINGLE, reading.true_altitude_m, 2500.0,
                  1e-9, "Single-point profile altitude at its timestamp");

  sensors_read(&config, 100.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_SINGLE, reading.true_altitude_m, 2500.0,
                  1e-9, "Single-point profile altitude after its timestamp");

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_SINGLE, reading.true_mach, 0.75, 1e-9,
                  "Single-point profile Mach after its timestamp");
}

/* ------------------------------------------------------------------------- */
/* Test 8 and 9: Linear interpolation and profile boundaries                 */
/* ------------------------------------------------------------------------- */

static void test_profile_interpolation(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  flight_profile_init(&config.profile);

  TEST_CHECK_EQUAL_INT(ctx, REQ_INTERPOLATION,
                       flight_profile_add_point(&config.profile, 0.0, 0.0, 0.0),
                       0, "First profile point must be accepted");

  TEST_CHECK_EQUAL_INT(
      ctx, REQ_INTERPOLATION,
      flight_profile_add_point(&config.profile, 10.0, 1000.0, 1.0), 0,
      "Second profile point must be accepted");

  config.mode = SENSOR_MODE_PROFILE;

  /* Before the first point. */

  sensors_read(&config, -5.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_LIMITS, reading.true_altitude_m, 0.0, 1e-9,
                  "Time before profile must return first altitude");

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_LIMITS, reading.true_mach, 0.0, 1e-9,
                  "Time before profile must return first Mach");

  /* Exact first point. */

  sensors_read(&config, 0.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_LIMITS, reading.true_altitude_m, 0.0, 1e-9,
                  "Exact first timestamp must return first altitude");

  /* Midpoint. */

  sensors_read(&config, 5.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_INTERPOLATION, reading.true_altitude_m, 500.0, 1e-9,
                  "Altitude must be linearly interpolated");

  TEST_CHECK_NEAR(ctx, REQ_INTERPOLATION, reading.true_mach, 0.5, 1e-9,
                  "Mach must be linearly interpolated");

  /* Exact final point. */

  sensors_read(&config, 10.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_LIMITS, reading.true_altitude_m, 1000.0,
                  1e-9, "Exact final timestamp must return final altitude");

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_LIMITS, reading.true_mach, 1.0, 1e-9,
                  "Exact final timestamp must return final Mach");

  /* After the final point. */

  sensors_read(&config, 20.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_LIMITS, reading.true_altitude_m, 1000.0,
                  1e-9, "Time after profile must return final altitude");

  TEST_CHECK_NEAR(ctx, REQ_PROFILE_LIMITS, reading.true_mach, 1.0, 1e-9,
                  "Time after profile must return final Mach");
}

/* ------------------------------------------------------------------------- */
/* Test 10: F-14 cruise profile                                              */
/* ------------------------------------------------------------------------- */

static void test_f14_cruise_profile(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  flight_profile_load_f14_cruise(&config.profile);

  config.mode = SENSOR_MODE_PROFILE;

  TEST_CHECK_EQUAL_INT(ctx, REQ_CRUISE, config.profile.count, 10,
                       "F-14 cruise profile must contain 10 points");

  /*
   * Exact profile points:
   *
   * t = 500 s:  altitude = 9500 m, Mach = 0.85
   * t = 650 s:  altitude = 10500 m, Mach = 1.05
   * t = 800 s:  altitude = 12000 m, Mach = 1.40
   */

  sensors_read(&config, 500.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_CRUISE, reading.true_altitude_m, 9500.0, 1e-9,
                  "Cruise profile altitude at 500 seconds");

  TEST_CHECK_NEAR(ctx, REQ_CRUISE, reading.true_mach, 0.85, 1e-9,
                  "Cruise profile Mach at 500 seconds");

  sensors_read(&config, 650.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_CRUISE, reading.true_altitude_m, 10500.0, 1e-9,
                  "Cruise profile altitude at 650 seconds");

  TEST_CHECK_NEAR(ctx, REQ_CRUISE, reading.true_mach, 1.05, 1e-9,
                  "Cruise profile Mach at 650 seconds");

  sensors_read(&config, 725.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_CRUISE, reading.true_altitude_m, 11250.0, 1e-9,
                  "Cruise profile interpolated altitude at 725 seconds");

  TEST_CHECK_NEAR(ctx, REQ_CRUISE, reading.true_mach, 1.225, 1e-9,
                  "Cruise profile interpolated Mach at 725 seconds");

  check_reading_finite(ctx, &reading, REQ_CRUISE);
}

/* ------------------------------------------------------------------------- */
/* Test 11: F-14 intercept profile                                           */
/* ------------------------------------------------------------------------- */

static void test_f14_intercept_profile(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  flight_profile_load_f14_intercept(&config.profile);

  config.mode = SENSOR_MODE_PROFILE;

  TEST_CHECK_EQUAL_INT(ctx, REQ_INTERCEPT, config.profile.count, 11,
                       "F-14 intercept profile must contain 11 points");

  sensors_read(&config, 500.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_INTERCEPT, reading.true_altitude_m, 16000.0, 1e-9,
                  "Intercept profile altitude at 500 seconds");

  TEST_CHECK_NEAR(ctx, REQ_INTERCEPT, reading.true_mach, 2.34, 1e-9,
                  "Intercept profile Mach at 500 seconds");

  sensors_read(&config, 575.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_INTERCEPT, reading.true_altitude_m, 15000.0, 1e-9,
                  "Intercept profile interpolated altitude at 575 seconds");

  TEST_CHECK_NEAR(ctx, REQ_INTERCEPT, reading.true_mach, 2.07, 1e-9,
                  "Intercept profile interpolated Mach at 575 seconds");

  check_reading_finite(ctx, &reading, REQ_INTERCEPT);
}

/* ------------------------------------------------------------------------- */
/* Test 12: Noiseless measurements                                           */
/* ------------------------------------------------------------------------- */

static void test_noiseless_measurements(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  sensors_set_constant(&config, 9000.0, 0.85);

  sensors_read(&config, 100.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_NOISELESS, reading.total_pressure_Pa,
                  reading.true_total_pressure_Pa, 1e-9,
                  "Noiseless total pressure must equal true total pressure");

  TEST_CHECK_NEAR(ctx, REQ_NOISELESS, reading.static_pressure_Pa,
                  reading.true_static_pressure_Pa, 1e-9,
                  "Noiseless static pressure must equal true static pressure");

  TEST_CHECK_NEAR(ctx, REQ_NOISELESS, reading.temperature_K,
                  reading.true_temperature_K, 1e-9,
                  "Noiseless temperature must equal true temperature");

  TEST_CHECK_NEAR(
      ctx, REQ_NOISELESS, reading.dynamic_pressure_Pa,
      reading.total_pressure_Pa - reading.static_pressure_Pa, 1e-9,
      "Pressure difference must equal measured Pt minus measured Ps");
}

/* ------------------------------------------------------------------------- */
/* Test 13: Noise activation                                                 */
/* ------------------------------------------------------------------------- */

static void test_noise_activation(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  sensors_set_constant(&config, 9000.0, 0.85);

  sensors_enable_noise(&config, 12345);

  TEST_CHECK_EQUAL_INT(ctx, REQ_NOISE_ACTIVATION, config.mode,
                       SENSOR_MODE_NOISY,
                       "Enabling noise must activate NOISY mode");

  TEST_CHECK_NEAR(ctx, REQ_NOISE_ACTIVATION, config.pitot_noise.sigma,
                  SENSOR_DEFAULT_PRESSURE_NOISE_PA, 1e-12,
                  "Pitot noise sigma must match configured default");

  TEST_CHECK_NEAR(ctx, REQ_NOISE_ACTIVATION, config.static_noise.sigma,
                  SENSOR_DEFAULT_PRESSURE_NOISE_PA, 1e-12,
                  "Static pressure noise sigma must match configured default");

  TEST_CHECK_NEAR(ctx, REQ_NOISE_ACTIVATION, config.temp_noise.sigma,
                  SENSOR_DEFAULT_TEMP_NOISE_K, 1e-12,
                  "Temperature noise sigma must match configured default");

  sensors_read(&config, 100.0, &reading);

  check_reading_finite(ctx, &reading, REQ_NOISE_ACTIVATION);

  /*
   * Noise is applied to measured values, not to the true state.
   */

  TEST_CHECK_NEAR(ctx, REQ_NOISE_ACTIVATION, reading.true_altitude_m, 9000.0,
                  1e-9, "Noise must not modify true altitude");

  TEST_CHECK_NEAR(ctx, REQ_NOISE_ACTIVATION, reading.true_mach, 0.85, 1e-9,
                  "Noise must not modify true Mach");

  TEST_CHECK(ctx, REQ_NOISE_ACTIVATION,
             reading.total_pressure_Pa != reading.true_total_pressure_Pa ||
                 reading.static_pressure_Pa !=
                     reading.true_static_pressure_Pa ||
                 reading.temperature_K != reading.true_temperature_K,
             "At least one measured channel should differ from its true value");

  TEST_CHECK_NEAR(
      ctx, REQ_READING_INVAR, reading.dynamic_pressure_Pa,
      reading.total_pressure_Pa - reading.static_pressure_Pa, 1e-9,
      "Dynamic pressure must equal measured total minus static pressure");
}

/* ------------------------------------------------------------------------- */
/* Test 14: Noise deactivation                                               */
/* ------------------------------------------------------------------------- */

static void test_noise_deactivation(TestContext *ctx) {
  SensorConfig config;
  SensorReading reading;

  sensors_config_init(&config);
  sensors_set_constant(&config, 9000.0, 0.85);

  sensors_enable_noise(&config, 12345);
  sensors_disable_noise(&config);

  TEST_CHECK_NEAR(ctx, REQ_NOISE_DISABLE, config.pitot_noise.sigma, 0.0, 0.0,
                  "Disabling noise must set Pitot sigma to zero");

  TEST_CHECK_NEAR(ctx, REQ_NOISE_DISABLE, config.static_noise.sigma, 0.0, 0.0,
                  "Disabling noise must set static pressure sigma to zero");

  TEST_CHECK_NEAR(ctx, REQ_NOISE_DISABLE, config.temp_noise.sigma, 0.0, 0.0,
                  "Disabling noise must set temperature sigma to zero");

  sensors_read(&config, 100.0, &reading);

  TEST_CHECK_NEAR(ctx, REQ_NOISE_DISABLE, reading.total_pressure_Pa,
                  reading.true_total_pressure_Pa, 1e-9,
                  "After noise deactivation, total pressure must be noiseless");

  TEST_CHECK_NEAR(
      ctx, REQ_NOISE_DISABLE, reading.static_pressure_Pa,
      reading.true_static_pressure_Pa, 1e-9,
      "After noise deactivation, static pressure must be noiseless");

  TEST_CHECK_NEAR(ctx, REQ_NOISE_DISABLE, reading.temperature_K,
                  reading.true_temperature_K, 1e-9,
                  "After noise deactivation, temperature must be noiseless");

  TEST_CHECK_NEAR(
      ctx, REQ_READING_INVAR, reading.dynamic_pressure_Pa,
      reading.total_pressure_Pa - reading.static_pressure_Pa, 1e-9,
      "Dynamic pressure must equal measured total minus static pressure");
}

/* ------------------------------------------------------------------------- */
/* Test 15: Deterministic noise with a fixed seed                            */
/* ------------------------------------------------------------------------- */

static void test_noise_reproducibility(TestContext *ctx) {
  enum { SAMPLE_COUNT = 16 };

  SensorConfig config;
  SensorReading first[SAMPLE_COUNT];
  SensorReading second[SAMPLE_COUNT];

  int i;

  sensors_config_init(&config);
  sensors_set_constant(&config, 9000.0, 0.85);
  sensors_enable_noise(&config, 12345);

  for (i = 0; i < SAMPLE_COUNT; i++) {
    sensors_read(&config, (double)i, &first[i]);
  }

  /*
   * Reinitializing and enabling noise with the same seed resets srand().
   */

  sensors_config_init(&config);
  sensors_set_constant(&config, 9000.0, 0.85);
  sensors_enable_noise(&config, 12345);

  for (i = 0; i < SAMPLE_COUNT; i++) {
    sensors_read(&config, (double)i, &second[i]);
  }

  for (i = 0; i < SAMPLE_COUNT; i++) {
    TEST_CHECK_NEAR(ctx, REQ_NOISE_REPRO, second[i].total_pressure_Pa,
                    first[i].total_pressure_Pa, 0.0,
                    "Fixed seed must reproduce total pressure sequence");

    TEST_CHECK_NEAR(ctx, REQ_NOISE_REPRO, second[i].static_pressure_Pa,
                    first[i].static_pressure_Pa, 0.0,
                    "Fixed seed must reproduce static pressure sequence");

    TEST_CHECK_NEAR(ctx, REQ_NOISE_REPRO, second[i].temperature_K,
                    first[i].temperature_K, 0.0,
                    "Fixed seed must reproduce temperature sequence");
  }
}

/* ------------------------------------------------------------------------- */
/* Test 16: Statistical properties of sensor noise                           */
/* ------------------------------------------------------------------------- */

static void test_noise_statistics(TestContext *ctx) {
  enum { SAMPLE_COUNT = 10000 };

  SensorConfig config;
  SensorReading reading;

  RunningStats pitot_stats;
  RunningStats static_stats;
  RunningStats temp_stats;

  double expected_pitot_sigma;
  double expected_static_sigma;
  double expected_temp_sigma;

  double pitot_mean_tolerance;
  double static_mean_tolerance;
  double temp_mean_tolerance;

  double pitot_sigma;
  double static_sigma;
  double temp_sigma;

  double true_pitot;
  double true_static;
  double true_temp;

  int i;

  sensors_config_init(&config);
  sensors_set_constant(&config, 9000.0, 0.85);

  sensors_read(&config, 0.0, &reading);

  true_pitot = reading.true_total_pressure_Pa;
  true_static = reading.true_static_pressure_Pa;
  true_temp = reading.true_temperature_K;

  sensors_enable_noise(&config, 12345);

  stats_init(&pitot_stats);
  stats_init(&static_stats);
  stats_init(&temp_stats);

  for (i = 0; i < SAMPLE_COUNT; i++) {
    sensors_read(&config, (double)i, &reading);

    stats_add(&pitot_stats, reading.total_pressure_Pa);
    stats_add(&static_stats, reading.static_pressure_Pa);
    stats_add(&temp_stats, reading.temperature_K);
  }

  expected_pitot_sigma = SENSOR_DEFAULT_PRESSURE_NOISE_PA;
  expected_static_sigma = SENSOR_DEFAULT_PRESSURE_NOISE_PA;
  expected_temp_sigma = SENSOR_DEFAULT_TEMP_NOISE_K;

  /*
   * Mean acceptance:
   *
   *   5 * sigma / sqrt(N)
   *
   * This is a statistical tolerance, not an exact equality criterion.
   */

  pitot_mean_tolerance =
      5.0 * expected_pitot_sigma / sqrt((double)SAMPLE_COUNT);

  static_mean_tolerance =
      5.0 * expected_static_sigma / sqrt((double)SAMPLE_COUNT);

  temp_mean_tolerance = 5.0 * expected_temp_sigma / sqrt((double)SAMPLE_COUNT);

  pitot_sigma = stats_stddev(&pitot_stats);
  static_sigma = stats_stddev(&static_stats);
  temp_sigma = stats_stddev(&temp_stats);

  TEST_CHECK_NEAR(ctx, REQ_NOISE_STATS, pitot_stats.mean, true_pitot,
                  pitot_mean_tolerance,
                  "Pitot noise sample mean must be near true pressure");

  TEST_CHECK_NEAR(
      ctx, REQ_NOISE_STATS, static_stats.mean, true_static,
      static_mean_tolerance,
      "Static pressure noise sample mean must be near true pressure");

  TEST_CHECK_NEAR(
      ctx, REQ_NOISE_STATS, temp_stats.mean, true_temp, temp_mean_tolerance,
      "Temperature noise sample mean must be near true temperature");

  /*
   * Standard deviation acceptance:
   *
   *   0.85 * configured_sigma <= observed_sigma
   *   observed_sigma <= 1.15 * configured_sigma
   *
   * The range is intentionally wider than the expected sampling error.
   */

  TEST_CHECK_RANGE(ctx, REQ_NOISE_STATS, pitot_sigma,
                   0.85 * expected_pitot_sigma, 1.15 * expected_pitot_sigma,
                   "Pitot noise standard deviation");

  TEST_CHECK_RANGE(ctx, REQ_NOISE_STATS, static_sigma,
                   0.85 * expected_static_sigma, 1.15 * expected_static_sigma,
                   "Static pressure noise standard deviation");

  TEST_CHECK_RANGE(ctx, REQ_NOISE_STATS, temp_sigma, 0.85 * expected_temp_sigma,
                   1.15 * expected_temp_sigma,
                   "Temperature noise standard deviation");
}

/* ------------------------------------------------------------------------- */
/* Test 18: Flight profile capacity                                          */
/* ------------------------------------------------------------------------- */

static void test_profile_capacity(TestContext *ctx) {
  FlightProfile profile;
  int result;
  int i;

  flight_profile_init(&profile);

  for (i = 0; i < SENSOR_MAX_PROFILE_POINTS; i++) {
    result = flight_profile_add_point(&profile, (double)i, (double)(i * 100),
                                      (double)i * 0.01);

    TEST_CHECK_EQUAL_INT(
        ctx, REQ_PROFILE_CAPACITY, result, 0,
        "Profile point must be accepted while capacity remains");
  }

  TEST_CHECK_EQUAL_INT(ctx, REQ_PROFILE_CAPACITY, profile.count,
                       SENSOR_MAX_PROFILE_POINTS,
                       "Profile count must equal maximum capacity");

  result = flight_profile_add_point(&profile, (double)SENSOR_MAX_PROFILE_POINTS,
                                    10000.0, 1.0);

  TEST_CHECK_EQUAL_INT(ctx, REQ_PROFILE_CAPACITY, result, -1,
                       "Insertion beyond maximum capacity must fail");

  TEST_CHECK_EQUAL_INT(ctx, REQ_PROFILE_CAPACITY, profile.count,
                       SENSOR_MAX_PROFILE_POINTS,
                       "Failed insertion must not increase profile count");
}

/* ------------------------------------------------------------------------- */
/* Test runner                                                               */
/* ------------------------------------------------------------------------- */

int main(void) {
  TestContext ctx;

  test_context_init(&ctx);

  test_configuration_initialization(&ctx);
  test_constant_sea_level(&ctx);
  test_zero_mach(&ctx);

  test_pitot_pressure_ratio(&ctx);

  test_empty_profile(&ctx);
  test_single_point_profile(&ctx);
  test_profile_interpolation(&ctx);

  test_f14_cruise_profile(&ctx);
  test_f14_intercept_profile(&ctx);

  test_noiseless_measurements(&ctx);
  test_noise_activation(&ctx);
  test_noise_deactivation(&ctx);
  test_noise_reproducibility(&ctx);
  test_noise_statistics(&ctx);

  test_profile_capacity(&ctx);

  return test_summary(&ctx);
}
