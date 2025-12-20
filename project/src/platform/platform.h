#ifndef PLATFORM_H
#define PLATFORM_H

typedef struct {
    int ALPHA_SHIFT;
    int RED_SHIFT;
    int GREEN_SHIFT;
    int BLUE_SHIFT;
} PlatformPixelFormatShift;

int platform_main();

#endif // PLATFORM_H
