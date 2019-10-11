// rstring
//
// A fast, reference counted, copy-on-write string class (c) Espen Harlinn
// https://www.codeproject.com/Articles/498251/A-Cplusplus-String-class

#include "rstring.hpp"
#include "routine.hpp"

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

rstring::rstring (LPCWSTR text) : data_ (nullptr)
{
	if (!_r_str_isempty (text))
	{
		const size_t length = _r_str_length (text);

		ReallocateUnique (length);

		if (data_)
			wmemcpy (data_, text, length);
	}
}

rstring::rstring (LPCWSTR text, size_t length) : data_ (nullptr)
{
	if (length)
	{
		ReallocateUnique (length);

		if (!_r_str_isempty (text))
		{
			if (data_)
				wmemcpy (data_, text, length);
		}
		else
		{
			if (data_)
				wmemset (data_, 0, length);
		}
	}
}

rstring::~rstring ()
{
	if (data_)
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

rstring& rstring::operator= (const rstring& other)
{
	AddRef (other);
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

rstring& rstring::operator= (LPCWSTR text)
{
	if (_r_str_isempty (text))
	{
		Release ();
	}
	else
	{
		if (text != data_)
		{
			Buffer* thisBuffer = toBuffer ();

			if (thisBuffer && text >= data_ && text < data_ + thisBuffer->length)
			{
				const size_t offset = text - data_;
				const size_t length = thisBuffer->length - offset;

				EnsureUnique ();
				text = data_ + offset;
				wmemmove (data_, text, length);
				ReallocateUnique (length);
			}
			else
			{
				const size_t length = _r_str_length (text);
				ReallocateUnique (length);
				wmemcpy (data_, text, length);
			}
		}
	}

	return *this;
}

WCHAR& rstring::At (size_t index) const
{
	Buffer* buffer = toBuffer ();

	if (index >= buffer->length)
		index = buffer->length;

	return buffer->data[index];
}

bool rstring::AsBool () const
{
	if (IsEmpty ())
		return false;

	if (AsInt () > 0 || _r_str_compare (GetString (), L"true") == 0)
		return true;

	return false;
}

INT rstring::AsInt (INT radix) const
{
	return (INT)AsLong (radix);
}

double rstring::AsDouble () const
{
	if (IsEmpty ())
		return 0.F;

	return wcstod (GetString (), nullptr);
}

LONG rstring::AsLong (INT radix) const
{
	if (IsEmpty ())
		return 0L;

	return wcstol (GetString (), nullptr, radix);
}

LONG64 rstring::AsLonglong (INT radix) const
{
	if (IsEmpty ())
		return 0LL;

	return wcstoll (GetString (), nullptr, radix);
}

size_t rstring::AsSizeT (INT radix) const
{
#ifdef _WIN64
	return AsUlonglong (radix);
#else
	return AsUint (radix);
#endif
}

UINT rstring::AsUint (INT radix) const
{
	return (UINT)AsUlong (radix);
}

ULONG rstring::AsUlong (INT radix) const
{
	if (IsEmpty ())
		return 0UL;

	return wcstoul (GetString (), nullptr, radix);
}

ULONG64 rstring::AsUlonglong (INT radix) const
{
	if (IsEmpty ())
		return 0ULL;

	return wcstoull (GetString (), nullptr, radix);
}

bool rstring::IsEmpty () const
{
	return GetLength () == 0;
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
			const size_t oldLength = thisBuffer->length;

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

rstring& rstring::Append (LPCWSTR text)
{
	if (_r_str_isempty (text))
		return *this;

	size_t length = _r_str_length (text);
	size_t oldLength = GetLength ();

	Buffer* thisBuffer = toBuffer ();

	if (!thisBuffer)
	{
		if (length)
		{
			ReallocateUnique (length);
			wmemcpy (data_, text, length);
		}
	}
	else if ((text == data_) && (length >= oldLength))
	{
		thisBuffer = ReallocateUnique (oldLength + oldLength);
		wmemmove (&thisBuffer->data[oldLength], thisBuffer->data, oldLength);
	}
	else if (text > data_ && text < (data_ + oldLength))
	{
		size_t offset = text - data_;

		if (length > (oldLength - offset))
			length = oldLength - offset;

		thisBuffer = ReallocateUnique (oldLength + length);
		text = data_ + offset;
		wmemmove (&thisBuffer->data[oldLength], text, length);
	}
	else
	{
		thisBuffer = ReallocateUnique (oldLength + length);
		wmemmove (&thisBuffer->data[oldLength], text, length);
	}

	return *this;
}

rstring& rstring::AppendFormat (LPCWSTR text, ...)
{
	rstring p;

	va_list args;
	va_start (args, text);
	p.FormatV (text, args);
	va_end (args);

	return Append (p);
}

rstring& rstring::Insert (size_t position, LPCWSTR text)
{
	if (_r_str_isempty (text))
		return *this;

	size_t length = _r_str_length (text);

	if (length)
	{
		Buffer* thisBuffer = toBuffer ();
		if (thisBuffer)
		{
			if (position >= thisBuffer->length)
			{
				Append (text);
			}
			else
			{
				if ((text >= data_) && (text < (data_ + thisBuffer->length)))
				{
					const size_t offset = text - data_;

					if (length > (thisBuffer->length - offset))
						length = thisBuffer->length - offset;

					rstring s (text, length);
					const size_t oldLength = thisBuffer->length;

					thisBuffer = ReallocateUnique (oldLength + length);
					wmemmove (&thisBuffer->data[position + length], &thisBuffer->data[position], oldLength - position);
					wmemmove (&thisBuffer->data[position], s.GetBuffer (), length);
					s.Release ();
				}
				else
				{
					const size_t oldLength = thisBuffer->length;
					thisBuffer = ReallocateUnique (oldLength + length);
					wmemmove (&thisBuffer->data[position + length], &thisBuffer->data[position], oldLength - position);
					wmemcpy (&thisBuffer->data[position], text, length);
				}
			}
		}
		else
		{
			thisBuffer = allocate (length);
			wmemmove (thisBuffer->data, text, length);
		}
	}
	return *this;
}

rstring& rstring::InsertFormat (size_t position, LPCWSTR text, ...)
{
	rstring p;

	va_list args;
	va_start (args, text);
	p.FormatV (text, args);
	va_end (args);

	return Insert (position, p);
}

rstring & rstring::Replace (LPCWSTR from, LPCWSTR to)
{
	if (IsEmpty ())
		return *this;

	LPWSTR tok;
	LPWSTR newstr;
	LPWSTR oldstr;
	LPWSTR head;

	/* if either substr or replacement is NULL, duplicate string a let caller handle it */
	if (from == nullptr || to == nullptr) return *this;
	newstr = _wcsdup (GetString ());
	if (newstr == nullptr) return *this;
	head = newstr;
	size_t from_len = _r_str_length (from);
	size_t to_len = _r_str_length (to);

	while ((tok = wcsstr (head, from)) != nullptr)
	{
		size_t orig_l = _r_str_length (newstr);
		oldstr = newstr;
		newstr = (LPWSTR)malloc ((orig_l - from_len + to_len + 1) * sizeof (WCHAR));

		/*failed to alloc mem, free old string and return NULL */
		if (!newstr)
		{
			free (oldstr);
			return *this;
		}
		wmemcpy (newstr, oldstr, (tok - oldstr));
		wmemcpy (newstr + (tok - oldstr), to, to_len);
		wmemcpy (newstr + (tok - oldstr) + to_len, tok + from_len, (orig_l - from_len - (tok - oldstr)));
		wmemset (newstr + orig_l - from_len + to_len, 0, 1);

		/* move back head right after the last replacement */
		head = newstr + (tok - oldstr) + to_len;
		free (oldstr);
	}

	*this = newstr;

	free (newstr);

	return *this;
}

rstring& rstring::Format (LPCWSTR text, ...)
{
	if (_r_str_isempty (text))
		return *this;

	va_list args;
	va_start (args, text);
	FormatV (text, args);
	va_end (args);

	return *this;
}

rstring& rstring::FormatV (LPCWSTR text, va_list args)
{
	if (_r_str_isempty (text))
		return *this;

	INT length = _vscwprintf (text, args);

	if (!length || length == INVALID_INT)
		return *this;

	Buffer* thisBuffer = ReallocateUnique (length);

	if (thisBuffer)
		_r_str_vprintf (thisBuffer->data, thisBuffer->length + 1, text, args);

	return *this;
}

size_t rstring::Hash () const
{
	return _r_str_hash (GetString ());
}

LPWSTR rstring::GetBuffer (size_t length)
{
	EnsureUnique ();

	if (length)
	{
		Release ();
		SetLength (length);
	}

	return data_;
}

size_t rstring::GetLength () const
{
	Buffer* buffer = toBuffer ();

	if (buffer)
		return buffer->length;

	return 0;
}

LPCWSTR rstring::GetString () const
{
	if (!data_)
		return nullptr;

	return data_;
}

void rstring::ReleaseBuffer ()
{
	const size_t length = _r_str_length (data_);

	if (!length)
	{
		Release ();
		return;
	}

	SetLength (length);
}

rstring& rstring::SetLength (size_t length)
{
	Buffer* buffer = toBuffer ();

	if (buffer && buffer->length != length)
	{
		ReallocateUnique (length);
	}
	else if (length)
	{
		ReallocateUnique (length);
	}

	return *this;
}

rstring::Buffer* rstring::toBuffer () const
{
	if (data_)
		return (Buffer*)(((LPBYTE)data_) - offsetof (Buffer, data));

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

rstring& rstring::Release ()
{
	Buffer* buffer = toBuffer ();

	if (buffer)
	{
		const LONG64 rc = InterlockedDecrement64 (&buffer->referenceCount);

		if (!rc)
			SAFE_DELETE_ARRAY (buffer);

		data_ = nullptr;
	}

	return *this;
}

size_t rstring::allocationByteCount (size_t length)
{
	if (length)
	{
		if (length < minReservedCharacters)
			return minBufferByteCount;

		size_t bytesRequired = (nonTextBufferByteCount + length) * sizeof (WCHAR);
		const size_t remainder = bytesRequired % minBufferByteCount;

		if (remainder)
			bytesRequired += minBufferByteCount - remainder;

		return bytesRequired;
	}

	return 0;
}

rstring::Buffer* rstring::allocate (size_t length)
{
	if (length)
	{
		const size_t byteCount = allocationByteCount (length);
		Buffer* result = reinterpret_cast<Buffer*>(new BYTE[byteCount]);

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
			const size_t byteCount = allocationByteCount (buffer->length);

			Buffer* newBuffer = reinterpret_cast<Buffer*>(new BYTE[byteCount]);

			newBuffer->referenceCount = 1;
			newBuffer->length = buffer->length;
			wmemcpy (newBuffer->data, buffer->data, buffer->length);
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
		const size_t currentByteCount = allocationByteCount (buffer->length);
		const size_t newByteCount = allocationByteCount (length);

		if (!newByteCount)
		{
			Release ();
			return nullptr;
		}
		else
		{
			if ((buffer->referenceCount > 1) || (currentByteCount != newByteCount))
			{
				Buffer* newBuffer = reinterpret_cast<Buffer*>(new BYTE[newByteCount]);
				newBuffer->referenceCount = 1;
				newBuffer->length = length;
				const size_t copyCount = min (buffer->length, length) + sizeof (WCHAR);
				wmemcpy (newBuffer->data, buffer->data, copyCount);
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
