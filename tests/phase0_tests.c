#include <stdio.h>

#include "reliability-checks.h"

static int g_failures = 0;

#define CHECK(expr)                                                         \
    do                                                                      \
    {                                                                       \
        if (!(expr))                                                        \
        {                                                                   \
            fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__,  \
                __LINE__);                                                  \
            g_failures++;                                                   \
        }                                                                   \
    } while (0)

static void test_clamp_initial_text_len(void)
{
    CHECK(reliability_clamp_initial_text_len(NULL, 0) == 0);
    CHECK(reliability_clamp_initial_text_len("", 8) == 0);
    CHECK(reliability_clamp_initial_text_len("abc", 8) == 3);
    CHECK(reliability_clamp_initial_text_len("abcdef", 4) == 3);
    CHECK(reliability_clamp_initial_text_len("abcdef", 1) == 0);
}

static void test_sample_square_point(void)
{
    int y = -1;
    int x = -1;

    CHECK(reliability_sample_square_point(10, 10, 3, 3, 3, 40, 80, &y, &x));
    CHECK(y == 10);
    CHECK(x == 10);

    CHECK(!reliability_sample_square_point(1, 1, 3, 0, 0, 20, 20, &y, &x));
    CHECK(y == -2);
    CHECK(x == -2);

    CHECK(reliability_sample_square_point(1, 1, 3, 3, 3, 20, 20, &y, &x));
    CHECK(y == 1);
    CHECK(x == 1);

    CHECK(!reliability_sample_square_point(18, 18, 3, 6, 6, 20, 20, &y, &x));
    CHECK(y == 21);
    CHECK(x == 21);
}

static void test_rle_zero_run_guard(void)
{
    int empty_runs = 0;

    CHECK(reliability_accept_rle_count(0, &empty_runs, 8));
    CHECK(empty_runs == 1);
    CHECK(reliability_accept_rle_count(0, &empty_runs, 8));
    CHECK(empty_runs == 2);
    CHECK(reliability_accept_rle_count(5, &empty_runs, 8));
    CHECK(empty_runs == 0);

    empty_runs = 8;
    CHECK(!reliability_accept_rle_count(0, &empty_runs, 8));
    CHECK(empty_runs == 9);
}

int main(void)
{
    test_clamp_initial_text_len();
    test_sample_square_point();
    test_rle_zero_run_guard();

    if (g_failures != 0)
    {
        fprintf(stderr, "%d regression test(s) failed.\n", g_failures);
        return 1;
    }

    printf("Phase 0 regression tests passed.\n");
    return 0;
}
