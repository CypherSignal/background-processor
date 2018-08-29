#include "Pch.h"

#include "imageprocess.h"

Image processImage(const Image& img)
{
	Image processedImg = img;

	for (auto& imgData : processedImg.imgData)
	{
		imgData.r = 255 - imgData.r;
		imgData.g = 255 - imgData.g;
		imgData.b = 255 - imgData.b;
	}

	return processedImg;
}

