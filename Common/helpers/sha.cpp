#include "sha.h"
#include <memory.h>
#include <string>
#include <assert.h>
#include <iostream>

// Note: Non-intrinsic implementation borrows code from Kui Wang: https://github.com/wangkui0508/sha256

#if defined(__GNUC__)
#include <stdint.h>
#include <x86intrin.h>
#endif

namespace SHA
{
    int32_t gSHAAlg = UNKNOWN;

    static const union
    {
        uint32_t dw[64];
        __m128i x[16];
    } K =
    { {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    } };

    SHAHash::SHAHash(const SHAHash& hash)
    {
        mType = (SHA_ALG)gSHAAlg;
        assert(mType == ALG_SHA256 || mType == ALG_SHA3);

        memcpy(mHash, hash.mHash, 32);
    }


#define ROTATE(x,y)  (((x)>>(y)) | ((x)<<(32-(y))))
#define Sigma0(x)    (ROTATE((x), 2) ^ ROTATE((x),13) ^ ROTATE((x),22))
#define Sigma1(x)    (ROTATE((x), 6) ^ ROTATE((x),11) ^ ROTATE((x),25))
#define sigma0(x)    (ROTATE((x), 7) ^ ROTATE((x),18) ^ ((x)>> 3))
#define sigma1(x)    (ROTATE((x),17) ^ ROTATE((x),19) ^ ((x)>>10))


#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))


#define Ch(x,y,z)    (((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)   (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))


#if defined(__clang__) || defined(__GNUC__) || defined(__INTEL_COMPILER)

#include <cpuid.h>
    bool SHAHash::IntrinsicSupported() const
    {
        unsigned int CPUInfo[4];
        __cpuid(0, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
        if (CPUInfo[0] < 7)
            return 0;

        __cpuid_count(7, 0, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
        return CPUInfo[1] & (1 << 29); /* SHA */
    }

#else /* defined(__clang__) || defined(__GNUC__) */

    bool SHAHash::IntrinsicSupported() const
    {
        int CPUInfo[4];
        __cpuid(CPUInfo, 0);
        if (CPUInfo[0] < 7)
            return 0;

        __cpuidex(CPUInfo, 7, 0);
        return CPUInfo[1] & (1 << 29);
    }

#endif /* defined(__clang__) || defined(__GNUC__) */

    /* Avoid undefined behavior                    */
    /* https://stackoverflow.com/q/29538935/608639 */
    uint32_t B2U32(uint8_t val, uint8_t sh)
    {
        return ((uint32_t)val) << sh;
    }


    void SHAHash::sha256_ProcessIntrinsic(const uint8_t* pBuf)
    {
        // Cyclic W array
        // We keep the W array content cyclically in 4 variables
        // Initially:
        // cw0 = w3 : w2 : w1 : w0
        // cw1 = w7 : w6 : w5 : w4
        // cw2 = w11 : w10 : w9 : w8
        // cw3 = w15 : w14 : w13 : w12
        const __m128i byteswapindex = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);
        const __m128i* msgx = (const __m128i*)pBuf;
        __m128i cw0 = _mm_shuffle_epi8(_mm_loadu_si128(msgx), byteswapindex);
        __m128i cw1 = _mm_shuffle_epi8(_mm_loadu_si128(msgx + 1), byteswapindex);
        __m128i cw2 = _mm_shuffle_epi8(_mm_loadu_si128(msgx + 2), byteswapindex);
        __m128i cw3 = _mm_shuffle_epi8(_mm_loadu_si128(msgx + 3), byteswapindex);

        // Advance W array cycle
        // Inputs: 
        //  CW0 = w[t-13] : w[t-14] : w[t-15] : w[t-16]
        //  CW1 = w[t-9] : w[t-10] : w[t-11] : w[t-12]
        //  CW2 = w[t-5] : w[t-6] : w[t-7] : w[t-8]
        //  CW3 = w[t-1] : w[t-2] : w[t-3] : w[t-4]
        // Outputs: 
        //  CW1 = w[t-9] : w[t-10] : w[t-11] : w[t-12]
        //  CW2 = w[t-5] : w[t-6] : w[t-7] : w[t-8]
        //  CW3 = w[t-1] : w[t-2] : w[t-3] : w[t-4]
        //  CW0 = w[t+3] : w[t+2] : w[t+1] : w[t]
#define CYCLE_W(CW0, CW1, CW2, CW3)                                                             \
        CW0 = _mm_sha256msg1_epu32(CW0, CW1);                                                       \
        CW0 = _mm_add_epi32(CW0, _mm_alignr_epi8(CW3, CW2, 4)); /* add w[t-4]:w[t-5]:w[t-6]:w[t-7]*/\
        CW0 = _mm_sha256msg2_epu32(CW0, CW3);

        __m128i state1 = h0145;     // a:b:e:f
        __m128i state2 = h2367;     // c:d:g:h
        __m128i tmp;

        /* w0 - w3 */
#define SHA256_ROUNDS_4(cwN, n)                                                                             \
        tmp = _mm_add_epi32(cwN, K.x[n]);                   /* w3+K3 : w2+K2 : w1+K1 : w0+K0 */                 \
        state2 = _mm_sha256rnds2_epu32(state2, state1, tmp);/* state2 = a':b':e':f' / state1 = c':d':g':h' */   \
        tmp = _mm_unpackhi_epi64(tmp, tmp);                 /* - : - : w3+K3 : w2+K2 */                         \
        state1 = _mm_sha256rnds2_epu32(state1, state2, tmp);/* state1 = a':b':e':f' / state2 = c':d':g':h' */

        /* w0 - w3 */
        SHA256_ROUNDS_4(cw0, 0);
        /* w4 - w7 */
        SHA256_ROUNDS_4(cw1, 1);
        /* w8 - w11 */
        SHA256_ROUNDS_4(cw2, 2);
        /* w12 - w15 */
        SHA256_ROUNDS_4(cw3, 3);
        /* w16 - w19 */
        CYCLE_W(cw0, cw1, cw2, cw3);    /* cw0 = w19 : w18 : w17 : w16 */
        SHA256_ROUNDS_4(cw0, 4);
        /* w20 - w23 */
        CYCLE_W(cw1, cw2, cw3, cw0);    /* cw1 = w23 : w22 : w21 : w20 */
        SHA256_ROUNDS_4(cw1, 5);
        /* w24 - w27 */
        CYCLE_W(cw2, cw3, cw0, cw1);    /* cw2 = w27 : w26 : w25 : w24 */
        SHA256_ROUNDS_4(cw2, 6);
        /* w28 - w31 */
        CYCLE_W(cw3, cw0, cw1, cw2);    /* cw3 = w31 : w30 : w29 : w28 */
        SHA256_ROUNDS_4(cw3, 7);
        /* w32 - w35 */
        CYCLE_W(cw0, cw1, cw2, cw3);    /* cw0 = w35 : w34 : w33 : w32 */
        SHA256_ROUNDS_4(cw0, 8);
        /* w36 - w39 */
        CYCLE_W(cw1, cw2, cw3, cw0);    /* cw1 = w39 : w38 : w37 : w36 */
        SHA256_ROUNDS_4(cw1, 9);
        /* w40 - w43 */
        CYCLE_W(cw2, cw3, cw0, cw1);    /* cw2 = w43 : w42 : w41 : w40 */
        SHA256_ROUNDS_4(cw2, 10);
        /* w44 - w47 */
        CYCLE_W(cw3, cw0, cw1, cw2);    /* cw3 = w47 : w46 : w45 : w44 */
        SHA256_ROUNDS_4(cw3, 11);
        /* w48 - w51 */
        CYCLE_W(cw0, cw1, cw2, cw3);    /* cw0 = w51 : w50 : w49 : w48 */
        SHA256_ROUNDS_4(cw0, 12);
        /* w52 - w55 */
        CYCLE_W(cw1, cw2, cw3, cw0);    /* cw1 = w55 : w54 : w53 : w52 */
        SHA256_ROUNDS_4(cw1, 13);
        /* w56 - w59 */
        CYCLE_W(cw2, cw3, cw0, cw1);    /* cw2 = w59 : w58 : w57 : w56 */
        SHA256_ROUNDS_4(cw2, 14);
        /* w60 - w63 */
        CYCLE_W(cw3, cw0, cw1, cw2);    /* cw3 = w63 : w62 : w61 : w60 */
        SHA256_ROUNDS_4(cw3, 15);

        // Add to the intermediate hash
        h0145 = _mm_add_epi32(state1, h0145);
        h2367 = _mm_add_epi32(state2, h2367);
    }


    void SHAHash::sha256_ProcessC(const uint8_t* pBuf, size_t length)
    {
        uint32_t a, b, c, d, e, f, g, h, s0, s1, T1, T2;
        uint32_t X[16], i;

        size_t blocks = length / 64;
        while (blocks--)
        {
            a = mContext256.state[0];
            b = mContext256.state[1];
            c = mContext256.state[2];
            d = mContext256.state[3];
            e = mContext256.state[4];
            f = mContext256.state[5];
            g = mContext256.state[6];
            h = mContext256.state[7];

            for (i = 0; i < 16; i++)
            {
                X[i] = B2U32(pBuf[0], 24) | B2U32(pBuf[1], 16) | B2U32(pBuf[2], 8) | B2U32(pBuf[3], 0);
                pBuf += 4;

                T1 = h;
                T1 += Sigma1(e);
                T1 += Ch(e, f, g);
                T1 += K.dw[i];
                T1 += X[i];

                T2 = Sigma0(a);
                T2 += Maj(a, b, c);

                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
            }

            for (; i < 64; i++)
            {
                s0 = X[(i + 1) & 0x0f];
                s0 = sigma0(s0);
                s1 = X[(i + 14) & 0x0f];
                s1 = sigma1(s1);

                T1 = X[i & 0xf] += s0 + s1 + X[(i + 9) & 0xf];
                T1 += h + Sigma1(e) + Ch(e, f, g) + K.dw[i];
                T2 = Sigma0(a) + Maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
            }

            mContext256.state[0] += a;
            mContext256.state[1] += b;
            mContext256.state[2] += c;
            mContext256.state[3] += d;
            mContext256.state[4] += e;
            mContext256.state[5] += f;
            mContext256.state[6] += g;
            mContext256.state[7] += h;
        }
    }



    void SHAHash::sha256_transform(const uint8_t* data)
    {
        uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

        for (i = 0, j = 0; i < 16; ++i, j += 4)
            m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
        for (; i < 64; ++i)
            m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

        a = mContext256.state[0];
        b = mContext256.state[1];
        c = mContext256.state[2];
        d = mContext256.state[3];
        e = mContext256.state[4];
        f = mContext256.state[5];
        g = mContext256.state[6];
        h = mContext256.state[7];

        for (i = 0; i < 64; ++i) {
            t1 = h + EP1(e) + CH(e, f, g) + K.dw[i] + m[i];
            t2 = EP0(a) + MAJ(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        mContext256.state[0] += a;
        mContext256.state[1] += b;
        mContext256.state[2] += c;
        mContext256.state[3] += d;
        mContext256.state[4] += e;
        mContext256.state[5] += f;
        mContext256.state[6] += g;
        mContext256.state[7] += h;
    }

    SHAHash::SHAHash(const std::string& fromString)
    {
        mType = (SHA_ALG)gSHAAlg;
        assert(mType == ALG_SHA256 || mType == ALG_SHA3);

        FromString(fromString);
    }

#ifdef _DEBUG
//#define TESTHASH
#endif

#ifdef TESTHASH
    class HashTest
    {
    public:
        HashTest()
        {
            uint8_t buf[128];
            for (int i = 0; i < 128; i++)
                buf[i] = i;

            SHAHash h1(&buf[0], 128);
            std::string stringHash = h1.ToString();

            SHAHash h2(stringHash);

            if (!(h1 == h2))
            {
                assert(false);
            }
        }


    };

    HashTest gHashTest;


#endif



    static inline uint64_t sha3rotl(uint64_t x, uint64_t y)
    {
        return (((x) << (y)) | ((x) >> (64 - (y))));
    }

    static const uint64_t sha3KeccakfRoundContstant[24] = { 0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000, 0x000000000000808b,
                                                           0x0000000080000001, 0x8000000080008081, 0x8000000000008009, 0x000000000000008a, 0x0000000000000088,
                                                           0x0000000080008009, 0x000000008000000a, 0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
                                                           0x8000000000008003, 0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
                                                           0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008 };

    static void sha3Keccakf(uint64_t* st)
    {
        uint64_t st0 = st[0];
        uint64_t st1 = st[1];
        uint64_t st2 = st[2];
        uint64_t st3 = st[3];
        uint64_t st4 = st[4];
        uint64_t st5 = st[5];
        uint64_t st6 = st[6];
        uint64_t st7 = st[7];
        uint64_t st8 = st[8];
        uint64_t st9 = st[9];
        uint64_t st10 = st[10];
        uint64_t st11 = st[11];
        uint64_t st12 = st[12];
        uint64_t st13 = st[13];
        uint64_t st14 = st[14];
        uint64_t st15 = st[15];
        uint64_t st16 = st[16];
        uint64_t st17 = st[17];
        uint64_t st18 = st[18];
        uint64_t st19 = st[19];
        uint64_t st20 = st[20];
        uint64_t st21 = st[21];
        uint64_t st22 = st[22];
        uint64_t st23 = st[23];
        uint64_t st24 = st[24];

        for (int r = 0; r < 24; r++)
        {
            uint64_t a = st0 ^ st5 ^ st10 ^ st15 ^ st20;
            uint64_t b = st1 ^ st6 ^ st11 ^ st16 ^ st21;
            uint64_t c = st2 ^ st7 ^ st12 ^ st17 ^ st22;
            uint64_t d = st3 ^ st8 ^ st13 ^ st18 ^ st23;
            uint64_t e = st4 ^ st9 ^ st14 ^ st19 ^ st24;

            uint64_t ta = e ^ sha3rotl(b, 1);
            uint64_t tb = a ^ sha3rotl(c, 1);
            uint64_t tc = b ^ sha3rotl(d, 1);
            uint64_t td = c ^ sha3rotl(e, 1);
            uint64_t te = d ^ sha3rotl(a, 1);

            st0 ^= ta;
            st5 ^= ta;
            st10 ^= ta;
            st15 ^= ta;
            st20 ^= ta;
            st1 ^= tb;
            st6 ^= tb;
            st11 ^= tb;
            st16 ^= tb;
            st21 ^= tb;
            st2 ^= tc;
            st7 ^= tc;
            st12 ^= tc;
            st17 ^= tc;
            st22 ^= tc;
            st3 ^= td;
            st8 ^= td;
            st13 ^= td;
            st18 ^= td;
            st23 ^= td;
            st4 ^= te;
            st9 ^= te;
            st14 ^= te;
            st19 ^= te;
            st24 ^= te;

            uint64_t nst0 = st0;
            uint64_t nst1 = sha3rotl(st6, 44);
            uint64_t nst2 = sha3rotl(st12, 43);
            uint64_t nst3 = sha3rotl(st18, 21);
            uint64_t nst4 = sha3rotl(st24, 14);

            uint64_t nst5 = sha3rotl(st3, 28);
            uint64_t nst6 = sha3rotl(st9, 20);
            uint64_t nst7 = sha3rotl(st10, 3);
            uint64_t nst8 = sha3rotl(st16, 45);
            uint64_t nst9 = sha3rotl(st22, 61);

            uint64_t nst10 = sha3rotl(st1, 1);
            uint64_t nst11 = sha3rotl(st7, 6);
            uint64_t nst12 = sha3rotl(st13, 25);
            uint64_t nst13 = sha3rotl(st19, 8);
            uint64_t nst14 = sha3rotl(st20, 18);

            uint64_t nst15 = sha3rotl(st4, 27);
            uint64_t nst16 = sha3rotl(st5, 36);
            uint64_t nst17 = sha3rotl(st11, 10);
            uint64_t nst18 = sha3rotl(st17, 15);
            uint64_t nst19 = sha3rotl(st23, 56);

            uint64_t nst20 = sha3rotl(st2, 62);
            uint64_t nst21 = sha3rotl(st8, 55);
            uint64_t nst22 = sha3rotl(st14, 39);
            uint64_t nst23 = sha3rotl(st15, 41);
            uint64_t nst24 = sha3rotl(st21, 2);

            st0 = nst0 ^ (~nst1 & nst2);
            st1 = nst1 ^ (~nst2 & nst3);
            st2 = nst2 ^ (~nst3 & nst4);
            st3 = nst3 ^ (~nst4 & nst0);
            st4 = nst4 ^ (~nst0 & nst1);

            st5 = nst5 ^ (~nst6 & nst7);
            st6 = nst6 ^ (~nst7 & nst8);
            st7 = nst7 ^ (~nst8 & nst9);
            st8 = nst8 ^ (~nst9 & nst5);
            st9 = nst9 ^ (~nst5 & nst6);

            st10 = nst10 ^ (~nst11 & nst12);
            st11 = nst11 ^ (~nst12 & nst13);
            st12 = nst12 ^ (~nst13 & nst14);
            st13 = nst13 ^ (~nst14 & nst10);
            st14 = nst14 ^ (~nst10 & nst11);

            st15 = nst15 ^ (~nst16 & nst17);
            st16 = nst16 ^ (~nst17 & nst18);
            st17 = nst17 ^ (~nst18 & nst19);
            st18 = nst18 ^ (~nst19 & nst15);
            st19 = nst19 ^ (~nst15 & nst16);

            st20 = nst20 ^ (~nst21 & nst22);
            st21 = nst21 ^ (~nst22 & nst23);
            st22 = nst22 ^ (~nst23 & nst24);
            st23 = nst23 ^ (~nst24 & nst20);
            st24 = nst24 ^ (~nst20 & nst21);

            st0 ^= sha3KeccakfRoundContstant[r];
        }

        st[0] = st0;
        st[1] = st1;
        st[2] = st2;
        st[3] = st3;
        st[4] = st4;
        st[5] = st5;
        st[6] = st6;
        st[7] = st7;
        st[8] = st8;
        st[9] = st9;
        st[10] = st10;
        st[11] = st11;
        st[12] = st12;
        st[13] = st13;
        st[14] = st14;
        st[15] = st15;
        st[16] = st16;
        st[17] = st17;
        st[18] = st18;
        st[19] = st19;
        st[20] = st20;
        st[21] = st21;
        st[22] = st22;
        st[23] = st23;
        st[24] = st24;
    }

    void SHAHash::Init()
    {
        memset(mHash, 0, sizeof(mHash));
#ifdef _DEBUG
        sDebugHash = ToString();
#endif

        if (mType == ALG_SHA3)
        {
            // SHA3 256
            mContext3.digestLen = 32;

            for (size_t i = 0; i < sizeof(mContext3.state.b); ++i)
                mContext3.state.b[i] = 0;

            // size of sponge in bytes - 2 * digest size in bytes
            // 200 - 2 * 32 (for sha 256)
            mContext3.blockSize = 200 - 2 * mContext3.digestLen;
            mContext3.blockIndex = 0;

            return;
        }

        if (mType == ALG_SHA256)
        {
            mbSHAIntrinsic = IntrinsicSupported();

            memset(mHash, 0, sizeof(mHash));
#ifdef _DEBUG
            sDebugHash = ToString();
#endif

            mContext256.datalen = 0;
            mContext256.bitlen = 0;
            mContext256.state[0] = 0x6a09e667;
            mContext256.state[1] = 0xbb67ae85;
            mContext256.state[2] = 0x3c6ef372;
            mContext256.state[3] = 0xa54ff53a;
            mContext256.state[4] = 0x510e527f;
            mContext256.state[5] = 0x9b05688c;
            mContext256.state[6] = 0x1f83d9ab;
            mContext256.state[7] = 0x5be0cd19;

            if (mbSHAIntrinsic)
            {
                h0145 = _mm_set_epi32(0x6a09e667, 0xbb67ae85, 0x510e527f, 0x9b05688c);
                h2367 = _mm_set_epi32(0x3c6ef372, 0xa54ff53a, 0x1f83d9ab, 0x5be0cd19);
            }
            bufferCount = 0;
            mnBytesProcessed = 0;
            return;
        }

        assert(false);
    }

    void SHAHash::Compute(const uint8_t* pBuf, size_t length)
    {
        assert(pBuf);

        if (mType == ALG_SHA3)
        {
            const uint8_t k = mContext3.blockSize;
            uint8_t j = mContext3.blockIndex;

            for (size_t i = 0; i < length; ++i)
            {
                mContext3.state.b[j++] ^= pBuf[i];
                if (j < k)
                    continue;

                sha3Keccakf(mContext3.state.q);
                j = 0;
            }

            mContext3.blockIndex = j;
            return;
        }

        if (mType == ALG_SHA256)
        {
            if (mbSHAIntrinsic)
            {
                mnBytesProcessed += length;

                if (bufferCount)
                {
                    size_t c = kBufferSize - bufferCount;
                    if (length < c) {
                        memcpy(blockBuffer + bufferCount, pBuf, length);
                        bufferCount += length;
                        return;
                    }
                    else
                    {
                        memcpy(blockBuffer + bufferCount, pBuf, c);
                        pBuf += c;
                        length -= c;
                        sha256_ProcessIntrinsic(blockBuffer);
                        bufferCount = 0;
                    }
                }

                // When we reach here, we have no data left in the buffer
                while (length >= kBufferSize)
                {
                    // No need to copy into the internal block
                    sha256_ProcessIntrinsic(pBuf);
                    pBuf += kBufferSize;
                    length -= kBufferSize;
                }

                // Leave the remaining bytes in the buffer
                if (length)
                {
                    memcpy(blockBuffer, pBuf, length);
                    bufferCount = length;
                }
            }
            else
            {
                // if any data left over from previous, fill buffer and process
                if (mContext256.datalen > 0)
                {
                    assert(mContext256.datalen < 64);
                    size_t nBytesToFill = 64 - mContext256.datalen;
                    memcpy(mContext256.data + mContext256.datalen, pBuf, nBytesToFill);

                    mContext256.bitlen += 512;
                    sha256_ProcessC(pBuf, 64);
                    length -= nBytesToFill;
                }
                size_t nBlockBytesToConsume = 64 * (length / 64);
                mContext256.bitlen += (8 * nBlockBytesToConsume);
                mContext256.datalen = length % 64; // remainder

                sha256_ProcessC(pBuf, length);

                if (mContext256.datalen != 0)  // if remainder, store it for next time
                    memcpy(mContext256.data, pBuf + nBlockBytesToConsume, mContext256.datalen);
            }

            return;
        }

        assert(false);
    }

    SHAHash::SHAHash()
    {
        mType = (SHA_ALG)gSHAAlg;
        assert(mType == ALG_SHA256 || mType == ALG_SHA3);

        Init();
    }



    SHAHash::SHAHash(const uint8_t* pBuf, size_t length)
    {
        mType = (SHA_ALG)gSHAAlg;
        assert(mType == ALG_SHA256 || mType == ALG_SHA3);

        assert(pBuf && length > 0);
        Init();


        if (mType == ALG_SHA3)
        {

            uint8_t j = mContext3.blockIndex;

            if (j != 0)
            {
                Compute(pBuf, length);
                return;
            }

            const uint64_t* s = (const uint64_t*)pBuf;
            uint64_t* d = &mContext3.state.q[0];

            const uint8_t k = mContext3.blockSize;
            const size_t dloops = length / k;
            const size_t remaining = length % k;

            for (size_t l = 0; l < dloops; ++l)
            {
                size_t sloops = k / 8;
                for (size_t m = 0; m < sloops; ++m)
                {
                    d[m] ^= *s++;
                }

                sha3Keccakf(mContext3.state.q);
            }

            if (remaining)
            {
                for (size_t i = (dloops * k); i < length; ++i)
                {
                    mContext3.state.b[j++] ^= pBuf[i];
                    if (j < k)
                        continue;

                    sha3Keccakf(mContext3.state.q);
                    j = 0;
                }
            }

            mContext3.blockIndex = j;
        }
        else if (mType == ALG_SHA256)
        {
            mType = (SHA_ALG)gSHAAlg;
            assert(mType == ALG_SHA256 || mType == ALG_SHA3);

            Compute(pBuf, length);
        }

        Final();
    }

    void SHAHash::Final()
    {
        if (mType == ALG_SHA3)
        {
            mContext3.state.b[mContext3.blockIndex] ^= 0x06;
            mContext3.state.b[mContext3.blockSize - 1] ^= 0x80;
            sha3Keccakf(mContext3.state.q);

            for (uint32_t i = 0; i < mContext3.digestLen; i++)
            {
                mHash[i] = mContext3.state.b[i];
            }
#ifdef _DEBUG
            sDebugHash = ToString();
#endif
            return;
        }


        if (mType == ALG_SHA256)
        {
            if (mbSHAIntrinsic)
            {   // When we reach here, the block is supposed to be unfullfilled.
                // Add the terminating bit
                blockBuffer[bufferCount++] = 0x80;

                // Need to set total length in the last 8-byte of the block.
                // If there is no room for the length, process this block first
                if (bufferCount + 8 > kBufferSize)
                {
                    // Fill zeros and process
                    memset(blockBuffer + bufferCount, 0, kBufferSize - bufferCount);
                    sha256_ProcessIntrinsic(blockBuffer);
                    bufferCount = 0;
                }

                // Fill zeros before the last 8-byte of the block
                memset(blockBuffer + bufferCount, 0, kBufferSize - 8 - bufferCount);

                // Set the length of the message in big-endian
                __m128i tmp = _mm_loadl_epi64((__m128i*) & mnBytesProcessed);
                tmp = _mm_slli_epi64(tmp, 3);   // convert # of bytes to # of bits
                const __m128i mnBytesProcessed_byteswapindex = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7);
                tmp = _mm_shuffle_epi8(tmp, mnBytesProcessed_byteswapindex); // convert to big endian
                _mm_storel_epi64((__m128i*)(blockBuffer + kBufferSize - 8), tmp);

                // Process the last block
                sha256_ProcessIntrinsic(blockBuffer);

                // Get the resulting hash value.
                // h0:h1:h4:h5
                // h2:h3:h6:h7
                //      |
                //      V
                // h0:h1:h2:h3
                // h4:h5:h6:h7
                __m128i h0123 = _mm_unpackhi_epi64(h2367, h0145);
                __m128i h4567 = _mm_unpacklo_epi64(h2367, h0145);

                // Swap the byte order
                const __m128i byteswapindex = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

                h0123 = _mm_shuffle_epi8(h0123, byteswapindex);
                h4567 = _mm_shuffle_epi8(h4567, byteswapindex);

                __m256i h = _mm256_set_m128i(h4567, h0123);
                memcpy(mHash, &h, 32);
            }
            else
            {
                uint32_t i;

                i = mContext256.datalen;

                // Pad whatever data is left in the buffer.
                if (mContext256.datalen < 56) {
                    mContext256.data[i++] = 0x80;
                    while (i < 56)
                        mContext256.data[i++] = 0x00;
                }
                else {
                    mContext256.data[i++] = 0x80;
                    while (i < 64)
                        mContext256.data[i++] = 0x00;
                    sha256_transform(mContext256.data);
                    memset(mContext256.data, 0, 56);
                }

                // Append to the padding the total message's length in bits and transform.
                mContext256.bitlen += mContext256.datalen * 8;
                mContext256.data[63] = (uint8_t)(mContext256.bitlen);
                mContext256.data[62] = (uint8_t)(mContext256.bitlen >> 8);
                mContext256.data[61] = (uint8_t)(mContext256.bitlen >> 16);
                mContext256.data[60] = (uint8_t)(mContext256.bitlen >> 24);
                mContext256.data[59] = (uint8_t)(mContext256.bitlen >> 32);
                mContext256.data[58] = (uint8_t)(mContext256.bitlen >> 40);
                mContext256.data[57] = (uint8_t)(mContext256.bitlen >> 48);
                mContext256.data[56] = (uint8_t)(mContext256.bitlen >> 56);
                sha256_transform(mContext256.data);

                // Since this implementation uses little endian byte ordering and SHA uses big endian,
                // reverse all the bytes when copying the final state to the output hash.
                for (i = 0; i < 4; ++i) {
                    mHash[i]        = (mContext256.state[0] >> (24 - i * 8)) & 0x000000ff;
                    mHash[i + 4]    = (mContext256.state[1] >> (24 - i * 8)) & 0x000000ff;
                    mHash[i + 8]    = (mContext256.state[2] >> (24 - i * 8)) & 0x000000ff;
                    mHash[i + 12]   = (mContext256.state[3] >> (24 - i * 8)) & 0x000000ff;
                    mHash[i + 16]   = (mContext256.state[4] >> (24 - i * 8)) & 0x000000ff;
                    mHash[i + 20]   = (mContext256.state[5] >> (24 - i * 8)) & 0x000000ff;
                    mHash[i + 24]   = (mContext256.state[6] >> (24 - i * 8)) & 0x000000ff;
                    mHash[i + 28]   = (mContext256.state[7] >> (24 - i * 8)) & 0x000000ff;
                }
            }
#ifdef _DEBUG
            sDebugHash = ToString();
#endif
            return;
        }


        assert(false);
    }

    std::string SHAHash::ToString() const
    {
        char buf[64];
        char byteToAscii[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

        uint8_t* pWalker = (uint8_t*)&mHash;
        for (int i = 0; i < 32; i++)
        {
            uint8_t c = *pWalker++;
            buf[i * 2] = byteToAscii[c >> 4];
            buf[i * 2 + 1] = byteToAscii[c & 0x0F];
        }

        return std::string(buf, 64);
    }

    void SHAHash::FromString(const std::string& fromString)
    {
        assert(fromString.length() == 64);

        for (int i = 0; i < 32; i++)
        {
            uint8_t c1 = fromString[i * 2];
            uint8_t c2 = fromString[i * 2 + 1];

            if (c1 >= '0' && c1 <= '9')
                c1 -= '0';
            else
                c1 -= 'A' - 10;

            if (c2 >= '0' && c2 <= '9')
                c2 -= '0';
            else
                c2 -= 'A' - 10;

            mHash[i] = (c1 << 4) | c2;
        }
    }

    std::ostream& operator << (std::ostream& os, const SHAHash& h)
    {
        os.write((const char*)h.mHash, 32);
        return os;
    }

    std::istream& operator >> (std::istream& is, SHAHash& h)
    {
        is.read((char*)h.mHash, 32);
#ifdef _DEBUG
        h.sDebugHash = h.ToString();
#endif

        return is;
    }
}; // namespace SHA
