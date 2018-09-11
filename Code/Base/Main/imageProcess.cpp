#include "Pch.h"

#include "imageprocess.h"
#include "imageProcessIspc_ispc.h"

#include <EASTL/sort.h>
#include <EASTL/utility.h>
#include <EASTL/array.h>
#include <EASTL/bonus/tuple_vector.h>

using namespace eastl;

void quantizeToSinglePalette(const ProcessImageParams& params, ProcessImageStorage& out);
void quantizeToSinglePaletteWithHdma(const ProcessImageParams& params, ProcessImageStorage& out);

void updateHdmaAndPalette(const PalettizedImage::HdmaTable &hdmaTable, PalettizedImage::PaletteTable &activePalette, unsigned char &hdmaLineCounter, unsigned int &hdmaRowIdx)
{
	if (!hdmaLineCounter && hdmaRowIdx < hdmaTable.size())
	{
		const HdmaRow& activeHdmaRow = hdmaTable[hdmaRowIdx];
		hdmaLineCounter = activeHdmaRow.lineCount;
		activePalette[activeHdmaRow.paletteIdx] = activeHdmaRow.snesColor;

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
	auto palImgIter = palettizedImg.data.begin();
	auto newImgIter = newImg.data.begin();

	unsigned int hdmaRowIdx = 0;
	eastl::fixed_vector<unsigned char, 8, false> hdmaLineCounters(palettizedImg.hdmaTables.size(), 0);
	PalettizedImage::PaletteTable localPalette = palettizedImg.palette;
	for (unsigned int i = 0; i < newImg.height; ++i)
	{
		for (unsigned int i = 0; i < palettizedImg.hdmaTables.size(); ++i)
		{
			updateHdmaAndPalette(palettizedImg.hdmaTables[i], localPalette, hdmaLineCounters[i], hdmaRowIdx);
		}

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

eastl::vector<unsigned short> getDepalettizedSnesImage(const PalettizedImage& palettizedImg)
{
	eastl::vector<unsigned short> snesImgData;
	unsigned int width = palettizedImg.width;
	unsigned int height = palettizedImg.height;
	snesImgData.resize(palettizedImg.data.size());
	auto palImgIter = palettizedImg.data.begin();
	auto snesImgIter = snesImgData.begin();

	unsigned int hdmaRowIdx = 0;
	eastl::fixed_vector<unsigned char, 8, false> hdmaLineCounters(palettizedImg.hdmaTables.size(), 0);
	PalettizedImage::PaletteTable localPalette = palettizedImg.palette;
	for (unsigned int i = 0; i < height; ++i)
	{
		for (unsigned int i = 0; i < palettizedImg.hdmaTables.size(); ++i)
		{
			updateHdmaAndPalette(palettizedImg.hdmaTables[i], localPalette, hdmaLineCounters[i], hdmaRowIdx);
		}

		for (unsigned int j = 0; j < width; ++j, ++snesImgIter, ++palImgIter)
		{
			(*snesImgIter) = localPalette[(*palImgIter)];
		}
	}

	return snesImgData;
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

typedef tuple_vector<unsigned char, unsigned char, unsigned char, unsigned int> IndexedImageData;
typedef IndexedImageData::iterator IndexedImageDataIterator;
struct IndexedImageBucketRange
{
	unsigned char scanlineFirst, scanlineLast, scanlineGapSize, scanlineGapEnd;
	IndexedImageDataIterator begin;
	IndexedImageDataIterator end;
	int deltaColor;
	int channelDelta;
	unsigned char midColor;

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

			int pxCount = (int)distance(_begin, _end);
			ispc::minmaxUint8(&get<0>(*_begin), pxCount, lowestChannels.r, highestChannels.r);
			ispc::minmaxUint8(&get<1>(*_begin), pxCount, lowestChannels.g, highestChannels.g);
			ispc::minmaxUint8(&get<2>(*_begin), pxCount, lowestChannels.b, highestChannels.b);
			ispc::markScanlines(&get<unsigned int&>(*_begin), pxCount, (int8_t*)pxOnScanline.data(), width, scanlineFirst, scanlineLast);

			float midR = (highestChannels.r + lowestChannels.r) / 2.0f;
			int deltaR = (unsigned int)sqrt(pow(highestChannels.r - lowestChannels.r, 2.0f) * (2.0f + midR / 256.0f));
			int deltaG = (unsigned int)sqrt(pow(highestChannels.g - lowestChannels.g, 2.0f) * 4.0f);
			int deltaB = (unsigned int)sqrt(pow(highestChannels.b - lowestChannels.b, 2.0f) * (2.0f + (255.0f - midR) / 256.0f));

			if (deltaR >= deltaG && deltaR >= deltaB)
			{
				midColor = (highestChannels.r + lowestChannels.r) / 2;
				deltaColor = deltaR;
				channelDelta = 0;
			}
			else if (deltaG >= deltaR && deltaG >= deltaB)
			{
				midColor = (highestChannels.g + lowestChannels.g) / 2;
				deltaColor = deltaG;
				channelDelta = 1;
			}
			else
			{
				midColor = (highestChannels.b + lowestChannels.b) / 2;
				deltaColor = deltaB;
				channelDelta = 2;
			}
		}

		{
			// determine what the widest gap between the scanlines is
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
	}

	unsigned short getAverageColor()
	{
		long accumulatedR = 0;
		long accumulatedG = 0;
		long accumulatedB = 0;
		for (auto pxIter = begin; pxIter != end; ++pxIter)
		{
			accumulatedR += get<0>(*pxIter);
			accumulatedG += get<1>(*pxIter);
			accumulatedB += get<2>(*pxIter);
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
		indexedImageData.push_back(px.r, px.g, px.b, idx);
		++idx;
	}

	vector<IndexedImageBucketRange> bucketRanges;
	auto colorsToFind = min(params.maxColors - 1, 255); // we only support 256 colors, minus 1 for the 0th color
	bucketRanges.reserve(colorsToFind);
	IndexedImageBucketRange& newRange = bucketRanges.push_back();
	newRange.setBucketRange(indexedImageData.begin(), indexedImageData.end(), out.srcImg.width);

	// bucket all of the colors by finding which bucket has the greatest delta across each channel,
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
				[medianColor = bucketIter->midColor](IndexedImageData::const_reference_tuple a) 
				{ return get<0>(a) <= medianColor; });
			break;
		case 1:
			// partition around green
			medianIter = partition(bucketIter->begin, bucketIter->end,
				[medianColor = bucketIter->midColor](IndexedImageData::const_reference_tuple a)
				{ return  get<1>(a) <= medianColor; });
			break;
		case 2:
			// partition around blue
			medianIter = partition(bucketIter->begin, bucketIter->end,
				[medianColor = bucketIter->midColor](IndexedImageData::const_reference_tuple a)
				{ return  get<2>(a) <= medianColor; });
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
		eastl::for_each(bucket.begin, bucket.end, [&paletteIdx, &out](IndexedImageData::const_reference_tuple px)
		{
			out.palettizedImg.data[get<const unsigned int&>(px)] = paletteIdx;
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
		indexedImageData.push_back(px.r, px.g, px.b, idx);
		++idx;
	}

	const int MaxColors = 255;
	fixed_vector<IndexedImageBucketRange, MaxColors + MaxHeight, false> bucketRanges; // max possible buckets is 255 colors + 224 scanlines of hdma data
	auto colorsToFind = min(params.maxColors, MaxColors)-1; // we only support 256 colors, minus 1 for the 0th color
	IndexedImageBucketRange& newRange = bucketRanges.push_back();
	newRange.setBucketRange(indexedImageData.begin(), indexedImageData.end(), out.srcImg.width);

	fixed_vector<unsigned int, MaxColors + MaxHeight, false> baseBucketRangeIndices;
	fixed_vector<unsigned int, MaxColors + MaxHeight, false> availableHdmaIndices;
	fixed_vector<unsigned int, MaxColors, false> paletteBucketRangeIndices;
	fixed_vector<unsigned int, MaxHeight, false> hdmaBucketRangeIndices;
	
	// first element is what bucket got evicted; second element is what bucket is populating the eviction
	fixed_vector<pair<unsigned int, unsigned int>, MaxHeight-1, false> hdmaPopulationList; 
	
	while (true)
	{
		// reset the list of baseBucketRangeIndices and hdmaBucketRangeIndices
		baseBucketRangeIndices.resize(bucketRanges.size());
		paletteBucketRangeIndices.clear();
		hdmaBucketRangeIndices.clear();
		hdmaPopulationList.clear();
		if (bucketRanges.size() >= colorsToFind)
		{
			// with current set of bucketRanges, generate current hdma table
			// generate a list of buckets to be used for the base palette, and a list of buckets to be used as replacements via hdma

			eastl::generate(baseBucketRangeIndices.begin(), baseBucketRangeIndices.end(), [n = 0]()mutable{return n++; });
			eastl::sort(baseBucketRangeIndices.begin(), baseBucketRangeIndices.end(),
				[&bucketRanges](unsigned int a, unsigned int b) { return bucketRanges[a].scanlineFirst > bucketRanges[b].scanlineFirst; });
			availableHdmaIndices = baseBucketRangeIndices;
			// advance through the list of bucket ranges, finding candidates that can evict a color
			unsigned char minScanline = MaxHeight;
			for (auto bucketIndex : baseBucketRangeIndices)
			{
				minScanline = min(bucketRanges[bucketIndex].scanlineFirst, minScanline);
				
				// find the bucket range with the greatest last-scanline that is < our minimum scanline target
				auto hdmaBucketRangeIter = eastl::max_element(
					eastl::find_if(availableHdmaIndices.begin(), availableHdmaIndices.end(),
						[minScanline, &bucketRanges] (unsigned int a) { return bucketRanges[a].scanlineLast < minScanline; }),
					availableHdmaIndices.end(),
					[minScanline, &bucketRanges] (unsigned int a, unsigned int b)
						{ return  bucketRanges[b].scanlineLast < minScanline && bucketRanges[b].scanlineLast > bucketRanges[a].scanlineLast; });

				// if we could not find a candidate that we could replace, then we're a palette-bucket
				if (hdmaBucketRangeIter == availableHdmaIndices.end())
				{
					paletteBucketRangeIndices.push_back(bucketIndex);
				}
				else // if we could find a candidate that we could replace, then we're an hdma-bucket
				{
					// remove the population-candidate from baseBucketList - this is now a bucket that will be HDMA'd into the palette.
					// (note that hdmaBucketIndex is guaranteed to be after evictionIter, so that iterator is not invalidated)
					auto hdmaBucketIndex = *hdmaBucketRangeIter;// (unsigned int)distance(availableHdmaIndices.begin(), hdmaBucketRangeIter);
					hdmaBucketRangeIndices.push_back(bucketIndex);
					hdmaPopulationList.push_back(make_pair(hdmaBucketIndex, bucketIndex));
					availableHdmaIndices.erase(eastl::remove(availableHdmaIndices.begin(), availableHdmaIndices.end(), hdmaBucketIndex), availableHdmaIndices.end());
				}
				if (minScanline > 0)
				{
					--minScanline;
				}
			}
		}

		// we can still split on colors
		if (paletteBucketRangeIndices.size() < colorsToFind)
		{
			// first bucket all of the colors by finding which bucket has the greatest delta across each channel,
			// and split the bucket about the median color of each bucket
			auto bucketIter = eastl::max_element(bucketRanges.begin(), bucketRanges.end(),
				[](const IndexedImageBucketRange& a, const IndexedImageBucketRange& b) { return a.deltaColor < b.deltaColor; });

			IndexedImageDataIterator medianIter;
			switch (bucketIter->channelDelta)
			{
			case 0:
				// partition around red
				medianIter = partition(bucketIter->begin, bucketIter->end,
					[medianColor = bucketIter->midColor](IndexedImageData::const_reference_tuple a)
				{ return get<0>(a) <= medianColor; });
				break;
			case 1:
				// partition around green
				medianIter = partition(bucketIter->begin, bucketIter->end,
					[medianColor = bucketIter->midColor](IndexedImageData::const_reference_tuple a)
				{ return get<1>(a) <= medianColor; });
				break;
			case 2:
				// partition around blue
				medianIter = partition(bucketIter->begin, bucketIter->end,
					[medianColor = bucketIter->midColor](IndexedImageData::const_reference_tuple a)
				{ return get<2>(a) <= medianColor; });
				break;
			};

			// split the bucket about the median, and shift the current bucketrange down correspondingly
			IndexedImageBucketRange& newRange = bucketRanges.push_back();
			newRange.setBucketRange(medianIter, bucketIter->end, out.srcImg.width);
			bucketIter->setBucketRange(bucketIter->begin, medianIter, out.srcImg.width);
		}
		// if we can still fill up the hdma list, split on scanline gap
		else if (hdmaBucketRangeIndices.size() < hdmaBucketRangeIndices.capacity())
		{
			// find a bucket that WOULD contribute to hdma table and partition about that
			
			// first, fill up an array tracking what scanlines are theoretically occupied
			// (fill it up by going backwards through the hdmaBucketRangeIndices,
			// and marking what scanline it could be loaded on)
			// and fill up two arrays that know where, for each element, the next (or previous)
			// available scanline slot is at
			eastl::array<unsigned char, MaxHeight> nextAvailableHdmaScanline;
			eastl::array<unsigned char, MaxHeight> prevAvailableHdmaScanline;

			{
				eastl::array<bool, MaxHeight> hdmaScanlineInUse;
				hdmaScanlineInUse.fill(false);
				auto hdmaBucketRangeIndexEnd = hdmaBucketRangeIndices.rend();
				for (auto hdmaBucketIndexIter = hdmaBucketRangeIndices.rbegin(); hdmaBucketIndexIter != hdmaBucketRangeIndexEnd; ++hdmaBucketIndexIter)
				{
					unsigned char scanlineToMark = bucketRanges[*hdmaBucketIndexIter].scanlineFirst;
					while (scanlineToMark > 0 && hdmaScanlineInUse[scanlineToMark])
						--scanlineToMark;
					if (scanlineToMark > 0)
						hdmaScanlineInUse[scanlineToMark] = true;
					else // this should never be hit!
						scanlineToMark = scanlineToMark;
				}

				unsigned char lastAvailableScanline = 0;
				for (int i = 0; i < hdmaScanlineInUse.size(); ++i)
				{
					if (!hdmaScanlineInUse[i])
					{
						lastAvailableScanline = (unsigned char)i;
					}
					prevAvailableHdmaScanline[i] = lastAvailableScanline;
				}
				lastAvailableScanline = MaxHeight;
				for (int i = (int)(hdmaScanlineInUse.size() - 1); i >= 0; --i)
				{
					if (!hdmaScanlineInUse[i])
					{
						lastAvailableScanline = (unsigned char)i;
					}
					nextAvailableHdmaScanline[i] = lastAvailableScanline;
				}
			}

			auto bucketIter = eastl::find_if(bucketRanges.begin(), bucketRanges.end(),
				[&bucketRanges, &nextAvailableHdmaScanline, &prevAvailableHdmaScanline]
				(const IndexedImageBucketRange& bucket)
				{
					// find what scanline this bucket could be loaded in on, first
					auto scanlineGapEnd = bucket.scanlineGapEnd;
					auto scanlineGapStart = nextAvailableHdmaScanline[scanlineGapEnd - bucket.scanlineGapSize];

					if (scanlineGapStart >= scanlineGapEnd)
						return false;
					
					// find a bucket that we could split against
					auto evictionCandidate = eastl::find_if(bucketRanges.begin(), bucketRanges.end(),
						[scanlineGapStart, scanlineGapEnd, &nextAvailableHdmaScanline, &prevAvailableHdmaScanline](const IndexedImageBucketRange& hdmaCandidate)
						{ 
							// we already would have been evicting/populating between this pair
							//if (hdmaCandidate.scanlineFirst > bucket.scanlineLast || hdmaCandidate.scanlineLast < bucket.scanlineFirst)
							//	return false;

							// check if hdmaCandidate's final scanline is before our first possible scanline
							// this only takes into account bucket pairs that are like so:
							// -|||-------|||----- <- Bucket - split this along the scanline
							// ------|||---------- <- hdmaCandidate?
							// on next iteration of building hdma table, we should be able to unload hdmaCandidate,
							// (or maybe some other bucket will prove to be viable - but we know that ONE is)
							// and load in the lower-split of bucket
							if (nextAvailableHdmaScanline[hdmaCandidate.scanlineLast] >= scanlineGapEnd)
								return false;

							if (prevAvailableHdmaScanline[hdmaCandidate.scanlineFirst] <= scanlineGapStart)
								return false;

							// check if hdmaCandidate's first non-gap sequence (that we can see) is within 
							// bucket's gap. this takes into account bucket pairs like so:
							// -|||-------|||----- <- Bucket 
							// ------|||------|||- <- hdmaCandidate?
							// on next iteration of building hdma table, we probably won't match a candidate,
							// but we probably will have things set up so that maybe hdmaCandidate will split
							// about the split Bucket when we're looking for buckets to split
							if (nextAvailableHdmaScanline[hdmaCandidate.scanlineGapEnd - hdmaCandidate.scanlineGapSize] >= scanlineGapEnd)
								return false;

							if (prevAvailableHdmaScanline[hdmaCandidate.scanlineFirst] <= scanlineGapStart)
								return false;

							return true;
						});
					
					return evictionCandidate != bucketRanges.end();
			});
	
			// if a bucket could not positively contribute, then break
			if (bucketIter == bucketRanges.end())
				break;
			
			// partition bucket about scanline and continue
			//IndexedImageBucketRange& bucketToSplit = bucketRanges[*bucketIter];
			auto medianIter = partition(bucketIter->begin, bucketIter->end,
				[scanlineSplit = bucketIter->scanlineGapEnd, width = out.srcImg.width](IndexedImageData::const_reference_tuple a)
				{ return get<const unsigned int&>(a) / width < scanlineSplit; });
			IndexedImageBucketRange& newRange = bucketRanges.push_back();
			newRange.setBucketRange(medianIter, bucketIter->end, out.srcImg.width);
			bucketIter->setBucketRange(bucketIter->begin, medianIter, out.srcImg.width);
		}
		else
		{
			// paletteBucketRangeIndices and hdmaBucketRangeIndices are both maxed out - exit the loop
			break;
		}
	}

	// now that the colors have been bucketed, calculate the avg color of each bucket to determine the image's palette 
	out.palettizedImg.width = out.srcImg.width;
	out.palettizedImg.height = out.srcImg.height;
	out.palettizedImg.data.resize(out.srcImg.data.size());
	out.palettizedImg.palette.clear();
	out.palettizedImg.palette.push_back(0); // add 0 because that's a translucent pixel that should not be used

	// fill out the bucketRanges from the paletteBucketRangeIndices first, to fill out the palettizedImg's base palette,
	for (auto baseBucketRangeIndex : paletteBucketRangeIndices)
	{
		auto bucket = bucketRanges[baseBucketRangeIndex];
		auto paletteIdx = (unsigned char)(out.palettizedImg.palette.size());
		out.palettizedImg.palette.push_back(bucket.getAverageColor());
		eastl::for_each(bucket.begin, bucket.end, [&paletteIdx, &out](IndexedImageData::const_reference_tuple px)
		{
			out.palettizedImg.data[get<const unsigned int&>(px)] = paletteIdx;
		});
	}

	// next, go through the HDMA population list, to do two things:
	// 1) figure out the coloration of the bucket that is being used for the new color
	// 2) map the bucket being evicted back to an index in the palette
	struct HdmaAction
	{
		unsigned char scanline;
		unsigned char scanlineFirst;
		unsigned char scanlineRequired;
		unsigned char paletteIdx;
		unsigned short snesColor;
	};
	fixed_vector<HdmaAction, MaxHeight, false> hdmaActions;
	unsigned char previousScanline = 0;
	eastl::sort(hdmaPopulationList.begin(), hdmaPopulationList.end(), [&bucketRanges](const pair<unsigned int, unsigned int>& a, const pair<unsigned int, unsigned int>& b)
	{
		if (bucketRanges[a.first].scanlineLast < bucketRanges[b.first].scanlineLast) return true;
		if (bucketRanges[a.first].scanlineLast > bucketRanges[b.first].scanlineLast) return false;
		if (bucketRanges[a.second].scanlineFirst < bucketRanges[b.second].scanlineFirst) return true;
		else return false;
	});
	for (auto hdmaPopulationIter = hdmaPopulationList.begin(); hdmaPopulationIter != hdmaPopulationList.end(); ++hdmaPopulationIter)
	{
		auto hdmaPopulation = *hdmaPopulationIter;
		// find what paletteIdx this hdma action should occur in
		unsigned char paletteIdx;
		{
			auto bucketIndexToEvict = hdmaPopulation.first;
			while (true)
			{
				auto baseBucketIndexIter = eastl::find(paletteBucketRangeIndices.begin(), paletteBucketRangeIndices.end(), bucketIndexToEvict);
				// couldn't directly find the baseBucketRange that we're evicting, so we must be evicting something else in the hdmaPopulation
				if (baseBucketIndexIter == paletteBucketRangeIndices.end())
				{
					auto hdmaPopulationIter = eastl::find_if(hdmaPopulationList.begin(), hdmaPopulationList.end(),
						[bucketIndexToEvict](const pair<unsigned int, unsigned int>& hdmaPopulation)
					{
						return hdmaPopulation.second == bucketIndexToEvict;
					});
					bucketIndexToEvict = hdmaPopulationIter->first;
				}
				else
				{
					paletteIdx = (unsigned char)(baseBucketIndexIter - paletteBucketRangeIndices.begin() + 1);
					break;
				}
			}
		}

		auto bucket = bucketRanges[hdmaPopulation.second];
		HdmaAction hdmaAction;
		hdmaAction.scanline = max(bucketRanges[hdmaPopulation.first].scanlineLast, (unsigned char)(previousScanline))+1;
		hdmaAction.scanlineFirst = bucketRanges[hdmaPopulation.first].scanlineLast;
		hdmaAction.scanlineRequired = bucketRanges[hdmaPopulation.second].scanlineFirst;
		hdmaAction.paletteIdx = paletteIdx;
		hdmaAction.snesColor = bucket.getAverageColor();
		hdmaActions.push_back(hdmaAction);
		previousScanline = hdmaAction.scanline;

		eastl::for_each(bucket.begin, bucket.end, [&paletteIdx, &out](IndexedImageData::const_reference_tuple px)
		{
			out.palettizedImg.data[get<const unsigned int&>(px)] = paletteIdx;
		});
	}

	unsigned int numHdmaActions = (unsigned int)hdmaActions.size();
	hdmaActions.push_back({ 224, 0, 0 });
	out.palettizedImg.hdmaTables.push_back();
	{
		out.palettizedImg.hdmaTables[0].push_back({ hdmaActions[0].scanline, 0, 0 });
		for (unsigned int i = 0; i < numHdmaActions; ++i)
		{
			out.palettizedImg.hdmaTables[0].push_back({
				(unsigned char)(hdmaActions[i + 1].scanline - hdmaActions[i].scanline),
				hdmaActions[i].paletteIdx,
				hdmaActions[i].snesColor
				});
		}
	}
}
