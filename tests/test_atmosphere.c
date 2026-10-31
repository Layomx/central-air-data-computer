/* Automated verification of the public ISA atmosphere API. */

#include "../src/atmosphere.h"
#include "test_framework.h"

#include <math.h>
#include <stddef.h>

typedef struct {
    double altitude_m;
    double temperature_K;
    double pressure_Pa;
    double density_kg_m3;
    double speed_of_sound_ms;
} AtmosphereReference;

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/* Rounded independent ISA references. Tolerances account for the legacy
 * model's published constants and two-layer simplification. */
static const AtmosphereReference atmosphere_references[] = {
    {0.0,     288.15, 101325.00, 1.225012, 340.297},
    {1000.0,   281.65,  89874.76, 1.1116,   336.434},
    {5000.0,   255.65,  54020.50, 0.7361,   320.529},
    {11000.0,  216.65,  22632.65, 0.3639,   295.069},
    {15000.0,  216.65,  12044.72, 0.1937,   295.069},
    {20000.0,  216.65,   5474.90, 0.0880,   295.069}
};

static void test_reference_values(TestContext *ctx)
{
    for (size_t i = 0U; i < ARRAY_COUNT(atmosphere_references); i++) {
        const AtmosphereReference *reference = &atmosphere_references[i];
        AtmosphereState state;

        atmosphere_calculate(reference->altitude_m, &state);

        TEST_CHECK_NEAR(ctx, "ATM-REF-001", state.altitude_m,
                        reference->altitude_m, 1e-9,
                        "reported altitude matches input");
        TEST_CHECK_NEAR(ctx, "ATM-REF-002", state.temperature_K,
                        reference->temperature_K, 1e-3,
                        "temperature matches ISA reference");
        TEST_CHECK_NEAR(ctx, "ATM-REF-003", state.pressure_Pa,
                        reference->pressure_Pa, 1.0,
                        "pressure matches ISA reference");
        TEST_CHECK_NEAR(ctx, "ATM-REF-004", state.density_kg_m3,
                        reference->density_kg_m3, 2e-4,
                        "density matches ISA reference");
        TEST_CHECK_NEAR(ctx, "ATM-REF-005", state.speed_of_sound_ms,
                        reference->speed_of_sound_ms, 0.02,
                        "speed of sound matches ISA reference");

        TEST_CHECK_NEAR(ctx, "ATM-API-001",
                        atmosphere_temperature(reference->altitude_m),
                        reference->temperature_K, 1e-3,
                        "temperature function matches ISA reference");
        TEST_CHECK_NEAR(ctx, "ATM-API-002",
                        atmosphere_pressure(reference->altitude_m),
                        reference->pressure_Pa, 1.0,
                        "pressure function matches ISA reference");
        TEST_CHECK_NEAR(ctx, "ATM-API-003",
                        atmosphere_speed_of_sound(reference->temperature_K),
                        reference->speed_of_sound_ms, 0.02,
                        "speed-of-sound function matches ISA reference");
    }
}

static void test_boundary_behavior(TestContext *ctx)
{
    AtmosphereState state;

    atmosphere_calculate(-1000.0, &state);
    TEST_CHECK_NEAR(ctx, "ATM-BND-001", state.altitude_m, 0.0, 1e-9,
                    "negative altitude clamps to sea level");
    TEST_CHECK_NEAR(ctx, "ATM-BND-002", state.temperature_K,
                    ISA_T0_SEA_LEVEL_K, 1e-3,
                    "negative altitude uses sea-level temperature");
    TEST_CHECK_NEAR(ctx, "ATM-BND-003", state.pressure_Pa,
                    ISA_P0_SEA_LEVEL_PA, 0.1,
                    "negative altitude uses sea-level pressure");

    atmosphere_calculate(ISA_MAX_ALT_M, &state);
    TEST_CHECK_NEAR(ctx, "ATM-BND-004", state.altitude_m, ISA_MAX_ALT_M, 1e-9,
                    "maximum modeled altitude is accepted");

    atmosphere_calculate(25000.0, &state);
    TEST_CHECK_NEAR(ctx, "ATM-BND-005", state.altitude_m, ISA_MAX_ALT_M, 1e-9,
                    "altitude above model range clamps to maximum");
    TEST_CHECK_NEAR(ctx, "ATM-BND-006", state.temperature_K,
                    ISA_T_TROPOPAUSE_K, 1e-3,
                    "clamped upper altitude uses lower-stratosphere temperature");
    TEST_CHECK_NEAR(ctx, "ATM-BND-007", state.pressure_Pa, 5474.90, 1.0,
                    "clamped upper altitude uses maximum modeled pressure");
}

static void test_derived_properties(TestContext *ctx)
{
    for (double altitude_m = 0.0; altitude_m <= ISA_MAX_ALT_M;
         altitude_m += 1000.0) {
        AtmosphereState state;
        atmosphere_calculate(altitude_m, &state);

        double expected_density =
            state.pressure_Pa / (AIR_GAS_CONSTANT_R * state.temperature_K);
        double expected_sound =
            sqrt(AIR_GAMMA * AIR_GAS_CONSTANT_R * state.temperature_K);

        TEST_CHECK_FINITE(ctx, "ATM-DER-001", state.temperature_K,
                          "temperature is finite");
        TEST_CHECK_FINITE(ctx, "ATM-DER-002", state.pressure_Pa,
                          "pressure is finite");
        TEST_CHECK_FINITE(ctx, "ATM-DER-003", state.density_kg_m3,
                          "density is finite");
        TEST_CHECK_FINITE(ctx, "ATM-DER-004", state.speed_of_sound_ms,
                          "speed of sound is finite");
        TEST_CHECK_RANGE(ctx, "ATM-DER-005", state.temperature_K, 180.0,
                         300.0, "temperature is physically bounded");
        TEST_CHECK_RANGE(ctx, "ATM-DER-006", state.pressure_Pa, 0.0,
                         ISA_P0_SEA_LEVEL_PA, "pressure is physically bounded");
        TEST_CHECK_RANGE(ctx, "ATM-DER-007", state.density_kg_m3, 0.0,
                         ISA_RHO0_SEA_LEVEL, "density is physically bounded");
        TEST_CHECK_NEAR(ctx, "ATM-DER-008", state.density_kg_m3,
                        expected_density, 1e-12,
                        "density follows the ideal-gas identity");
        TEST_CHECK_NEAR(ctx, "ATM-DER-009", state.speed_of_sound_ms,
                        expected_sound, 1e-12,
                        "speed of sound follows the gas identity");
    }
}

static void test_monotonicity_and_continuity(TestContext *ctx)
{
    AtmosphereState previous;
    atmosphere_calculate(0.0, &previous);

    for (double altitude_m = 100.0; altitude_m <= ISA_MAX_ALT_M;
         altitude_m += 100.0) {
        AtmosphereState current;
        atmosphere_calculate(altitude_m, &current);

        TEST_CHECK(ctx, "ATM-MON-001",
                   current.temperature_K <= previous.temperature_K + 1e-12,
                   "temperature does not increase with altitude");
        TEST_CHECK(ctx, "ATM-MON-002",
                   current.pressure_Pa < previous.pressure_Pa,
                   "pressure decreases with altitude");
        TEST_CHECK(ctx, "ATM-MON-003",
                   current.density_kg_m3 < previous.density_kg_m3,
                   "density decreases with altitude");

        previous = current;
    }

    AtmosphereState below_boundary;
    AtmosphereState above_boundary;
    atmosphere_calculate(ISA_TROPOPAUSE_ALT_M - 1e-6, &below_boundary);
    atmosphere_calculate(ISA_TROPOPAUSE_ALT_M + 1e-6, &above_boundary);

    TEST_CHECK_NEAR(ctx, "ATM-MON-004", below_boundary.temperature_K,
                    above_boundary.temperature_K, 1e-6,
                    "temperature is continuous at the tropopause");
    TEST_CHECK_NEAR(ctx, "ATM-MON-005", below_boundary.pressure_Pa,
                    above_boundary.pressure_Pa, 1.0,
                    "pressure is continuous within model tolerance at the tropopause");
}

static void test_unit_conversions(TestContext *ctx)
{
    const double meters[] = {0.0, 1.0, 100.0, 1000.0, 18288.0};

    for (size_t i = 0U; i < ARRAY_COUNT(meters); i++) {
        TEST_CHECK_NEAR(ctx, "ATM-UNIT-001",
                        feet_to_meters(meter_to_feet(meters[i])), meters[i],
                        1e-3, "meter/feet conversion round trip");
    }

    TEST_CHECK_NEAR(ctx, "ATM-UNIT-002", ms_to_knots(1.0), 1.94384, 1e-4,
                    "meters per second to knots conversion");
}

int main(void)
{
    TestContext context;
    test_context_init(&context);

    test_reference_values(&context);
    test_boundary_behavior(&context);
    test_derived_properties(&context);
    test_monotonicity_and_continuity(&context);
    test_unit_conversions(&context);

    return test_summary(&context);
}
