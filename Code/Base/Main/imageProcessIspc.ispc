// Helper functions for imageProcess

// calculate lowest and highest value across the provided range
export uniform unsigned int8 minmaxUint8(uniform unsigned int8 vals[], uniform int count,
						uniform unsigned int8 &minUni, uniform unsigned int8 &maxUni)
{
	unsigned int32 min = 255;
	unsigned int32 max = 0;
	foreach (index = 0 ... count) {
		// Load the appropriate input value for this program instance.
		unsigned int32 val = vals[index];
		if (val < min)
			min = val;
		if (val > max)
			max = val;
	}
	minUni = (unsigned int8)(reduce_min(min));
	maxUni = (unsigned int8)(reduce_max(max));
}

// mark which scanlines are being utilized by each px
export void markScanlines(uniform unsigned int pxLocations[], uniform int pxCount,
						uniform int8 pxOnScanline[224], uniform unsigned int width,
						uniform unsigned int8 &scanlineFirst, uniform unsigned int8 &scanlineLast)
{
	unsigned int minScanline = 224;
	unsigned int maxScanline = 0;
	foreach (index = 0 ... pxCount) {
		// Load the appropriate input value for this program instance.
		int pxLocation = pxLocations[index];
		int scanline = (float)pxLocation / (float)width;
		pxOnScanline[scanline] = true;
		if (scanline < minScanline)
			minScanline = scanline;
		if (scanline > maxScanline)
			maxScanline = scanline;
	}
	scanlineFirst = (unsigned int8)reduce_min(minScanline);
	scanlineLast =  (unsigned int8)reduce_max(maxScanline);
}