#pragma once
#include <stdint.h>
#include <string>
#include <iostream>

inline std::string HexValueOut(uint32_t nVal, bool bIncludeDecimal = true)
{
    char buf[64];
    if (bIncludeDecimal)
        sprintf_s(buf, "0x%08x (%ld)", nVal, nVal);
    else
        sprintf_s(buf, "0x%08x", nVal);
    return std::string(buf);
}

inline std::string HexValueOut(uint64_t nVal, bool bIncludeDecimal = true)
{
    char buf[64];
    if (bIncludeDecimal)
        sprintf_s(buf, "0x%016llx (%lld)", nVal, nVal);
    else
        sprintf_s(buf, "0x%016llx", nVal);
    return std::string(buf);
}

inline void DumpMemoryToCout(uint8_t* pBuf, uint32_t nBytes, uint32_t nBaseMemoryOffset = 0, uint32_t nColumns = 32)
{
    char byteToAscii[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    uint32_t nBytesOnLine = 0;
    uint8_t* pWalker = pBuf;
    std::string sAscii;
    while (pWalker < pBuf + nBytes)
    {
        if (nBytesOnLine % nColumns == 0)    
        {
            // if we've reached a line end and we've accumulated ascii text
            if (!sAscii.empty())
                std::cout << " |" << sAscii << "|";

            nBytesOnLine = 0;
            std::cout << "\n" << HexValueOut((uint32_t)(nBaseMemoryOffset + (pWalker - pBuf)), false) << ": ";
            sAscii.clear();
        }
        else if (nBytesOnLine % 4 == 0)  // extra space every 4 bytes
        {
            std::cout << " ";
        }

        uint8_t c = *pWalker;
        std::cout << byteToAscii[c >> 4] << byteToAscii[c & 0x0F] << " ";

        // Accumulate visible ascii characters or just substitute a space
        if (c > ' ' && c != 127)
            sAscii += c;
        else
            sAscii += '.';

        nBytesOnLine++;
        pWalker++;
    }

    // remainder of text
    if (!sAscii.empty())
    {
        // add spaces for any missing chars
        while (nBytesOnLine < nColumns)
        {
            sAscii += ' ';
            std::cout << "   ";
            if (nBytesOnLine % 4 == 0 && nBytesOnLine < nColumns)
                std::cout << " ";
            nBytesOnLine++;
        }
        std::cout << " |" << sAscii << "|\n";
    }
}