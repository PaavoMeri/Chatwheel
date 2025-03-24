#ifndef HEADSET_H
#define HEADSET_H

#define CHATMIX_MAX 128
#define CHATMIX_MIN 0

int get_chatmix_value(void);
const char* get_chatmix_mode(int value);

#endif // HEADSET_H