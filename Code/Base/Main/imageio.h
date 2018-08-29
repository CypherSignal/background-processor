#pragma once

#include <filesystem>

#include "imageCommon.h"

Image loadImage(const std::filesystem::path& filename);
void saveImage(const Image& img, const std::filesystem::path& file);

