// test.h: a small test runner. Each test file adds its tests with TEST().
#ifndef FERN_TEST_H
#define FERN_TEST_H

#include <stdio.h>
#include <string.h>

typedef void (*FernTestFn)(void);
void fern_test_add(const char *name, FernTestFn fn);
extern int fern_test_failed;

#define TEST(name)                                                        \
    static void test_##name(void);                                        \
    __attribute__((constructor)) static void add_##name(void) {           \
        fern_test_add(#name, test_##name);                                \
    }                                                                     \
    static void test_##name(void)

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("  %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond);    \
            fern_test_failed = 1;                                         \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        long long a_ = (long long)(a), b_ = (long long)(b);               \
        if (a_ != b_) {                                                   \
            printf("  %s:%d: %s is %lld, not %lld\n", __FILE__, __LINE__, \
                   #a, a_, b_);                                           \
            fern_test_failed = 1;                                         \
        }                                                                 \
    } while (0)

#define CHECK_STR(a, b)                                                   \
    do {                                                                  \
        if (strcmp((a), (b)) != 0) {                                      \
            printf("  %s:%d: %s is \"%s\", not \"%s\"\n", __FILE__,       \
                   __LINE__, #a, (a), (b));                               \
            fern_test_failed = 1;                                         \
        }                                                                 \
    } while (0)

#endif
