#pragma once

#include <filesystem>

#include "imageCommon.h"

Image loadImage(const std::filesystem::path& filename);
void saveImage(const Image& img, const std::filesystem::path& file);
void savePalettizedImage(const PalettizedImage& pltImg, const std::filesystem::path& file);
void saveSnesPalette(const PalettizedImage::PaletteTable& palette, const std::filesystem::path& file);
void saveSnesTiles(const PalettizedImage& img, const std::filesystem::path& file);
void saveSnesTilemap(unsigned int width, unsigned int height, const std::filesystem::path& file);
void saveSnesHdmaTable(const PalettizedImage& img, const std::filesystem::path &file);
void saveImageStatistics(const ProcessImageStorage& storage, const std::filesystem::path &file);