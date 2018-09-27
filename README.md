background-processor

This will be a tool that accepts a single image (e.g. png, jpg, tga, bmp) or directory of images and processes it to make it suitable for use on the SNES.

It will take the image, and palletize it down to be used for a Mode 3/4 background, with a single up-to-256c palette for the entire image, plus data for HDMA to write in new palette entries per scanline.

Note that all input images must be smaller than 256x224 - larger images will error out.

The output data will include a png file showing expected results, plus individual files for each output, e.g. palette data, hdma table, tilemap, and tile data, s.t. it can be directly loaded into vram or utilized by a simple rom.


Note that this has not been built for significant platform agnosticism - this has some dependencies on the Windows SDK for the concurrency runtime, the precompiled headers have not been tested against compilers-not-MSVC, and the [ISPC compiler](https://ispc.github.io/downloads.html) needs to be installed somewhere and added to a %PATH% directory. After cloning the repo, make sure to initialize the submodules for EASTL, stb, and flags.

When running the program, make sure to the in and out command line arguments, and others, e.g., to scan all of the files in some input directory and another directory for all of the outputs:

-in="..\Test Backgrounds\resized" -hdmaChannels=4 -paletteSize=128 -outDir="..\Test Backgrounds\resized-processed"
