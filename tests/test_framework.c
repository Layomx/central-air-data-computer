#include "test_framework.h"

#include <math.h>
#include <stdio.h>

static const char *safe_string(const char *value)
{
    return value != NULL ? value : "(unspecified)";
}

void test_context_init(TestContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->total = 0UL;
    ctx->passed = 0UL;
    ctx->failed = 0UL;
}

void test_check(TestContext *ctx, int condition, const char *requirement_id,
                const char *description, const char *file, int line)
{
    if (ctx == NULL) {
        fprintf(stderr, "TEST FRAMEWORK ERROR: NULL context\n");
        return;
    }

    ctx->total++;

    if (condition) {
        ctx->passed++;
        return;
    }

    ctx->failed++;
    fprintf(stderr, "FAIL: [%s] %s (%s:%d)\n", safe_string(requirement_id),
            safe_string(description), safe_string(file), line);
}

void test_check_near(TestContext *ctx, double actual, double expected,
                     double tolerance, const char *requirement_id,
                     const char *description, const char *file, int line)
{
    int valid = isfinite(actual) && isfinite(expected) &&
                isfinite(tolerance) && tolerance >= 0.0 &&
                fabs(actual - expected) <= tolerance;

    test_check(ctx, valid, requirement_id, description, file, line);

    if (!valid) {
        fprintf(stderr, "  actual=%.12g expected=%.12g tolerance=%.12g\n",
                actual, expected, tolerance);
    }
}

void test_check_finite(TestContext *ctx, double value,
                       const char *requirement_id, const char *description,
                       const char *file, int line)
{
    int valid = isfinite(value);

    test_check(ctx, valid, requirement_id, description, file, line);

    if (!valid) {
        fprintf(stderr, "  received non-finite value: %.12g\n", value);
    }
}

void test_check_equal_int(TestContext *ctx, int actual, int expected,
                          const char *requirement_id,
                          const char *description, const char *file, int line)
{
    int valid = actual == expected;

    test_check(ctx, valid, requirement_id, description, file, line);

    if (!valid) {
        fprintf(stderr, "  actual=%d expected=%d\n", actual, expected);
    }
}

void test_check_equal_uint(TestContext *ctx, unsigned int actual,
                           unsigned int expected, const char *requirement_id,
                           const char *description, const char *file,
                           int line)
{
    int valid = actual == expected;

    test_check(ctx, valid, requirement_id, description, file, line);

    if (!valid) {
        fprintf(stderr, "  actual=%u expected=%u\n", actual, expected);
    }
}

void test_check_range(TestContext *ctx, double value, double minimum,
                      double maximum, const char *requirement_id,
                      const char *description, const char *file, int line)
{
    int valid = isfinite(value) && isfinite(minimum) && isfinite(maximum) &&
                minimum <= maximum && value >= minimum && value <= maximum;

    test_check(ctx, valid, requirement_id, description, file, line);

    if (!valid) {
        fprintf(stderr, "  value=%.12g range=[%.12g, %.12g]\n", value,
                minimum, maximum);
    }
}

int test_summary(const TestContext *ctx)
{
    if (ctx == NULL) {
        fprintf(stderr, "TEST FRAMEWORK ERROR: NULL context\n");
        printf("VERDICT: FAIL\n");
        return 2;
    }

    printf("\n-----------------------------------------\n");
    printf("Test assertions: %lu\n", ctx->total);
    printf("Tests passed: %lu\n", ctx->passed);
    printf("Tests failed: %lu\n", ctx->failed);
    printf("-----------------------------------------\n");

    if (ctx->total == 0UL || ctx->failed != 0UL ||
        ctx->passed + ctx->failed != ctx->total) {
        printf("VERDICT: FAIL\n");
        return 1;
    }

    printf("VERDICT: PASS\n");
    return 0;
}
