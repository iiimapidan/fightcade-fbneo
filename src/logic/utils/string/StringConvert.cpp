////////////////////////////////////////////////////////////////////////////////
//
// Written/Rewritten by Jeffery Jiang (china_jeffery@163.com)
//
// You may opt to use, copy, modify, merge, publish, distribute and/or sell
// copies of the Software, and permit persons to whom the Software is
// furnished to do so.
//
// Expect bugs.
//
// Please use and enjoy. Please let me know of any bugs/mods/improvements
// that you have found/implemented and I will fix/incorporate them into this
// file.
//
////////////////////////////////////////////////////////////////////////////////
 
#include <Windows.h>
#include <vector>
#include <wchar.h>
#include <algorithm>

#include "StringConvert.h"

bool UnicodeToAnsi(const std::wstring& src, std::string& result) {
	int ascii_size = ::WideCharToMultiByte(CP_ACP, 0, src.c_str(), -1, NULL, 0, NULL, NULL);

	if (ascii_size == 0) {
		return false;
	}

	std::vector<char> result_buf(ascii_size, 0);
	int result_size = ::WideCharToMultiByte(CP_ACP, 0, src.c_str(), -1, &result_buf[0], ascii_size, NULL, NULL);

	if (result_size != ascii_size) {
		return false;
	}

	result = &result_buf[0];

	return true;
}

bool AnsiToUnicode(const std::string& src, std::wstring& result) {
	int wide_size = ::MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, NULL, 0);

	if (wide_size == 0) {
		return false;
	}

	std::vector<wchar_t> result_buf(wide_size, 0);
	int result_size = MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, &result_buf[0], wide_size);

	if (result_size != wide_size) {
		return false;
	}

	result = &result_buf[0];

	return true;
}

bool UnicodeToUtf8(const std::wstring& src, std::string& result) {
	int utf8_size = ::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, NULL, 0, NULL, NULL);

	if (utf8_size == 0) {
		return false;
	}

	std::vector<char> result_buf(utf8_size, 0);

	int result_size = ::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, &result_buf[0], utf8_size, NULL, NULL);

	if (result_size != utf8_size) {
		return false;
	}

	result = &result_buf[0];

	return true;
}


bool Utf8ToUnicode(const std::string& src, std::wstring& result) {
	int wide_size = ::MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, NULL, 0);

	if (wide_size == 0) {
		return false;
	}

	std::vector<wchar_t> result_buf(wide_size, 0);

	int result_size = ::MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, &result_buf[0], wide_size);

	if (result_size != wide_size) {
		return false;
	}

	result = &result_buf[0];

	return true;
}

bool AnsiToUtf8(const std::string& src, std::string& result) {
	std::wstring wstr;
	if (!AnsiToUnicode(src, wstr))
		return false;

	return UnicodeToUtf8(wstr, result);
}

bool Utf8ToAnsi(const std::string& src, std::string& result) {
	std::wstring wstr;
	if (!Utf8ToUnicode(src, wstr))
		return false;

	return UnicodeToAnsi(wstr, result);
}

std::string UrlStringEncode(const std::string& szToEncode) {
	std::string src = szToEncode;
	char hex[] = "0123456789ABCDEF";
	std::string dst;


	for (size_t i = 0; i < src.size(); ++i) {
		unsigned char cc = src[i];

		if (cc >= 'A' && cc <= 'Z'
			|| cc >= 'a' && cc <= 'z'
			|| cc >= '0' && cc <= '9'
			|| cc == '.'
			|| cc == '_'
			|| cc == '-'
			|| cc == '*'
			|| cc == '~') {
			dst += cc;
		} else {
			unsigned char c = static_cast<unsigned char>(src[i]);
			dst += '%';
			dst += hex[c / 16];
			dst += hex[c % 16];
		}
	}

	return dst;
}

std::string Easy_UnicodeToAnsi(const std::wstring& src) {
	std::string strRet;
	if (!UnicodeToAnsi(src, strRet))
		return "";

	return strRet;
}

std::wstring Easy_AnsiToUnicode(const std::string& src) {
	std::wstring strRet;
	if (!AnsiToUnicode(src, strRet))
		return L"";

	return strRet;
}

std::string Easy_UnicodeToUtf8(const std::wstring& src) {
	std::string strRet;
	if (!UnicodeToUtf8(src, strRet))
		return "";

	return strRet;
}

std::wstring Easy_Utf8ToUnicode(const std::string& src) {
	std::wstring strRet;
	if (!Utf8ToUnicode(src, strRet))
		return L"";

	return strRet;
}

std::string Easy_AnsiToUtf8(const std::string& src) {
	std::string strRet;
	if (!AnsiToUtf8(src, strRet))
		return "";

	return strRet;
}

std::string Easy_Utf8ToAnsi(const std::string& src) {
	std::string strRet;
	if (!Utf8ToAnsi(src, strRet))
		return "";

	return strRet;
}

std::wstring ToLower(const std::wstring& str) {
	std::wstring ret = str;
	std::transform(ret.begin(), ret.end(), ret.begin(), towlower);
	return ret;
}

std::wstring ToUpper(const std::wstring& str) {
	std::wstring ret = str;
	std::transform(ret.begin(), ret.end(), ret.begin(), towupper);
	return ret;
}

std::string ToLower(const std::string& str) {
	std::string ret = str;
	std::transform(ret.begin(), ret.end(), ret.begin(), towlower);
	return ret;
}

std::string ToUpper(const std::string& str) {
	std::string ret = str;
	std::transform(ret.begin(), ret.end(), ret.begin(), towupper);
	return ret;
}

std::string UrlEncode(const std::string& str) {
	const static unsigned char m[] = {
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,
		2,1,2,2,1,2,4,2,1,1,1,2,1,1,1,4,1,1,1,1,1,1,1,1,1,1,4,4,2,4,
		2,4,
		4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,
		2,1,
		2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,
		2,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0
	};
	const static char* h = "0123456789ABCDEF";
	std::string encoded;
	char* p = const_cast<char*>(str.c_str());

	for (; *p != 0; p++) {
		unsigned char ii = *p;
		if (ii > 0xFF || m[ii] & 1) {
			encoded += ii;
		} else {
			encoded += '%';
			encoded += h[ii >> 4];
			encoded += h[ii & 0x0F];
		}
	}

	return encoded;
}

std::string UrlDecode(const std::string& str) {
	std::string strEscape = str;

	std::string strString;

	for (unsigned long i = 0; i < strEscape.size();) {
		if (strEscape[i] == L'%') {
			if (i < strEscape.size() - 2) {
				char pBuf[4];
				memset(pBuf, 0, sizeof(char) * 4);
				pBuf[0] = strEscape[i + 1];
				pBuf[1] = strEscape[i + 2];
				unsigned int c;
				sscanf_s(pBuf, "%02X", &c);
				strString += c;
				i += 3;
			} else {
				return strString;
			}
		} else if (strEscape[i] == '+') {
			strString += ' ';
			i++;
		} else {
			strString += strEscape[i];
			i++;
		}
	}

	return strString;
}
