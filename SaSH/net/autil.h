﻿// Arminius' protocol utilities ver 0.1
//
// Any questions and bugs, mailto: arminius@mail.hwaei.com.tw

#ifndef __UTIL_H_
#define __UTIL_H_
namespace Autil
{
	constexpr size_t NETBUFSIZ = 1024 * 64;
	constexpr size_t SLICE_MAX = 20;
	constexpr size_t SLICE_SIZE = 65500;
	extern char MesgSlice[][Autil::SLICE_SIZE];	// store message slices
	extern int SliceCount;		// count slices in MesgSlice

	extern char PersonalKey[];

	constexpr const char* SEPARATOR = ";";

	constexpr const char* DEFAULTTABLE = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz{}";
	constexpr const char* DEFAULTFUNCBEGIN = "&";
	constexpr const char* DEFAULTFUNCEND = "#";

	void util_Init(void);
	void util_Release(void);
	bool util_SplitMessage(char* source, size_t dstlen, char* separator);
	void util_EncodeMessage(char* dst, size_t dstlen, char* src);
	void util_DecodeMessage(char* dst, size_t dstlen, char* src);
	int util_GetFunctionFromSlice(int* func, int* fieldcount);
	void util_DiscardMessage(void);
	void util_SendMesg(int fd, int func, char* buffer);

	// -------------------------------------------------------------------
	// Encoding function units.  Use in Encrypting functions.
	int util_256to64(char* dst, char* src, int len, char* table);
	int util_64to256(char* dst, char* src, char* table);
	int util_256to64_shr(char* dst, char* src, int len, char* table, char* key);
	int util_shl_64to256(char* dst, char* src, char* table, char* key);
	int util_256to64_shl(char* dst, char* src, int len, char* table, char* key);
	int util_shr_64to256(char* dst, char* src, char* table, char* key);

	void util_swapint(int* dst, int* src, char* rule);
	void util_xorstring(char* dst, char* src);
	void util_shrstring(char* dst, size_t dstlen, char* src, int offs);
	void util_shlstring(char* dst, size_t dstlen, char* src, int offs);
	// -------------------------------------------------------------------
	// Encrypting functions
	int util_deint(int sliceno, int* value);
	int util_mkint(char* buffer, int value);
	int util_destring(int sliceno, char* value);
	int util_mkstring(char* buffer, char* value);




	int strcmptail(char* s1, char* s2);
#ifndef _AUTIL_H_
#define _AUTIL_H_
#ifdef _FONT_STYLE_
	typedef struct
	{
		int x;
		int size;
		int color;
		char str[128];
	}WM_STR_STYLE;

	typedef struct
	{
		int flg;
		WM_STR_STYLE style[30];
	}WM_STR;


	typedef struct
	{
		int x;
		int size;
		int color;
		char str[32];
	}TITLE_STR_STYLE;

	typedef struct
	{
		int flg;
		int len;
		TITLE_STR_STYLE style[10];
	}TITLE_STR;
	void getCharTitleSplit(char* str, TITLE_STR* title);
	extern WM_STR wmstr[25];
	void getStrSplitNew(char str[][256]);
	extern char* sunday(char* str, char* subStr);
	void PutWinText(int x, int y, char fontPrio, int color, char* str, BOOL hitFlag, int index);
#endif
#endif
	BOOL getStringFromIndexWithDelim_body(char* src, char* delim, int index, char* buf, int buflen);

}

#endif


