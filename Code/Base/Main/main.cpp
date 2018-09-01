#include "Pch.h"

#include <External/flags/include/flags.h>

#include <Main/imageIo.h>
#include <Main/imageNtscFilter.h>
#include <Main/imageProcess.h>

void processFile(const std::filesystem::path &inFilePath, const std::filesystem::path &outDirPath)
{
	ProcessImageOutParams outParams;
	ProcessImageParams processImageParams;
	processImageParams.lowBitDepthPalette = false;
	processImageParams.maxColors = 16;
	// load image in and process it according to parameters set above
	processImageParams.img = loadImage(inFilePath);

	// if the file wasn't an image, skip out
	if (processImageParams.img.imgData.size() == 0)
		return;

	// if the image was too big on either dimention, skip out
	if (processImageParams.img.width > 256 || processImageParams.img.height > 224)
		return;

	processImage(processImageParams, outParams);

	// write out raw as bmp
	{
		std::filesystem::path outbmpPath = outDirPath / inFilePath.stem().concat(".bmp");
		saveImage(outParams.img, outbmpPath);
	}

	// write out ntsc-processed bmp
	{
		Image ntscFilteredImg = applyNtscFilter(outParams.img);
		std::filesystem::path outFilteredbmpPath = outDirPath / inFilePath.stem().concat("-filtered.bmp");
		saveImage(ntscFilteredImg, outFilteredbmpPath);
	}

	// write out palette information
	{
		std::filesystem::path outPltImgPath = outDirPath / inFilePath.stem().concat("-pltidx.bmp");
		savePalettizedImage(outParams.palettizedImage, outParams.img.width, outParams.img.height, outPltImgPath);
	}

	// write out raw as snes binary data
	{
		std::filesystem::path outSnesPltImgPath = outDirPath / inFilePath.stem().concat(".clr");
		saveSnesPalette(outParams.palettizedImage.palette, outSnesPltImgPath);

		std::filesystem::path outSnesTileImgPath = outDirPath / inFilePath.stem().concat(".pic");
		saveSnesTiles(outParams.palettizedImage.img, outParams.img.width, outParams.img.height, outSnesTileImgPath);

		std::filesystem::path outSnesMapImgPath = outDirPath / inFilePath.stem().concat(".map");
		saveSnesTilemap(outParams.img.width, outParams.img.height, outSnesMapImgPath);
	}
}

int main(int argc, char** argv)
{
	// load in necessary command line arguments
	const flags::args args(argc, argv);
	const auto inFile = args.get<std::string_view>("in");

	if (!inFile.has_value())
	{
		std::cout << "Missing input file or directory as cli arg: \"in=<file/dir>\"";
		return 1;
	}

	const auto outDir = args.get<std::string_view>("outDir");
	if (!outDir.has_value() || !std::filesystem::is_directory(outDir.value()))
	{
		std::cout << "Missing output dir as cli arg: \"outDir=<directory>\"";
		return 1;
	}

	std::filesystem::path outDirPath = outDir.value();
	std::filesystem::path inFilePath = inFile.value();

	if (std::filesystem::is_regular_file(inFilePath))
	{
		processFile(inFilePath, outDirPath);
	}
	else if (std::filesystem::is_directory(inFilePath))
	{
		for (const auto& entry : std::filesystem::directory_iterator(inFilePath))
		{
			if (is_regular_file(entry.path()))
			{
				processFile(entry.path(), outDirPath);
			}
		}
	}
	else
	{
		std::cout << "File or directory for input cli arg does not exist: " << inFilePath.c_str();
		return 1;
	}


	return 0;
}
