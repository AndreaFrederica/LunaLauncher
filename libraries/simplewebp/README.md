Simple WebP
=====

Simple WebP is a WebP image decoder in a single source file for easiest integration.

Features
-----

SimpleWebP does:
* Load WebP easily. Loading WebP can be done in 7 lines of code.
* Try to be compatible across all compilers.
* Integrate easily on any C/C++ projects.
* Decode alpha transparency.
* Decode lossless WebP.
* Passes all [`libwebp-test-data`](https://chromium.googlesource.com/webm/libwebp-test-data) test vectors.

SimpleWebP does not:
* Try to be the fastest WebP decoder.
* Support encoding WebP image.
* Support partial decoding (cropping and incremental).
* Support animation ~~(only support decoding first frame)~~ (decoding first frame is TODO).
* Support extracting EXIF/XMP metadata.

Installation
-----

Drop and use `simplewebp.h` in your project. In one (and only one) of your C/C++ source file, define
`SIMPLEWEBP_IMPLEMENTATION` before including `simplewebp.h`.

The license and patent grant of WebM codec is already included in the header file. A separate file `LICENSE.md` and `PATENTS.md` is provided for convenience.

A CMake project is also provided for convenience. You're not required to use it. However, if CMake project is used, there's no need to define `SIMPLEWEBP_IMPLEMENTATION` anywhere. It's automatically handled by CMake.

Quick Example
-----

The "Feature" sections says 7 lines of code right?

```c
size_t width, height;
simplewebp *swebp;
simplewebp_load_from_filename("image.webp", NULL, &swebp);
simplewebp_get_dimensions(swebp, &width, &height);
unsigned char *imageBuffer = malloc(width * height * 4);
simplewebp_decode(swebp, imageBuffer, NULL);
simplewebp_unload(swebp); /* Frees the simplewebp instance */
/* `imageBuffer`` now contains decoded RGBA data of the WebP image */
```
