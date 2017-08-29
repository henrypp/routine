// rstring
//
// A fast, reference counted, copy-on-write string class (c) Espen Harlinn
// http://www.codeproject.com/Articles/498251/A-Cplusplus-String-class

#pragma once

#include <windows.h>

#include <unordered_map>
#include <vector>

#include "rconfig.hpp"

#pragma pack(push, 8)

class rstring
{

public:

	static const size_t npos;

	struct hash { size_t operator() (const rstring& k) const { return k.Hash (); } };
	struct is_equal { bool operator() (const rstring& lhs, const rstring& rhs) const { return lhs.CompareNoCase (rhs) == 0; } };

	typedef std::vector<rstring> rvector;

	typedef std::unordered_map<rstring, rstring, rstring::hash, rstring::is_equal> map_one;
	typedef std::unordered_map<rstring, rstring::map_one, rstring::hash, rstring::is_equal> map_two;

	rstring ();
	rstring (const rstring& other);
	rstring (LPCWSTR str);
	rstring (LPCWSTR str, size_t length);
	rstring (rstring&& other);

	rstring (LPCSTR val);
	rstring (LPCWSTR str1, size_t length1, LPCWSTR str2, size_t length2);

	~rstring ();

	operator LPCWSTR () const;
	explicit operator bool () const;

	bool operator== (const rstring& str) const;
	bool operator!= (const rstring& str) const;

	bool operator== (LPCWSTR str) const;
	bool operator!= (LPCWSTR str) const;

	bool operator<= (const rstring& str) const;
	bool operator>= (const rstring& str) const;

	bool operator<= (LPCWSTR str) const;
	bool operator>= (LPCWSTR str) const;

	bool operator< (const rstring& str) const;
	bool operator> (const rstring& str) const;

	bool operator< (LPCWSTR str) const;
	bool operator> (LPCWSTR str) const;

	WCHAR operator[] (const size_t index) const;
	WCHAR& operator[] (const size_t index);

	rstring& operator= (const rstring& other);
	rstring& operator= (LPCWSTR str);
	rstring& operator= (rstring&& other);

	rstring& operator+= (const rstring& other);
	rstring& operator+= (LPCWSTR str);

	friend rstring operator+ (const rstring& str1, const rstring& str2);
	friend rstring operator+ (const rstring& str1, LPCWSTR str2);

	WCHAR& At (size_t index) const;

	bool AsBool () const;
	INT AsInt (INT radix = 10) const;
	DOUBLE AsDouble () const;
	LONG AsLong (INT radix = 10) const;
	LONGLONG AsLonglong (INT radix = 10) const;
	size_t AsSizeT (INT radix = 10) const;
	UINT AsUint (INT radix = 10) const;
	ULONG AsUlong (INT radix = 10) const;
	ULONGLONG AsUlonglong (INT radix = 10) const;
	rvector AsVector (LPCWSTR delimiters) const;

	bool IsEmpty () const;
	bool IsNumeric () const;

	rstring& Append (const rstring& str);
	rstring& Append (LPCWSTR str);
	rstring& Insert (LPCWSTR text, size_t pos);
	rstring& Mid (size_t start, size_t length = npos);
	rstring& Remove (size_t start, size_t length = npos);
	rstring& Replace (LPCWSTR from, LPCWSTR to);
	rstring& Trim (LPCWSTR chars);

	rstring& Format (LPCWSTR fmt, ...);
	rstring& FormatV (LPCWSTR fmt, va_list args);

	rstring Appended (LPCWSTR str) const;
	rstring Appended (const rstring& other) const;

	rstring Midded (size_t start, size_t length = npos) const;
	rstring Replaced (LPCWSTR from, LPCWSTR to) const;

	INT Compare (const rstring& other) const;
	INT Compare (LPCWSTR str) const;
	INT CompareNoCase (const rstring& other) const;
	INT CompareNoCase (LPCWSTR str) const;

	size_t Hash () const;

	LPWSTR GetBuffer (size_t newLength = 0);
	size_t GetLength () const;
	LPCWSTR GetString () const;

	rstring& Clear ();
	void ReleaseBuffer ();
	rstring& SetLength (size_t newLength);

	size_t Find (WCHAR chr, size_t start_pos = 0) const;
	size_t Find (LPCWSTR chars, size_t start_pos = 0) const;

	size_t ReverseFind (WCHAR chr, size_t start_pos = npos) const;
	size_t ReverseFind (LPCWSTR chars, size_t start_pos = npos) const;

	bool Match (LPCWSTR pattern) const;
	bool Match (LPCWSTR str, LPCWSTR pattern) const;

	rstring& ToLower ();

private:

	LPWSTR data_ = nullptr;
	static LPWSTR empty;

	struct Buffer
	{
		volatile LONGLONG referenceCount;
		size_t length;
		WCHAR data[128];
	};

	static const size_t nonTextBufferByteCount = offsetof (Buffer, data) + sizeof (WCHAR);
	static const size_t minBufferByteCount = 64;
	static const size_t minReservedCharacters = (minBufferByteCount - nonTextBufferByteCount) / sizeof (WCHAR);

	Buffer* toBuffer () const;

	static size_t allocationByteCount (size_t length);
	static Buffer* allocate (size_t length);

	Buffer* EnsureUnique ();
	Buffer* ReallocateUnique (size_t length);

	void AddRef (const rstring& other);
	void Release ();

	static int _Compare (Buffer* buffer1, Buffer* buffer2);
	static int _Compare (Buffer* buffer1, LPCWSTR buffer2);

	static int _CompareI (Buffer* buffer1, Buffer* buffer2);
	static int _CompareI (Buffer* buffer1, LPCWSTR buffer2);

	static LPCWSTR wmemichr (LPCWSTR buf, wint_t chr, size_t cnt);
	static BOOL _wmemicmp (LPCWSTR first, LPCWSTR second, size_t count);
};

#pragma pack(pop)
