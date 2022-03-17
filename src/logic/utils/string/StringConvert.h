#ifndef __COMMON_STRING_CONVERT_H__
#define __COMMON_STRING_CONVERT_H__

#include <string>
#include <sstream>

bool UnicodeToAnsi(const std::wstring& src, std::string& result);
bool AnsiToUnicode(const std::string& src, std::wstring& result);
bool UnicodeToUtf8(const std::wstring& src, std::string& result);
bool Utf8ToUnicode(const std::string& src, std::wstring& result);
bool AnsiToUtf8(const std::string& src, std::string& result);
bool Utf8ToAnsi(const std::string& src, std::string& result);

// Easy Interface
std::string Easy_UnicodeToAnsi(const std::wstring& src);
std::wstring Easy_AnsiToUnicode(const std::string& src);
std::string Easy_UnicodeToUtf8(const std::wstring& src);
std::wstring Easy_Utf8ToUnicode(const std::string& src);
std::string Easy_AnsiToUtf8(const std::string& src);
std::string Easy_Utf8ToAnsi(const std::string& src);

// transform
std::wstring ToLower(const std::wstring& str);
std::wstring ToUpper(const std::wstring& str);

std::string ToLower(const std::string& str);
std::string ToUpper(const std::string& str);

std::string UrlEncode(const std::string& str);
std::string UrlDecode(const std::string& str);

// wrapper
inline std::wstring a2w(const std::string& src) { return Easy_AnsiToUnicode(src); }
inline std::wstring u2w(const std::string& src) { return Easy_Utf8ToUnicode(src); }

inline std::string w2a(const std::wstring& src) { return Easy_UnicodeToAnsi(src); }
inline std::string u2a(const std::string& src) { return Easy_Utf8ToAnsi(src); }

inline std::string w2u(const std::wstring& src) { return Easy_UnicodeToUtf8(src); }
inline std::string a2u(const std::string& src) { return Easy_AnsiToUtf8(src); }

#endif