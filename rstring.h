// rstring
// A fast, reference counted, copy-on-write string class (c) Espen Harlinn

// lastmod: Jan 17, 2016

#pragma once

#include <windows.h>

#include <unordered_map>
#include <vector>

#include "rconfig.h"

#pragma pack(push, 8)

class rstring
{

public:

	static const size_t npos = MAXDWORD64;

	struct hash { size_t operator() (const rstring& k) const { return k.Hash (); } };
	struct is_equal { bool operator() (const rstring& lhs, const rstring& rhs) const { return lhs.CompareNoCase (rhs) == 0; } };

	typedef std::vector<rstring> vector;

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

	DWORD AsInt (INT radix = 10) const;
	vector AsVector (LPCWSTR delimiters) const;

	rstring Appended (LPCWSTR str) const;
	rstring Appended (const rstring& other) const;

	rstring& Append (const rstring& str);
	rstring& Append (LPCWSTR str);
	rstring Mid (size_t start, size_t length = npos) const;
	rstring& Replace (LPCWSTR from, LPCWSTR to);
	rstring& Trim (LPCWSTR chars);

	rstring& Format (LPCWSTR fmt, ...);
	rstring& FormatV (LPCWSTR fmt, va_list args);

	INT Compare (const rstring& other) const;
	INT Compare (LPCWSTR str) const;
	INT CompareNoCase (const rstring& other) const;
	INT CompareNoCase (LPCWSTR str) const;

	size_t Hash () const;

	rstring& Clear ();
	bool IsEmpty () const;

	LPWSTR GetBuffer (size_t newLength = 0);
	LPCWSTR GetString () const;
	size_t GetLength () const;

	rstring& SetLength (size_t newLength);
	void ReleaseBuffer ();

	size_t Find (WCHAR chr, size_t start_pos = 0) const;
	size_t Find (LPCWSTR chars, size_t start_pos = 0) const;

	size_t ReverseFind (WCHAR chr, size_t start_pos = npos) const;
	size_t ReverseFind (LPCWSTR chars, size_t start_pos = npos) const;

	bool Match (LPCWSTR pattern);
	bool Match (LPCWSTR str, LPCWSTR pattern);

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
};

#pragma pack(pop)
