﻿// Arminius' protocol utilities ver 0.1
//
/*
				GNU GENERAL PUBLIC LICENSE
				   Version 2, June 1991
COPYRIGHT (C) Bestkakkoii 2023 All Rights Reserved.
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

// -------------------------------------------------------------------
// The following definitions is to define game-dependent codes.
// Before compiling, remove the "//".

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include "autil.h"

#include <injector.h>


//char msgSlice_[sizeof(char*) * SLICE_MAX][SLICE_SIZE];

Autil::Autil(long long index)
	: Indexer(index)
{
}

// -------------------------------------------------------------------
// Initialize utilities
//
void Autil::util_Init(void)
{
	QMutexLocker locker(&msgMutex_);
	//constexpr auto size = sizeof(char*) * SLICE_MAX * SLICE_SIZE;
	msgSlice_.clear();
	SliceCount = 0;
}

void Autil::util_Release(void)
{
	QMutexLocker locker(&msgMutex_);
	//constexpr auto size = sizeof(char*) * SLICE_MAX * SLICE_SIZE;
	msgSlice_.clear();
	SliceCount = 0;
}

void Autil::util_Clear(void)
{
	QMutexLocker locker(&msgMutex_);
	//constexpr auto size = sizeof(char*) * SLICE_MAX * SLICE_SIZE;
	msgSlice_.clear();
	SliceCount = 0;
}

// -------------------------------------------------------------------
// Split up a message into slices by spearator.  Store those slices
// into a global buffer "char **msgSlice_"
//
// arg: source=message string;  separator=message separator (1 byte)
// ret: (none)
bool Autil::util_SplitMessage(const QByteArray& source, char separator)
{
	QMutexLocker locker(&msgMutex_);

	QByteArrayList list = source.split(separator);

	long long count = 0;
	for (QByteArray& slice : list)
	{
		msgSlice_.insert(count, std::move(slice));
		++count;
	}
	SliceCount = count;
	return count > 0;

	//if (separator)
	//{	// NULL input is invalid.
	//	char* ptr = nullptr;
	//	char* head = source;

	//	while ((ptr = reinterpret_cast<char*>(strstr(head, separator))) && (SliceCount <= SLICE_MAX))
	//	{
	//		ptr[0] = '\0';
	//		if (strlen(head) < SLICE_SIZE)
	//		{
	//			// discard slices too large
	//			_snprintf_s(msgSlice_[SliceCount], SLICE_SIZE, _TRUNCATE, "%s", head);
	//			int count = SliceCount;
	//			++count;
	//			SliceCount = count;
	//		}

	//		head = ptr + 1;
	//	}

	//	strcpy_s(source, dstlen, head);	// remove splited slices
	//}
	//return true;
}

// -------------------------------------------------------------------
// Encode the message
//
// arg: dst=output  src=input
// ret: (none)
void Autil::util_EncodeMessage(char* dst, size_t dstlen, char* src)
{
	std::mt19937 generator(std::random_device{}());
	std::uniform_int_distribution<int> distribution(0, 99);
	int rn = distribution(generator);
	int t1 = 0, t2 = 0;
	char t3[LBUFSIZE], tz[LBUFSIZE];
	memset(tz, 0, sizeof(tz));
	memset(t3, 0, sizeof(t3));

#ifdef _BACK_VERSION
	util_swapint(&t1, &rn, "3421");	// encode seed
#else
	Autil::util_swapint(&t1, &rn, const_cast<char*>("2413"));	// encode seed
#endif
	//  t2 = t1 ^ 0x0f0f0f0f;
	t2 = t1 ^ 0xffffffff;
	Autil::util_256to64(tz, reinterpret_cast<char*>(&t2), sizeof(int), const_cast<char*>(DEFAULTTABLE));

	Autil::util_shlstring(t3, dstlen, src, rn);
	//  printf("random number=%d\n", rn);
	//strcat_s(tz.data(), t3.data());
	strcat_s(tz, LBUFSIZE, t3);
	Autil::util_xorstring(dst, tz);

}

// -------------------------------------------------------------------
// Decode the message
//
// arg: dst=output  src=input
// ret: (none)
void Autil::util_DecodeMessage(QByteArray& dst, QByteArray src)
{
	//  strcpy(dst, src);
	//  util_xorstring(dst, src);

	constexpr auto INTCODESIZE = (sizeof(int) * 8 + 5) / 6;

	int rn = 0;
	int* t1 = nullptr;
	int t2 = 0;
	char t3[4096], t4[4096];	// This buffer is enough for an integer.
	char tz[LBUFSIZE];
	memset(tz, 0, sizeof(tz));
	memset(t3, 0, sizeof(t3));
	memset(t4, 0, sizeof(t4));

	//if (src[strlen(src) - 1] == '\n')
	//	src[strlen(src) - 1] = 0;
	src.replace('\n', '\0');
	Autil::util_xorstring(tz, src.data());

	rn = INTCODESIZE;
	//  printf("INTCODESIZE=%d\n", rn);

	strncpy_s(t4, SBUFSIZE, tz, INTCODESIZE);
	t4[INTCODESIZE] = '\0';
	Autil::util_64to256(t3, t4, const_cast<char*>(DEFAULTTABLE));
	t1 = reinterpret_cast<int*>(t3);

	//  t2 = *t1 ^ 0x0f0f0f0f;
	t2 = *t1 ^ 0xffffffff;
#ifdef _BACK_VERSION
	util_swapint(&rn, &t2, "4312");
#else
	Autil::util_swapint(&rn, &t2, const_cast<char*>("3142"));
#endif
	//  printf("random number=%d\n", rn);
	Autil::util_shrstring(dst, tz + INTCODESIZE, rn);

}

// -------------------------------------------------------------------
// Get a function information from msgSlice_.  A function is a complete
// and identifiable message received, beginned at DEFAULTFUNCBEGIN and
// ended at DEFAULTFUNCEND.  This routine will return the function ID
// (Action ID) and how many fields this function have.
//
// arg: func=return function ID    fieldcount=return fields of the function
// ret: 1=success  0=failed (function not complete)
long long Autil::util_GetFunctionFromSlice(long long* func, long long* fieldcount, long long offest)
{
	QMutexLocker locker(&msgMutex_);
	QByteArray t1(NETDATASIZE, '\0');

	// 从QMap中获取第一个元素来替代msgSlice_[0]
	auto firstItem = msgSlice_.value(0);

	if (firstItem != DEFAULTFUNCBEGIN)
		util_DiscardMessage();

	// 将第二个元素复制到t1中
	auto secondItem = msgSlice_.value(1);

	*func = secondItem.toLongLong() - offest;

	// 在QMap中寻找DEFAULTFUNCEND
	for (auto it = msgSlice_.begin(); it != msgSlice_.end(); ++it)
	{
		if (it.value() == DEFAULTFUNCEND)
		{
			*fieldcount = it.key() - 2;	// - "&" - "#" - "func" 3 fields
			return 1;
		}
	}

	return 0;	// failed: message not complete

	//QByteArray t1(NETDATASIZE, '\0');
	//long long i = 0;

	//if (strcmp(msgSlice_[0], DEFAULTFUNCBEGIN) != 0)
	//	util_DiscardMessage();

	//strcpy_s(t1.data(), NETDATASIZE, msgSlice_[1]);

	//// Robin adjust
	////*func=atoi(t1);
	//*func = std::atoi(t1.data()) - offest;
	//for (i = 0; i < SLICE_MAX; ++i)
	//{
	//	if (strcmp(msgSlice_[i], DEFAULTFUNCEND) == 0)
	//	{
	//		*fieldcount = i - 2;	// - "&" - "#" - "func" 3 fields
	//		return 1;
	//	}
	//}

	//return 0;	// failed: message not complete
}

// -------------------------------------------------------------------
// Discard a message from msgSlice_.
//
void Autil::util_DiscardMessage(void)
{
	SliceCount = 0;
}

// -------------------------------------------------------------------
// Send a message
//
// arg: fd=socket fd   func=function ID   buffer=data to send
void Autil::util_SendMesg(int func, char* buffer)
{
	long long currentIndex = getIndex();
	Injector& injector = Injector::getInstance(currentIndex);
	if (!injector.isValid())
		return;

	char t1[NETDATASIZE], t2[NETDATASIZE];
	memset(t1, 0, sizeof(t1));
	memset(t2, 0, sizeof(t2));

	//sprintf_s(t1.data(), NETDATASIZE, "&;%d%s;#;", func + 13, buffer);
	constexpr auto FUNCTION_OFFSET = 13;
	constexpr auto FORMAT = "&;%d%s;#;";
	_snprintf_s(t1, NETDATASIZE, _TRUNCATE, FORMAT, func + FUNCTION_OFFSET, buffer);
#ifdef _NEWNET_
	util_EncodeMessageTea(t2, t1);
#else
	Autil::util_EncodeMessage(t2, NETDATASIZE, t1);
#endif

	int size = static_cast<int>(strlen(t2));
	t2[size] = '\n';
	size += 1;

	HANDLE hProcess = injector.getProcess();
	util::VirtualMemory ptr(hProcess, size, true);

	mem::write(hProcess, ptr, t2, size);
	injector.sendMessage(kSendPacket, ptr, size);
}

// -------------------------------------------------------------------
// Convert 8-bit strings into 6-bit strings, buffers that store these strings
// must have enough space.
//
// arg: dst=8-bit string;  src=6-bit string;  len=src strlen;
//      table=mapping table
// ret: 0=failed  >0=bytes converted
int Autil::util_256to64(char* dst, char* src, int len, char* table)
{
	unsigned int dw = 0u, dwcounter = 0u;
	int i = 0;

	if (!dst || !src || !table)
		return 0;

	dw = 0;
	dwcounter = 0;

	for (i = 0; i < len; ++i)
	{
		dw = ((static_cast<unsigned int>(src[i]) & 0xff) << ((i % 3) << 1)) | dw;

		dst[dwcounter++] = table[dw & 0x3f];

		dw = (dw >> 6);

		if (i % 3 == 2)
		{
			dst[dwcounter++] = table[dw & 0x3f];
			dw = 0;
		}
	}

	if (dw)
		dst[dwcounter++] = table[dw];

	dst[dwcounter] = '\0';
	return dwcounter;
}

// -------------------------------------------------------------------
// Convert 6-bit strings into 8-bit strings, buffers that store these strings
// must have enough space.
//
// arg: dst=6-bit string;  src=8-bit string;  table=mapping table
// ret: 0=failed  >0=bytes converted
int Autil::util_64to256(char* dst, char* src, char* table)
{
	unsigned int i = 0u, j = 0u;
	char* ptr = nullptr;

	unsigned int dw = 0u;
	unsigned int dwcounter = 0u;

	if (!dst || !src || !table)
		return 0;

	char c = '\0';
	for (i = 0; i < strlen(src); ++i)
	{
		c = src[i];
		for (j = 0; j < strlen(table); ++j)
		{
			if (table[j] == c)
			{
				ptr = table + j;
				break;
			}
		}

		if (!ptr)
			return 0;

		if (i % 4)
		{
			dw = ((unsigned int)(ptr - table) & 0x3f) << ((4 - (i % 4)) << 1) | dw;
			dst[dwcounter++] = dw & 0xff;
			dw = dw >> 8;
		}
		else
		{
			dw = (unsigned int)(ptr - table) & 0x3f;
		}
	}

	if (dw)
		dst[dwcounter++] = dw & 0xff;

	dst[dwcounter] = '\0';
	return dwcounter;
}

// -------------------------------------------------------------------
// This basically is a 256to64 encoder.  But it shifts the result by key.
//
// arg: dst=6-bit string;  src=8-bit string;  len=src strlen;
//      table=mapping table;  key=rotate key;
// ret: 0=failed  >0=bytes converted
int Autil::util_256to64_shr(char* dst, char* src, int len, char* table, char* key)
{
	unsigned int j = 0u;
	int i = 0u;

	if (!dst || !src || !table || !key)
		return 0;

	if (strlen(key) < 1)
		return 0;	// key can't be empty.

	unsigned int dw = 0u;
	unsigned int dwcounter = 0u;

	j = 0;
	for (i = 0; i < len; ++i)
	{
		dw = ((static_cast<unsigned int>(src[i]) & 0xff) << ((i % 3) << 1)) | dw;

		dst[dwcounter++] = table[((dw & 0x3f) + key[j]) % 64];	// check!

		j++;

		if (!key[j])
			j = 0;

		dw = (dw >> 6);

		if (i % 3 == 2)
		{
			dst[dwcounter++] = table[((dw & 0x3f) + key[j]) % 64];// check!
			j++;

			if (!key[j])
				j = 0;

			dw = 0;
		}
	}

	if (dw)
		dst[dwcounter++] = table[(dw + key[j]) % 64];	// check!

	dst[dwcounter] = '\0';
	return dwcounter;
}

// -------------------------------------------------------------------
// Decoding function of util_256to64_shr.
//
// arg: dst=8-bit string;  src=6-bit string;  table=mapping table;
//      key=rotate key;
// ret: 0=failed  >0=bytes converted
int Autil::util_shl_64to256(char* dst, char* src, char* table, char* key)
{
	unsigned int i = 0u, j = 0u, k = 0u;
	char* ptr = nullptr;

	if (!key || (strlen(key) < 1))
		return 0;	// must have key

	unsigned int dw = 0u;
	unsigned int dwcounter = 0u;
	j = 0;

	if (!dst || !src || !table)
		return 0;

	char c = '\0';
	for (i = 0; i < strlen(src); ++i)
	{
		c = src[i];

		for (k = 0; k < strlen(table); ++k)
		{
			if (table[k] == c)
			{
				ptr = table + k;
				break;
			}
		}

		if (!ptr)
			return 0;

		if (i % 4)
		{
			// check!
			dw = (((static_cast<unsigned int>(ptr - table) & 0x3f) + 64 - key[j]) % 64)
				<< ((4 - (i % 4)) << 1) | dw;

			j++;

			if (!key[j])
				j = 0;

			dst[dwcounter++] = dw & 0xff;
			dw = dw >> 8;
		}
		else
		{
			// check!
			dw = ((static_cast<unsigned int>(ptr - table) & 0x3f) + 64 - key[j]) % 64;
			j++;

			if (!key[j])
				j = 0;
		}
	}

	if (dw)
		dst[dwcounter++] = dw & 0xff;

	dst[dwcounter] = '\0';

	return dwcounter;
}

// -------------------------------------------------------------------
// This basically is a 256to64 encoder.  But it shifts the result by key.
//
// arg: dst=6-bit string;  src=8-bit string;  len=src strlen;
//      table=mapping table;  key=rotate key;
// ret: 0=failed  >0=bytes converted
int Autil::util_256to64_shl(char* dst, char* src, int len, char* table, char* key)
{
	int i = 0, j = 0;

	if (!dst || !src || !table || !key)
		return 0;

	if (strlen(key) < 1)
		return 0;	// key can't be empty.

	unsigned int dw = 0u;
	unsigned int dwcounter = 0u;
	j = 0;

	for (i = 0; i < len; ++i)
	{
		dw = ((static_cast<unsigned int>(src[i]) & 0xff) << ((i % 3) << 1)) | dw;

		dst[dwcounter++] = table[((dw & 0x3f) + 64 - key[j]) % 64];	// check!

		j++;
		if (!key[j])
			j = 0;

		dw = (dw >> 6);

		if (i % 3 == 2)
		{
			dst[dwcounter++] = table[((dw & 0x3f) + 64 - key[j]) % 64];	// check!
			j++;

			if (!key[j])
				j = 0;

			dw = 0;
		}
	}

	if (dw)
		dst[dwcounter++] = table[(dw + 64 - key[j]) % 64];	// check!

	dst[dwcounter] = '\0';

	return dwcounter;
}

// -------------------------------------------------------------------
// Decoding function of util_256to64_shl.
// 
// arg: dst=8-bit string;  src=6-bit string;  table=mapping table;
//      key=rotate key;
// ret: 0=failed  >0=bytes converted
int Autil::util_shr_64to256(char* dst, char* src, char* table, char* key)
{
	unsigned int i, k;
	char* ptr = nullptr;

	if (!key || (strlen(key) < 1))
		return 0;	// must have key

	unsigned int dw = 0u;
	unsigned int dwcounter = 0u;
	unsigned int j = 0u;

	if (!dst || !src || !table)
		return 0;

	char c = '\0';
	for (i = 0; i < strlen(src); ++i)
	{
		c = src[i];
		for (k = 0; k < strlen(table); ++k)
		{
			if (table[k] == c)
			{
				ptr = table + k;
				break;
			}
		}

		if (!ptr)
			return 0;

		if (i % 4)
		{
			// check!
			dw = (((static_cast<unsigned int>(ptr - table) & 0x3f) + key[j]) % 64)
				<< ((4 - (i % 4)) << 1) | dw;
			j++;

			if (!key[j])
				j = 0;

			dst[dwcounter++] = dw & 0xff;
			dw = dw >> 8;
		}
		else
		{
			// check!
			dw = ((static_cast<unsigned int>(ptr - table) & 0x3f) + key[j]) % 64;
			j++;

			if (!key[j]) j = 0;
		}
	}

	if (dw)
		dst[dwcounter++] = dw & 0xff;

	dst[dwcounter] = '\0';

	return dwcounter;
}

// -------------------------------------------------------------------
// Swap a integer (4 byte).
// The value "rule" indicates the swaping rule.  It's a 4 byte string
// such as "1324" or "2431".
//
void Autil::util_swapint(int* dst, int* src, char* rule)
{
	int i = 0;
	char* ptr = reinterpret_cast<char*>(src);
	char* qtr = reinterpret_cast<char*>(dst);
	for (i = 0; i < 4; ++i)
		qtr[rule[i] - '1'] = ptr[i];
}

// -------------------------------------------------------------------
// Xor a string.  Be careful that your string contains '0xff'.  Your
// data may lose.
//
void Autil::util_xorstring(char* dst, char* src)
{
	unsigned int i = 0;

	for (i = 0; i < strlen(src); ++i)
		dst[i] = src[i] ^ 255;

	dst[i] = '\0';
}

// -------------------------------------------------------------------
// Shift the string right.
//
void Autil::util_shrstring(QByteArray& dst, char* src, int offs)
{
	char* ptr = nullptr;
	//int len = strlen(src);

	if (!src || (strlen(src) < 1))
		return;

	offs = static_cast<int>(strlen(src)) - (offs % strlen(src));
	ptr = src + offs;
	//strcpy_s(dst, dstlen, ptr);
	dst = ptr;
	//strncat_s(dst, dstlen, src, offs);
	dst.append(src);
	//dst[strlen(src)] = '\0';
}

// -------------------------------------------------------------------
// Shift the string left.
//
void Autil::util_shlstring(char* dst, size_t dstlen, char* src, int offs)
{
	char* ptr = nullptr;
	if (!dst || !src || (strlen(src) < 1))
		return;

	offs = offs % strlen(src);
	ptr = src + offs;
	strcpy_s(dst, dstlen, ptr);
	strncat_s(dst, dstlen, src, offs);
	dst[strlen(src)] = '\0';
}

// -------------------------------------------------------------------
// Convert a message slice into integer.  Return a checksum.
//
// arg: sliceno=slice index in msgSlice_    value=result
// ret: checksum, this value must match the one generated by util_mkint
int Autil::util_deint(int sliceno, int* value)
{
	QMutexLocker locker(&msgMutex_);
	if (!msgSlice_.contains(sliceno))
		return 0;

	QByteArray slice = msgSlice_.value(sliceno);
	int* t1 = nullptr;
	int t2 = 0;
	char t3[4096];	// This buffer is enough for an integer.
	memset(t3, 0, sizeof(t3));

	Autil::util_shl_64to256(t3, slice.data(), const_cast<char*>(DEFAULTTABLE), PersonalKey.get().toUtf8().data());
	t1 = reinterpret_cast<int*>(t3);
	t2 = *t1 ^ 0xffffffff;
#ifdef _BACK_VERSION
	util_swapint(value, &t2, "3421");
#else
	Autil::util_swapint(value, &t2, const_cast<char*>("2413"));
#endif
	return *value;
}

// -------------------------------------------------------------------
// Pack a integer into buffer (a string).  Return a checksum.
//
// arg: buffer=output   value=data to pack
// ret: checksum, this value must match the one generated by util_deint
int Autil::util_mkint(char* buffer, int value)
{
	int t1 = 0, t2 = 0;
	char t3[4096];	// This buffer is enough for an integer.
	memset(t3, 0, sizeof(t3));

#ifdef _BACK_VERSION
	util_swapint(&t1, &value, "4312");
#else
	Autil::util_swapint(&t1, &value, const_cast<char*>("3142"));
#endif
	t2 = t1 ^ 0xffffffff;
	Autil::util_256to64_shr(t3, (char*)&t2, sizeof(int), const_cast<char*>(DEFAULTTABLE), PersonalKey.data().toUtf8().data());
	strcat_s(buffer, NETDATASIZE, ";");	// It's important to append a SEPARATOR between fields
	strcat_s(buffer, NETDATASIZE, t3);

	return value;
}

// -------------------------------------------------------------------
// Convert a message slice into string.  Return a checksum.
//
// arg: sliceno=slice index in msgSlice_    value=result
// ret: checksum, this value must match the one generated by util_mkstring
int Autil::util_destring(int sliceno, char* value)
{
	QMutexLocker locker(&msgMutex_);
	if (!msgSlice_.contains(sliceno))
		return 0;

	QByteArray slice = msgSlice_.value(sliceno);

	Autil::util_shr_64to256(value, slice.data(), const_cast<char*>(DEFAULTTABLE), PersonalKey.data().toUtf8().data());

	return static_cast<int>(strlen(value));
}

// -------------------------------------------------------------------
// Convert a string into buffer (a string).  Return a checksum.
//
// arg: buffer=output   value=data to pack
// ret: checksum, this value must match the one generated by util_destring
int Autil::util_mkstring(char* buffer, char* value)
{
	char t1[SLICE_SIZE];
	memset(t1, 0, sizeof(t1));

	Autil::util_256to64_shl(t1, value, static_cast<int>(strlen(value)), const_cast<char*>(DEFAULTTABLE), PersonalKey.data().toUtf8().data());
	strcat_s(buffer, NETDATASIZE, ";");	// It's important to append a SEPARATOR between fields
	strcat_s(buffer, NETDATASIZE, t1);

	return static_cast<int>(strlen(value));
}