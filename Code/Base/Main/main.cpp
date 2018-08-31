#include "Pch.h"

#include <External/flags/include/flags.h>

#include <Main/imageIo.h>
#include <Main/imageNtscFilter.h>
#include <Main/imageProcess.h>

int main(int argc, char** argv)
{
	// load in necessary command line arguments
	const flags::args args(argc, argv);
	const auto inFile = args.get<std::string_view>("in");

	if (!inFile.has_value() || !std::filesystem::is_regular_file(inFile.value()))
	{
		std::cout << "Missing input file as cli arg: \"in=<file>\"";
		return 1;
	}

	const auto outDir = args.get<std::string_view>("outDir");

	if (!outDir.has_value() || !std::filesystem::is_directory(outDir.value()))
	{
		std::cout << "Missing output dir as cli arg: \"outDir=<directory>\"";
		return 1;
	}

	std::filesystem::path inFilePath = inFile.value();
	std::filesystem::path outDirPath = outDir.value();

	ProcessImageOutParams outParams;
	ProcessImageParams processImageParams;
	processImageParams.lowBitDepthPalette = false;
	processImageParams.maxColors = 128;
	// load image in and process it according to parameters set above
	processImageParams.img = loadImage(inFilePath);

	processImage(processImageParams, outParams);

	// write out raw as bmp
	{
		std::filesystem::path outbmpPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat(".bmp");
		saveImage(outParams.img, outbmpPath);
	}

	// write out ntsc-processed bmp
	{
		Image ntscFilteredImg = applyNtscFilter(outParams.img);
		std::filesystem::path outFilteredbmpPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat("-filtered.bmp");
		saveImage(ntscFilteredImg, outFilteredbmpPath);
	}

	// write out palette information
	{
		std::filesystem::path outPltImgPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat("-pltidx.bmp");
		savePalettizedImage(outParams.palettizedImage, outParams.img.width, outParams.img.height, outPltImgPath);
	}

	// write out raw as snes binary data
	{
		std::filesystem::path outSnesPltImgPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat(".clr");
		saveSnesPalette(outParams.palettizedImage.palette, outSnesPltImgPath);

		std::filesystem::path outSnesTileImgPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat(".pic");
		saveSnesTiles(outParams.palettizedImage.img, outParams.img.width, outParams.img.height, outSnesTileImgPath);

		std::filesystem::path outSnesMapImgPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat(".map");
		saveSnesTilemap(outParams.img.width, outParams.img.height, outSnesMapImgPath);
	}
	return 0;
}
