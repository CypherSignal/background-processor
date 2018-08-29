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

	// load image in
	Image img = loadImage(inFilePath);

	// do processing on it
	Image processedImg = processImage(img);
	Image ntscFilteredImg = applyNtscFilter(processedImg);

	// write out raw as png
	{
		std::filesystem::path outPngPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat(".png");
		saveImage(processedImg, outPngPath);
	}

	// write out ntsc-processed png
	{
		std::filesystem::path outFilteredPngPath = std::filesystem::path(outDirPath) / inFilePath.stem().concat("-filtered.png");
		saveImage(ntscFilteredImg, outFilteredPngPath);
	}

	// write out raw as snes binary data

	return 0;
}
