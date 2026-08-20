#include "pulse_stream_lifecycle.h"

int pulse_stream_lifecycle_record(audio_stream_inventory_t *inventory,
                                  uint32_t index,
                                  const pa_proplist *properties) {
    const char *application_name = NULL;
    const char *process_binary = NULL;

    if (properties) {
        application_name = pa_proplist_gets(properties, "application.name");
        process_binary = pa_proplist_gets(
            properties,
            "application.process.binary");
    }

    return audio_stream_inventory_upsert(
        inventory,
        index,
        application_name,
        process_binary);
}
