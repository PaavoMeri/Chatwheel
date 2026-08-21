#ifndef PULSE_STREAM_LIFECYCLE_H
#define PULSE_STREAM_LIFECYCLE_H

#include <pulse/proplist.h>
#include <stdint.h>

#include "../audio_stream_inventory.h"

int pulse_stream_lifecycle_record(audio_stream_inventory_t *inventory,
                                  uint32_t index,
                                  unsigned int channel_count,
                                  const pa_proplist *properties);

#endif
