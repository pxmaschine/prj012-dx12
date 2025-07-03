#include <LoadImage.h>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb/stb_image.h>


void load_image_rgba(const char* path, s32* width, s32* height, s32* original_channels, u8** out_data)
{
    stbi_set_flip_vertically_on_load(true);
    *out_data = stbi_load(path, width, height, original_channels, 4);
}

void free_image(u8* data)
{
    stbi_image_free(data);
}
