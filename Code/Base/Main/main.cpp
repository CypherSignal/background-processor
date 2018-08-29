#include "Pch.h"

#include <External/flags/include/flags.h>

int main(int argc, char** argv)
{
	// load in necessary command line arguments
	const flags::args args(argc, argv);
	const auto inFile = args.get<std::string_view>("in");

	// load image in
	// do processing on it
	// write it out as png
	// write it out as snes binary data

	return 0;
}
