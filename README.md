background-processor

This will be a tool that accepts a single image (e.g. png, jpg, tga, bmp) and processes it to make it suitable for use on the SNES.

It will take the image, and palletize it down to be appropriate for:

- 256c
- 16c per 8x8 tile with <8 palettes (minus one colour reserved for transparency)
- 256c with palette write in HDMA tables
- 16c per 8x8 tile with palette write HDMA tables

Images larger than 256x224 will error out.

The output data will include a png file showing expected results (w/ some option of filters - stretched to SNES pixel size, with CRT filter, or nothing) and individual files for each output, e.g. palette data, hdma table, tilemap, and tile data, s.t. it can be directly loaded into vram or utilized by a simple rom.

Speed will also be emphasized, to allow for quick artist iteration.

