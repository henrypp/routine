// rstring
//
// A fast, reference counted, copy-on-write string class (c) Espen Harlinn
// https://www.codeproject.com/Articles/498251/A-Cplusplus-String-class

#pragma once

#include <windows.h>

#include "rconfig.hpp"

size_t _r_str_hash (LPCWSTR text);
INT _r_str_compare (LPCWSTR str1, LPCWSTR str2, size_t length = INVALID_SIZE_T);

class rstring
{

private:

	LPWSTR data_ = nullptr;

	struct Buffer
	{
		volatile LONG64 referenceCount = 0;
		size_t length = 0;
		WCHAR buff[128];
	};


	Buffer* toBuffer () const;

	static size_t allocationByteCount (size_t length);
	static Buffer* allocate (size_t length);

	Buffer* EnsureUnique ();
	Buffer* ReallocateUnique (size_t length);

	void AddRef (const rstring& other);

public:

	rstring ();
	rstring (const rstring& other);
	rstring (rstring&& other);
	rstring (LPCWSTR text);
	rstring (LPCWSTR text, size_t length);

	~rstring ();

	operator LPCWSTR () const;
	explicit operator bool () const;

	rstring& operator= (const rstring& other);
	rstring& operator= (rstring&& other);
	rstring& operator= (LPCWSTR text);

	WCHAR At (size_t index) const;

	bool AsBool () const;
	INT AsInt (INT radix = 10) const;
	double AsDouble () const;
	LONG AsLong (INT radix = 10) const;
	LONG64 AsLonglong (INT radix = 10) const;
	size_t AsSizeT (INT radix = 10) const;
	UINT AsUint (INT radix = 10) const;
	ULONG AsUlong (INT radix = 10) const;
	ULONG64 AsUlonglong (INT radix = 10) const;

	bool IsEmpty () const;

	rstring& Append (const rstring& other);
	rstring& Append (LPCWSTR text);
	rstring& AppendFormat (LPCWSTR text, ...);
	rstring& Insert (size_t position, LPCWSTR text);
	rstring& InsertFormat (size_t position, LPCWSTR text, ...);
	rstring& rstring::Replace (LPCWSTR from, LPCWSTR to);

	rstring& Format (LPCWSTR text, ...);
	rstring& FormatV (LPCWSTR text, va_list args);

	LPWSTR GetBuffer (size_t length = 0);
	size_t GetLength () const;
	LPCWSTR GetString () const;

	rstring& SetLength (size_t length);
	rstring& ReleaseBuffer ();
	rstring& Release ();
};

struct rstring_hash
{
	size_t operator() (const rstring& k) const
	{
		return _r_str_hash (k);
	}
};

struct rstring_is_equal
{
	bool operator() (const rstring& lhs, const rstring& rhs) const
	{
		return _r_str_compare (lhs.GetString (), rhs.GetString ()) == 0;
	}
};
