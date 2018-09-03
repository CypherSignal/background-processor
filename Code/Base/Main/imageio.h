#pragma once

#include <filesystem>

#include "imageCommon.h"

Image loadImage(const std::filesystem::path& filename);
void saveImage(const Image& img, const std::filesystem::path& file);
void savePalettizedImage(const PalettizedImage& pltImg, const std::filesystem::path& file);
void saveSnesPalette(eastl::fixed_vector<unsigned short, 256, false>& palette, const std::filesystem::path& file);
void saveSnesTiles(const PalettizedImage& img, const std::filesystem::path& file);
void saveSnesTilemap(unsigned int width, unsigned int height, const std::filesystem::path& file);