#include "Pch.h"

#include "imageprocess.h"

#include <EASTL/sort.h>
#include <EASTL/utility.h>

using namespace eastl;

void quantizeToSinglePalette(Image& img, int maxColors);

Image processImage(const Image& img, const ProcessImageParams& params)
{
	Image processedImage = img;
	
	// we should always quantize the image to the SNES' palette first (5b per color)
	for (auto& px : processedImage.imgData)
	{
		px.r &= 0xf8;
		px.g &= 0xf8;
		px.b &= 0xf8;
	}

	if (params.lowBitDepthPalette)
	{
		// todo
	}
	else
	{
		quantizeToSinglePalette(processedImage, params.maxColors);
	}
	return processedImage;

}

template<typename Iterator>
void colorBucketPartition(Iterator imgBegin, Iterator imgEnd, int colorsToFind)
{
	vector<pair<Iterator, Iterator>> bucketRanges;
	bucketRanges.reserve(colorsToFind);
	bucketRanges.push_back(make_pair(imgBegin, imgEnd));
	auto bucketRangeIter = bucketRanges.begin();

	while (bucketRanges.size() < colorsToFind)
	{
		// first, find which bucket has the greatest delta across all of the rgb channels: that's the one to split
		vector<pair<Iterator, Iterator>>::iterator bucketIter = bucketRanges.begin();
		unsigned char maxDelta = 0;
		int channelToSort = 0;
		for (auto bucketSearchIter = bucketRanges.begin(); bucketSearchIter != bucketRanges.end(); ++bucketSearchIter)
		{
			// find which channel has most variance
			Color lowestChannels{ 255,255,255 };
			Color highestChannels{ 0,0,0 };
			// TODO: Each bucket is having its delta recomputed here. Would be good if we could cache this
			for (auto pxIter = bucketSearchIter->first; pxIter != bucketSearchIter->second; ++pxIter)
			{
				lowestChannels.r = min(pxIter->first.r, lowestChannels.r);
				lowestChannels.g = min(pxIter->first.g, lowestChannels.g);
				lowestChannels.b = min(pxIter->first.b, lowestChannels.b);
				highestChannels.r = max(pxIter->first.r, highestChannels.r);
				highestChannels.g = max(pxIter->first.g, highestChannels.g);
				highestChannels.b = max(pxIter->first.b, highestChannels.b);
			}
			unsigned char deltaR = highestChannels.r - lowestChannels.r;
			unsigned char deltaG = highestChannels.g - lowestChannels.g;
			unsigned char deltaB = highestChannels.b - lowestChannels.b;

			if (deltaR > maxDelta)
			{
				maxDelta = deltaR;
				channelToSort = 0;
				bucketIter = bucketSearchIter;
			}
			else if (deltaG > maxDelta)
			{
				maxDelta = deltaG;
				channelToSort = 1;
				bucketIter = bucketSearchIter;
			}
			else if (deltaB > maxDelta)
			{
				maxDelta = deltaB;
				channelToSort = 2;
				bucketIter = bucketSearchIter;
			}
		}

		Iterator medianIter;
		unsigned char medianColor;
		switch (channelToSort)
		{
		case 0:
			// sort around red
			sort(bucketIter->first, bucketIter->second, [](const pair<Color, unsigned int>& a, const pair<Color, unsigned int>& b) { return a.first.r < b.first.r; });
			medianColor = (bucketIter->first + ((bucketIter->second - bucketIter->first) / 2))->first.r;
			medianIter = find_if(bucketIter->first, bucketIter->second, [medianColor](const pair<Color, unsigned int>& a) { return a.first.r == medianColor; });
			// if we matched the first (i.e. the median color ends up filling half of the partition) then find the first entry that doesn't match the median and we'll split on that
			if (medianIter == bucketIter->first)
				medianIter = find_if(medianIter, bucketIter->second, [medianColor](const pair<Color, unsigned int>& a) { return a.first.r != medianColor; });
			break;
		case 1:
			// sort around green
			sort(bucketIter->first, bucketIter->second, [](const pair<Color, unsigned int>& a, const pair<Color, unsigned int>& b) { return a.first.g < b.first.g; });
			medianColor = (bucketIter->first + ((bucketIter->second - bucketIter->first) / 2))->first.g;
			medianIter = find_if(bucketIter->first, bucketIter->second, [medianColor](const pair<Color, unsigned int>& a) { return a.first.g == medianColor; });
			// if we matched the first (i.e. the median color ends up filling half of the partition) then find the first entry that doesn't match the median and we'll split on that
			if (medianIter == bucketIter->first)
				medianIter = find_if(medianIter, bucketIter->second, [medianColor](const pair<Color, unsigned int>& a) { return a.first.g != medianColor; });
			break;
		case 2:
			// sort around blue
			sort(bucketIter->first, bucketIter->second, [](const pair<Color, unsigned int>& a, const pair<Color, unsigned int>& b) { return a.first.b < b.first.b; });
			medianColor = (bucketIter->first + ((bucketIter->second - bucketIter->first) / 2))->first.b;
			medianIter = find_if(bucketIter->first, bucketIter->second, [medianColor](const pair<Color, unsigned int>& a) { return a.first.b == medianColor; });
			// if we matched the first (i.e. the median color ends up filling half of the partition) then find the first entry that doesn't match the median and we'll split on that
			if (medianIter == bucketIter->first)
				medianIter = find_if(medianIter, bucketIter->second, [medianColor](const pair<Color, unsigned int>& a) { return a.first.b != medianColor; });
			break;
		};

		// if the find_if for a new medianIter failed, it's because
		if (medianIter == bucketIter->first)
			continue;

		// split the bucket about the median
		bucketRanges.push_back(make_pair(medianIter, bucketIter->second));

		// and shift the current bucketrange down correspondingly
		bucketIter->second = medianIter;
	}
	
	vector<Color> bucketColors;
	// once we have all of the buckets, calculate appropriate averages
	for (auto bucket : bucketRanges)
	{
		if (bucket.first == bucket.second)
			continue;

		// calculate the average in the provided bucket, and update all values to that one
		long accumulatedR = 0;
		long accumulatedG = 0;
		long accumulatedB = 0;

		for (auto pxIter = bucket.first; pxIter != bucket.second; ++pxIter)
		{
			accumulatedR += pxIter->first.r;
			accumulatedG += pxIter->first.g;
			accumulatedB += pxIter->first.b;
		}

		Color averageColor;
		size_t bucketSize = bucket.second - bucket.first;
		averageColor.r = unsigned char(accumulatedR / bucketSize);
		averageColor.g = unsigned char(accumulatedG / bucketSize);
		averageColor.b = unsigned char(accumulatedB / bucketSize);
		for (auto pxIter = bucket.first; pxIter != bucket.second; ++pxIter)
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

