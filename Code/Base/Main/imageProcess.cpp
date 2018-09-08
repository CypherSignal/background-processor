#include "Pch.h"

#include "imageprocess.h"

#include <EASTL/sort.h>
#include <EASTL/utility.h>
#include <EASTL/array.h>

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
	}
}

typedef vector<pair<Color, unsigned int>> IndexedImageData;
typedef IndexedImageData::iterator IndexedImageDataIterator;
struct IndexedImageBucketRange
{
	unsigned char scanlineFirst, scanlineLast, scanlineGapSize, scanlineGapEnd;
	IndexedImageDataIterator begin;
	IndexedImageDataIterator end;
	int deltaColor;
	int channelDelta;
	Color midColor;

	void setBucketRange(IndexedImageDataIterator _begin, IndexedImageDataIterator _end, unsigned int width)
	{
		begin = _begin;
		end = _end;

		// find which channel has most variance, and mark which scanlines contain a pixel in this bucket
		eastl::array<bool, MaxHeight> pxOnScanline;
		pxOnScanline.fill(false);
		{
			Color lowestChannels{ 255,255,255 };
			Color highestChannels{ 0,0,0 };
			for (auto pxIter = _begin; pxIter != _end; ++pxIter)
			{
				lowestChannels.r = min(pxIter->first.r, lowestChannels.r);
				lowestChannels.g = min(pxIter->first.g, lowestChannels.g);
				lowestChannels.b = min(pxIter->first.b, lowestChannels.b);
				highestChannels.r = max(pxIter->first.r, highestChannels.r);
				highestChannels.g = max(pxIter->first.g, highestChannels.g);
				highestChannels.b = max(pxIter->first.b, highestChannels.b);
				pxOnScanline[pxIter->second / width] = true;
			}

			float midR = (highestChannels.r + lowestChannels.r) / 2.0f;
			int deltaR = (unsigned int)sqrt(pow(highestChannels.r - lowestChannels.r, 2.0f) * (2.0f + midR / 256.0f));
			int deltaG = (unsigned int)sqrt(pow(highestChannels.g - lowestChannels.g, 2.0f) * 4.0f);
			int deltaB = (unsigned int)sqrt(pow(highestChannels.b - lowestChannels.b, 2.0f) * (2.0f + (255.0f - midR) / 256.0f));

			if (deltaR >= deltaG && deltaR >= deltaB)
			{
				deltaColor = deltaR;
				channelDelta = 0;
			}
			else if (deltaG >= deltaR && deltaG >= deltaB)
			{
				deltaColor = deltaG;
				channelDelta = 1;
			}
			else
			{
				deltaColor = deltaB;
				channelDelta = 2;
			}
			midColor.r = (highestChannels.r + lowestChannels.r) / 2;
			midColor.g = (highestChannels.g + lowestChannels.g) / 2;
			midColor.b = (highestChannels.b + lowestChannels.b) / 2;
		}

		// determine the first and last scanline a pixel in this bucket appears in, and what the widest gap 
		// between scanlines is
		auto scanlineIter = eastl::find(pxOnScanline.begin(), pxOnScanline.end(), true);
		auto scanlineLastIter = eastl::find(pxOnScanline.rbegin(), pxOnScanline.rend(), true).base();
		scanlineFirst = (unsigned char)(scanlineIter - pxOnScanline.begin());
		scanlineLast = (unsigned char)(scanlineLastIter - pxOnScanline.begin());

		unsigned char largestRunningGap = 0;
		unsigned char largestRunningGapEnd = 0;
		unsigned char runningGap = 0;
		for (unsigned char i = scanlineFirst; i != scanlineLast; ++i)
		{
			if (pxOnScanline[i])
			{
				if (runningGap > largestRunningGap)
				{
					largestRunningGap = runningGap;
					largestRunningGapEnd = i;
				}
				runningGap = 0;
			}
			else
			{
				++runningGap;
			}
		}

		scanlineGapSize = largestRunningGap;
		scanlineGapEnd = largestRunningGapEnd;
	}

	unsigned short getAverageColor()
	{
		long accumulatedR = 0;
		long accumulatedG = 0;
		long accumulatedB = 0;
		for (auto pxIter = begin; pxIter != end; ++pxIter)
		{
			accumulatedR += pxIter->first.r;
			accumulatedG += pxIter->first.g;
			accumulatedB += pxIter->first.b;
		}

		size_t bucketSize = distance(begin, end);
		unsigned short snesB = ((unsigned short(accumulatedB / bucketSize) & 0xf8) << 7);
		unsigned short snesG = ((unsigned short(accumulatedG / bucketSize) & 0xf8) << 2);
		unsigned short snesR = ((unsigned short(accumulatedR / bucketSize) & 0xf8) >> 3);
		return (snesB | snesG | snesR);
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
	newRange.setBucketRange(indexedImageData.begin(), indexedImageData.end(), out.srcImg.width);

	// bucket all of the colours by finding which bucket has the greatest delta across each channel,
	// and split the bucket about the median color of each bucket
	// in the end, bucketRanges should have colorsToFind number of buckets, and each should be a unique range
	while (bucketRanges.size() < colorsToFind)
	{
		auto bucketIter = eastl::max_element(bucketRanges.begin(), bucketRanges.end(),
			[](const IndexedImageBucketRange& a, const IndexedImageBucketRange& b) { return a.deltaColor < b.deltaColor; });

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
		newRange.setBucketRange(medianIter, bucketIter->end, out.srcImg.width);
		bucketIter->setBucketRange(bucketIter->begin, medianIter, out.srcImg.width);
	}

	// now that the colors have been bucketed, calculate the avg color of each bucket to determine the image's palette 
	out.palettizedImg.width = out.srcImg.width;
	out.palettizedImg.height = out.srcImg.height;
	out.palettizedImg.data.resize(out.srcImg.data.size());
	out.palettizedImg.palette.clear();
	out.palettizedImg.palette.push_back(0); // add 0 because that's a translucent pixel that should not be used
	for (auto bucket : bucketRanges)
	{
		auto paletteIdx = (unsigned char)(out.palettizedImg.palette.size());
		out.palettizedImg.palette.push_back(bucket.getAverageColor());
		eastl::for_each(bucket.begin, bucket.end, [&paletteIdx, &out](const pair<Color, unsigned int>& px)
		{
			out.palettizedImg.data[px.second] = paletteIdx;
		});
	}
}

void quantizeToSinglePaletteWithHdma(const ProcessImageParams& params, ProcessImageStorage& out)
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

	fixed_vector<IndexedImageBucketRange, 256 + MaxHeight, false> bucketRanges; // max possible buckets is 256 colors + 224 scanlines of hdma data
	auto colorsToFind = min(params.maxColors - 1, 255); // we only support 256 colors, minus 1 for the 0th color
	IndexedImageBucketRange& newRange = bucketRanges.push_back();
	newRange.setBucketRange(indexedImageData.begin(), indexedImageData.end(), out.srcImg.width);

	fixed_vector<unsigned int, 256 + MaxHeight, false> baseBucketRangeIndices;
	// first element is what bucket got evicted; second element is what bucket is populating the eviction
	fixed_vector<pair<unsigned int, unsigned int>, MaxHeight-1, false> hdmaPopulationList; 
	
	while (true)
	{
		// reset the list of baseBucketRangeIndices
		fixed_vector<unsigned int, 256 + MaxHeight, false> unusedBucketRangeIndices(bucketRanges.size());
		baseBucketRangeIndices.resize(bucketRanges.size());
		if (bucketRanges.size() >= colorsToFind)
		{
			eastl::generate(baseBucketRangeIndices.begin(), baseBucketRangeIndices.end(), [n = 0]()mutable{return n++; });
			eastl::generate(unusedBucketRangeIndices.begin(), unusedBucketRangeIndices.end(), [n = 0]()mutable{return n++; });

			// with current set of bucketRanges, generate current hdma table
			// generate a list of possible buckets to be evicted, and a list of buckets to replace those evictions
			eastl::sort(baseBucketRangeIndices.begin(), baseBucketRangeIndices.end(),
				[&bucketRanges](unsigned int a, unsigned int b) { return bucketRanges[a].scanlineLast < bucketRanges[b].scanlineLast; });

			hdmaPopulationList.clear();

			unsigned char minScanline = 0;
			// advance through the list of bucket ranges, finding candidates that can evict a color
			for (auto evictionIter = baseBucketRangeIndices.begin(); evictionIter != baseBucketRangeIndices.end(); ++evictionIter)
			{
				minScanline = max(bucketRanges[*evictionIter].scanlineLast, minScanline);
				
				// find the element with the _lowest_ first scanline that is >= our minimum scanline target
				auto populationIter = eastl::min_element(
					eastl::find_if(evictionIter, baseBucketRangeIndices.end(),
						[minScanline, &bucketRanges] (unsigned int a) { return bucketRanges[a].scanlineFirst > minScanline; }),
					baseBucketRangeIndices.end(),
					[minScanline, &bucketRanges] (unsigned int a, unsigned int b)
					{ return bucketRanges[a].scanlineFirst > minScanline && bucketRanges[a].scanlineFirst < bucketRanges[b].scanlineFirst; });

				// if we could not find a population candidate, then we're done
				if (populationIter == baseBucketRangeIndices.end())
					break;

				// remove the population-candidate from baseBucketList - this is now a bucket that will be HDMA'd into the palette.
				// (note that populationIter is guaranteed to be after evictionIter so that iterator is not invalidated)
				// TODO: this system will preclude the ability for 1 palette entry to have 2 entries dma'd into it over time.
				//		there may be a slightly higher quality way of handling this where searching for PopulationIters _also_ checks
				//		for entries we've already marked (but we STILL want to iterate over them as _eviction candidates_)
				auto evictionBucketIndex = *evictionIter;
				auto populationBucketIndex = *populationIter;
				baseBucketRangeIndices.erase(populationIter);

				auto unusedEvictionIter = eastl::find(unusedBucketRangeIndices.begin(), unusedBucketRangeIndices.end(), evictionBucketIndex);
				if (unusedEvictionIter != unusedBucketRangeIndices.end())
					unusedBucketRangeIndices.erase_unsorted(unusedEvictionIter);

				auto unusedPopulationIter = eastl::find(unusedBucketRangeIndices.begin(), unusedBucketRangeIndices.end(), populationBucketIndex);
				if (unusedPopulationIter != unusedBucketRangeIndices.end())
					unusedBucketRangeIndices.erase_unsorted(unusedPopulationIter);

				// we can conclude that we have enough HDMA candidates that we can continue to split based on colours,
				// so skip out of the current process and let the loop continue
				if (baseBucketRangeIndices.size() < colorsToFind)
					break;

 				hdmaPopulationList.push_back(make_pair(evictionBucketIndex, populationBucketIndex));
				++minScanline;
			}
		}

		// we can conclude that we have enough HDMA candidates that we can continue to split based on colours, so do so
		if (baseBucketRangeIndices.size() < colorsToFind)
		{
			// first bucket all of the colours by finding which bucket has the greatest delta across each channel,
			// and split the bucket about the median color of each bucket
			auto bucketIter = eastl::max_element(bucketRanges.begin(), bucketRanges.end(),
				[](const IndexedImageBucketRange& a, const IndexedImageBucketRange& b) { return a.deltaColor < b.deltaColor; });

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
			newRange.setBucketRange(medianIter, bucketIter->end, out.srcImg.width);
			bucketIter->setBucketRange(bucketIter->begin, medianIter, out.srcImg.width);
		}
		// if we can still fill up the hdma list, split on scanline gap
		else if (hdmaPopulationList.size() < hdmaPopulationList.capacity())
		{
			//	find a bucket that WOULD contribute to hdma table and partition about that
			//	"Would contribute" means that the "scanlineGapEnd" matches with something else from the baseBucketRange,
			//			while still not colliding with other hdmaBucketCandidates
			eastl::array<bool, MaxHeight> hdmaScanlineInUse;
			hdmaScanlineInUse.fill(false);
			for (auto hdmaPopulation : hdmaPopulationList)
			{
				unsigned char scanlineToMark = bucketRanges[hdmaPopulation.first].scanlineLast;
				while (scanlineToMark < MaxHeight && hdmaScanlineInUse[scanlineToMark])
					++scanlineToMark;
				if (scanlineToMark < MaxHeight)
					hdmaScanlineInUse[scanlineToMark] = true;
				else
					scanlineToMark = scanlineToMark;
			}

			auto bucketIter = eastl::find_if(unusedBucketRangeIndices.begin(), unusedBucketRangeIndices.end(),
				[&bucketRanges, &unusedBucketRangeIndices, &hdmaScanlineInUse]
				(unsigned int a)
				{
					// find what scanline this bucket could be loaded in on, first
					auto maxScanline = bucketRanges[a].scanlineGapEnd;
					auto minScanline = maxScanline - bucketRanges[a].scanlineGapSize;

					// advance minScanline forward one-by-one for as long as there's an entry in the hdmaPopulationList 
					// that refers to a bucketRange with a matching scanlineLast, so that minScanline ends up 
					// referring to the first possible scanline for an eviction to occur on
					while (hdmaScanlineInUse[minScanline])
						++minScanline;

					if (minScanline >= maxScanline)
						return false;

					auto evictionCandidate = eastl::find_if(unusedBucketRangeIndices.begin(), unusedBucketRangeIndices.end(),
						[minScanline, &bucketRanges](unsigned int a)
						{ return bucketRanges[a].scanlineLast < minScanline; } );

					return evictionCandidate != unusedBucketRangeIndices.end();
				});
	
			// if a bucket could not positively contribute, then break
			if (bucketIter == unusedBucketRangeIndices.end())
				break;
			
			// partition bucket about scanline and continue
			IndexedImageBucketRange& bucketToSplit = bucketRanges[*bucketIter];
			auto medianIter = partition(bucketToSplit.begin, bucketToSplit.end,
				[scanlineSplit = bucketToSplit.scanlineGapEnd, width = out.srcImg.width](const pair<Color, unsigned int>& a)
				{ return a.second / width < scanlineSplit; });
			IndexedImageBucketRange& newRange = bucketRanges.push_back();
			newRange.setBucketRange(medianIter, bucketToSplit.end, out.srcImg.width);
			bucketToSplit.setBucketRange(bucketToSplit.begin, medianIter, out.srcImg.width);
		}
		else
		{
			// baseBucketList and hdmaPopulationList are both maxed out - exit the loop
			break;
		}
	}

	// now that the colors have been bucketed, calculate the avg color of each bucket to determine the image's palette 
	out.palettizedImg.width = out.srcImg.width;
	out.palettizedImg.height = out.srcImg.height;
	out.palettizedImg.data.resize(out.srcImg.data.size());
	out.palettizedImg.palette.clear();
	out.palettizedImg.palette.push_back(0); // add 0 because that's a translucent pixel that should not be used

	// fill out the bucketRanges from the baseBucketRangeIndices first, to fill out the palettizedImg's base palette,
	for (auto baseBucketRangeIndex : baseBucketRangeIndices)
	{
		auto bucket = bucketRanges[baseBucketRangeIndex];
		auto paletteIdx = (unsigned char)(out.palettizedImg.palette.size());
		out.palettizedImg.palette.push_back( bucket.getAverageColor());
		eastl::for_each(bucket.begin, bucket.end, [&paletteIdx, &out](const pair<Color, unsigned int>& px)
		{
			out.palettizedImg.data[px.second] = paletteIdx;
		});
	}

	// next, go through the HDMA population list, to do two things:
	// 1) figure out the coloration of the bucket that is being used for the new color
	// 2) map the bucket being evicted back to an index in the palette
	struct HdmaAction
	{
		unsigned char scanline;
		unsigned char scanlineRequired;
		unsigned char paletteIdx;
		unsigned short snesColor;
	};
	fixed_vector<HdmaAction, MaxHeight, false> hdmaActions;
	unsigned char previousScanline = 0;
	for (auto hdmaPopulation : hdmaPopulationList)
	{
		auto bucket = bucketRanges[hdmaPopulation.second];
		auto baseBucketRangeIter = eastl::find(baseBucketRangeIndices.begin(), baseBucketRangeIndices.end(), hdmaPopulation.first);
		unsigned char paletteIdx = (unsigned char)(baseBucketRangeIter - baseBucketRangeIndices.begin() + 1);

		HdmaAction hdmaAction;
		hdmaAction.scanline = max(bucketRanges[hdmaPopulation.first].scanlineLast, (unsigned char)(previousScanline+1));
		hdmaAction.scanlineRequired = bucketRanges[hdmaPopulation.second].scanlineFirst;
		hdmaAction.paletteIdx = paletteIdx;
		hdmaAction.snesColor = bucket.getAverageColor();
		hdmaActions.push_back(hdmaAction);
		previousScanline = hdmaAction.scanline;

		eastl::for_each(bucket.begin, bucket.end, [&paletteIdx, &out](const pair<Color, unsigned int>& px)
		{
			out.palettizedImg.data[px.second] = paletteIdx;
		});
	}

	// add a fake hdma action to close off the table
	unsigned int actualHdmaActions = (unsigned int)hdmaActions.size();
	hdmaActions.push_back({ 224, 0,0 });
	// we need a base-line entry because we don't start the hdma on scanline 0
	out.palettizedImg.hdmaTable.push_back({ hdmaActions[0].scanline, 0, 0, {0,0} });
	for (unsigned int i = 0; i < actualHdmaActions; ++i)
	{
		const HdmaAction& hdmaAction = hdmaActions[i];
		HdmaRow hdmaRow;
		hdmaRow.lineCounter = hdmaActions[i+1].scanline - hdmaAction.scanline;
		hdmaRow.cgramAddr = hdmaAction.paletteIdx;
		hdmaRow.cgramData[1] = (unsigned char)((hdmaAction.snesColor & 0x7f00) >> 8);
		hdmaRow.cgramData[0] = (unsigned char)((hdmaAction.snesColor & 0x00ff) >> 0);
		out.palettizedImg.hdmaTable.push_back(hdmaRow);
	}
}