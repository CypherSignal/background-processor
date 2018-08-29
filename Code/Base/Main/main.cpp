#include "Pch.h"

#include <External/flags/include/flags.h>
#include <Main\imageio.h>

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

	// load image in
	Image img = loadImage(inFile.value());

	// do processing on it

	// write out raw as png
	// write out raw as snes binary data
	// write out ntsc-processed png

	return 0;
}
