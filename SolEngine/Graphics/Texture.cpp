#include "Texture.h"



/***********************************************************************************/
Texture::Texture(const std::string_view Path) : path(Path), width(0), height(0), numChannels(0) {
}

/***********************************************************************************/
void Texture::init(const VkDevice device) {
}

/***********************************************************************************/
void Texture::destroy(const VkDevice device) {
}
