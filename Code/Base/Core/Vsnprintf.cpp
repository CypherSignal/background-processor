#include "Pch.h"

#include <EABase\eabase.h>

int Vsnprintf8(char8_t* pDestination, size_t n, const char8_t* pFormat, va_list arguments)
{
	return _vsnprintf_s(pDestination, n, _TRUNCATE, pFormat, arguments);
}

int Vsnprintf16(char16_t* pDestination, size_t n, const char16_t* pFormat, va_list arguments)
{
	return _vsnwprintf_s((wchar_t*)pDestination, n, _TRUNCATE, (wchar_t*)pFormat, arguments);
}