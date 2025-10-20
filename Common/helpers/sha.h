#pragma once
#include <stdint.h>
#include <cstring>
#include <string>
#include <iostream>
#include <memory>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif

namespace SHA
{
    enum SHA_ALG : int32_t
    {
        UNKNOWN         = -1,
        ALG_SHA256      = 1,
        ALG_SHA3        = 2
    };

    extern int32_t gSHAAlg;

    typedef struct
    {
        uint8_t data[64];
        uint32_t datalen;
        uint64_t bitlen;
        uint32_t state[8];
    } SHA256_CTX;


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SHA3

    struct Sha3Context_t
    {
        union
        {
            uint8_t  b[200]; // sponge state in bytes
            uint64_t q[25];  // sponge state in qwords
        } state;

        uint8_t     blockIndex;
        uint8_t     blockSize;
        uint8_t     digestLen;
    };


    class SHAHash
    {
    public:
        SHAHash();
        SHAHash(const SHAHash& rhs);
        SHAHash(const uint8_t* pBuf, size_t length);
        SHAHash(const std::string& fromString);


        SHA_ALG         GetType() const { return mType; }

        void            Init();
        void            Compute(const uint8_t* pBuf, size_t length);
        void            Final();

        std::string     ToString() const;
        void            FromString(const std::string& fromString);

        // IO streaming
        friend std::ostream& operator<<(std::ostream& os, const SHAHash& hash);
        friend std::istream& operator>>(std::istream& is, SHAHash& hash);

        inline bool operator==(const SHAHash& rhs) const
        { 
            return memcmp(mHash, rhs.mHash, 32) == 0; 
        }

        inline bool operator < (const SHAHash& rhs) const
        {
            return std::memcmp(mHash, rhs.mHash, 32) < 0 ? true : false;
        }

        uint8_t     mHash[32];

    protected:
        SHA_ALG     mType;
#ifdef _DEBUG
        std::string sDebugHash;
#endif



        // SHA256 functionality
        virtual bool IntrinsicSupported() const;
        void sha256_ProcessIntrinsic(const uint8_t* pBuf);
        void sha256_ProcessC(const uint8_t* pBuf, size_t length);
        void sha256_transform(const uint8_t* data);

        static const size_t kBufferSize = 64;
        uint8_t             blockBuffer[kBufferSize];
        size_t              bufferCount;
        uint64_t            mnBytesProcessed;
        __m128i             h0145;  // h0:h1:h4:h5
        __m128i             h2367;  // h2:h3:h6:h7
        SHA256_CTX          mContext256;
        bool                mbSHAIntrinsic;



        // SHA3 functionality
        Sha3Context_t mContext3;
    };
};// namespace SHA
