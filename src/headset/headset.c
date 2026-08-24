#include "headset.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/*
 * HeadsetControl 4.0 standard output includes the supported-device count and
 * one block per device. That lets the parser reject ambiguous multi-device
 * output. Stderr is kept out of both the parser and the daemon journal. No
 * user-controlled data is added to this command.
 *
 * --timeout bounds an individual HID read. GNU timeout bounds the whole CLI
 * command and escalates from SIGTERM to SIGKILL after a short grace period.
 * This is not an absolute kernel deadline: an uninterruptible process can
 * still delay pclose() while the process is being reaped.
 */
static const char HEADSETCONTROL_COMMAND[] =
    "/usr/bin/timeout --kill-after=0.2s 1s "
    "headsetcontrol -m --timeout 500 -o standard 2>/dev/null";

static const char STANDARD_HEADER[] = "Found 1 supported device(s):\n";
static const char CHATMIX_PREFIX[] = "Chatmix: ";

static headset_chatmix_result_t make_result(
    headset_query_status_t status,
    int chatmix) {
    headset_chatmix_result_t result = {
        .status = status,
        .chatmix = chatmix,
    };
    return result;
}

static bool is_lower_hex_digit(char value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f');
}

static bool is_valid_device_line(const char *line, size_t length) {
    const size_t id_suffix_length = 16;
    size_t suffix_offset;

    if (!line || length < 18 || line[0] != ' ') {
        return false;
    }

    suffix_offset = length - id_suffix_length;
    if (suffix_offset <= 1) {
        return false;
    }

    const char *suffix = line + suffix_offset;
    if (suffix[0] != ' ' || suffix[1] != '[' ||
        suffix[2] != '0' || suffix[3] != 'x' || suffix[8] != ':' ||
        suffix[9] != '0' || suffix[10] != 'x' || suffix[15] != ']') {
        return false;
    }
    for (size_t i = 4; i <= 7; i++) {
        if (!is_lower_hex_digit(suffix[i])) {
            return false;
        }
    }
    for (size_t i = 11; i <= 14; i++) {
        if (!is_lower_hex_digit(suffix[i])) {
            return false;
        }
    }
    return true;
}

static headset_chatmix_result_t parse_chatmix_token(
    const char *token,
    size_t length) {
    char number[4];
    char *end = NULL;
    long value;

    if (!token || length == 0 || length >= sizeof(number)) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    /* ostream's integer output is canonical: digits only and no leading zero. */
    if (token[0] == '0' && length != 1) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }
    for (size_t i = 0; i < length; i++) {
        if (token[i] < '0' || token[i] > '9') {
            return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
        }
    }

    memcpy(number, token, length);
    number[length] = '\0';
    errno = 0;
    value = strtol(number, &end, 10);
    if (errno == ERANGE || end == number || *end != '\0' ||
        value < CHATMIX_MIN || value > CHATMIX_MAX) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    return make_result(HEADSET_QUERY_OK, (int)value);
}

headset_chatmix_result_t headset_parse_standard_output(
    const char *output,
    size_t output_length) {
    const char *device_end;
    const char *chatmix_end;
    const char *cursor;
    const char *output_end;
    size_t remaining;

    if (!output || output_length >= HEADSET_CLI_OUTPUT_CAPACITY ||
        output[output_length] != '\0' ||
        memchr(output, '\0', output_length) != NULL) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    output_end = output + output_length;
    if (output_length < sizeof(STANDARD_HEADER) - 1 ||
        memcmp(output, STANDARD_HEADER, sizeof(STANDARD_HEADER) - 1) != 0) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    cursor = output + sizeof(STANDARD_HEADER) - 1;
    remaining = (size_t)(output_end - cursor);
    device_end = memchr(cursor, '\n', remaining);
    if (!device_end ||
        !is_valid_device_line(cursor, (size_t)(device_end - cursor))) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    cursor = device_end + 1;
    remaining = (size_t)(output_end - cursor);
    if (remaining < sizeof(CHATMIX_PREFIX) - 1 ||
        memcmp(cursor, CHATMIX_PREFIX, sizeof(CHATMIX_PREFIX) - 1) != 0) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    cursor += sizeof(CHATMIX_PREFIX) - 1;
    remaining = (size_t)(output_end - cursor);
    chatmix_end = memchr(cursor, '\n', remaining);
    if (!chatmix_end || chatmix_end + 1 != output_end) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    return parse_chatmix_token(cursor, (size_t)(chatmix_end - cursor));
}

headset_query_status_t headset_classify_wait_status(int wait_status) {
    if (wait_status == -1) {
        return HEADSET_QUERY_COMMAND_ERROR;
    }
    if (WIFEXITED(wait_status)) {
        int exit_status = WEXITSTATUS(wait_status);
        if (exit_status == 0) {
            return HEADSET_QUERY_OK;
        }
        if (exit_status == 124) {
            return HEADSET_QUERY_TIMEOUT;
        }
        /*
         * GNU timeout can return 137 after --kill-after, but 137 can also mean
         * that the command was killed independently. It is not uniquely a
         * timeout and is therefore a command error here.
         */
        return HEADSET_QUERY_COMMAND_ERROR;
    }
    if (WIFSIGNALED(wait_status)) {
        return HEADSET_QUERY_COMMAND_ERROR;
    }
    return HEADSET_QUERY_COMMAND_ERROR;
}

headset_chatmix_result_t get_chatmix_value(void) {
    FILE *pipe = popen(HEADSETCONTROL_COMMAND, "r");
    char output[HEADSET_CLI_OUTPUT_CAPACITY];
    char drain[256];
    size_t output_length = 0;
    bool oversized = false;
    bool read_failed = false;
    int wait_status;
    headset_query_status_t command_status;

    if (!pipe) {
        return make_result(HEADSET_QUERY_COMMAND_ERROR, 0);
    }

    output_length = fread(output, 1, sizeof(output) - 1, pipe);
    if (ferror(pipe)) {
        read_failed = true;
    }

    while (!read_failed) {
        size_t drained = fread(drain, 1, sizeof(drain), pipe);
        if (drained > 0) {
            oversized = true;
        }
        if (drained < sizeof(drain)) {
            if (ferror(pipe)) {
                read_failed = true;
            }
            break;
        }
    }
    output[output_length] = '\0';

    wait_status = pclose(pipe);
    command_status = headset_classify_wait_status(wait_status);
    if (command_status != HEADSET_QUERY_OK) {
        return make_result(command_status, 0);
    }
    if (read_failed) {
        return make_result(HEADSET_QUERY_COMMAND_ERROR, 0);
    }
    if (oversized) {
        return make_result(HEADSET_QUERY_INVALID_OUTPUT, 0);
    }

    return headset_parse_standard_output(output, output_length);
}

headset_log_event_t headset_query_log_update(
    headset_query_log_state_t *state,
    headset_query_status_t status) {
    headset_log_event_t event = HEADSET_LOG_NONE;

    if (!state) {
        return HEADSET_LOG_NONE;
    }

    if (status == HEADSET_QUERY_OK) {
        if (state->has_status && state->status != HEADSET_QUERY_OK) {
            event = HEADSET_LOG_RECOVERY;
        }
    } else if (!state->has_status || state->status != status) {
        event = HEADSET_LOG_ERROR;
    }

    state->has_status = true;
    state->status = status;
    return event;
}

bool headset_chatmix_should_apply(
    int previous_chatmix,
    headset_chatmix_result_t result) {
    return result.status == HEADSET_QUERY_OK &&
           result.chatmix != previous_chatmix;
}

const char *headset_query_status_name(headset_query_status_t status) {
    switch (status) {
        case HEADSET_QUERY_OK:
            return "ok";
        case HEADSET_QUERY_TIMEOUT:
            return "timeout";
        case HEADSET_QUERY_COMMAND_ERROR:
            return "command error";
        case HEADSET_QUERY_INVALID_OUTPUT:
            return "invalid output";
        default:
            return "unknown error";
    }
}

const char *get_chatmix_mode(int value) {
    if (value < 0) return "Unknown";
    if (value < 32) return "100% Game";     // Top quarter
    if (value < 64) return "Game Focus";    // Upper middle
    if (value == 64) return "Balanced";     // Middle
    if (value < 96) return "Chat Focus";    // Lower middle
    return "100% Chat";                     // Bottom quarter
}
