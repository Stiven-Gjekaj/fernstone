// main.c: run each test, or the tests with a name that contains argv[1].
#include "test.h"

#define MAX_TESTS 512

static const char *names[MAX_TESTS];
static FernTestFn fns[MAX_TESTS];
static int count;
int fern_test_failed;

void fern_test_add(const char *name, FernTestFn fn) {
    if (count < MAX_TESTS) {
        names[count] = name;
        fns[count] = fn;
        count++;
    }
}

int main(int argc, char **argv) {
    int i, run = 0, failed = 0;
    for (i = 0; i < count; i++) {
        if (argc > 1 && !strstr(names[i], argv[1])) continue;
        fern_test_failed = 0;
        fns[i]();
        run++;
        if (fern_test_failed) {
            printf("FAIL %s\n", names[i]);
            failed++;
        }
    }
    printf("%d tests, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
