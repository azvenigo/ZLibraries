#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <cstdio>
#include <stdarg.h> 

class InlineFormatter
{
public:
    static std::string Format(const char *format, ...)
    {
        // az: used to have a dynamic buffer but that had race conditions. Perhaps someday take advantage for thread specific data to allocate buffers per thread.
        // So for now this is slow and creates/copies strings around for convenience
        va_list args;
        va_start(args, format);

        int32_t nRequiredLength = vsnprintf(nullptr, 0, format, args);
        char* pBuf = (char*)malloc(nRequiredLength + 1);
        vsnprintf(pBuf, nRequiredLength + 1, format, args);

        va_end(args);

        std::string sReturn(pBuf, nRequiredLength);
        free(pBuf);
        return sReturn;
    }
};

extern InlineFormatter gFormatter;





/*class InlineFormatter
{
public:
    static const uint32_t kInitialBufferLength = 1024;

    InlineFormatter() { mpBuf = (char*) malloc(kInitialBufferLength); mnBufLength = kInitialBufferLength; }
    ~InlineFormatter() { free(mpBuf); }

    char* Format(const char *format, ...)
    {
        va_list args;
        va_start(args, format);

        int32_t nRequiredLength = vsnprintf(mpBuf, mnBufLength, format, args);
        if (nRequiredLength > mnBufLength)
        {
            // static buffer's too small. Reallocate and print again.
            mpBuf = (char*) realloc((void*)mpBuf, nRequiredLength);
            mnBufLength = nRequiredLength;

            vsnprintf(mpBuf, mnBufLength, format, args);
        }

        va_end(args);

        return mpBuf;
    }

private:
    char*   mpBuf;
    int32_t mnBufLength;
};*/




extern InlineFormatter gFormatter;