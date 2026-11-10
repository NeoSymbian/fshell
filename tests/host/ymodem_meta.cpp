// SPDX-License-Identifier: Apache-2.0
#include <cassert>
#include <cstring>
#include "../../core/builtins/ymodem_meta.h"

static unsigned short block[1024];
static NeoYmodem::THeader header;

static void Make(const char* name, const char* size)
	{
	std::memset(block, 0, sizeof(block));
	unsigned p = 0;
	while (*name) block[p++] = static_cast<unsigned char>(*name++);
	++p;
	while (*size) block[p++] = static_cast<unsigned char>(*size++);
	}

int main()
	{
	Make("binary.dat", "16777472");
	assert(NeoYmodem::ParseHeader(block, 128, header) == 1);
	assert(header.iNameBytes == 10 && header.iNameUnits == 10 && header.iSize == 16777472);
	Make("\xd5\xb0\xd5\xa1\xd5\xb5 file.bin", "0");
	assert(NeoYmodem::ParseHeader(block, 128, header) == 1 && header.iNameUnits == 12 && header.iSize == 0);
	Make("\xf0\x9f\x93\x81.bin", "2147483647");
	assert(NeoYmodem::ParseHeader(block, 1024, header) == 1 && header.iNameUnits == 6);
	const char* invalidNames[] = { "../escape", "..", ".", "C:escape", "a\\b", "a/b", "a?b", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\x80" };
	for (unsigned i = 0; i < sizeof(invalidNames) / sizeof(invalidNames[0]); ++i)
		{
		Make(invalidNames[i], "1");
		assert(NeoYmodem::ParseHeader(block, 128, header) == -1);
		}
	const char* invalidSizes[] = { "", "-1", "+1", "2147483648", "9999999999999999999999", "1x" };
	for (unsigned i = 0; i < sizeof(invalidSizes) / sizeof(invalidSizes[0]); ++i)
		{
		Make("file", invalidSizes[i]);
		assert(NeoYmodem::ParseHeader(block, 128, header) == -1);
		}
	Make("file", "1 12345 644");
	assert(NeoYmodem::ParseHeader(block, 128, header) == 1 && header.iSize == 1);
	std::memset(block, 0, sizeof(block));
	assert(NeoYmodem::ParseHeader(block, 128, header) == 0);
	for (unsigned i = 0; i < 128; ++i) block[i] = 'a';
	assert(NeoYmodem::ParseHeader(block, 128, header) == -1);
	assert(NeoYmodem::ParseHeader(block, 127, header) == -1);
	assert(NeoYmodem::ParseHeader(0, 128, header) == -1);
	Make("file", "1"); block[2] = 0x100;
	assert(NeoYmodem::ParseHeader(block, 128, header) == -1);
	std::memset(block, 0, sizeof(block));
	for (unsigned i = 0; i < 257; ++i) block[i] = 'a';
	block[258] = '1';
	assert(NeoYmodem::ParseHeader(block, 1024, header) == -1);
	return 0;
	}
