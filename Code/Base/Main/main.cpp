#include "Pch.h"

#include <External/flags/include/flags.h>

#include <Main/imageIo.h>
#include <Main/imageNtscFilter.h>
#include <Main/imageProcess.h>

Concurrency::task<void> processFile(const ProcessImageParams &params)
{
	return Concurrency::create_task([&params]{
		ProcessImageStorage storage;

		// load image in and process it according to parameters set above
		storage.srcImg = loadImage(params.inFilePath);

		// if the file wasn't an image, skip out
		if (storage.srcImg.imgData.size() == 0)
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
				saveImage(storage.processedImg, outPngPath);
			}),

			// write out ntsc-processed png
			Concurrency::create_task([&params, &storage]
			{
				Image ntscFilteredImg = applyNtscFilter(storage.processedImg);
				std::filesystem::path outFilteredPngPath = params.outDirPath / params.inFilePath.stem().concat("-filtered.png");
				saveImage(ntscFilteredImg, outFilteredPngPath);
			}),

			// write out palette information
			Concurrency::create_task([&params, &storage]
			{
				std::filesystem::path outPltImgPath = params.outDirPath / params.inFilePath.stem().concat("-pltidx.png");
				savePalettizedImage(storage.palettizedImage, storage.processedImg.width, storage.processedImg.height, outPltImgPath);
			}),

			// write out palette data
			Concurrency::create_task([&params, &storage]
			{
				std::filesystem::path outSnesPltImgPath = params.outDirPath / params.inFilePath.stem().concat(".clr");
				saveSnesPalette(storage.palettizedImage.palette, outSnesPltImgPath);
			}),

			// write out tile data
			Concurrency::create_task([&params, &storage]
			{
				std::filesystem::path outSnesTileImgPath = params.outDirPath / params.inFilePath.stem().concat(".pic");
				saveSnesTiles(storage.palettizedImage.img, storage.processedImg.width, storage.processedImg.height, outSnesTileImgPath);
			}),

			// write out tilemap data
			Concurrency::create_task([&params, &storage]
			{
				std::filesystem::path outSnesMapImgPath = params.outDirPath / params.inFilePath.stem().concat(".map");
				saveSnesTilemap(storage.processedImg.width, storage.processedImg.height, outSnesMapImgPath);
			})
		};
		Concurrency::when_all(tasks.begin(), tasks.end()).wait();
	});
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
		ProcessImageParams params;
		params.lowBitDepthPalette = false;
		params.maxColors = 256;
		params.inFilePath = inFilePath;
		params.outDirPath = outDirPath;
		processFile(params).wait();
	}
	else if (std::filesystem::is_directory(inFilePath))
	{
		ProcessImageParams processImageParams;
		processImageParams.lowBitDepthPalette = false;
		processImageParams.maxColors = 256;
		processImageParams.outDirPath = outDirPath;

		eastl::vector<ProcessImageParams> processParams;
		for (const auto& entry : std::filesystem::directory_iterator(inFilePath))
		{
			if (is_regular_file(entry.path()))
			{
				processImageParams.inFilePath = entry.path();
				processParams.push_back(processImageParams);
			}
		}
		eastl::vector<Concurrency::task<void>> tasks;
		tasks.reserve(processParams.size());
		for (int i = 0; i < processParams.size(); ++i)
		{
			tasks.push_back(processFile(processParams[i]));
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
