#include "Pch.h"

#include <External/flags/include/flags.h>

#include <Main/imageIo.h>
#include <Main/imageNtscFilter.h>
#include <Main/imageProcess.h>

void processFile(const ProcessImageParams &params)
{
	ProcessImageStorage storage;

	// load image in and process it according to parameters set above
	storage.srcImg = loadImage(params.inFilePath);

	// if the file wasn't an image, skip out
	if (storage.srcImg.data.size() == 0)
		return;

	// if the image was too big on either dimension, skip out
	if (storage.srcImg.width > 256 || storage.srcImg.height > 224)
		return;

	processImage(params, storage);
		
	eastl::vector<Concurrency::task<void>> tasks{
		// write out raw as png
		Concurrency::create_task([&params, &storage]
		{
			std::filesystem::path outPngPath = params.outDirPath / params.inFilePath.stem().concat(".png");
			saveImage(getDepalettizedImage(storage.palettizedImg), outPngPath);
		}),

		// write out ntsc-processed png
		Concurrency::create_task([&params, &storage]
		{
			std::filesystem::path outFilteredPngPath = params.outDirPath / params.inFilePath.stem().concat("-filtered.png");
			saveImage(applyNtscFilter(storage.palettizedImg), outFilteredPngPath);
		}),

		// write out palette information
		Concurrency::create_task([&params, &storage]
		{
			std::filesystem::path outPltImgPath = params.outDirPath / params.inFilePath.stem().concat("-pltidx.png");
			savePalettizedImage(storage.palettizedImg, outPltImgPath);
		}),

		// write out palette data
		Concurrency::create_task([&params, &storage]
		{
			std::filesystem::path outSnesPltImgPath = params.outDirPath / params.inFilePath.stem().concat(".clr");
			saveSnesPalette(storage.palettizedImg.palette, outSnesPltImgPath);
		}),

		// write out tile data
		Concurrency::create_task([&params, &storage]
		{
			std::filesystem::path outSnesTileImgPath = params.outDirPath / params.inFilePath.stem().concat(".pic");
			saveSnesTiles(storage.palettizedImg, outSnesTileImgPath);
		}),

		// write out tilemap data
		Concurrency::create_task([&params, &storage]
		{
			std::filesystem::path outSnesMapImgPath = params.outDirPath / params.inFilePath.stem().concat(".map");
			saveSnesTilemap(storage.palettizedImg.width, storage.palettizedImg.height, outSnesMapImgPath);
		})
	};
	Concurrency::when_all(tasks.begin(), tasks.end()).wait();
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

	ProcessImageParams params;
	params.lowBitDepthPalette = false;
	params.maxColors = 256;
	params.outDirPath = outDirPath;
	if (std::filesystem::is_regular_file(inFilePath))
	{
		params.inFilePath = inFilePath;
		processFile(params);
	}
	else if (std::filesystem::is_directory(inFilePath))
	{
		eastl::vector<ProcessImageParams> processParamsList;
		{
			for (const auto& entry : std::filesystem::directory_iterator(inFilePath))
			{
				if (is_regular_file(entry.path()))
				{
					params.inFilePath = entry.path();
					processParamsList.push_back(params);
				}
			}
		}
		eastl::vector<Concurrency::task<void>> tasks;
		tasks.reserve(processParamsList.size());
		for (auto& processParams : processParamsList)
		{
			tasks.push_back(Concurrency::create_task([&processParams]() { processFile(processParams); }));
		}
		Concurrency::when_all(tasks.begin(), tasks.end()).wait();
	}
	else
	{
		std::cout << "File or directory for input cli arg does not exist: " << inFilePath.c_str();
		return 1;
	}

	return 0;
}
