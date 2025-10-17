/*
picoPNG version 20101224
Copyright (c) 2005-2010 Lode Vandevenne

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/

#ifndef PICOPNG_H
#define PICOPNG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
decodePNG: The picoPNG function, decodes a PNG file buffer in memory, into a raw pixel buffer.
out_image: Pointer to a pointer that will contain the raw pixel data.
           The memory is allocated by this function, and you must free it yourself using free().
image_width: Pointer to an unsigned long that will be filled with the image width.
image_height: Pointer to an unsigned long that will be filled with the image height.
out_size: Pointer to a size_t that will be filled with the size of the output buffer in bytes.
in_png: Pointer to the buffer of the PNG file in memory.
in_size: Size of the input PNG file in bytes.
convert_to_rgba32: 1 to convert to 32-bit RGBA, 0 for original color type.
Returns 0 on success, or a non-zero error code on failure.
*/
int zest__decode_png(unsigned char** out_image, unsigned long* image_width, unsigned long* image_height, size_t* out_size, const unsigned char* in_png, size_t in_size, int convert_to_rgba32);

#ifdef ZEST_PICO_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ////////////////////////////////////////////////////////////////////////// */
/* ////////////////////////////// C++ TO C ADAPTATIONS ////////////////////// */
/* ////////////////////////////////////////////////////////////////////////// */

/* Simple dynamic array for unsigned char */
typedef struct uc_vector {
    unsigned char* data;
    size_t size;
    size_t capacity;
} uc_vector;

static void uc_vector_init(uc_vector* v) {
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}

static void uc_vector_resize(uc_vector* v, size_t new_size) {
    if (new_size > v->capacity) {
        size_t new_capacity = v->capacity;
        if (new_capacity == 0) new_capacity = 1;
        while (new_capacity < new_size) new_capacity *= 2;
        v->data = (unsigned char*)realloc(v->data, new_capacity);
        v->capacity = new_capacity;
    }
    v->size = new_size;
}

static void uc_vector_clear(uc_vector* v) {
    v->size = 0;
}

static void uc_vector_insert(uc_vector* v, const unsigned char* buffer, size_t size) {
    size_t old_size = v->size;
    uc_vector_resize(v, v->size + size);
    memcpy(v->data + old_size, buffer, size);
}

static void uc_vector_free(uc_vector* v) {
    if (v->data) free(v->data);
    uc_vector_init(v);
}

/* Simple dynamic array for unsigned long */
typedef struct ul_vector {
    unsigned long* data;
    size_t size;
    size_t capacity;
} ul_vector;

static void ul_vector_init(ul_vector* v) {
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}

static void ul_vector_resize(ul_vector* v, size_t new_size) {
    if (new_size > v->capacity) {
        size_t new_capacity = v->capacity;
        if (new_capacity == 0) new_capacity = 1;
        while (new_capacity < new_size) new_capacity *= 2;
        v->data = (unsigned long*)realloc(v->data, new_capacity * sizeof(unsigned long));
        v->capacity = new_capacity;
    }
    v->size = new_size;
}

static void ul_vector_clear(ul_vector* v) {
    v->size = 0;
}

static void ul_vector_free(ul_vector* v) {
    if (v->data) free(v->data);
    ul_vector_init(v);
}

/* ////////////////////////////////////////////////////////////////////////// */
/* ////////////////////////////// DECODER /////////////////////////////////// */
/* ////////////////////////////////////////////////////////////////////////// */

static const unsigned long LENBASE[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned long LENEXTRA[29] = {0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0};
static const unsigned long DISTBASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const unsigned long DISTEXTRA[30] = {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13};
static const unsigned long CLCL[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

typedef struct HuffmanTree {
    ul_vector tree2d;
} HuffmanTree;

static void HuffmanTree_init(HuffmanTree* tree) {
    ul_vector_init(&tree->tree2d);
}

static void HuffmanTree_free(HuffmanTree* tree) {
    ul_vector_free(&tree->tree2d);
}

static int HuffmanTree_makeFromLengths(HuffmanTree* tree, const ul_vector* bitlen, unsigned long maxbitlen) {
    unsigned long numcodes = (unsigned long)bitlen->size, treepos = 0, nodefilled = 0;
    ul_vector tree1d;
    ul_vector_init(&tree1d);
    ul_vector_resize(&tree1d, numcodes);

    ul_vector blcount;
    ul_vector_init(&blcount);
    ul_vector_resize(&blcount, maxbitlen + 1);
    memset(blcount.data, 0, (maxbitlen + 1) * sizeof(unsigned long));

    ul_vector nextcode;
    ul_vector_init(&nextcode);
    ul_vector_resize(&nextcode, maxbitlen + 1);
    memset(nextcode.data, 0, (maxbitlen + 1) * sizeof(unsigned long));

    for(unsigned long bits = 0; bits < numcodes; bits++) blcount.data[bitlen->data[bits]]++;
    for(unsigned long bits = 1; bits <= maxbitlen; bits++) nextcode.data[bits] = (nextcode.data[bits - 1] + blcount.data[bits - 1]) << 1;
    for(unsigned long n = 0; n < numcodes; n++) if(bitlen->data[n] != 0) tree1d.data[n] = nextcode.data[bitlen->data[n]]++;
    
    ul_vector_clear(&tree->tree2d);
    ul_vector_resize(&tree->tree2d, numcodes * 2);
    for(unsigned long n = 0; n < numcodes * 2; n++) tree->tree2d.data[n] = 32767;

    for(unsigned long n = 0; n < numcodes; n++) {
        for(unsigned long i = 0; i < bitlen->data[n]; i++) {
            unsigned long bit = (tree1d.data[n] >> (bitlen->data[n] - i - 1)) & 1;
            if(treepos > numcodes - 2) { ul_vector_free(&tree1d); ul_vector_free(&blcount); ul_vector_free(&nextcode); return 55; }
            if(tree->tree2d.data[2 * treepos + bit] == 32767) {
                if(i + 1 == bitlen->data[n]) { tree->tree2d.data[2 * treepos + bit] = n; treepos = 0; } 
                else { tree->tree2d.data[2 * treepos + bit] = ++nodefilled + numcodes; treepos = nodefilled; }
            } else {
                treepos = tree->tree2d.data[2 * treepos + bit] - numcodes;
            }
        }
    }
    ul_vector_free(&tree1d);
    ul_vector_free(&blcount);
    ul_vector_free(&nextcode);
    return 0;
}

static int HuffmanTree_decode(const HuffmanTree* tree, int* decoded, unsigned long* result, size_t* treepos, unsigned long bit) {
    unsigned long numcodes = (unsigned long)tree->tree2d.size / 2;
    if(*treepos >= numcodes) return 11;
    *result = tree->tree2d.data[2 * *treepos + bit];
    *decoded = (*result < numcodes);
    *treepos = *decoded ? 0 : *result - numcodes;
    return 0;
}

static unsigned long readBitFromStream(size_t* bitp, const unsigned char* bits) {
    unsigned long result = (bits[*bitp >> 3] >> (*bitp & 0x7)) & 1;
    (*bitp)++;
    return result;
}

static unsigned long readBitsFromStream(size_t* bitp, const unsigned char* bits, size_t nbits) {
    unsigned long result = 0;
    for(size_t i = 0; i < nbits; i++) result += (readBitFromStream(bitp, bits)) << i;
    return result;
}

typedef struct Inflator {
    int error;
    HuffmanTree codetree, codetreeD, codelengthcodetree;
} Inflator;

static void Inflator_init(Inflator* inflator) {
    inflator->error = 0;
    HuffmanTree_init(&inflator->codetree);
    HuffmanTree_init(&inflator->codetreeD);
    HuffmanTree_init(&inflator->codelengthcodetree);
}

static void Inflator_free(Inflator* inflator) {
    HuffmanTree_free(&inflator->codetree);
    HuffmanTree_free(&inflator->codetreeD);
    HuffmanTree_free(&inflator->codelengthcodetree);
}

static unsigned long huffmanDecodeSymbol(Inflator* inflator, const unsigned char* in, size_t* bp, const HuffmanTree* codetree, size_t inlength) {
    int decoded;
    unsigned long ct;
    size_t treepos = 0;
    for(;;) {
        if((*bp & 0x07) == 0 && (*bp >> 3) > inlength) { inflator->error = 10; return 0; }
        inflator->error = HuffmanTree_decode(codetree, &decoded, &ct, &treepos, readBitFromStream(bp, in));
        if(inflator->error) return 0;
        if(decoded) return ct;
    }
}

static void getTreeInflateDynamic(Inflator* inflator, const unsigned char* in, size_t* bp, size_t inlength) {
    ul_vector bitlen, bitlenD, codelengthcode;
    ul_vector_init(&bitlen);
    ul_vector_init(&bitlenD);
    ul_vector_init(&codelengthcode);

    ul_vector_resize(&bitlen, 288);
    memset(bitlen.data, 0, 288 * sizeof(unsigned long));
    ul_vector_resize(&bitlenD, 32);
    memset(bitlenD.data, 0, 32 * sizeof(unsigned long));

    if(*bp >> 3 >= inlength - 2) { inflator->error = 49; goto cleanup; }
    size_t HLIT = readBitsFromStream(bp, in, 5) + 257;
    size_t HDIST = readBitsFromStream(bp, in, 5) + 1;
    size_t HCLEN = readBitsFromStream(bp, in, 4) + 4;

    ul_vector_resize(&codelengthcode, 19);
    memset(codelengthcode.data, 0, 19 * sizeof(unsigned long));
    for(size_t i = 0; i < 19; i++) codelengthcode.data[CLCL[i]] = (i < HCLEN) ? readBitsFromStream(bp, in, 3) : 0;
    
    inflator->error = HuffmanTree_makeFromLengths(&inflator->codelengthcodetree, &codelengthcode, 7);
    if(inflator->error) goto cleanup;

    size_t i = 0, replength;
    while(i < HLIT + HDIST) {
        unsigned long code = huffmanDecodeSymbol(inflator, in, bp, &inflator->codelengthcodetree, inlength);
        if(inflator->error) goto cleanup;
        if(code <= 15) {
            if(i < HLIT) bitlen.data[i++] = code;
            else bitlenD.data[i++ - HLIT] = code;
        } else if(code == 16) {
            if(*bp >> 3 >= inlength) { inflator->error = 50; goto cleanup; }
            replength = 3 + readBitsFromStream(bp, in, 2);
            unsigned long value;
            if((i - 1) < HLIT) value = bitlen.data[i - 1];
            else value = bitlenD.data[i - HLIT - 1];
            for(size_t n = 0; n < replength; n++) {
                if(i >= HLIT + HDIST) { inflator->error = 13; goto cleanup; }
                if(i < HLIT) bitlen.data[i++] = value;
                else bitlenD.data[i++ - HLIT] = value;
            }
        } else if(code == 17) {
            if(*bp >> 3 >= inlength) { inflator->error = 50; goto cleanup; }
            replength = 3 + readBitsFromStream(bp, in, 3);
            for(size_t n = 0; n < replength; n++) {
                if(i >= HLIT + HDIST) { inflator->error = 14; goto cleanup; }
                if(i < HLIT) bitlen.data[i++] = 0;
                else bitlenD.data[i++ - HLIT] = 0;
            }
        } else if(code == 18) {
            if(*bp >> 3 >= inlength) { inflator->error = 50; goto cleanup; }
            replength = 11 + readBitsFromStream(bp, in, 7);
            for(size_t n = 0; n < replength; n++) {
                if(i >= HLIT + HDIST) { inflator->error = 15; goto cleanup; }
                if(i < HLIT) bitlen.data[i++] = 0;
                else bitlenD.data[i++ - HLIT] = 0;
            }
        } else { inflator->error = 16; goto cleanup; }
    }

    if(bitlen.data[256] == 0) { inflator->error = 64; goto cleanup; }
    inflator->error = HuffmanTree_makeFromLengths(&inflator->codetree, &bitlen, 15);
    if(inflator->error) goto cleanup;
    inflator->error = HuffmanTree_makeFromLengths(&inflator->codetreeD, &bitlenD, 15);

cleanup:
    ul_vector_free(&bitlen);
    ul_vector_free(&bitlenD);
    ul_vector_free(&codelengthcode);
}

static void generateFixedTrees(Inflator* inflator) {
    ul_vector bitlen, bitlenD;
    ul_vector_init(&bitlen);
    ul_vector_init(&bitlenD);

    ul_vector_resize(&bitlen, 288);
    ul_vector_resize(&bitlenD, 32);

    for(size_t i = 0; i <= 143; i++) bitlen.data[i] = 8;
    for(size_t i = 144; i <= 255; i++) bitlen.data[i] = 9;
    for(size_t i = 256; i <= 279; i++) bitlen.data[i] = 7;
    for(size_t i = 280; i <= 287; i++) bitlen.data[i] = 8;
    for(size_t i = 0; i <= 31; i++) bitlenD.data[i] = 5;

    HuffmanTree_makeFromLengths(&inflator->codetree, &bitlen, 15);
    HuffmanTree_makeFromLengths(&inflator->codetreeD, &bitlenD, 15);

    ul_vector_free(&bitlen);
    ul_vector_free(&bitlenD);
}

static void inflateHuffmanBlock(Inflator* inflator, uc_vector* out, size_t* pos, const unsigned char* in, size_t* bp, size_t inlength, unsigned long btype) 
{
    if(btype == 1) { generateFixedTrees(inflator); }
    else if(btype == 2) { getTreeInflateDynamic(inflator, in, bp, inlength); if(inflator->error) return; }
    for(;;)
    {
        unsigned long code = huffmanDecodeSymbol(inflator, in, bp, &inflator->codetree, inlength); if(inflator->error) return;
        if(code == 256) return; //end code
        else if(code <= 255) //literal symbol
        {
            uc_vector_resize(out, *pos + 1);
            out->data[(*pos)++] = (unsigned char)(code);
        }
        else if(code >= 257 && code <= 285) //length code
        {
            size_t length = LENBASE[code - 257], numextrabits = LENEXTRA[code - 257];
            if((*bp >> 3) >= inlength) { inflator->error = 51; return; }
            length += readBitsFromStream(bp, in, numextrabits);
            unsigned long codeD = huffmanDecodeSymbol(inflator, in, bp, &inflator->codetreeD, inlength); if(inflator->error) return;
            if(codeD > 29) { inflator->error = 18; return; } //error: invalid dist code (30-31 are never used)
            unsigned long dist = DISTBASE[codeD], numextrabitsD = DISTEXTRA[codeD];
            if((*bp >> 3) >= inlength) { inflator->error = 51; return; } //error, bit pointer will jump past memory
            dist += readBitsFromStream(bp, in, numextrabitsD);
            size_t start = *pos;
            size_t back = start - dist;
            uc_vector_resize(out, *pos + length);

            size_t read_ptr = back;
            for (size_t i = 0; i < length; i++) {
                out->data[start + i] = out->data[read_ptr];
                read_ptr++;
                if (read_ptr >= start) {
                    read_ptr = back;
                }
            }
            *pos += length;
        }
    }
}

static void inflateNoCompression(Inflator* inflator, uc_vector* out, size_t* pos, const unsigned char* in, size_t* bp, size_t inlength) {
    while((*bp & 0x7) != 0) (*bp)++;
    size_t p = *bp / 8;
    if(p >= inlength - 4) { inflator->error = 52; return; }
    unsigned long LEN = in[p] + 256 * in[p + 1], NLEN = in[p + 2] + 256 * in[p + 3]; p += 4;
    if(LEN + NLEN != 65535) { inflator->error = 21; return; }

    uc_vector_resize(out, *pos + LEN);

    if(p + LEN > inlength) { inflator->error = 23; return; }
    memcpy(out->data + *pos, &in[p], LEN);
    *pos += LEN;
    p += LEN;
    *bp = p * 8;
}

static void Inflator_inflate(Inflator* inflator, uc_vector* out, const uc_vector* in, size_t inpos) {
    size_t bp = 0, pos = 0;
    inflator->error = 0;
    unsigned long BFINAL = 0;
    while(!BFINAL && !inflator->error) {
        if(bp >> 3 >= in->size) { inflator->error = 52; return; }
        BFINAL = readBitFromStream(&bp, &in->data[inpos]);
        unsigned long BTYPE = readBitFromStream(&bp, &in->data[inpos]); BTYPE += 2 * readBitFromStream(&bp, &in->data[inpos]);
        if(BTYPE == 3) { inflator->error = 20; return; }
        else if(BTYPE == 0) inflateNoCompression(inflator, out, &pos, &in->data[inpos], &bp, in->size);
        else inflateHuffmanBlock(inflator, out, &pos, &in->data[inpos], &bp, in->size, BTYPE);
    }
    if (!inflator->error) {
        out->size = pos;
    }
}

static int decompress(uc_vector* out, const uc_vector* in) {
    Inflator inflator;
    Inflator_init(&inflator);
    if(in->size < 2) { Inflator_free(&inflator); return 53; }
    if((in->data[0] * 256 + in->data[1]) % 31 != 0) { Inflator_free(&inflator); return 24; }
    unsigned long CM = in->data[0] & 15, CINFO = (in->data[0] >> 4) & 15, FDICT = (in->data[1] >> 5) & 1;
    if(CM != 8 || CINFO > 7) { Inflator_free(&inflator); return 25; }
    if(FDICT != 0) { Inflator_free(&inflator); return 26; }
    Inflator_inflate(&inflator, out, in, 2);
    int error = inflator.error;
    Inflator_free(&inflator);
    return error;
}

typedef struct PNG_Info {
    unsigned long width, height, colorType, bitDepth, compressionMethod, filterMethod, interlaceMethod, key_r, key_g, key_b;
    int key_defined;
    uc_vector palette;
} PNG_Info;

static void PNG_Info_init(PNG_Info* info) {
    info->key_defined = 0;
    uc_vector_init(&info->palette);
}

static void PNG_Info_free(PNG_Info* info) {
    uc_vector_free(&info->palette);
}

typedef struct PNG {
    PNG_Info info;
    int error;
} PNG;

static unsigned long read32bitInt(const unsigned char* buffer) {
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

static int checkColorValidity(unsigned long colorType, unsigned long bd) {
    if((colorType == 2 || colorType == 4 || colorType == 6)) { if(!(bd == 8 || bd == 16)) return 37; else return 0; }
    else if(colorType == 0) { if(!(bd == 1 || bd == 2 || bd == 4 || bd == 8 || bd == 16)) return 37; else return 0; }
    else if(colorType == 3) { if(!(bd == 1 || bd == 2 || bd == 4 || bd == 8)) return 37; else return 0; }
    else return 31;
}

static unsigned long getBpp(const PNG_Info* info) {
    if(info->colorType == 2) return (3 * info->bitDepth);
    else if(info->colorType >= 4) return (info->colorType - 2) * info->bitDepth;
    else return info->bitDepth;
}

static unsigned char paethPredictor(short a, short b, short c) {
    short p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (unsigned char)((pa <= pb && pa <= pc) ? a : pb <= pc ? b : c);
}

static void unFilterScanline(PNG* png, unsigned char* recon, const unsigned char* scanline, const unsigned char* precon, size_t bytewidth, unsigned long filterType, size_t length) {
    switch(filterType) {
        case 0: for(size_t i = 0; i < length; i++) recon[i] = scanline[i]; break;
        case 1:
            for(size_t i = 0; i < bytewidth; i++) recon[i] = scanline[i];
            for(size_t i = bytewidth; i < length; i++) recon[i] = scanline[i] + recon[i - bytewidth];
            break;
        case 2:
            if(precon) for(size_t i = 0; i < length; i++) recon[i] = scanline[i] + precon[i];
            else for(size_t i = 0; i < length; i++) recon[i] = scanline[i];
            break;
        case 3:
            if(precon) {
                for(size_t i = 0; i < bytewidth; i++) recon[i] = scanline[i] + precon[i] / 2;
                for(size_t i = bytewidth; i < length; i++) recon[i] = scanline[i] + ((recon[i - bytewidth] + precon[i]) / 2);
            } else {
                for(size_t i = 0; i < bytewidth; i++) recon[i] = scanline[i];
                for(size_t i = bytewidth; i < length; i++) recon[i] = scanline[i] + recon[i - bytewidth] / 2;
            }
            break;
        case 4:
            if(precon) {
                for(size_t i = 0; i < bytewidth; i++) recon[i] = scanline[i] + paethPredictor(0, precon[i], 0);
                for(size_t i = bytewidth; i < length; i++) recon[i] = scanline[i] + paethPredictor(recon[i - bytewidth], precon[i], precon[i - bytewidth]);
            } else {
                for(size_t i = 0; i < bytewidth; i++) recon[i] = scanline[i];
                for(size_t i = bytewidth; i < length; i++) recon[i] = scanline[i] + paethPredictor(recon[i - bytewidth], 0, 0);
            }
            break;
        default: png->error = 36; return;
    }
}

static unsigned long readBitFromReversedStream(size_t* bitp, const unsigned char* bits) {
    unsigned long result = (bits[*bitp >> 3] >> (7 - (*bitp & 0x7))) & 1;
    (*bitp)++;
    return result;
}

static void setBitOfReversedStream(size_t* bitp, unsigned char* bits, unsigned long bit) {
    bits[*bitp >> 3] |= (bit << (7 - (*bitp & 0x7)));
    (*bitp)++;
}

static void adam7Pass(PNG* png, unsigned char* out, uc_vector* scanlinen, uc_vector* scanlineo, const unsigned char* in, unsigned long w, size_t passleft, size_t passtop, size_t spacex, size_t spacey, size_t passw, size_t passh, unsigned long bpp) {
    if(passw == 0) return;
    size_t bytewidth = (bpp + 7) / 8, linelength = 1 + ((bpp * passw + 7) / 8);
    for(unsigned long y = 0; y < passh; y++) {
        unsigned char filterType = in[y * linelength];
        unsigned char* prevline = (y == 0) ? 0 : scanlineo->data;
        unFilterScanline(png, scanlinen->data, &in[y * linelength + 1], prevline, bytewidth, filterType, (w * bpp + 7) / 8);
        if(png->error) return;
        if(bpp >= 8) {
            for(size_t i = 0; i < passw; i++) {
                for(size_t b = 0; b < bytewidth; b++) {
                    out[bytewidth * w * (passtop + spacey * y) + bytewidth * (passleft + spacex * i) + b] = scanlinen->data[bytewidth * i + b];
                }
            }
        } else {
            for(size_t i = 0; i < passw; i++) {
                size_t obp = bpp * w * (passtop + spacey * y) + bpp * (passleft + spacex * i), bp = i * bpp;
                for(size_t b = 0; b < bpp; b++) {
                    unsigned long bit = readBitFromReversedStream(&bp, scanlinen->data);
                    setBitOfReversedStream(&obp, out, bit);
                }
            }
        }
        unsigned char* temp = scanlinen->data; scanlinen->data = scanlineo->data; scanlineo->data = temp;
    }
}

static int convert(uc_vector* out, const unsigned char* in, PNG_Info* infoIn, unsigned long w, unsigned long h) {
    size_t numpixels = w * h, bp = 0;
    uc_vector_resize(out, numpixels * 4);
    unsigned char* out_ = out->data;

    if(infoIn->bitDepth == 8 && infoIn->colorType == 0) {
        for(size_t i = 0; i < numpixels; i++) {
            out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[i];
            out_[4 * i + 3] = (infoIn->key_defined && in[i] == infoIn->key_r) ? 0 : 255;
        }
    } else if(infoIn->bitDepth == 8 && infoIn->colorType == 2) {
        for(size_t i = 0; i < numpixels; i++) {
            for(size_t c = 0; c < 3; c++) out_[4 * i + c] = in[3 * i + c];
            out_[4 * i + 3] = (infoIn->key_defined && in[3 * i + 0] == infoIn->key_r && in[3 * i + 1] == infoIn->key_g && in[3 * i + 2] == infoIn->key_b) ? 0 : 255;
        }
    } else if(infoIn->bitDepth == 8 && infoIn->colorType == 3) {
        for(size_t i = 0; i < numpixels; i++) {
            if(4U * in[i] >= infoIn->palette.size) return 46;
            for(size_t c = 0; c < 4; c++) out_[4 * i + c] = infoIn->palette.data[4 * in[i] + c];
        }
    } else if(infoIn->bitDepth == 8 && infoIn->colorType == 4) {
        for(size_t i = 0; i < numpixels; i++) {
            out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[2 * i + 0];
            out_[4 * i + 3] = in[2 * i + 1];
        }
    } else if(infoIn->bitDepth == 8 && infoIn->colorType == 6) {
        memcpy(out_, in, numpixels * 4);
    } else if(infoIn->bitDepth == 16 && infoIn->colorType == 0) {
        for(size_t i = 0; i < numpixels; i++) {
            out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[2 * i];
            out_[4 * i + 3] = (infoIn->key_defined && 256U * in[i] + in[i + 1] == infoIn->key_r) ? 0 : 255;
        }
    } else if(infoIn->bitDepth == 16 && infoIn->colorType == 2) {
        for(size_t i = 0; i < numpixels; i++) {
            for(size_t c = 0; c < 3; c++) out_[4 * i + c] = in[6 * i + 2 * c];
            out_[4 * i + 3] = (infoIn->key_defined && 256U*in[6*i+0]+in[6*i+1] == infoIn->key_r && 256U*in[6*i+2]+in[6*i+3] == infoIn->key_g && 256U*in[6*i+4]+in[6*i+5] == infoIn->key_b) ? 0 : 255;
        }
    } else if(infoIn->bitDepth == 16 && infoIn->colorType == 4) {
        for(size_t i = 0; i < numpixels; i++) {
            out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = in[4 * i];
            out_[4 * i + 3] = in[4 * i + 2];
        }
    } else if(infoIn->bitDepth == 16 && infoIn->colorType == 6) {
        for(size_t i = 0; i < numpixels; i++) for(size_t c = 0; c < 4; c++) out_[4 * i + c] = in[8 * i + 2 * c];
    } else if(infoIn->bitDepth < 8 && infoIn->colorType == 0) {
        for(size_t i = 0; i < numpixels; i++) {
            unsigned long value = (readBitFromReversedStream(&bp, in) * 255) / ((1 << infoIn->bitDepth) - 1);
            out_[4 * i + 0] = out_[4 * i + 1] = out_[4 * i + 2] = (unsigned char)(value);
            out_[4 * i + 3] = (infoIn->key_defined && value && ((1U << infoIn->bitDepth) - 1U) == infoIn->key_r && ((1U << infoIn->bitDepth) - 1U)) ? 0 : 255;
        }
    } else if(infoIn->bitDepth < 8 && infoIn->colorType == 3) {
        for(size_t i = 0; i < numpixels; i++) {
            unsigned long value = readBitFromReversedStream(&bp, in);
            if(4 * value >= infoIn->palette.size) return 47;
            for(size_t c = 0; c < 4; c++) out_[4 * i + c] = infoIn->palette.data[4 * value + c];
        }
    }
    return 0;
}

static void readPngHeader(PNG* png, const unsigned char* in, size_t inlength) {
    if(inlength < 29) { png->error = 27; return; }
    if(in[0] != 137 || in[1] != 80 || in[2] != 78 || in[3] != 71 || in[4] != 13 || in[5] != 10 || in[6] != 26 || in[7] != 10) { png->error = 28; return; }
    if(in[12] != 'I' || in[13] != 'H' || in[14] != 'D' || in[15] != 'R') { png->error = 29; return; }
    png->info.width = read32bitInt(&in[16]);
    png->info.height = read32bitInt(&in[20]);
    png->info.bitDepth = in[24];
    png->info.colorType = in[25];
    png->info.compressionMethod = in[26];
    if(in[26] != 0) { png->error = 32; return; }
    png->info.filterMethod = in[27];
    if(in[27] != 0) { png->error = 33; return; }
    png->info.interlaceMethod = in[28];
    if(in[28] > 1) { png->error = 34; return; }
    png->error = checkColorValidity(png->info.colorType, png->info.bitDepth);
}

static void PNG_decode(PNG* png, uc_vector* out, const unsigned char* in, size_t size, int convert_to_rgba32) {
    png->error = 0;
    if(size == 0 || in == 0) { png->error = 48; return; }
    readPngHeader(png, &in[0], size);
    if(png->error) return;

    size_t pos = 33;
    uc_vector idat;
    uc_vector_init(&idat);
    int IEND = 0;

    while(!IEND) {
        if(pos + 8 >= size) { png->error = 30; goto cleanup; }
        size_t chunkLength = read32bitInt(&in[pos]); pos += 4;
        if(chunkLength > 2147483647) { png->error = 63; goto cleanup; }
        if(pos + chunkLength >= size) { png->error = 35; goto cleanup; }
        
        if(in[pos + 0] == 'I' && in[pos + 1] == 'D' && in[pos + 2] == 'A' && in[pos + 3] == 'T') {
            uc_vector_insert(&idat, &in[pos + 4], chunkLength);
            pos += (4 + chunkLength);
        } else if(in[pos + 0] == 'I' && in[pos + 1] == 'E' && in[pos + 2] == 'N' && in[pos + 3] == 'D') {
            pos += 4;
            IEND = 1;
        } else if(in[pos + 0] == 'P' && in[pos + 1] == 'L' && in[pos + 2] == 'T' && in[pos + 3] == 'E') {
            pos += 4;
            uc_vector_resize(&png->info.palette, 4 * (chunkLength / 3));
            if(png->info.palette.size > (4 * 256)) { png->error = 38; goto cleanup; }
            for(size_t i = 0; i < png->info.palette.size; i += 4) {
                for(size_t j = 0; j < 3; j++) png->info.palette.data[i + j] = in[pos++];
                png->info.palette.data[i + 3] = 255;
            }
        } else if(in[pos + 0] == 't' && in[pos + 1] == 'R' && in[pos + 2] == 'N' && in[pos + 3] == 'S') {
            pos += 4;
            if(png->info.colorType == 3) {
                if(4 * chunkLength > png->info.palette.size) { png->error = 39; goto cleanup; }
                for(size_t i = 0; i < chunkLength; i++) png->info.palette.data[4 * i + 3] = in[pos++];
            } else if(png->info.colorType == 0) {
                if(chunkLength != 2) { png->error = 40; goto cleanup; }
                png->info.key_defined = 1;
                png->info.key_r = png->info.key_g = png->info.key_b = 256 * in[pos] + in[pos + 1]; pos += 2;
            } else if(png->info.colorType == 2) {
                if(chunkLength != 6) { png->error = 41; goto cleanup; }
                png->info.key_defined = 1;
                png->info.key_r = 256 * in[pos] + in[pos + 1]; pos += 2;
                png->info.key_g = 256 * in[pos] + in[pos + 1]; pos += 2;
                png->info.key_b = 256 * in[pos] + in[pos + 1]; pos += 2;
            } else { png->error = 42; goto cleanup; }
        } else {
            if(!(in[pos + 0] & 32)) { png->error = 69; goto cleanup; }
            pos += (chunkLength + 4);
        }
        pos += 4;
    }

    unsigned long bpp = getBpp(&png->info);
    uc_vector scanlines;
    uc_vector_init(&scanlines);
    uc_vector_resize(&scanlines, ((png->info.width * (png->info.height * bpp + 7)) / 8) + png->info.height);
    png->error = decompress(&scanlines, &idat);
    if(png->error) goto cleanup;

    size_t bytewidth = (bpp + 7) / 8, outlength = (png->info.height * png->info.width * bpp + 7) / 8;
    uc_vector_resize(out, outlength);
    unsigned char* out_ = out->size ? out->data : 0;

    if(png->info.interlaceMethod == 0) {
        size_t linestart = 0, linelength = (png->info.width * bpp + 7) / 8;
        if(bpp >= 8) {
            for(unsigned long y = 0; y < png->info.height; y++) {
                unsigned long filterType = scanlines.data[linestart];
                const unsigned char* prevline = (y == 0) ? 0 : &out_[(y - 1) * png->info.width * bytewidth];
                unFilterScanline(png, &out_[linestart - y], &scanlines.data[linestart + 1], prevline, bytewidth, filterType, linelength);
                if(png->error) goto cleanup;
                linestart += (1 + linelength);
            }
        } else {
            uc_vector templine;
            uc_vector_init(&templine);
            uc_vector_resize(&templine, (png->info.width * bpp + 7) >> 3);
            for(size_t y = 0, obp = 0; y < png->info.height; y++) {
                unsigned long filterType = scanlines.data[linestart];
                const unsigned char* prevline = (y == 0) ? 0 : &out_[(y - 1) * png->info.width * bytewidth];
                unFilterScanline(png, templine.data, &scanlines.data[linestart + 1], prevline, bytewidth, filterType, linelength);
                if(png->error) { uc_vector_free(&templine); goto cleanup; }
                for(size_t bp = 0; bp < png->info.width * bpp;) {
                    unsigned long bit = readBitFromReversedStream(&bp, templine.data);
                    setBitOfReversedStream(&obp, out->data, bit);
                }
                linestart += (1 + linelength);
            }
            uc_vector_free(&templine);
        }
    } else {
        size_t passw[7] = { (png->info.width + 7) / 8, (png->info.width + 3) / 8, (png->info.width + 3) / 4, (png->info.width + 1) / 4, (png->info.width + 1) / 2, (png->info.width + 0) / 2, (png->info.width + 0) / 1 };
        size_t passh[7] = { (png->info.height + 7) / 8, (png->info.height + 7) / 8, (png->info.height + 3) / 8, (png->info.height + 3) / 4, (png->info.height + 1) / 4, (png->info.height + 1) / 2, (png->info.height + 0) / 2 };
        size_t passstart[7] = {0};
        size_t pattern[28] = {0,4,0,2,0,1,0,0,0,4,0,2,0,1,8,8,4,4,2,2,1,8,8,8,4,4,2,2};
        for(int i = 0; i < 6; i++) passstart[i + 1] = passstart[i] + passh[i] * ((passw[i] ? 1 : 0) + (passw[i] * bpp + 7) / 8);
        
        uc_vector scanlineo, scanlinen;
        uc_vector_init(&scanlineo);
        uc_vector_init(&scanlinen);
        uc_vector_resize(&scanlineo, (png->info.width * bpp + 7) / 8);
        uc_vector_resize(&scanlinen, (png->info.width * bpp + 7) / 8);

        for(int i = 0; i < 7; i++) {
            adam7Pass(png, out_, &scanlinen, &scanlineo, &scanlines.data[passstart[i]], png->info.width, pattern[i], pattern[i + 7], pattern[i + 14], pattern[i + 21], passw[i], passh[i], bpp);
        }
        uc_vector_free(&scanlineo);
        uc_vector_free(&scanlinen);
    }

    if(convert_to_rgba32 && (png->info.colorType != 6 || png->info.bitDepth != 8)) {
        uc_vector data;
        uc_vector_init(&data);
        uc_vector_resize(&data, out->size);
        memcpy(data.data, out->data, out->size);
        png->error = convert(out, data.data, &png->info, png->info.width, png->info.height);
        uc_vector_free(&data);
    }

cleanup:
    uc_vector_free(&idat);
    uc_vector_free(&scanlines);
}

int zest__decode_png(unsigned char** out_image, unsigned long* image_width, unsigned long* image_height, size_t* out_size, const unsigned char* in_png, size_t in_size, int convert_to_rgba32) {
    PNG decoder;
    PNG_Info_init(&decoder.info);
    uc_vector out_vec;
    uc_vector_init(&out_vec);

    PNG_decode(&decoder, &out_vec, in_png, in_size, convert_to_rgba32);

    *image_width = decoder.info.width;
    *image_height = decoder.info.height;
    
    if (decoder.error == 0) {
        *out_image = out_vec.data;
        *out_size = out_vec.size;
    } else {
        *out_image = NULL;
        *out_size = 0;
        uc_vector_free(&out_vec);
    }

    PNG_Info_free(&decoder.info);
    return decoder.error;
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* PICOPNG_H */
