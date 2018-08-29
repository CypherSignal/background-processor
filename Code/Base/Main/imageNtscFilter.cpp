#include "Pch.h"

#include "imageNtscFilter.h"

#include <EASTL/vector.h>
#include <External/blargg_ntsc/snes_ntsc.h>

#include <External/blargg_ntsc/snes_ntsc.c>

Image applyNtscFilter(const Image& img)
{
	snes_ntsc_t* snesNtscObj = new snes_ntsc_t;

	snes_ntsc_setup_t setup;
	setup = snes_ntsc_svideo;
	snes_ntsc_init(snesNtscObj, &setup);

	// prep the data for input into the ntsc filter
	eastl::vector<unsigned short> snesImgData;
	snesImgData.resize(img.imgData.size());

	{
		auto srcImgIter = img.imgData.begin();
		auto srcImgIterEnd = img.imgData.end();
		auto snesImgIter = snesImgData.begin();
		
		while (srcImgIter != srcImgIterEnd)
		{
			// TODO: this quantization of the colours is bad. Rounds down all of the time.
			unsigned short snesPx = ((srcImgIter->r & 0xf8) << 8) | ((srcImgIter->g & 0xfc) << 3) | (srcImgIter->b & 0xf8) >> 3;
			(*snesImgIter) = snesPx;

			srcImgIter++;
			snesImgIter++;
		}
	}

	// run the filter
	eastl::vector<char> filteredData;
	int outImgWidth = SNES_NTSC_OUT_WIDTH(img.width);
	filteredData.resize(outImgWidth * img.height * 2 * 4); // times 2 because it's doubling height; times 4 because output is 4Bpp
	snes_ntsc_blit(snesNtscObj, snesImgData.data(), img.width, 0, img.width, img.height, filteredData.data(), outImgWidth * 4);
	delete snesNtscObj;

	// scale up data and prepare it for usage as an image
	Image outImg;
	outImg.comp = img.comp;
	outImg.width = outImgWidth;
	outImg.height = img.height * 2;
	outImg.imgData.resize(outImgWidth * outImg.height * 2);
	{
		for (int row = 0; row < img.height; ++row)
		{
			for (int col = 0; col < outImgWidth; ++col)
			{
				int outImgIdx = row * outImgWidth * 2 + col;
				int filteredImgIdx = (row * outImgWidth + col) * 4;

				outImg.imgData[outImgIdx].b = filteredData[filteredImgIdx + 0];
				outImg.imgData[outImgIdx].g = filteredData[filteredImgIdx + 1];
				outImg.imgData[outImgIdx].r = filteredData[filteredImgIdx + 2];
				outImg.imgData[outImgIdx].a = 255;

				outImgIdx += outImgWidth;
				outImg.imgData[outImgIdx].b = filteredData[filteredImgIdx + 0];
				outImg.imgData[outImgIdx].g = filteredData[filteredImgIdx + 1];
				outImg.imgData[outImgIdx].r = filteredData[filteredImgIdx + 2];
				outImg.imgData[outImgIdx].a = 255;
			}
		}
	}

	return outImg;
}
