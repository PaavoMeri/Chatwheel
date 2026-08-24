#ifndef HEADSET_H
#define HEADSET_H

#include <stdbool.h>
#include <stddef.h>

#define CHATMIX_MAX 128
#define CHATMIX_MIN 0

/*
 * A 4 KiB stack buffer leaves room for variable HID device/product labels
 * while keeping this small fixed CLI response strictly bounded. The capacity
 * includes space for the trailing NUL byte.
 */
#define HEADSET_CLI_OUTPUT_CAPACITY 4096

typedef enum {
    HEADSET_QUERY_OK = 0,
    HEADSET_QUERY_TIMEOUT,
    HEADSET_QUERY_COMMAND_ERROR,
    HEADSET_QUERY_INVALID_OUTPUT,
} headset_query_status_t;

/*
 * There is intentionally no NO_DEVICE status. This scoped bridge preserves
 * nonzero-process-status priority and maps HeadsetControl's exit 1 to
 * COMMAND_ERROR without reclassifying it from stdout. Stderr remains hidden.
 */

/*
 * Copyable value result. chatmix is valid only when status is
 * HEADSET_QUERY_OK. The caller receives no pointers and owns no heap memory.
 */
typedef struct {
    headset_query_status_t status;
    int chatmix;
} headset_chatmix_result_t;

typedef struct {
    bool has_status;
    headset_query_status_t status;
} headset_query_log_state_t;

typedef enum {
    HEADSET_LOG_NONE = 0,
    HEADSET_LOG_ERROR,
    HEADSET_LOG_RECOVERY,
} headset_log_event_t;

/*
 * Parse only this HeadsetControl 4.0 standard output for a ChatMix-only request:
 *
 *   Found 1 supported device(s):\n
 *    <nonempty device descriptor> [0xhhhh:0xhhhh]\n
 *   Chatmix: <canonical decimal integer 0-128>\n
 *
 * Here h is a lowercase hexadecimal digit. The descriptor can contain any
 * non-NUL byte except newline. The complete output, including its final
 * newline, must match and therefore proves that exactly one supported device
 * produced exactly one ChatMix value. output_length excludes the caller's
 * required trailing NUL byte. A NULL pointer, an embedded NUL, or a missing
 * terminator at output[output_length] produces HEADSET_QUERY_INVALID_OUTPUT.
 */
headset_chatmix_result_t headset_parse_standard_output(
    const char *output,
    size_t output_length);

/* Classify the raw wait status returned by pclose(). */
headset_query_status_t headset_classify_wait_status(int wait_status);

/* Run the fixed, bounded HeadsetControl ChatMix command. */
headset_chatmix_result_t get_chatmix_value(void);

/*
 * Return the one log transition caused by status. Repeated identical errors
 * and ordinary successful samples return HEADSET_LOG_NONE. A NULL state is a
 * supported no-op and also returns HEADSET_LOG_NONE.
 */
headset_log_event_t headset_query_log_update(
    headset_query_log_state_t *state,
    headset_query_status_t status);

/* True only for a valid value that differs from the caller's last value. */
bool headset_chatmix_should_apply(
    int previous_chatmix,
    headset_chatmix_result_t result);

const char *headset_query_status_name(headset_query_status_t status);
const char *get_chatmix_mode(int value);

#endif // HEADSET_H
