#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

/* Shared assertion and reporting framework for C verification tests. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long total;
    unsigned long passed;
    unsigned long failed;
} TestContext;

void test_context_init(TestContext *ctx);

void test_check(TestContext *ctx, int condition, const char *requirement_id,
                const char *description, const char *file, int line);

void test_check_near(TestContext *ctx, double actual, double expected,
                     double tolerance, const char *requirement_id,
                     const char *description, const char *file, int line);

void test_check_finite(TestContext *ctx, double value,
                       const char *requirement_id, const char *description,
                       const char *file, int line);

void test_check_equal_int(TestContext *ctx, int actual, int expected,
                          const char *requirement_id,
                          const char *description, const char *file, int line);

void test_check_equal_uint(TestContext *ctx, unsigned int actual,
                           unsigned int expected, const char *requirement_id,
                           const char *description, const char *file,
                           int line);

void test_check_range(TestContext *ctx, double value, double minimum,
                      double maximum, const char *requirement_id,
                      const char *description, const char *file, int line);

/* Returns 0 for pass, 1 for assertion failure, 2 for invalid context. */
int test_summary(const TestContext *ctx);

#define TEST_CHECK(ctx, req, condition, description)                         \
    test_check((ctx), (condition), (req), (description), __FILE__, __LINE__)

#define TEST_CHECK_NEAR(ctx, req, actual, expected, tolerance, description)   \
    test_check_near((ctx), (actual), (expected), (tolerance), (req),           \
                    (description), __FILE__, __LINE__)

#define TEST_CHECK_FINITE(ctx, req, value, description)                       \
    test_check_finite((ctx), (value), (req), (description), __FILE__, __LINE__)

#define TEST_CHECK_EQUAL_INT(ctx, req, actual, expected, description)          \
    test_check_equal_int((ctx), (actual), (expected), (req), (description),    \
                         __FILE__, __LINE__)

#define TEST_CHECK_EQUAL_UINT(ctx, req, actual, expected, description)         \
    test_check_equal_uint((ctx), (actual), (expected), (req), (description),   \
                          __FILE__, __LINE__)

#define TEST_CHECK_RANGE(ctx, req, value, minimum, maximum, description)       \
    test_check_range((ctx), (value), (minimum), (maximum), (req),              \
                     (description), __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif
