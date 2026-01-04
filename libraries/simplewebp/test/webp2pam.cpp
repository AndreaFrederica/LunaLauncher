/**
 * Converts WebP to PAM format. Simple WebP compiled with memory
 * allocation tracking to detect memory leaks.
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

#include <cstdint>
#include <exception>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define SIMPLEWEBP_IMPLEMENTATION
#include "../simplewebp.h"

struct MyAllocator: public simplewebp_allocator
{
	std::map<void*, std::vector<uint8_t>*> mem;

	MyAllocator()
	: simplewebp_allocator()
	{
		this->alloc = &allocStatic;
		this->free = &freeStatic;
		this->userdata = this;
	}

	bool verify()
	{
		bool ok = true;

		for (const auto &data: mem)
		{
			if (data.second != nullptr)
			{
				ok = false;
				fprintf(stderr, "Memory %p is not freed!\n", data.first);
			}
		}

		return ok;
	}

	void *allocMember(size_t size)
	{
		if (size == 0)
		{
			fprintf(stderr, "Attempt to allocate memory with size of 0 (simplewebp bug)\n");
			std::terminate();
			return nullptr;
		}

		std::vector<uint8_t> *output = new std::vector<uint8_t>(size);
		uint8_t *data = output->data();
		fprintf(stderr, "Allocated memory %p size %zu\n", data, size);
		mem[data] = output;
		return data;
	}

	void freeMember(void *ptr)
	{
		if (mem.find(ptr) == mem.end())
		{
			fprintf(stderr, "Memory %p is unknown!\n", ptr);
			std::terminate();
		}

		std::vector<uint8_t> *vec = mem[ptr];
		if (vec == nullptr)
		{
			fprintf(stderr, "Memory %p has been freed (DOUBLE FREE)!\n", ptr);
			std::terminate();
		}

		delete vec;
		mem[ptr] = nullptr;
		fprintf(stderr, "Memory %p freed successfully\n", ptr);
	}

	static void *allocStatic(void *ud, size_t size)
	{
		return ((MyAllocator*) ud)->allocMember(size);
	}

	static void freeStatic(void *ud, void *ptr)
	{
		return ((MyAllocator*) ud)->freeMember(ptr);
	}
};

static void writePAM(FILE *f, uint32_t width, uint32_t height, const uint8_t *rgba)
{
	fprintf(f, "P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", width, height);
	fwrite(rgba, 4, width * height, f);
}

static std::vector<uint8_t> readcontents(FILE *f)
{
	std::vector<uint8_t> output;
	uint8_t chunk[4096];

	while (true)
	{
		size_t readed = fread(chunk, 1, sizeof(chunk), f);

		if (readed > 0)
			output.insert(output.end(), chunk, chunk + readed);

		if (readed < sizeof(chunk))
			break;
	}

	return output;
}

int main(int argc, char *argv[])
{
	bool fromstdin = true, tostdout = true;
	FILE *infile = stdin, *outfile = stdout;

#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif

	if (argc >= 2)
	{
		infile = fopen(argv[1], "rb");
		fromstdin = false;
		if (!infile)
		{
			fprintf(stderr, "Cannot open file %s\n", argv[1]);
			return 1;
		}
	}

	if (argc >= 3)
	{
		outfile = fopen(argv[2], "wb");
		tostdout = false;
		if (!outfile)
		{
			fprintf(stderr, "Cannot open file %s\n", argv[2]);
			return 1;
		}
	}

	MyAllocator swebp_allocator;
	simplewebp_input swebp_input;
	simplewebp *webp;
	simplewebp_error err;
	std::vector<uint8_t> contents = readcontents(infile);

	err = simplewebp_load_from_memory(contents.data(), contents.size(), &swebp_allocator, &webp);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		fprintf(stderr, "Cannot open webp: %d\n", int(err));
		if (!swebp_allocator.verify())
			std::terminate();

		return 1;
	}

	size_t width, height;
	simplewebp_get_dimensions(webp, &width, &height);
	std::vector<uint8_t> rgba(width * height * 4);

	err = simplewebp_decode(webp, rgba.data(), nullptr);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		fprintf(stderr, "Cannot decode webp: %d\n", int(err));
		simplewebp_unload(webp);
		if (!swebp_allocator.verify())
			std::terminate();

		return 1;
	}
	simplewebp_unload(webp);
	if (!swebp_allocator.verify())
		std::terminate();

	writePAM(outfile, uint32_t(width), uint32_t(height), rgba.data());
	if (!fromstdin)
		fclose(infile);
	if (!tostdout)
		fclose(outfile);
	return 0;
}
