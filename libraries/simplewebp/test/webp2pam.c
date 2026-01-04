/**
 * Converts WebP to PAM format.
 * 
 * Licensed under MIT No Attribution
 *
 * Copyright (c) 2025 Miku AuahDark
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define SIMPLEWEBP_IMPLEMENTATION
#include "../simplewebp.h"

static void writePAM(FILE *f, simplewebp_u32 width, simplewebp_u32 height, const simplewebp_u8 *rgba)
{
	fprintf(f, "P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", width, height);
	fwrite(rgba, 4, width * height, f);
}

static simplewebp_u8 *readcontents(FILE *f, size_t *out_size)
{
	simplewebp_u8 *output = NULL;
	size_t output_size = 0;
	simplewebp_u8 chunk[4096];

	for (;;)
	{
		size_t readed = fread(chunk, 1, sizeof(chunk), f);

		if (readed > 0)
		{
			simplewebp_u8 *new_output = realloc(output, output_size + readed);
			if (!new_output)
			{
				free(output);
				*out_size = 0;
				return NULL;
			}
			output = new_output;
			memcpy(output + output_size, chunk, readed);
			output_size += readed;
		}

		if (readed < sizeof(chunk))
			break;
	}

	*out_size = output_size;
	return output;
}

int main(int argc, char *argv[])
{
	simplewebp_input swebp_input;
	simplewebp *webp;
	simplewebp_error err;
	size_t contents_size;
	simplewebp_u8 *contents;
	size_t width, height;
	simplewebp_u8 *rgba;
	simplewebp_bool fromstdin = 1, tostdout = 1;
	FILE *infile = stdin, *outfile = stdout;

#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif

	if (argc >= 2)
	{
		infile = fopen(argv[1], "rb");
		fromstdin = 0;
		if (!infile)
		{
			fprintf(stderr, "Cannot open file %s\n", argv[1]);
			return 1;
		}
	}

	if (argc >= 3)
	{
		outfile = fopen(argv[2], "wb");
		tostdout = 0;
		if (!outfile)
		{
			fprintf(stderr, "Cannot open file %s\n", argv[2]);
			return 1;
		}
	}

	contents = readcontents(infile, &contents_size);

	err = simplewebp_load_from_memory(contents, contents_size, NULL, &webp);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		fprintf(stderr, "Cannot open webp: %d\n", (int) err);
		return 1;
	}

	simplewebp_get_dimensions(webp, &width, &height);
	rgba = malloc(width * height * 4);

	err = simplewebp_decode(webp, rgba, NULL);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		fprintf(stderr, "Cannot decode webp: %d\n", (int) err);
		simplewebp_unload(webp);
		return 1;
	}
	simplewebp_unload(webp);

	writePAM(outfile, (simplewebp_u32) width, (simplewebp_u32) height, rgba);
	if (!fromstdin)
		fclose(infile);
	if (!tostdout)
		fclose(outfile);

	return 0;
}
