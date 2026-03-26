#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "angband.h"
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

static void test_serialized_layout_validation(void)
{
    size_t expected_size = 0;

    CHECK(reliability_validate_serialized_layout(64, 16, 32, 8, 8,
        &expected_size) == RELIABILITY_LAYOUT_VALID);
    CHECK(expected_size == 64);

    CHECK(reliability_validate_serialized_layout(63, 16, 32, 8, 8,
        &expected_size) == RELIABILITY_LAYOUT_TRUNCATED);
    CHECK(expected_size == 64);

    CHECK(reliability_validate_serialized_layout(65, 16, 32, 8, 8,
        &expected_size) == RELIABILITY_LAYOUT_TRAILING_BYTES);
    CHECK(expected_size == 64);

    CHECK(reliability_validate_serialized_layout(0, (size_t)-1, 16, 0, 0,
        &expected_size) == RELIABILITY_LAYOUT_OVERFLOW);
}

static void test_metarun_layout_detection(void)
{
    size_t payload_size = 0;
    size_t entry_size = 0;

    CHECK(reliability_detect_metarun_layout(16 + 3 * 32, 16, 3, 32, 28, 24,
        20, &payload_size, &entry_size) == RELIABILITY_METARUN_LAYOUT_CURRENT);
    CHECK(payload_size == 96);
    CHECK(entry_size == 32);

    CHECK(reliability_detect_metarun_layout(16 + 2 * 28, 16, 2, 32, 28, 24,
        20, &payload_size, &entry_size) == RELIABILITY_METARUN_LAYOUT_V10);
    CHECK(payload_size == 56);
    CHECK(entry_size == 28);

    CHECK(reliability_detect_metarun_layout(16 + 2 * 24, 16, 2, 32, 28, 24,
        20, &payload_size, &entry_size) == RELIABILITY_METARUN_LAYOUT_V9);
    CHECK(payload_size == 48);
    CHECK(entry_size == 24);

    CHECK(reliability_detect_metarun_layout(16 + 2 * 20, 16, 2, 32, 28, 24,
        20, &payload_size, &entry_size) == RELIABILITY_METARUN_LAYOUT_V8);
    CHECK(payload_size == 40);
    CHECK(entry_size == 20);

    CHECK(reliability_detect_metarun_layout(17 + 2 * 32, 16, 2, 32, 28, 24,
        20, &payload_size, &entry_size) == RELIABILITY_METARUN_LAYOUT_INVALID);
    CHECK(reliability_detect_metarun_layout(16 + 2 * 30, 16, 2, 32, 28, 24,
        20, &payload_size, &entry_size) == RELIABILITY_METARUN_LAYOUT_INVALID);
    CHECK(reliability_detect_metarun_layout(16 + 2 * 32, 16, 0, 32, 28, 24,
        20, &payload_size, &entry_size) == RELIABILITY_METARUN_LAYOUT_INVALID);
}

static void test_runs_db_policy(void)
{
    CHECK(reliability_should_update_runs_db(true, true));
    CHECK(!reliability_should_update_runs_db(true, false));
    CHECK(reliability_should_update_runs_db(false, true));
    CHECK(reliability_should_update_runs_db(false, false));
}

static void parse_parity_flags(const char* flags_text, u32b* f2, u32b* f3,
    u32b* f4)
{
    char flags_buf[128];
    char* token;

    if (f2)
        *f2 = 0;
    if (f3)
        *f3 = 0;
    if (f4)
        *f4 = 0;

    if (!flags_text)
        return;

    if (strlen(flags_text) >= sizeof(flags_buf))
    {
        CHECK(false);
        return;
    }

    strcpy(flags_buf, flags_text);
    token = strtok(flags_buf, "|");
    while (token)
    {
        if (strcmp(token, "SUBTLETY_THROW") == 0)
        {
            if (f4)
                *f4 |= TR4_SUBTLETY_THROW;
        }
        else if (strcmp(token, "OATH_BOOST") == 0)
        {
            if (f3)
                *f3 |= TR3_OATH_BOOST;
        }
        else if (strcmp(token, "OATH_NEGATE") == 0)
        {
            if (f3)
                *f3 |= TR3_OATH_NEGATE;
        }
        else if (strcmp(token, "TRAITOR") == 0)
        {
            if (f2)
                *f2 |= TR2_TRAITOR;
        }
        else
        {
            CHECK(false);
        }

        token = strtok(NULL, "|");
    }
}

static void trim_line(char* text)
{
    size_t len;

    if (!text)
        return;

    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
    {
        text[--len] = '\0';
    }
}

static void test_smithing_phase01_corpus(const char* path)
{
    FILE* fp;
    char line[256];
    int line_no = 0;

    CHECK(path != NULL);
    if (!path)
        return;

    fp = fopen(path, "r");
    CHECK(fp != NULL);
    if (!fp)
        return;

    while (fgets(line, sizeof(line), fp))
    {
        char* name;
        char* tval;
        char* type;
        char* flags;
        char* expected;
        u32b f2;
        u32b f3;
        u32b f4;
        int actual;

        line_no++;
        trim_line(line);

        if (line_no == 1)
            continue;
        if (line[0] == '\0' || line[0] == '#')
            continue;

        name = strtok(line, ",");
        tval = strtok(NULL, ",");
        type = strtok(NULL, ",");
        flags = strtok(NULL, ",");
        expected = strtok(NULL, ",");

        CHECK(name != NULL);
        CHECK(tval != NULL);
        CHECK(type != NULL);
        CHECK(flags != NULL);
        CHECK(expected != NULL);
        if (!name || !tval || !type || !flags || !expected)
            continue;

        CHECK(atoi(tval) > 0);
        CHECK(strcmp(type, "normal") == 0 || strcmp(type, "artefact") == 0);

        parse_parity_flags(flags, &f2, &f3, &f4);
        actual = reliability_smithing_phase01_flag_delta(f2, f3, f4);
        CHECK(actual == atoi(expected));
    }

    fclose(fp);
}

int main(int argc, char** argv)
{
    const char* smithing_corpus = (argc > 1) ? argv[1] : NULL;

    test_clamp_initial_text_len();
    test_sample_square_point();
    test_rle_zero_run_guard();
    test_serialized_layout_validation();
    test_metarun_layout_detection();
    test_runs_db_policy();
    test_smithing_phase01_corpus(smithing_corpus);

    if (g_failures != 0)
    {
        fprintf(stderr, "%d regression test(s) failed.\n", g_failures);
        return 1;
    }

    printf("Phase 0/1 regression tests passed.\n");
    return 0;
}
