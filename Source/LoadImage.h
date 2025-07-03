#pragma once

#include <CoreTypes.h>

void load_image_rgba(const char* path, s32* width, s32* height, s32* original_channels, u8** out_data);
void free_image(u8* data);
