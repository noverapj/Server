#include <cstdio>
#include <cstdarg>

extern "C" FILE* __cdecl __iob_func(void)
{
    static FILE _iob[] = { *stdin, *stdout, *stderr };
    return _iob;
}

extern "C" int __cdecl _vfprintf(FILE* stream, const char* format, va_list args)
{
    return vfprintf(stream, format, args);
}
