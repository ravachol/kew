#ifndef OPTIONS_H
#define OPTIONS_H

// Some preprocessor magic to generate an enum and string array with the same items.
#define OUTPUT_MODES(MODE) \
        MODE(ANSI)         \
        MODE(SOLID_ANSI)   \
        MODE(ASCII)        \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum OutputModes {OUTPUT_MODES(GENERATE_ENUM)};
static const char * OutputModeStr[] = {OUTPUT_MODES(GENERATE_STRING)}; // Strings used for output.

typedef struct {
    unsigned int width;
    unsigned int height;

    enum OutputModes output_mode;

    bool original_size;
    bool true_color;
    bool squashing_enabled;
    bool suppress_header;
} ImageOptions;

#endif  /* OPTIONS_H */
