/* Minimal helper for the Phase 1 audit experiments (not a test framework).
 * AUDIT_CHECK(id, cond, fmt, ...) prints one line:
 *   CHECK <id> REPRODUCED|NOT-REPRODUCED <detail>
 * "REPRODUCED" means the finding (or the "found correct" claim) in
 * docs/audit-phase1.md holds on the audited tree. */
#ifndef AUDIT_H
#define AUDIT_H
#include <stdio.h>

static int audit_not_reproduced = 0;

#define AUDIT_CHECK(id, cond, ...)                                           \
    do {                                                                     \
        int ok_ = (cond) ? 1 : 0;                                            \
        printf("CHECK %-5s %-14s ", (id), ok_ ? "REPRODUCED" : "NOT-REPRODUCED"); \
        printf(__VA_ARGS__);                                                 \
        printf("\n");                                                        \
        if (!ok_) audit_not_reproduced++;                                    \
    } while (0)

#endif
