/* Audit experiments: air-data computation.
 * Findings: F-03, F-04, F-05, R-1; "found correct": OK-04 (CAS inside shipped profiles). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "audit.h"
#include "airdata.h"
#include "cadc.h"
#include "sensors.h"
#include "atmosphere.h"

/* Independent CAS oracle (standard relations): qc/P0 as a function of Vc/a0,
 * subsonic and supersonic branches; inverted by bisection. */
static double qc_over_p0(double x)
{
    if (x <= 1.0) return pow(1.0 + 0.2 * x * x, 3.5) - 1.0;
    return 166.92158 * pow(x, 7.0) / pow(7.0 * x * x - 1.0, 2.5) - 1.0;
}
static double cas_oracle_kt(double qc)
{
    double target = qc / ISA_P0_SEA_LEVEL_PA, lo = 0.0, hi = 6.0;
    for (int i = 0; i < 200; i++) {
        double m = 0.5 * (lo + hi);
        if (qc_over_p0(m) < target) lo = m; else hi = m;
    }
    return 0.5 * (lo + hi) * AIRDATA_A0_MS / 0.514444;
}

static AirDataState state_at(double alt, double mach, SensorReading *r)
{
    SensorConfig c;
    sensors_config_init(&c);
    sensors_set_constant(&c, alt, mach);
    sensors_read(&c, 0.0, r);
    AirDataState s;
    airdata_compute(r, 0.0, 0.0, &s);
    return s;
}

static double cas_err_pct(double alt, double mach)
{
    SensorReading r;
    AirDataState s = state_at(alt, mach, &r);
    double o = cas_oracle_kt(r.total_pressure_Pa - r.static_pressure_Pa);
    return 100.0 * (s.cas_kt - o) / o;
}

int main(void)
{
    /* ---- F-03: uninitialised outputs on invalid input ---- */
    {
        AirDataState s;
        memset(&s, 0xAA, sizeof s);
        SensorReading bad;
        memset(&bad, 0, sizeof bad);
        bad.total_pressure_Pa = 29990.0; bad.static_pressure_Pa = 30000.0; bad.temperature_K = 230.0; /* Pt < Ps */
        airdata_compute(&bad, 0.0, 1.0, &s);
        double pat;
        memset(&pat, 0xAA, sizeof pat);
        int n = 0;
#define U(f) n += (memcmp(&s.f, &pat, sizeof pat) == 0)
        U(static_temp_K); U(static_temp_C); U(density_kg_m3); U(density_ratio);
        U(speed_of_sound_ms); U(dynamic_pressure_Pa); U(impact_pressure_Pa);
        AUDIT_CHECK("F-03", s.valid == 0 && n == 7, "invalid input: valid=%d, %d of 7 output fields left untouched (documented 7)", s.valid, n);
    }

    /* ---- F-04: pressure semantics ---- */
    {
        SensorReading r;
        AirDataState s = state_at(9000.0, 0.85, &r);
        double qc = r.total_pressure_Pa - r.static_pressure_Pa;
        double q = 0.5 * AIR_GAMMA * r.static_pressure_Pa * 0.85 * 0.85;
        AUDIT_CHECK("F-04", fabs(s.dynamic_pressure_Pa - qc) < 1e-6 && fabs(s.dynamic_pressure_Pa - q) / q > 0.10,
            "dynamic_pressure_Pa=%.1f equals Pt-Ps=%.1f, true q=%.1f Pa (field is impact pressure)", s.dynamic_pressure_Pa, qc, q);
        AUDIT_CHECK("F-04", s.impact_pressure_Pa < -50000.0,
            "impact_pressure_Pa=%.1f Pa (= Pt-P0, negative at altitude; documented -52019)", s.impact_pressure_Pa);
    }

    /* ---- F-05: valid does not imply finite / in-range ---- */
    {
        AirDataState s;
        SensorReading n;
        memset(&n, 0, sizeof n);
        n.total_pressure_Pa = NAN; n.static_pressure_Pa = 30000; n.temperature_K = 230;
        airdata_compute(&n, 0, 1, &s);
        AUDIT_CHECK("F-05", s.valid == 1 && isnan(s.cas_kt), "Pt=NaN: valid=%d, CAS=%g", s.valid, s.cas_kt);
        n.total_pressure_Pa = INFINITY;
        airdata_compute(&n, 0, 1, &s);
        AUDIT_CHECK("F-05", s.valid == 1 && s.mach == 5.0, "Pt=Inf: valid=%d, Mach=%g", s.valid, s.mach);
        n.total_pressure_Pa = 60000; n.static_pressure_Pa = 1.0;
        airdata_compute(&n, 0, 1, &s);
        AUDIT_CHECK("F-05", s.valid == 1 && s.altitude_m == 20000.0, "Ps=1 Pa: valid=%d, altitude=%.0f m (clamped)", s.valid, s.altitude_m);
        AtmosphereState a;
        atmosphere_calculate(NAN, &a);
        AUDIT_CHECK("F-05", isfinite(a.temperature_K) && isnan(a.pressure_Pa),
            "atmosphere_calculate(NaN): T=%g (finite) but P=%g", a.temperature_K, a.pressure_Pa);
    }

    /* ---- R-1: CAS uses the subsonic formula in all regimes ---- */
    {
        double e11 = cas_err_pct(0, 1.1), e13 = cas_err_pct(0, 1.3), e15 = cas_err_pct(0, 1.5), e18 = cas_err_pct(0, 1.8);
        AUDIT_CHECK("R-1", e11 > -0.3 && e11 <= 0.0 && e13 > -1.5 && e13 < -0.9 && e15 > -3.6 && e15 < -3.2 && e18 > -7.9 && e18 < -7.3,
            "CAS error at sea level vs standard supersonic relation: M1.1 %+.2f%%, M1.3 %+.2f%%, M1.5 %+.2f%%, M1.8 %+.2f%% (documented -0.1, -1.2, -3.4, -7.6)",
            e11, e13, e15, e18);
    }

    /* ---- OK-04: inside the shipped profiles CAS < a0 and matches the oracle ---- */
    {
        double worst = 0.0, maxcas = 0.0;
        for (int p = 0; p < 2; p++) {
            SensorConfig c;
            sensors_config_init(&c);
            c.mode = SENSOR_MODE_PROFILE;
            if (p == 0) flight_profile_load_f14_cruise(&c.profile); else flight_profile_load_f14_intercept(&c.profile);
            for (double t = 0.0; t <= 1400.0; t += 1.0) {
                SensorReading r;
                sensors_read(&c, t, &r);
                AirDataState s;
                airdata_compute(&r, 0.0, 0.0, &s);
                if (!s.valid) continue;
                double e = fabs(s.cas_kt - cas_oracle_kt(r.total_pressure_Pa - r.static_pressure_Pa));
                if (e > worst) worst = e;
                if (s.cas_kt > maxcas) maxcas = s.cas_kt;
            }
        }
        AUDIT_CHECK("OK-04", maxcas < 661.0 && worst < 0.05,
            "shipped profiles: max CAS %.1f kt (< a0=661 kt), max |CAS - oracle| %.4f kt", maxcas, worst);
    }
    return audit_not_reproduced ? 1 : 0;
}
