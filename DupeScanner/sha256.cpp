#include <memory.h>
#include "sha256.h"

SHA256Hash::SHA256Hash(uint8_t* pBuf, size_t length)
{
    Init();
    Compute(pBuf, length);
    Final();
}

SHA256Hash::SHA256Hash()
{
    Init();
}

void SHA256Hash::Compute(uint8_t* pBuf, size_t length)
{
    uint8_t* p = pBuf;
    mnBytesProcessed += length;

    if (bufferCount) 
    {
        size_t c = kBufferSize - bufferCount;
        if (length < c) {
            memcpy(blockBuffer + bufferCount, p, length);
            bufferCount += length;
            return;
        }
        else 
        {
            memcpy(blockBuffer + bufferCount, p, c);
            p += c;
            length -= c;
            ProcessBlock(blockBuffer);
            bufferCount = 0;
        }
    }

    // When we reach here, we have no data left in the buffer
    while (length >= kBufferSize) 
    {
        // No need to copy into the internal block
        ProcessBlock(p);
        p += kBufferSize;
        length -= kBufferSize;
    }

    // Leave the remaining bytes in the buffer
    if (length) 
    {
        memcpy(blockBuffer, p, length);
        bufferCount = length;
    }
}

void SHA256Hash::Final()
{
    // When we reach here, the block is supposed to be unfullfilled.
    // Add the terminating bit
    blockBuffer[bufferCount++] = 0x80;

    // Need to set total length in the last 8-byte of the block.
    // If there is no room for the length, process this block first
    if (bufferCount + 8 > kBufferSize) 
    {
        // Fill zeros and process
        memset(blockBuffer + bufferCount, 0, kBufferSize - bufferCount);
        ProcessBlock(blockBuffer);
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
    ProcessBlock(blockBuffer);

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

    mHash = _mm256_set_m128i(h0123, h4567);
}

void SHA256Hash::ProcessBlock(uint8_t* pBuf)
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