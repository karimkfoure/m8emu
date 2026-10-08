#ifndef M8_HOST_PROBE_H
#define M8_HOST_PROBE_H
#include <stdbool.h>
#include <stdio.h>

/* Guest-free SDL diagnostics. Both functions own their SDL initialization. */
int m8_host_info(FILE *output);
int m8_host_video_probe(FILE *output, bool fullscreen, bool integer_scale, int vsync);
#endif
