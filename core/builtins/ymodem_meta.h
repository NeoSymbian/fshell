// SPDX-License-Identifier: Apache-2.0
// Bounded YMODEM block-zero validation, independent of Symbian descriptors.
#ifndef FSHELL_YMODEM_META_H
#define FSHELL_YMODEM_META_H

namespace NeoYmodem
	{
	struct THeader
		{
		unsigned iNameBytes;
		unsigned iNameUnits;
		unsigned iSize;
		};

	// 1 = file header, 0 = terminating empty header, -1 = invalid metadata.
	inline int ParseHeader(const unsigned short* aBlock, unsigned aLength, THeader& aHeader)
		{
		if (!aBlock || (aLength != 128 && aLength != 1024)) return -1;
		aHeader.iNameBytes = aHeader.iNameUnits = aHeader.iSize = 0;
		unsigned n = 0;
		while (n < aLength && aBlock[n])
			{
			if (aBlock[n] > 255) return -1;
			++n;
			}
		if (n == aLength) return -1;
		if (!n) return 0;
		if ((n == 1 && aBlock[0] == '.') || (n == 2 && aBlock[0] == '.' && aBlock[1] == '.')) return -1;
		unsigned units = 0;
		for (unsigned p = 0; p < n; )
			{
			unsigned c = aBlock[p++], need = 0, minimum = 0;
			if (c < 0x80)
				{
				if (c < 0x20 || c == 0x7f || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return -1;
				}
			else if (c >= 0xc2 && c <= 0xdf) { c &= 0x1f; need = 1; minimum = 0x80; }
			else if (c >= 0xe0 && c <= 0xef) { c &= 15; need = 2; minimum = 0x800; }
			else if (c >= 0xf0 && c <= 0xf4) { c &= 7; need = 3; minimum = 0x10000; }
			else return -1;
			if (p + need > n) return -1;
			for (unsigned i = 0; i < need; ++i)
				{
				const unsigned b = aBlock[p++];
				if (b < 0x80 || b > 0xbf) return -1;
				c = (c << 6) | (b & 0x3f);
				}
			if (need && (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))) return -1;
			units += c > 0xffff ? 2 : 1;
			if (units > 256) return -1;
			}
		unsigned p = n + 1, size = 0, digits = 0;
		while (p < aLength && aBlock[p] >= '0' && aBlock[p] <= '9')
			{
			const unsigned digit = aBlock[p++] - '0';
			// This command uses RFile's signed 32-bit size API, never truncate.
			if (size > (0x7fffffffu - digit) / 10) return -1;
			size = size * 10 + digit;
			++digits;
			}
		if (!digits || p == aLength || (aBlock[p] != 0 && aBlock[p] != ' ')) return -1;
		aHeader.iNameBytes = n;
		aHeader.iNameUnits = units;
		aHeader.iSize = size;
		return 1;
		}
	}

#endif
