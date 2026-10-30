/* Audit experiments: ISA model, constants, reference table, Mach inversion.
 * Findings: F-08, F-09, R-2; "found correct": OK-01, OK-02, OK-03.
 * usage: exp_atmosphere <path/to/data/isa_reference.txt> */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "audit.h"
#include "atmosphere.h"
#include "sensors.h"
#include "airdata.h"

/* Independent oracle: ISA / U.S. Standard Atmosphere 1976 equations with the
 * standard's constants, geopotential altitude. */
static void iso(double h, double *T, double *P, double *rho)
{
    const double g0 = 9.80665, R = 287.05287, T0 = 288.15, P0 = 101325.0, L = 0.0065;
    if (h <= 11000.0) {
        *T = T0 - L * h;
        *P = P0 * pow(*T / T0, g0 / (R * L));
    } else {
        double P11 = P0 * pow((T0 - L * 11000.0) / T0, g0 / (R * L));
        *T = 216.65;
        *P = P11 * exp(-g0 * (h - 11000.0) / (R * 216.65));
    }
    *rho = *P / (R * (*T));
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <isa_reference.txt>\n", argv[0]); return 2; }

    /* ---- F-08: duplicated / inconsistent constants ---- */
    double expo = GRAVITY_G / (AIR_GAS_CONSTANT_R * ISA_LAPSE_RATE);
    double d = fabs(expo - 5.25587611);
    AUDIT_CHECK("F-08", d > 1e-5 && d < 2e-4,
        "pressure exponent atmosphere.c=%.8f vs literal in airdata.c=5.25587611 (diff %.2e; documented ~9e-5)", expo, d);

    double a0c = sqrt(AIR_GAMMA * AIR_GAS_CONSTANT_R * ISA_T0_SEA_LEVEL_K);
    d = fabs(a0c - AIRDATA_A0_MS);
    AUDIT_CHECK("F-08", d > 1e-3 && d < 1e-2,
        "a0 from code constants=%.4f vs AIRDATA_A0_MS=%.3f (diff %.4f m/s)", a0c, AIRDATA_A0_MS, d);

    double jump = fabs(atmosphere_pressure(11000.0) - ISA_P_TROPOPAUSE_PA);
    AUDIT_CHECK("F-08", jump > 0.3 && jump < 0.8,
        "P(11000 m) by troposphere formula=%.4f vs constant ISA_P_TROPOPAUSE_PA=%.4f (jump %.3f Pa; documented ~0.55)",
        atmosphere_pressure(11000.0), ISA_P_TROPOPAUSE_PA, jump);

    double maxe = 0.0;
    for (double h = 0.0; h <= 20000.0; h += 10.0) {
        double e = fabs(airdata_calc_altitude(atmosphere_pressure(h)) - h);
        if (e > maxe) maxe = e;
    }
    AUDIT_CHECK("F-08", maxe > 0.05 && maxe < 0.5,
        "altitude round trip h->P->h max error %.4f m (documented ~0.16)", maxe);

    double r1 = sensors_calc_total_pressure(1.0, 1.0);
    d = r1 - 1.8929;
    AUDIT_CHECK("F-08", d > 1e-5 && d < 1e-4,
        "Pt/Ps at M=1 = %.9f vs truncated threshold 1.8929 in airdata.c (gap %.2e)", r1, d);

    /* ---- F-09: reference table vs independent oracle; code vs oracle ---- */
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    char line[512];
    int n = 0;
    double mtT = 0, mtP = 0, mtr = 0, moP = 0;
    while (fgets(line, sizeof line, f)) {
        double h, hf, T, Tc, P, rho, a, akt;
        char cap[32];
        if (sscanf(line, "%lf %lf %lf %lf %lf %lf %lf %lf %31s", &h, &hf, &T, &Tc, &P, &rho, &a, &akt, cap) != 9)
            continue;
        double oT, oP, orho;
        iso(h, &oT, &oP, &orho);
        AtmosphereState s;
        atmosphere_calculate(h, &s);
        double e;
        e = fabs(T - oT) / oT;      if (e > mtT) mtT = e;
        e = fabs(P - oP) / oP;      if (e > mtP) mtP = e;
        e = fabs(rho - orho) / orho; if (e > mtr) mtr = e;
        e = fabs(s.pressure_Pa - oP) / oP; if (e > moP) moP = e;
        n++;
    }
    fclose(f);
    AUDIT_CHECK("F-09", n == 37, "reference table rows parsed: %d (expected 37)", n);
    AUDIT_CHECK("F-09", mtP > 1e-3 && mtP < 3e-3 && mtr > 2e-3 && mtr < 1e-2 && mtT > 3e-4 && mtT < 1e-3,
        "table vs oracle max rel. error: T %.2e, P %.2e, rho %.2e (documented ~5e-4, 1.6e-3, 3.9e-3)", mtT, mtP, mtr);
    double r = 6356766.0;
    double d11 = 11000.0 - r * 11000.0 / (r + 11000.0), d20 = 20000.0 - r * 20000.0 / (r + 20000.0);
    AUDIT_CHECK("F-09", d11 > 18.0 && d11 < 20.0 && d20 > 62.0 && d20 < 64.0,
        "geometric vs geopotential altitude: %.1f m at 11 km, %.1f m at 20 km (table says 'geometric', model is geopotential)", d11, d20);
    AUDIT_CHECK("OK-02", moP < 5e-5, "atmosphere.c vs oracle max rel. pressure error %.2e (< 5e-5)", moP);

    /* ---- OK-01: pitot relations vs tabulated ideal-gas values ---- */
    {
        double M[] = {1.0, 1.5, 2.0, 3.0}, ref[] = {1.8929, 3.4133, 5.6404, 12.0610};
        double worst = 0;
        for (int i = 0; i < 4; i++) {
            double e = fabs(sensors_calc_total_pressure(1.0, M[i]) - ref[i]);
            if (e > worst) worst = e;
        }
        double sub = fabs(sensors_calc_total_pressure(1.0, 0.5) - 1.18622);
        AUDIT_CHECK("OK-01", worst < 1e-4 && sub < 1e-4,
            "Pt/Ps vs tabulated gamma=1.4 values: max diff supersonic %.1e, subsonic %.1e (reference values from standard tables; cite source when formalised)", worst, sub);
    }

    /* ---- OK-03: Mach inversion round trip ---- */
    {
        double worst = 0;
        for (double M = 0.01; M <= 3.0; M += 0.005) {
            double Pt = sensors_calc_total_pressure(30000.0, M);
            double e = fabs(airdata_calc_mach(Pt, 30000.0) - M);
            if (e > worst) worst = e;
        }
        AUDIT_CHECK("OK-03", worst < 5e-7, "Mach round trip max error %.2e over M in [0.01, 3.0]", worst);
    }

    /* ---- R-2: silent saturation ---- */
    {
        double Pt = sensors_calc_total_pressure(30000.0, 6.0);
        double M = airdata_calc_mach(Pt, 30000.0);
        AUDIT_CHECK("R-2", fabs(M - 5.0) < 1e-9, "M=6 input recovers Mach %.6f (silent clamp at 5.0, no indication)", M);
    }
    return audit_not_reproduced ? 1 : 0;
}
