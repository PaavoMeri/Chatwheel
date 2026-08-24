#include "headset/headset.h"

#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char mock_directory[] = "/tmp/chatwheel-headset-test.XXXXXX";
static char mock_path[PATH_MAX];
static char *saved_path;

static const char TEST_HEADER[] = "Found 1 supported device(s):\n";
static const char TEST_DEVICE[] =
    " Mock Headset (Mock Product) [0xf00b:0xa00c]\n";

static void write_mock_headsetcontrol(void) {
    FILE *script;
    int written = snprintf(
        mock_path,
        sizeof(mock_path),
        "%s/headsetcontrol",
        mock_directory);
    assert(written > 0 && (size_t)written < sizeof(mock_path));

    script = fopen(mock_path, "w");
    assert(script != NULL);
    assert(fputs(
        "#!/bin/sh\n"
        "if [ \"$#\" -ne 5 ] || [ \"$1\" != '-m' ] || "
        "[ \"$2\" != '--timeout' ] || [ \"$3\" != '500' ] || "
        "[ \"$4\" != '-o' ] || [ \"$5\" != 'standard' ]; then\n"
        "    exit 99\n"
        "fi\n"
        "print_header() { printf '%s\\n' 'Found 1 supported device(s):'; }\n"
        "print_header2() { printf '%s\\n' 'Found 2 supported device(s):'; }\n"
        "print_device1() { printf '%s\\n' "
        "' Mock Headset (Mock Product) [0xf00b:0xa00c]'; }\n"
        "print_device2() { printf '%s\\n' "
        "' Other Headset [0x1234:0xabcd]'; }\n"
        "print_valid() { print_header; print_device1; "
        "printf 'Chatmix: %s\\n' \"$1\"; }\n"
        "case \"${HSC_MOCK_MODE-}\" in\n"
        "    valid0) print_valid 0 ;;\n"
        "    valid64) print_valid 64 ;;\n"
        "    valid128) print_valid 128 ;;\n"
        "    two_devices) print_header2; print_device1; "
        "printf 'Chatmix: 32\\n'; print_device2; printf 'Chatmix: 96\\n' ;;\n"
        "    header1_two_blocks) print_header; print_device1; "
        "printf 'Chatmix: 32\\n'; print_device2; printf 'Chatmix: 96\\n' ;;\n"
        "    header2_one_block) print_header2; print_device1; "
        "printf 'Chatmix: 64\\n' ;;\n"
        "    missing_chatmix) print_header; print_device1 ;;\n"
        "    duplicate_chatmix) print_header; print_device1; "
        "printf 'Chatmix: 64\\nChatmix: 64\\n' ;;\n"
        "    zero_devices) printf '%s\\n' 'No supported device found' ;;\n"
        "    bad_prefix) print_header; print_device1; printf 'ChatMix: 64\\n' ;;\n"
        "    missing_suffix) print_header; print_device1; printf '64\\n' ;;\n"
        "    extra_before) print_header; print_device1; "
        "printf 'Chatmix: 1 64\\n' ;;\n"
        "    extra_after) print_header; print_device1; "
        "printf 'Chatmix: 64 1\\n' ;;\n"
        "    negative) print_valid -1 ;;\n"
        "    out_of_range) print_valid 129 ;;\n"
        "    overflow) print_valid 999999999999999999999999999999999999 ;;\n"
        "    trailing) print_valid 64x ;;\n"
        "    double_zero) print_valid 00 ;;\n"
        "    leading_zero) print_valid 01 ;;\n"
        "    leading_plus) print_valid +1 ;;\n"
        "    leading_space) print_header; print_device1; "
        "printf 'Chatmix:  64\\n' ;;\n"
        "    trailing_space) print_header; print_device1; "
        "printf 'Chatmix: 64 \\n' ;;\n"
        "    truncated) print_header; print_device1; printf 'Chatmix: 64' ;;\n"
        "    empty) : ;;\n"
        "    boundary) i=0; while [ \"$i\" -lt 4095 ]; do printf '1'; "
        "i=$((i + 1)); done ;;\n"
        "    oversized) i=0; while [ \"$i\" -lt 4096 ]; do printf '1'; "
        "i=$((i + 1)); done ;;\n"
        "    stderr_oversized) i=0; while [ \"$i\" -lt 512 ]; do "
        "printf 'e' >&2; i=$((i + 1)); done; print_valid 64 ;;\n"
        "    stderr_valid_only) printf 'Chatmix: 64\\n' >&2 ;;\n"
        "    partial_valid) print_header; print_device1; printf 'Chatmix: '; "
        "printf '6'; /bin/sleep 0.01; printf '4\\n' ;;\n"
        "    nul_trailing) print_valid 64; printf '\\000trailing' ;;\n"
        "    nul_device) print_header; printf "
        "' Mock\\000 Headset [0xf00b:0xa00c]\\nChatmix: 64\\n' ;;\n"
        "    nul_chatmix) print_header; print_device1; "
        "printf 'Chatmix: 6\\0004\\n' ;;\n"
        "    nonzero_valid) print_valid 64; exit 7 ;;\n"
        "    nonzero) exit 7 ;;\n"
        "    no_device) printf 'No supported device found\\n'; exit 1 ;;\n"
        "    timeout) /bin/sleep 5 ;;\n"
        "    kill_after) trap '' TERM; while :; do :; done ;;\n"
        "    *) exit 98 ;;\n"
        "esac\n",
        script) >= 0);
    assert(fclose(script) == 0);
    assert(chmod(mock_path, 0700) == 0);
}

static void validate_mock_syntax(void) {
    pid_t child = fork();
    int wait_status;

    assert(child >= 0);
    if (child == 0) {
        execl("/bin/sh", "sh", "-n", mock_path, (char *)NULL);
        _exit(127);
    }
    assert(waitpid(child, &wait_status, 0) == child);
    assert(WIFEXITED(wait_status));
    assert(WEXITSTATUS(wait_status) == 0);
}

static void setup_mock_path(void) {
    const char *current_path = getenv("PATH");

    assert(mkdtemp(mock_directory) != NULL);
    saved_path = current_path ? strdup(current_path) : NULL;
    assert(!current_path || saved_path != NULL);
    write_mock_headsetcontrol();
    validate_mock_syntax();
    assert(setenv("PATH", mock_directory, 1) == 0);
    assert(strcmp(getenv("PATH"), mock_directory) == 0);
}

static void cleanup_mock_path(void) {
    if (saved_path) {
        assert(setenv("PATH", saved_path, 1) == 0);
    } else {
        assert(unsetenv("PATH") == 0);
    }
    assert(unsetenv("HSC_MOCK_MODE") == 0);
    assert(unlink(mock_path) == 0);
    assert(rmdir(mock_directory) == 0);
    free(saved_path);
}

static void expect_parse_ok(int expected) {
    char output[256];
    int written = snprintf(
        output,
        sizeof(output),
        "%s%sChatmix: %d\n",
        TEST_HEADER,
        TEST_DEVICE,
        expected);
    headset_chatmix_result_t result;

    assert(written > 0 && (size_t)written < sizeof(output));
    result = headset_parse_standard_output(output, (size_t)written);
    assert(result.status == HEADSET_QUERY_OK);
    assert(result.chatmix == expected);
}

static void expect_parse_bytes_ok(
    const char *output,
    size_t output_length,
    int expected) {
    headset_chatmix_result_t result =
        headset_parse_standard_output(output, output_length);

    assert(result.status == HEADSET_QUERY_OK);
    assert(result.chatmix == expected);
}

static void expect_parse_invalid_bytes(
    const char *output,
    size_t output_length) {
    headset_chatmix_result_t result =
        headset_parse_standard_output(output, output_length);
    assert(result.status == HEADSET_QUERY_INVALID_OUTPUT);
}

static void expect_parse_invalid(const char *output) {
    expect_parse_invalid_bytes(output, output ? strlen(output) : 0);
}

static void test_exact_standard_output_parser(void) {
    static const char tab_description[] =
        "Found 1 supported device(s):\n"
        " Mock\tHeadset [0xf00b:0xa00c]\n"
        "Chatmix: 64\n";
    static const char nul_trailing[] =
        "Found 1 supported device(s):\n"
        " Mock Headset [0xf00b:0xa00c]\n"
        "Chatmix: 64\n\0trailing";
    static const char nul_device[] =
        "Found 1 supported device(s):\n"
        " Mock\0 Headset [0xf00b:0xa00c]\n"
        "Chatmix: 64\n";
    static const char nul_chatmix[] =
        "Found 1 supported device(s):\n"
        " Mock Headset [0xf00b:0xa00c]\n"
        "Chatmix: 6\0 4\n";

    expect_parse_ok(0);
    expect_parse_ok(64);
    expect_parse_ok(128);
    expect_parse_bytes_ok(
        tab_description,
        sizeof(tab_description) - 1,
        64);

    expect_parse_invalid(NULL);
    expect_parse_invalid("");
    expect_parse_invalid_bytes(nul_trailing, sizeof(nul_trailing) - 1);
    expect_parse_invalid_bytes(nul_device, sizeof(nul_device) - 1);
    expect_parse_invalid_bytes(nul_chatmix, sizeof(nul_chatmix) - 1);
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        "  [0xf00b:0xa00c]\n"
        "Chatmix: 64\n");
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        " Mock Headset [1234:abcd]\n"
        "Chatmix: 64\n");
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        " Mock Headset\n"
        "Chatmix: 64\n");
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        " Mock Headset (Mock Product) [0xf00b:0xa00c]\n"
        "Chatmix: 00\n");
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        " Mock Headset (Mock Product) [0xf00b:0xa00c]\n"
        "Chatmix: 01\n");
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        " Mock Headset (Mock Product) [0xf00b:0xa00c]\n"
        "Chatmix: +1\n");
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        " Mock Headset (Mock Product) [0xF00B:0xa00c]\n"
        "Chatmix: 64\n");
    expect_parse_invalid(
        "Found 1 supported device(s):\n"
        " Mock Headset (Mock Product) [0xf00b:0xa00c]\n"
        "Chatmix: 64");
}

static headset_chatmix_result_t run_mock(const char *mode) {
    assert(setenv("HSC_MOCK_MODE", mode, 1) == 0);
    return get_chatmix_value();
}

static void expect_query(
    const char *mode,
    headset_query_status_t expected_status,
    int expected_chatmix) {
    headset_chatmix_result_t result = run_mock(mode);
    assert(result.status == expected_status);
    if (expected_status == HEADSET_QUERY_OK) {
        assert(result.chatmix == expected_chatmix);
    }
}

static void test_bounded_command_and_output(void) {
    assert(HEADSET_CLI_OUTPUT_CAPACITY == 4096);
    expect_query("valid0", HEADSET_QUERY_OK, 0);
    expect_query("valid64", HEADSET_QUERY_OK, 64);
    expect_query("valid128", HEADSET_QUERY_OK, 128);
    expect_query("two_devices", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("header1_two_blocks", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("header2_one_block", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("missing_chatmix", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("duplicate_chatmix", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("zero_devices", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("bad_prefix", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("missing_suffix", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("extra_before", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("extra_after", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("negative", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("out_of_range", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("overflow", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("trailing", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("double_zero", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("leading_zero", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("leading_plus", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("leading_space", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("trailing_space", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("truncated", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("empty", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("boundary", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("oversized", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("stderr_oversized", HEADSET_QUERY_OK, 64);
    expect_query("stderr_valid_only", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("partial_valid", HEADSET_QUERY_OK, 64);
    expect_query("nul_trailing", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("nul_device", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("nul_chatmix", HEADSET_QUERY_INVALID_OUTPUT, 0);
    expect_query("nonzero_valid", HEADSET_QUERY_COMMAND_ERROR, 0);
    expect_query("nonzero", HEADSET_QUERY_COMMAND_ERROR, 0);
    expect_query("no_device", HEADSET_QUERY_COMMAND_ERROR, 0);
    expect_query("timeout", HEADSET_QUERY_TIMEOUT, 0);
    expect_query("kill_after", HEADSET_QUERY_COMMAND_ERROR, 0);
}

static void test_wait_status_classification(void) {
    pid_t child;
    int wait_status;

    assert(headset_classify_wait_status(-1) == HEADSET_QUERY_COMMAND_ERROR);

    child = fork();
    assert(child >= 0);
    if (child == 0) {
        raise(SIGTERM);
        _exit(1);
    }
    assert(waitpid(child, &wait_status, 0) == child);
    assert(WIFSIGNALED(wait_status));
    assert(headset_classify_wait_status(wait_status) ==
           HEADSET_QUERY_COMMAND_ERROR);
}

static void test_log_transitions(void) {
    headset_query_log_state_t state = {0};
    headset_query_log_state_t initial_error = {0};

    assert(headset_query_log_update(NULL, HEADSET_QUERY_TIMEOUT) ==
           HEADSET_LOG_NONE);
    assert(headset_query_log_update(&initial_error, HEADSET_QUERY_TIMEOUT) ==
           HEADSET_LOG_ERROR);
    assert(headset_query_log_update(&initial_error, HEADSET_QUERY_TIMEOUT) ==
           HEADSET_LOG_NONE);
    assert(headset_query_log_update(&state, HEADSET_QUERY_OK) ==
           HEADSET_LOG_NONE);
    assert(headset_query_log_update(&state, HEADSET_QUERY_OK) ==
           HEADSET_LOG_NONE);
    assert(headset_query_log_update(&state, HEADSET_QUERY_TIMEOUT) ==
           HEADSET_LOG_ERROR);
    assert(headset_query_log_update(&state, HEADSET_QUERY_TIMEOUT) ==
           HEADSET_LOG_NONE);
    assert(headset_query_log_update(&state, HEADSET_QUERY_COMMAND_ERROR) ==
           HEADSET_LOG_ERROR);
    assert(headset_query_log_update(&state, HEADSET_QUERY_COMMAND_ERROR) ==
           HEADSET_LOG_NONE);
    assert(headset_query_log_update(&state, HEADSET_QUERY_OK) ==
           HEADSET_LOG_RECOVERY);
    assert(headset_query_log_update(&state, HEADSET_QUERY_OK) ==
           HEADSET_LOG_NONE);
}

static void test_value_change_decision(void) {
    headset_chatmix_result_t valid64 = {
        .status = HEADSET_QUERY_OK,
        .chatmix = 64,
    };
    headset_chatmix_result_t invalid = {
        .status = HEADSET_QUERY_INVALID_OUTPUT,
        .chatmix = 32,
    };

    assert(!headset_chatmix_should_apply(64, valid64));
    assert(headset_chatmix_should_apply(32, valid64));
    assert(headset_chatmix_should_apply(-1, valid64));
    assert(!headset_chatmix_should_apply(64, invalid));
}

int main(void) {
    test_exact_standard_output_parser();
    setup_mock_path();
    test_bounded_command_and_output();
    cleanup_mock_path();
    test_wait_status_classification();
    test_log_transitions();
    test_value_change_decision();

    printf("headset tests passed\n");
    return 0;
}
