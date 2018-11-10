#include "decoder.h"

enum encoding_format {
        ENCODING_FORMAT_UNKNOWN = 0,
        ENCODING_FORMAT_MPEG,
        ENCODING_FORMAT_WEBM_VORBIS,
        ENCODING_FORMAT_WEBM_OPUS,
        ENCODING_FORMAT_AAC
};

enum encoding_format audio_format = ENCODING_FORMAT_UNKNOWN;
