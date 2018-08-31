#include "Pch.h"

#include "imageprocess.h"

#include <EASTL/sort.h>
#include <EASTL/utility.h>

using namespace eastl;

void quantizeToSinglePalette(Image& img, int maxColors);

Image processImage(const Image& img, const ProcessImageParams& params)
{
	Image processedImage = img;
	
	if (params.lowBitDepthPalette)
	{
		// todo
	}
	else
	{
		quantizeToSinglePalette(processedImage, params.maxColors);
	}

	// quantize the image to the SNES' palette at the end
	for (auto& px : processedImage.imgData)
	{
		px.r &= 0xf8;
		px.g &= 0xf8;
		px.b &= 0xf8;
	}

	return processedImage;
}

template<typename Iterator>
Color calculateColorDelta(Iterator begin, Iterator end)
{
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
	Color delta;
	delta.r = highestChannels.r - lowestChannels.r;
	delta.g = highestChannels.g - lowestChannels.g;
	delta.b = highestChannels.b - lowestChannels.b;
	return delta;
}

template<typename Iterator>
void colorBucketPartition(Iterator imgBegin, Iterator imgEnd, int colorsToFind)
{
	struct BucketRange
	{
		Iterator begin;
		Iterator end;
		Color colorDelta;
	};
	vector<BucketRange> bucketRanges;
	bucketRanges.reserve(colorsToFind);
	{
		BucketRange& newRange = bucketRanges.push_back();
		newRange.begin = imgBegin;
		newRange.end = imgEnd;
		newRange.colorDelta = calculateColorDelta(imgBegin, imgEnd);
	}

	auto bucketRangeIter = bucketRanges.begin();

	while (bucketRanges.size() < colorsToFind)
	{
		// first, find which bucket has the greatest delta across all of the rgb channels: that's the one to split
		vector<BucketRange>::iterator bucketIter = bucketRanges.begin();
		unsigned char maxDelta = 0;
		int channelToSort = 0;
		for (auto deltaSearchIter = bucketRanges.begin(); deltaSearchIter != bucketRanges.end(); ++deltaSearchIter)
		{
			Color delta = deltaSearchIter->colorDelta;
			if (delta.r > maxDelta)
			{
				maxDelta = delta.r;
				channelToSort = 0;
				bucketIter = deltaSearchIter;
			}
			if (delta.g > maxDelta)
			{
				maxDelta = delta.g;
				channelToSort = 1;
				bucketIter = deltaSearchIter;
			}
			if (delta.b > maxDelta)
			{
				maxDelta = delta.b;
				channelToSort = 2;
				bucketIter = deltaSearchIter;
			}
		}

		Iterator medianIter;
		unsigned char medianColor;
		switch (channelToSort)
		{
		case 0:
			// sort around red
			sort(bucketIter->begin, bucketIter->end, [](const pair<Color, unsigned int>& a, const pair<Color, unsigned int>& b) { return a.first.r < b.first.r; });
			medianColor = (bucketIter->begin + ((bucketIter->end - bucketIter->begin) / 2))->first.r;
			medianIter = find_if(bucketIter->begin, bucketIter->end, [medianColor](const pair<Color, unsigned int>& a) { return a.first.r == medianColor; });
			// if we matched the first (i.e. the median color ends up filling half of the partition) then find the first entry that doesn't match the median and we'll split on that
			if (medianIter == bucketIter->begin)
				medianIter = find_if(medianIter, bucketIter->end, [medianColor](const pair<Color, unsigned int>& a) { return a.first.r != medianColor; });
			break;
		case 1:
			// sort around green
			sort(bucketIter->begin, bucketIter->end, [](const pair<Color, unsigned int>& a, const pair<Color, unsigned int>& b) { return a.first.g < b.first.g; });
			medianColor = (bucketIter->begin + ((bucketIter->end - bucketIter->begin) / 2))->first.g;
			medianIter = find_if(bucketIter->begin, bucketIter->end, [medianColor](const pair<Color, unsigned int>& a) { return a.first.g == medianColor; });
			// if we matched the first (i.e. the median color ends up filling half of the partition) then find the first entry that doesn't match the median and we'll split on that
			if (medianIter == bucketIter->begin)
				medianIter = find_if(medianIter, bucketIter->end, [medianColor](const pair<Color, unsigned int>& a) { return a.first.g != medianColor; });
			break;
		case 2:
			// sort around blue
			sort(bucketIter->begin, bucketIter->end, [](const pair<Color, unsigned int>& a, const pair<Color, unsigned int>& b) { return a.first.b < b.first.b; });
			medianColor = (bucketIter->begin + ((bucketIter->end - bucketIter->begin) / 2))->first.b;
			medianIter = find_if(bucketIter->begin, bucketIter->end, [medianColor](const pair<Color, unsigned int>& a) { return a.first.b == medianColor; });
			// if we matched the first (i.e. the median color ends up filling half of the partition) then find the first entry that doesn't match the median and we'll split on that
			if (medianIter == bucketIter->begin)
				medianIter = find_if(medianIter, bucketIter->end, [medianColor](const pair<Color, unsigned int>& a) { return a.first.b != medianColor; });
			break;
		};

		// split the bucket about the median
		{
			BucketRange& newRange = bucketRanges.push_back();
			newRange.begin = medianIter;
			newRange.end = bucketIter->end;
			newRange.colorDelta = calculateColorDelta(medianIter, bucketIter->end);
		}

		// and shift the current bucketrange down correspondingly
		bucketIter->end = medianIter;
		bucketIter->colorDelta = calculateColorDelta(bucketIter->begin, medianIter);
	}
	
	vector<Color> bucketColors;
	// once we have all of the buckets, calculate appropriate averages
	for (auto bucket : bucketRanges)
	{
		// calculate the average in the provided bucket, and update all values to that one
		long accumulatedR = 0;
		long accumulatedG = 0;
		long accumulatedB = 0;

		for (auto pxIter = bucket.begin; pxIter != bucket.end; ++pxIter)
		{
			accumulatedR += pxIter->first.r;
			accumulatedG += pxIter->first.g;
			accumulatedB += pxIter->first.b;
		}

		Color averageColor;
		size_t bucketSize = bucket.end - bucket.begin;
		averageColor.r = unsigned char(accumulatedR / bucketSize);
		averageColor.g = unsigned char(accumulatedG / bucketSize);
		averageColor.b = unsigned char(accumulatedB / bucketSize);
		for (auto pxIter = bucket.begin; pxIter != bucket.end; ++pxIter)
		{
			pxIter->first = averageColor;
		}
		bucketColors.push_back(averageColor);
	}
}

void quantizeToSinglePalette(Image& img, int maxColors)
{
	// median cut algorithm to find palette
	vector<pair<Color, unsigned int>> imageDataWithIndex;
	imageDataWithIndex.reserve(img.imgData.size());
	
	unsigned int idx = 0;
	for (auto px : img.imgData)
	{
		imageDataWithIndex.push_back(make_pair(px, idx));
		++idx;
	}

	// bucket the colors and generate a list of colors
	colorBucketPartition(imageDataWithIndex.begin(), imageDataWithIndex.end(), maxColors);

	// now that imageDataWithIndex is sorted, map the colours it has back to the imgData
	for (auto indexedData : imageDataWithIndex)
	{
		img.imgData[indexedData.second] = indexedData.first;
	}
}

