#include "Pch.h"

#include "imageNtscFilter.h"

#include <Base/Main/imageProcess.h>
#include <EASTL/vector.h>
#include <External/blargg_ntsc/snes_ntsc.h>

#include <External/blargg_ntsc/snes_ntsc.c>

struct SnesNtscObject
{
	SnesNtscObject()
	{
		m_ntscConfig = new snes_ntsc_t;
		
		m_configTask.run([ntscConfig = this->m_ntscConfig]()
		{
			snes_ntsc_setup_t setup = snes_ntsc_svideo;
			snes_ntsc_init(ntscConfig, &setup);
		});
	}

	snes_ntsc_t const* getNtscConfig()
	{
		m_configTask.wait();
		return m_ntscConfig;
	}
private:
	Concurrency::task_group m_configTask;
	snes_ntsc_t* m_ntscConfig;
};

static SnesNtscObject s_snesNtscObj;

Image applyNtscFilter(const PalettizedImage& palettizedImg)
{
	// prep the data for input into the ntsc filter
	auto width = palettizedImg.width;
	auto height = palettizedImg.height;

	eastl::vector<unsigned short> snesImgData = getDepalettizedSnesImage(palettizedImg);

	// run the filter
	eastl::vector<char> filteredData;
	unsigned int outImgWidth = SNES_NTSC_OUT_WIDTH(width);
	filteredData.resize(outImgWidth * height * 2 * 4); // times 2 because it's doubling height; times 4 because output is 4Bpp
	snes_ntsc_blit(s_snesNtscObj.getNtscConfig(), snesImgData.data(), width, 0, width, height, filteredData.data(), outImgWidth * 4);

	// scale up data and prepare it for usage as an image
	Image outImg;
	outImg.width = outImgWidth;
	outImg.height = height * 2;
	outImg.data.resize(outImgWidth * outImg.height * 2);
	{
		for (unsigned int row = 0; row < height; ++row)
		{
			for (unsigned int col = 0; col < outImgWidth; ++col)
			{
				unsigned int outImgIdx = row * outImgWidth * 2 + col;
				unsigned int filteredImgIdx = (row * outImgWidth + col) * 4;

				outImg.data[outImgIdx].b = filteredData[filteredImgIdx + 0];
				outImg.data[outImgIdx].g = filteredData[filteredImgIdx + 1];
				outImg.data[outImgIdx].r = filteredData[filteredImgIdx + 2];

				outImgIdx += outImgWidth;
				outImg.data[outImgIdx].b = filteredData[filteredImgIdx + 0];
				outImg.data[outImgIdx].g = filteredData[filteredImgIdx + 1];
				outImg.data[outImgIdx].r = filteredData[filteredImgIdx + 2];
			}
		}
	}

	return outImg;
}
