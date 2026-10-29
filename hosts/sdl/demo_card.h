#ifndef M8_DESKTOP_DEMO_CARD_H
#define M8_DESKTOP_DEMO_CARD_H
#include "fat.h"
/* The web and desktop hosts use the same unmodified sample project. */
#define M8_DESKTOP_DEMO_PROJECT "DEMO1"
m8_fat_result m8_desktop_demo_create(m8_fat_card **card,m8_fat_error *error);
#endif
