// rstring
//
// A fast, reference counted, copy-on-write string class (c) Espen Harlinn
// http://www.codeproject.com/Articles/498251/A-Cplusplus-String-class

#include "rstring.hpp"
#include "routine.hpp"

LPWSTR rstring::empty = L"";

#pragma warning(push)
#pragma warning(disable: 4309)
const size_t rstring::npos = MAXDWORD64;
#pragma warning(pop)

rstring::rstring () : data_ (nullptr)
{}

rstring::rstring (const rstring& other) : data_ (nullptr)
{
	if (other.data_)
	{
		data_ = other.data_;
		Buffer* buffer = toBuffer ();
		InterlockedIncrement64 (&buffer->referenceCount);
	}
}

rstring::rstring (rstring&& other) : data_ (nullptr)
{
	data_ = other.data_;
	other.data_ = nullptr;
}

rstring::rstring (LPCWSTR str) : data_ (nullptr)
{
	if (str)
	{
		size_t length = wcslen (str);
		if (length)
		{
			ReallocateUnique (length);
			wmemcpy_s (data_, length, str, length);
		}
	}
}

rstring::rstring (LPCSTR str) : data_ (nullptr)
{
	size_t length = strlen (str);
	if (length)
	{
		ReallocateUnique (length);
		MultiByteToWideChar (CP_ACP, 0, str, static_cast<int>(length), data_, static_cast<int>(length));
	}
}

rstring::rstring (LPCWSTR str1, size_t length1, LPCWSTR str2, size_t length2) : data_ (nullptr)
{
	size_t totalLength = length1 + length2;
	if (totalLength)
	{
		ReallocateUnique (totalLength);
		if (str1)
		{
			wmemcpy (data_, str1, length1);
		}
		else
		{
			wmemset (data_, 0, length1);
		}

		if (str2)
		{
			wmemcpy (&data_[length1], str2, length2);
		}
		else
		{
			wmemset (data_ + length1, 0, length2);
		}
	}
}

rstring::rstring (LPCWSTR str, size_t length) : data_ (nullptr)
{
	if (length)
	{
		ReallocateUnique (length);
		if (str)
		{
			wmemcpy (data_, str, length);
		}
		else
		{
			wmemset (data_, 0, length);
		}
	}
}

rstring::~rstring ()
{
	Release ();
}

rstring::operator LPCWSTR () const
{
	return GetString ();
}

rstring::operator bool () const
{
	return GetLength () != 0;
}

bool rstring::operator== (const rstring& str) const
{
	return Compare (str) == 0;
}

bool rstring::operator!= (const rstring& str) const
{
	return Compare (str) != 0;
}

bool rstring::operator== (LPCWSTR str) const
{
	return Compare (str) == 0;
}

bool rstring::operator!= (LPCWSTR str) const
{
	return Compare (str) != 0;
}

bool rstring::operator<= (const rstring& str) const
{
	return Compare (str) <= 0;
}

bool rstring::operator>= (const rstring& str) const
{
	return Compare (str) >= 0;
}

bool rstring::operator<= (LPCWSTR str) const
{
	return Compare (str) <= 0;
}

bool rstring::operator>= (LPCWSTR str) const
{
	return Compare (str) >= 0;
}

bool rstring::operator< (const rstring& str) const
{
	return Compare (str) < 0;
}

bool rstring::operator> (const rstring& str) const
{
	return Compare (str) > 0;
}

bool rstring::operator< (LPCWSTR str) const
{
	return Compare (str) < 0;
}

bool rstring::operator> (LPCWSTR str) const
{
	return Compare (str) > 0;
}

WCHAR rstring::operator[] (const size_t index) const
{
	return At (index);
}

WCHAR& rstring::operator[] (const size_t index)
{
	return At (index);
}

rstring& rstring::operator= (const rstring& other)
{
	AddRef (other);
	return *this;
}

rstring& rstring::operator= (LPCWSTR str)
{
	Release ();
	if (str != data_)
	{
		Buffer* thisBuffer = toBuffer ();
		if (thisBuffer && str >= data_ && str < data_ + thisBuffer->length)
		{
			size_t offset = str - data_;
			size_t length = thisBuffer->length - offset;

			EnsureUnique ();
			str = data_ + offset;
			memmove (data_, str, length * sizeof (WCHAR));
			ReallocateUnique (length);
		}
		else
		{
			size_t length = wcslen (str);
			thisBuffer = ReallocateUnique (length);
			wmemcpy (data_, str, length);
		}
	}
	return *this;
}

rstring& rstring::operator= (rstring&& other)
{
	if (this != &other)
	{
		data_ = other.data_;
		other.data_ = nullptr;
	}
	return *this;
}

rstring& rstring::operator+= (const rstring& other)
{
	return Append (other);
}

rstring& rstring::operator+= (LPCWSTR str)
{
	return Append (str);
}

rstring operator+ (const rstring& str1, const rstring& str2)
{
	return str1.Appended (str2);
}

rstring operator+ (const rstring& str1, LPCWSTR str2)
{
	return str1.Appended (str2);
}

WCHAR& rstring::At (size_t index) const
{
	Buffer* theBuffer = toBuffer ();

	return theBuffer->data[min (index, theBuffer->length)];
}

bool rstring::AsBool () const
{
	if (IsEmpty ())
		return false;

	if (AsInt () > 0 || CompareNoCase (L"true") == 0)
		return true;

	return false;
}

INT rstring::AsInt (INT radix) const
{
	return static_cast<INT>(AsLong (radix));
}

DOUBLE rstring::AsDouble () const
{
	if (IsEmpty ())
		return 0;

	return wcstod (GetString (), nullptr);
}

LONG rstring::AsLong (INT radix) const
{
	if (IsEmpty ())
		return 0;

	return wcstol (GetString (), nullptr, radix);
}

LONGLONG rstring::AsLonglong (INT radix) const
{
	if (IsEmpty ())
		return 0;

	return wcstoll (GetString (), nullptr, radix);
}

size_t rstring::AsSizeT (INT radix) const
{
	if (IsEmpty ())
		return 0;

#ifdef _WIN64
	return wcstoull (GetString (), nullptr, radix);
#else
	return static_cast<size_t>(AsUlong (radix));
#endif
}

UINT rstring::AsUint (INT radix) const
{
	return static_cast<INT>(AsUlong (radix));
}

ULONG rstring::AsUlong (INT radix) const
{
	if (IsEmpty ())
		return 0;

	return wcstoul (GetString (), nullptr, radix);
}

ULONGLONG rstring::AsUlonglong (INT radix) const
{
	if (IsEmpty ())
		return 0;

	return wcstoull (GetString (), nullptr, radix);
}

rstring::rvector rstring::AsVector (LPCWSTR delimiters) const
{
	rstring::rvector result;
	size_t theSize = GetLength ();
	if (theSize)
	{
		LPCWSTR start = data_;
		LPCWSTR end = start;
		LPCWSTR thisEnd = start + theSize;
		while (end < thisEnd)
		{
			LPCWSTR ptr = delimiters;
			while (*ptr)
			{
				if (*end == *ptr)
				{
					if (start < end)
					{
						rstring item_s = start;
						size_t item_l = end - start;
						result.emplace_back (item_s.SetLength (item_l).Trim (L" \r\n"), item_l);
					}
					start = end + 1;
					break;
				}
				ptr++;
			}
			end++;
		}
		if (start < end)
		{
			result.emplace_back (start, size_t (end - start));
		}
	}
	return std::move (result);
}

bool rstring::IsEmpty () const
{
	return GetLength () == 0;
}

bool rstring::IsNumeric () const
{
	if (IsEmpty ())
		return false;

	for (size_t i = 0; i < GetLength (); i++)
	{
		if (iswdigit (data_[i]) == 0)
			return false;
	}

	return true;
}

rstring& rstring::Append (const rstring& other)
{
	Buffer* otherBuffer = other.toBuffer ();
	if (otherBuffer && otherBuffer->length)
	{
		Buffer* thisBuffer = toBuffer ();
		if (!thisBuffer)
		{
			data_ = otherBuffer->data;
			InterlockedIncrement64 (&otherBuffer->referenceCount);
		}
		else
		{
			size_t oldLength = thisBuffer->length;
			if (thisBuffer == otherBuffer)
			{
				thisBuffer = ReallocateUnique (thisBuffer->length + thisBuffer->length);
				wmemcpy (&thisBuffer->data[oldLength], thisBuffer->data, oldLength);
			}
			else
			{
				thisBuffer = ReallocateUnique (thisBuffer->length + otherBuffer->length);
				wmemcpy (&thisBuffer->data[oldLength], otherBuffer->data, otherBuffer->length);
			}
		}
	}
	return *this;
}

rstring&rstring::Append (LPCWSTR str)
{
	if (str && str[0])
	{
		Buffer* thisBuffer = toBuffer ();
		if (!thisBuffer)
		{
			size_t length = wcslen (str);
			if (length)
			{
				ReallocateUnique (length);
				wmemmove_s (data_, length, str, length);
			}
		}
		else if (str == data_)
		{
			size_t oldLength = thisBuffer->length;

			thisBuffer = ReallocateUnique (thisBuffer->length + thisBuffer->length);
			wmemmove_s (&thisBuffer->data[oldLength], oldLength, thisBuffer->data, oldLength);
		}
		else if (str > data_ && str < (data_ + thisBuffer->length))
		{
			size_t offset = str - data_;
			size_t length = thisBuffer->length - offset;
			size_t oldLength = thisBuffer->length;
			thisBuffer = ReallocateUnique (thisBuffer->length + length);
			str = data_ + offset;
			wmemmove_s (&thisBuffer->data[oldLength], length, str, length);
		}
		else
		{
			size_t length = wcslen (str);
			size_t oldLength = thisBuffer->length;
			thisBuffer = ReallocateUnique (oldLength + length);
			wmemmove_s (&thisBuffer->data[oldLength], length, str, length);
		}
	}
	return *this;
}

rstring& rstring::Insert (LPCWSTR text, size_t pos)
{
	size_t textLength = wcslen (text);
	if (text && textLength)
	{
		Buffer* thisBuffer = toBuffer ();
		if (thisBuffer)
		{
			if (pos >= thisBuffer->length)
			{
				Append (text);
			}
			else
			{
				if ((text >= data_) && (text < (data_ + thisBuffer->length)))
				{
					size_t offset = text - data_;
					if (textLength > (thisBuffer->length - offset))
					{
						textLength = thisBuffer->length - offset;
					}
					rstring s (text, textLength);
					size_t oldLength = thisBuffer->length;
					thisBuffer = ReallocateUnique (oldLength + textLength);
					memmove (&thisBuffer->data[pos + textLength], &thisBuffer->data[pos], (oldLength - pos) * sizeof (WCHAR));
					memmove (&thisBuffer->data[pos], s.GetBuffer (), textLength * sizeof (WCHAR));
					s.Release ();
				}
				else
				{
					size_t oldLength = thisBuffer->length;
					thisBuffer = ReallocateUnique (oldLength + textLength);
					memmove (&thisBuffer->data[pos + textLength], &thisBuffer->data[pos], (oldLength - pos) * sizeof (WCHAR));
					memcpy (&thisBuffer->data[pos], text, textLength * sizeof (WCHAR));
				}
			}
		}
		else
		{
			thisBuffer = allocate (textLength);
			memmove (thisBuffer->data, text, textLength * sizeof (WCHAR));
		}
	}
	return *this;
}

rstring& rstring::Mid (size_t start, size_t length)
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer)
	{
		if ((start == 0) && (length >= thisBuffer->length))
		{
			return *this;
		}
		if (start >= thisBuffer->length)
		{
			Release ();
			return *this;
		}
		if (length > (thisBuffer->length - start))
		{
			length = thisBuffer->length - start;
		}

		thisBuffer = EnsureUnique ();

		memmove (thisBuffer->data, &thisBuffer->data[start], (length * sizeof (WCHAR)));

		ReallocateUnique (length);
	}
	return *this;
}

rstring& rstring::Remove (size_t start, size_t length)
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer)
	{
		if ((start == 0) && (length >= thisBuffer->length))
		{
			Release ();
		}
		else if (start < thisBuffer->length)
		{
			if (length >= thisBuffer->length - start)
			{
				ReallocateUnique (start);
			}
			else
			{
				thisBuffer = EnsureUnique ();

				memmove (&thisBuffer->data[start], &thisBuffer->data[start + length], (thisBuffer->length - (start + length)) * sizeof (WCHAR));

				ReallocateUnique (thisBuffer->length - length);
			}
		}
	}
	return *this;
}

rstring& rstring::Replace (LPCWSTR from, LPCWSTR to)
{
	if (IsEmpty ())
		return *this;

	LPWSTR tok = nullptr;
	LPWSTR newstr = nullptr;
	LPWSTR oldstr = nullptr;
	LPWSTR head = nullptr;

	/* if either substr or replacement is NULL, duplicate string a let caller handle it */
	if (from == nullptr || to == nullptr) return *this;
	newstr = _wcsdup (GetString ());
	head = newstr;
	size_t from_len = wcslen (from);
	size_t to_len = wcslen (to);
	while ((tok = wcsstr (head, from)) != nullptr)
	{
		size_t orig_l = wcslen (newstr);
		oldstr = newstr;
		newstr = (LPWSTR)malloc ((orig_l - from_len + to_len + 1) * sizeof (WCHAR));
		/*failed to alloc mem, free old string and return NULL */
		if (newstr == NULL)
		{
			free (oldstr);
			return *this;
		}
		memcpy (newstr, oldstr, (tok - oldstr) * sizeof (WCHAR));
		memcpy (newstr + (tok - oldstr), to, to_len * sizeof (WCHAR));
		memcpy (newstr + (tok - oldstr) + to_len, tok + from_len, (orig_l - from_len - (tok - oldstr)) * sizeof (WCHAR));
		memset (newstr + orig_l - from_len + to_len, 0, 1 * sizeof (WCHAR));
		/* move back head right after the last replacement */
		head = newstr + (tok - oldstr) + to_len;
		free (oldstr);
	}

	*this = newstr;

	free (newstr);

	return *this;
}

rstring& rstring::Trim (LPCWSTR chars)
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer && chars)
	{
		size_t chars_len = wcslen (chars);
		for (size_t i = 0; i < thisBuffer->length; i++)
		{
			LPCWSTR res = wmemichr (chars, thisBuffer->data[i], chars_len);
			if (!res)
			{
				size_t last = thisBuffer->length - 1;
				do
				{
					res = wmemichr (chars, thisBuffer->data[last], chars_len);
					if (!res)
					{
						last++;
						Mid (i, last - i);
						break;
					}
				}
				while (last--);

				return *this;
			}
		}
		Release ();
	}
	return *this;
}

rstring& rstring::Format (LPCWSTR str, ...)
{
	va_list args;
	va_start (args, str);
	FormatV (str, args);
	va_end (args);
	return *this;
}

rstring& rstring::FormatV (LPCWSTR fmt, va_list args)
{
	size_t length = _vscwprintf (fmt, args);
	Buffer* thisBuffer = ReallocateUnique (length);

	if (thisBuffer)
		StringCchVPrintf (thisBuffer->data, thisBuffer->length + 1, fmt, args);

	return *this;
}

rstring rstring::Appended (const rstring& other) const
{
	Buffer* thisBuffer = toBuffer ();
	Buffer* otherBuffer = other.toBuffer ();
	if (thisBuffer && otherBuffer)
	{
		rstring result (thisBuffer->data, thisBuffer->length, otherBuffer->data, otherBuffer->length);
		return result;
	}
	else if (thisBuffer)
	{
		rstring result (*this);
		return result;
	}
	return other;
}

rstring rstring::Appended (LPCWSTR str) const
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer && str && str[0])
	{
		rstring result (thisBuffer->data, thisBuffer->length, str, wcslen (str));
		return result;
	}
	else if (thisBuffer)
	{
		rstring result (*this);
		return result;
	}
	else if (str && str[0])
	{
		return rstring (str);
	}
	return rstring ();
}

rstring rstring::Midded (size_t start, size_t length) const
{
	auto tmp = *this;
	tmp.Mid (start, length);
	return tmp;
}

rstring rstring::Replaced (LPCWSTR from, LPCWSTR to) const
{
	auto tmp = *this;
	tmp.Replace (from, to);
	return tmp;
}

INT rstring::Compare (const rstring& str) const
{
	return _Compare (toBuffer (), str.toBuffer ());
}

INT rstring::Compare (LPCWSTR str) const
{
	return _Compare (toBuffer (), str);
}

INT rstring::CompareNoCase (const rstring& str) const
{
	return _CompareI (toBuffer (), str.toBuffer ());
}

INT rstring::CompareNoCase (LPCWSTR str) const
{
	return _CompareI (toBuffer (), str);
}

size_t rstring::Hash () const
{
	if (!GetLength ())
		return 0;

	return _r_str_hash (GetString ());
}

LPWSTR rstring::GetBuffer (size_t newLength)
{
	EnsureUnique ();
	if (newLength)
	{
		Release ();
		SetLength (newLength);
	}
	else if (!data_)
	{
		return empty;
	}
	return data_;
}

size_t rstring::GetLength () const
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		return buffer->length;
	}
	return 0;
}

LPCWSTR rstring::GetString () const
{
	if (!data_)
	{
		return empty;
	}
	return data_;
}

rstring& rstring::Clear ()
{
	Release ();
	return *this;
}

void rstring::ReleaseBuffer ()
{
	size_t length = wcslen (data_);
	if (!length)
	{
		Release ();
	}
	else
	{
		SetLength (length);
	}
}

rstring& rstring::SetLength (size_t newLength)
{
	ReallocateUnique (newLength);
	return *this;
}

size_t rstring::Find (WCHAR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer)
	{
		for (size_t i = start_pos; i <= GetLength (); i++)
		{
			if (towupper (thisBuffer->data[i]) == towupper (chars))
			{
				return i;
			}
		}
	}

	return npos;
}

size_t rstring::Find (LPCWSTR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer && chars)
	{
		size_t len = wcslen (chars);
		while (start_pos < thisBuffer->length)
		{
			WCHAR c = towupper (thisBuffer->data[start_pos]);
			size_t i = 0;
			for (; i < len; i++)
			{
				WCHAR sc = towupper (chars[i]);
				if (c == sc)
				{
					break;
				}
			}
			if (i == len)
			{
				return start_pos;
			}
			start_pos++;
		}
	}
	return npos;
}

size_t rstring::ReverseFind (WCHAR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer)
	{
		if (start_pos >= thisBuffer->length)
		{
			start_pos = thisBuffer->length - 1;
		}
		do
		{
			if (towupper (thisBuffer->data[start_pos]) == towupper (chars))
			{
				return start_pos;
			}
		}
		while (start_pos--);
	}
	return npos;
}

size_t rstring::ReverseFind (LPCWSTR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	size_t searchStringLength = wcslen (chars);
	if (thisBuffer && (searchStringLength <= thisBuffer->length))
	{
		if (start_pos >= thisBuffer->length)
		{
			start_pos = thisBuffer->length - 1;
		}
		if (searchStringLength == 1)
		{
			start_pos = ReverseFind (*chars, start_pos);
		}
		else if (searchStringLength)
		{
			do
			{
				if (_wmemicmp (&thisBuffer->data[start_pos], chars, searchStringLength))
				{
					return start_pos;
				}
			}
			while (start_pos--);
		}
	}
	return npos;
}

bool rstring::Match (LPCWSTR pattern) const
{
	return Match (data_, pattern);
}

bool rstring::Match (LPCWSTR str, LPCWSTR pattern) const
{
	// If we reach at the end of both strings, we are done
	if (*pattern == 0 && *str == 0)
	{
		return TRUE;
	}

	// Make sure that the characters after '*' are present
	// in second string. This function assumes that the first
	// string will not contain two consecutive '*'
	if (*pattern == L'*' && *(pattern + 1) != 0 && *str == 0)
	{
		return FALSE;
	}

	// If the first string contains '?', or current characters
	// of both strings match
	if (*pattern == L'?' || towlower (*str) == towlower (*pattern))
	{
		return Match (str + 1, pattern + 1);
	}

	// If there is *, then there are two possibilities
	// a) We consider current character of second string
	// b) We ignore current character of second string.
	if (*pattern == L'*')
	{
		return Match (str + 1, pattern) || Match (str, pattern + 1);
	}

	return FALSE;
}

rstring::Buffer* rstring::toBuffer () const
{
	if (data_)
	{
		return (Buffer*)(((char*)data_) - offsetof (Buffer, data));
	}
	return nullptr;
}

void rstring::AddRef (const rstring& other)
{
	if (other.data_ != data_)
	{
		Release ();
		if (other.data_)
		{
			data_ = other.data_;
			Buffer* buffer = toBuffer ();
			InterlockedIncrement64 (&buffer->referenceCount);
		}
	}
}

void rstring::Release ()
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		LONG64 res = InterlockedDecrement64 (&buffer->referenceCount);
		if (res == 0)
		{
			//char* bytes = (char*)buffer;
			delete[] buffer;
		}
		data_ = nullptr;
	}
}

size_t rstring::allocationByteCount (size_t length)
{
	if (length)
	{
		if (length < minReservedCharacters)
		{
			return minBufferByteCount;
		}

		size_t bytesRequired = (nonTextBufferByteCount + length) * sizeof (WCHAR);

		size_t remainder = bytesRequired % minBufferByteCount;
		if (remainder != 0)
		{
			bytesRequired += minBufferByteCount - remainder;
		}
		return bytesRequired;
	}
	return 0;
}

rstring::Buffer* rstring::allocate (size_t length)
{
	if (length)
	{
		size_t byteCount = allocationByteCount (length);
		Buffer* result = (Buffer*)new char[byteCount];
		result->referenceCount = 1;
		result->length = length;
		result->data[length] = 0;

		return result;
	}
	return nullptr;
}

rstring::Buffer* rstring::EnsureUnique ()
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		if (buffer->referenceCount > 1)
		{
			size_t byteCount = allocationByteCount (buffer->length);

			Buffer* newBuffer = (Buffer*)new char[byteCount];
			newBuffer->referenceCount = 1;
			newBuffer->length = buffer->length;
			memcpy (newBuffer->data, buffer->data, buffer->length * sizeof (WCHAR));
			newBuffer->data[buffer->length] = 0;
			Release ();
			data_ = newBuffer->data;
			return newBuffer;
		}
	}
	return buffer;
}

rstring::Buffer* rstring::ReallocateUnique (size_t length)
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		size_t currentByteCount = allocationByteCount (buffer->length);
		size_t newByteCount = allocationByteCount (length);
		if (!newByteCount)
		{
			Release ();
			return nullptr;
		}
		else
		{
			if ((buffer->referenceCount > 1) || (currentByteCount != newByteCount))
			{
				Buffer* newBuffer = (Buffer*)new char[newByteCount];
				newBuffer->referenceCount = 1;
				newBuffer->length = length;
				size_t copyCount = min (buffer->length, length) + 2;
				memcpy (newBuffer->data, buffer->data, copyCount * sizeof (WCHAR));
				newBuffer->data[length] = 0;
				Release ();
				data_ = newBuffer->data;
				return newBuffer;
			}
			buffer->length = length;
			buffer->data[length] = 0;
		}
	}
	else
	{
		buffer = allocate (length);
		data_ = buffer->data;
	}
	return buffer;
}

int rstring::_Compare (Buffer* buffer1, Buffer* buffer2)
{
	if (buffer1 == buffer2)
	{
		return 0;
	}
	if (buffer1 && buffer2)
	{
		size_t compareLength = min (buffer1->length, buffer2->length);
		for (size_t i = 0; i < compareLength; i++)
		{
			int res = int (buffer1->data[i]) - int (buffer2->data[i]);
			if (res != 0)
			{
				return res;
			}
		}
		if (buffer1->length > buffer2->length)
		{
			return 1;
		}
		if (buffer1->length < buffer2->length)
		{
			return -1;
		}
		return 0;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2)
	{
		return -1;
	}
	return 0;
}

int rstring::_Compare (Buffer* buffer1, LPCWSTR buffer2)
{
	if (buffer1 && buffer2 && buffer2[0] != 0)
	{
		size_t length = wcslen (buffer2);
		size_t compareLength = min (buffer1->length, length);
		for (size_t i = 0; i < compareLength; i++)
		{
			INT res = INT (buffer1->data[i]) - INT (buffer2[i]);

			if (res != 0)
			{
				return res;
			}
		}
		if (buffer1->length > length)
		{
			return 1;
		}
		if (buffer1->length < length)
		{
			return -1;
		}
		return 0;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2 && buffer2[0] != 0)
	{
		return -1;
	}
	return 0;
}

int rstring::_CompareI (Buffer* buffer1, Buffer* buffer2)
{
	if (buffer1 == buffer2)
	{
		return 0;
	}
	if (buffer1 && buffer2)
	{
		auto result = CompareString (LOCALE_USER_DEFAULT, NORM_IGNORECASE, buffer1->data, static_cast<int>(buffer1->length), buffer2->data, static_cast<int>(buffer2->length));

		return result - CSTR_EQUAL;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2)
	{
		return -1;
	}
	return 0;
}

int rstring::_CompareI (Buffer* buffer1, LPCWSTR buffer2)
{
	if (buffer1 && buffer2)
	{
		auto result = CompareString (LOCALE_USER_DEFAULT, NORM_IGNORECASE, buffer1->data, static_cast<int>(buffer1->length), buffer2, -1);

		return result - CSTR_EQUAL;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2)
	{
		return -1;
	}
	return 0;
}

LPCWSTR rstring::wmemichr (LPCWSTR buf, wint_t chr, size_t cnt)
{
	chr = towupper (chr);
	while (cnt && (towupper (*buf) != chr))
	{
		buf++;
		cnt--;
	}
	return cnt ? buf : nullptr;
}

BOOL rstring::_wmemicmp (LPCWSTR first, LPCWSTR second, size_t count)
{
	while (count)
	{
		if (towupper (*first) == (*second))
			return TRUE;
		first++;
		second++;
		count--;
	}
	return FALSE;
}
