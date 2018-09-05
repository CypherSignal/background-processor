#include "Pch.h"

#include "imageprocess.h"

#include <EASTL/sort.h>
#include <EASTL/utility.h>

using namespace eastl;

void quantizeToSinglePalette(const ProcessImageParams& params, ProcessImageStorage& out);
void quantizeToSinglePaletteWithHdma(const ProcessImageParams& params, ProcessImageStorage& out);

void updateHdmaAndPalette(const PalettizedImage::HdmaTable &hdmaTable, PalettizedImage::PaletteTable &activePalette, unsigned char &hdmaLineCounter, unsigned int &hdmaRowIdx)
{
	if (!hdmaLineCounter && hdmaRowIdx < hdmaTable.size())
	{
		const HdmaRow& activeHdmaRow = hdmaTable[hdmaRowIdx];
		hdmaLineCounter = activeHdmaRow.lineCounter;
		activePalette[activeHdmaRow.cgramAddr] = ((unsigned short)activeHdmaRow.cgramData[1] << 8 | activeHdmaRow.cgramData[0]);

		// terminate the hdma updates if fetched linecounter was 0
		if (hdmaLineCounter)
			++hdmaRowIdx;
		else
			hdmaRowIdx = (unsigned int)hdmaTable.size();
	}
	--hdmaLineCounter;
}

Image getDepalettizedImage(const PalettizedImage& palettizedImg)
{
	Image newImg;
	newImg.width = palettizedImg.width;
	newImg.height = palettizedImg.height;
	newImg.data.resize(newImg.width * newImg.height);
	auto newImgIter = newImg.data.begin();
	auto newImgEnd = newImg.data.end();
	auto palImgIter = palettizedImg.data.begin();

	unsigned int hdmaRowIdx = 0;
	unsigned char hdmaLineCounter = 0;
	const PalettizedImage::HdmaTable& hdmaTable = palettizedImg.hdmaTable;
	PalettizedImage::PaletteTable localPalette = palettizedImg.palette;

	for (unsigned int i = 0; i < newImg.height; ++i)
	{
		updateHdmaAndPalette(hdmaTable, localPalette, hdmaLineCounter, hdmaRowIdx);

		for (unsigned int j = 0; j < newImg.width; ++j, ++newImgIter, ++palImgIter)
		{
			unsigned short snesCol = localPalette[(*palImgIter)];
			Color col;
			col.r = (snesCol & 0x001f) << 3;
			col.g = (snesCol & 0x03e0) >> 2;
			col.b = (snesCol & 0x7c00) >> 7;
			(*newImgIter) = col;
		}
	}

	return newImg;
}

void processImage(const ProcessImageParams& params, ProcessImageStorage& out)
{
	if (params.generateHdmaData)
	{
		quantizeToSinglePaletteWithHdma(params, out);
	}
	else
	{
		quantizeToSinglePalette(params, out);

		//if (params.generateHdmaData)
		//{
		//	out.palettizedImg.hdmaTable.resize(224);
		//	unsigned char i = 0;
		//	for (auto& hdmaRow : out.palettizedImg.hdmaTable)
		//	{
		//		hdmaRow.lineCounter = 0x01;
		//		hdmaRow.cgramAddr = i;
		//		hdmaRow.cgramData[1] = 0;
		//		hdmaRow.cgramData[0] = i;
		//		++i;
		//	}
		//}
	}
}

typedef vector<pair<Color, unsigned int>> IndexedImageData;
typedef IndexedImageData::iterator IndexedImageDataIterator;
struct IndexedImageBucketRange
{
	IndexedImageDataIterator begin;
	IndexedImageDataIterator end;
	Color midColor;
	int delta;
	int channelDelta;

	void setBucketRange(IndexedImageDataIterator _begin, IndexedImageDataIterator _end)
	{
		begin = _begin;
		end = _end;

		// find which channel has most variance
		Color lowestChannels{ 255,255,255 };
		Color highestChannels{ 0,0,0 };
		for (auto pxIter = begin; pxIter != end; ++pxIter)
		{
			lowestChannels.r = min(pxIter->first.r, lowestChannels.r);
			lowestChannels.g = min(pxIter->first.g, lowestChannels.g);
			lowestChannels.b = min(pxIter->first.b, lowestChannels.b);
			highestChannels.r = max(pxIter->first.r, highestChannels.r);
			highestChannels.g = max(pxIter->first.g, highestChannels.g);
			highestChannels.b = max(pxIter->first.b, highestChannels.b);
		}

		float midR = (highestChannels.r + lowestChannels.r) / 2.0f;
		int deltaR = (unsigned int)sqrt(pow(highestChannels.r - lowestChannels.r, 2.0f) * (2.0f + midR / 256.0f));
		int deltaG = (unsigned int)sqrt(pow(highestChannels.g - lowestChannels.g, 2.0f) * 4.0f);
		int deltaB = (unsigned int)sqrt(pow(highestChannels.b - lowestChannels.b, 2.0f) * (2.0f + (255.0f - midR) / 256.0f));
		
		if (deltaR >= deltaG && deltaR >= deltaB)
		{
			delta = deltaR;
			channelDelta = 0;
		}
		else if (deltaG >= deltaR && deltaG >= deltaB)
		{
			delta = deltaG;
			channelDelta = 1;
		}
		else
		{
			delta = deltaB;
			channelDelta = 2;
		}
		midColor.r = (highestChannels.r + lowestChannels.r) / 2;
		midColor.g = (highestChannels.g + lowestChannels.g) / 2;
		midColor.b = (highestChannels.b + lowestChannels.b) / 2;
	}
};

void quantizeToSinglePalette(const ProcessImageParams& params, ProcessImageStorage& out)
{
	// copy the src image data into an array that will let us track how it gets sorted and reordered
	IndexedImageData indexedImageData;
	indexedImageData.reserve(out.srcImg.data.size());
	
	unsigned int idx = 0;
	for (auto px : out.srcImg.data)
	{
		indexedImageData.push_back(make_pair(px, idx));
		++idx;
	}

	vector<IndexedImageBucketRange> bucketRanges;
	auto colorsToFind = min(params.maxColors - 1, 255); // we only support 256 colors, minus 1 for the 0th color
	bucketRanges.reserve(colorsToFind);
	IndexedImageBucketRange& newRange = bucketRanges.push_back();
	newRange.setBucketRange(indexedImageData.begin(), indexedImageData.end());

	// bucket all of the colours by finding which bucket has the greatest delta across each channel,
	// and split the bucket about the median color of each bucket
	// in the end, bucketRanges should have colorsToFind number of buckets, and each should be a unique range
	while (bucketRanges.size() < colorsToFind)
	{
		auto bucketIter = eastl::max_element(bucketRanges.begin(), bucketRanges.end(),
			[](const IndexedImageBucketRange& a, const IndexedImageBucketRange& b) { return a.delta < b.delta; });

		IndexedImageDataIterator medianIter;
		switch (bucketIter->channelDelta)
		{
		case 0:
			// partition around red
			medianIter = partition(bucketIter->begin, bucketIter->end, 
				[medianColor = bucketIter->midColor.r](const pair<Color, unsigned int>& a) 
				{ return a.first.r <= medianColor; });
			break;
		case 1:
			// partition around green
			medianIter = partition(bucketIter->begin, bucketIter->end,
				[medianColor = bucketIter->midColor.g](const pair<Color, unsigned int>& a)
				{ return a.first.g <= medianColor; });
			break;
		case 2:
			// partition around blue
			medianIter = partition(bucketIter->begin, bucketIter->end,
				[medianColor = bucketIter->midColor.b](const pair<Color, unsigned int>& a)
				{ return a.first.b <= medianColor; });
			break;
		};

		// split the bucket about the median, and shift the current bucketrange down correspondingly
		IndexedImageBucketRange& newRange = bucketRanges.push_back();
		newRange.setBucketRange(medianIter, bucketIter->end);
		bucketIter->setBucketRange(bucketIter->begin, medianIter);
	}

	// now that the colors have been bucketed, calculate the avg color of each bucket to determine the image's palette 
	out.palettizedImg.width = out.srcImg.width;
	out.palettizedImg.height = out.srcImg.height;
	out.palettizedImg.data.resize(out.srcImg.data.size());
	out.palettizedImg.palette.clear();
	out.palettizedImg.palette.push_back(0); // add 0 because that's a translucent pixel that should not be used
	for (auto bucket : bucketRanges)
	{
		long accumulatedR = 0;
		long accumulatedG = 0;
		long accumulatedB = 0;

		for (auto pxIter = bucket.begin; pxIter != bucket.end; ++pxIter)
		{
			accumulatedR += pxIter->first.r;
			accumulatedG += pxIter->first.g;
			accumulatedB += pxIter->first.b;
		}

		size_t bucketSize = bucket.end - bucket.begin;
		
		unsigned short snesB = ((unsigned short(accumulatedB / bucketSize) & 0xf8) << 7);
		unsigned short snesG = ((unsigned short(accumulatedG / bucketSize) & 0xf8) << 2);
		unsigned short snesR = ((unsigned short(accumulatedR / bucketSize) & 0xf8) >> 3);
		unsigned short snesColor = snesB | snesG | snesR;
		auto paletteIdx = (unsigned char)(out.palettizedImg.palette.size());
		out.palettizedImg.palette.push_back(snesColor);
		for (auto pxIter = bucket.begin; pxIter != bucket.end; ++pxIter)
		{
			out.palettizedImg.data[pxIter->second] = paletteIdx;
		}
	}
}

void quantizeToSinglePaletteWithHdma(const ProcessImageParams& params, ProcessImageStorage& out)
{

}