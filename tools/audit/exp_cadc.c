/* Audit experiments: CADC cycle, VSI, ground behaviour, monitoring.
 * Findings: F-06, F-07, F-10, F-11. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "audit.h"
#include "cadc.h"
#include "logger.h"
#include "sensors.h"

int main(void)
{
    /* ---- F-06a: first cycle invalid -> VSI computed against prev_altitude_m = 0 ---- */
    {
        int found = 0, seed_found = 0;
        double vsi = 0.0;
        for (int seed = 1; seed <= 200 && !found; seed++) {
            CadcConfig c;
            cadc_config_init(&c);
            sensors_set_constant(&c.sensor_cfg, 9000, 0.05);
            sensors_enable_noise(&c.sensor_cfg, seed);
            c.sensor_cfg.static_noise.sigma = 0; c.sensor_cfg.temp_noise.sigma = 0; c.sensor_cfg.pitot_noise.sigma = 60.0;
            CadcState s;
            cadc_state_init(&s);
            cadc_update(&c, 0.0, &s);
            if (!s.airdata.valid) {
                cadc_update(&c, 1.0, &s);
                if (s.airdata.valid && fabs(s.airdata.vsi_ftmin) > 1e5) { found = 1; seed_found = seed; vsi = s.airdata.vsi_ftmin; }
            }
        }
        AUDIT_CHECK("F-06", found, "constant altitude, cycle 0 invalid then valid: VSI=%+.0f ft/min (seed %d; documented +1771623 at seed 35)", vsi, seed_found);
    }

    /* ---- F-06b: VSI after invalid cycles (true climb 10 m/s, noise only on Pitot) ---- */
    {
        CadcConfig cfg;
        cadc_config_init(&cfg);
        flight_profile_init(&cfg.sensor_cfg.profile);
        flight_profile_add_point(&cfg.sensor_cfg.profile, 0, 3000, 0.30);
        flight_profile_add_point(&cfg.sensor_cfg.profile, 1000, 13000, 0.30);
        sensors_enable_noise(&cfg.sensor_cfg, 7);
        cfg.sensor_cfg.mode = SENSOR_MODE_NOISY;
        cfg.sensor_cfg.static_noise.sigma = 0.0; cfg.sensor_cfg.temp_noise.sigma = 0.0; cfg.sensor_cfg.pitot_noise.sigma = 2500.0;
        CadcState st;
        cadc_state_init(&st);
        int nvalid = 0, ninv = 0, nbad = 0;
        double maxv = 0.0;
        for (int k = 0; k < 600; k++) {
            cadc_update(&cfg, (double)k, &st);
            if (!st.airdata.valid) { ninv++; continue; }
            nvalid++;
            if (k > 0 && fabs(st.airdata.vsi_ms - 10.0) > 3.0) nbad++;
            if (fabs(st.airdata.vsi_ms) > maxv) maxv = fabs(st.airdata.vsi_ms);
        }
        AUDIT_CHECK("F-06", nbad >= 20 && maxv >= 19.0,
            "true VSI +10 m/s: %d valid cycles with error > 3 m/s of %d valid (%d invalid); max |VSI| %.1f m/s (documented 62 of 523, 40.0)", nbad, nvalid, ninv, maxv);
    }

    /* ---- F-07: invalid fraction on the ground with default noise ---- */
    {
        CadcConfig c;
        cadc_config_init(&c);
        sensors_set_constant(&c.sensor_cfg, 0, 0.0);
        sensors_enable_noise(&c.sensor_cfg, 42);
        CadcState s;
        cadc_state_init(&s);
        int inv = 0, N = 2000;
        for (int k = 0; k < N; k++) { cadc_update(&c, (double)k, &s); if (!s.airdata.valid) inv++; }
        AUDIT_CHECK("F-07", inv > 0.40 * N && inv < 0.60 * N, "M=0 on the ground, default noise: %d of %d cycles invalid (%.1f%%; documented 49.4%%)", inv, N, 100.0 * inv / N);
    }

    /* ---- F-11: open-loop profile triggers low_speed at takeoff; no total-temperature model ---- */
    {
        CadcConfig c;
        cadc_config_init(&c);
        c.sensor_cfg.mode = SENSOR_MODE_PROFILE;
        flight_profile_load_f14_cruise(&c.sensor_cfg.profile);
        CadcState s;
        cadc_state_init(&s);
        int low = 0;
        double vsi10 = 0, cas10 = 0, dT = 0.0;
        for (int t = 0; t <= 800; t++) {
            cadc_update(&c, (double)t, &s);
            if (t >= 10 && t <= 15 && s.alerts.low_speed) low++;
            if (t == 10) { vsi10 = s.airdata.vsi_ftmin; cas10 = s.airdata.cas_kt; }
            if (t == 800) dT = fabs(s.sensor.temperature_K - s.sensor.true_temperature_K);
        }
        AUDIT_CHECK("F-11", low == 6 && vsi10 > 3000.0 && cas10 < 100.0,
            "default cruise profile t=10..15 s: low_speed alert in %d of 6 cycles; at t=10: VSI %.0f ft/min with CAS %.1f kt", low, vsi10, cas10);
        AUDIT_CHECK("F-11", dT < 1e-12, "at t=800 s (M=1.4) thermometer reading differs from static ISA T by %.1e K (no total-temperature effect)", dT);
    }

    /* ---- F-10: dead plausibility check; sensor-fault latch never clears ---- */
    {
        CadcConfig c;
        cadc_config_init(&c);
        c.sensor_cfg.mode = SENSOR_MODE_PROFILE;
        flight_profile_load_f14_intercept(&c.sensor_cfg.profile);
        sensors_enable_noise(&c.sensor_cfg, 42);
        c.sensor_cfg.mode = SENSOR_MODE_NOISY;
        CadcState s;
        cadc_state_init(&s);
        int neg = 0, N = 2200;
        for (int k = 0; k < N; k++) {
            cadc_update(&c, k * 0.5, &s);
            if (s.airdata.valid && s.airdata.dynamic_pressure_Pa < 0.0) neg++;
        }
        AUDIT_CHECK("F-10", neg == 0, "valid cycles with dynamic_pressure_Pa < 0: %d of %d (check is unreachable: airdata_compute requires Pt >= Ps)", neg, N);

        LoggerState lg;
        logger_init(&lg, NULL, LOG_ERROR);
        SensorReading bad, good;
        memset(&bad, 0, sizeof bad); memset(&good, 0, sizeof good);
        bad.total_pressure_Pa = 0.0; bad.static_pressure_Pa = 30000; bad.temperature_K = 250;
        good.total_pressure_Pa = 50000; good.static_pressure_Pa = 30000; good.temperature_K = 250;
        for (int k = 0; k < LOGGER_FAULT_THRESHOLD; k++) logger_update_fault_detection(&lg, &bad);
        int failed_after_bad = lg.faults.pitot_failed;
        for (int k = 0; k < 1000; k++) logger_update_fault_detection(&lg, &good);
        AUDIT_CHECK("F-10", failed_after_bad == 1 && lg.faults.pitot_failed == 1,
            "pitot declared failed after %d bad cycles (%d); still failed after 1000 good cycles (%d)", LOGGER_FAULT_THRESHOLD, failed_after_bad, lg.faults.pitot_failed);
    }
    return audit_not_reproduced ? 1 : 0;
}
