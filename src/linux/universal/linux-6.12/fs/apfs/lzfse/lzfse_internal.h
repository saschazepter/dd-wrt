/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2015-2016, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder(s) nor the names of any contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LZFSE_INTERNAL_H
#define LZFSE_INTERNAL_H

#include "lzfse_fse.h"
#include <linux/limits.h>
#include <linux/stddef.h>

/*
 * Throughout LZFSE we refer to "L", "M" and "D"; these will always appear as
 * a triplet, and represent a "usual" LZ-style literal and match pair. "L"
 * is the number of literal bytes, "M" is the number of match bytes, and "D"
 * is the match "distance"; the distance in bytes between the current pointer
 * and the start of the match.
 */
#define LZFSE_ENCODE_HASH_VALUES (1 << LZFSE_ENCODE_HASH_BITS)
#define LZFSE_ENCODE_L_SYMBOLS 20
#define LZFSE_ENCODE_M_SYMBOLS 20
#define LZFSE_ENCODE_D_SYMBOLS 64
#define LZFSE_ENCODE_LITERAL_SYMBOLS 256
#define LZFSE_ENCODE_L_STATES 64
#define LZFSE_ENCODE_M_STATES 64
#define LZFSE_ENCODE_D_STATES 256
#define LZFSE_ENCODE_LITERAL_STATES 1024
#define LZFSE_MATCHES_PER_BLOCK 10000
#define LZFSE_LITERALS_PER_BLOCK (4 * LZFSE_MATCHES_PER_BLOCK)
#define LZFSE_DECODE_LITERALS_PER_BLOCK (4 * LZFSE_DECODE_MATCHES_PER_BLOCK)

/*
 * LZFSE internal status. These values are used by internal LZFSE routines
 * as return codes. There should not be any good reason to change their
 * values; it is plausible that additional codes might be added in the
 * future.
 */
#define LZFSE_STATUS_OK 0
#define LZFSE_STATUS_SRC_EMPTY -1
#define LZFSE_STATUS_DST_FULL -2
#define LZFSE_STATUS_ERROR -3

/*
 * Type representing an offset between elements in a buffer. On 64-bit
 * systems, this is stored in a 64-bit container to avoid extra sign-
 * extension operations in addressing arithmetic, but the value is always
 * representable as a 32-bit signed value in LZFSE's usage.
 */
#if defined(_M_AMD64) || defined(__x86_64__) || defined(__arm64__)
typedef int64_t lzfse_offset;
#else
typedef int32_t lzfse_offset;
#endif

typedef uint64_t uintmax_t;

/*! @abstract Decoder state object for lzfse compressed blocks. */
typedef struct {
	/* Number of matches remaining in the block. */
	uint32_t n_matches;
	/* Number of bytes used to encode L, M, D triplets for the block. */
	uint32_t n_lmd_payload_bytes;
	/* Pointer to the next literal to emit. */
	const uint8_t *current_literal;
	/*
	 * L, M, D triplet for the match currently being emitted. This is used
	 * only if we need to restart after reaching the end of the destination
	 * buffer in the middle of a literal or match.
	 */
	int32_t l_value, m_value, d_value;
	/* FSE stream object. */
	fse_in_stream lmd_in_stream;
	/*
	 * Offset of L,M,D encoding in the input buffer. Because we read through
	 * an FSE stream *backwards* while decoding, this is decremented as we
	 * move through a block.
	 */
	uint32_t lmd_in_buf;
	/* The current state of the L, M, and D FSE decoders. */
	uint16_t l_state, m_state, d_state;
	/*
	 * Internal FSE decoder tables for the current block. These have
	 * alignment forced to 8 bytes to guarantee that a single state's
	 * entry cannot span two cachelines.
	 */
	struct fse_value_decoder_entry l_decoder[LZFSE_ENCODE_L_STATES] __aligned(8);
	struct fse_value_decoder_entry m_decoder[LZFSE_ENCODE_M_STATES] __aligned(8);
	struct fse_value_decoder_entry d_decoder[LZFSE_ENCODE_D_STATES] __aligned(8);
	int32_t literal_decoder[LZFSE_ENCODE_LITERAL_STATES];
	/*
	 * The literal stream for the block, plus padding to allow for faster
	 * copy operations.
	 */
	uint8_t literals[LZFSE_LITERALS_PER_BLOCK + 64];
} lzfse_compressed_block_decoder_state;

/* Decoder state object for uncompressed blocks. */
typedef struct {
	uint32_t n_raw_bytes;
} uncompressed_block_decoder_state;

/*! @abstract Decoder state object for lzvn-compressed blocks. */
typedef struct {
	uint32_t n_raw_bytes;
	uint32_t n_payload_bytes;
	uint32_t d_prev;
} lzvn_compressed_block_decoder_state;

/*! @abstract Decoder state object. */
typedef struct {
	/*
	 * Pointer to next byte to read from source buffer (this is advanced as
	 * we decode; src_begin describe the buffer and do not change).
	 */
	const uint8_t *src;
	/* Pointer to first byte of source buffer. */
	const uint8_t *src_begin;
	/* Pointer to one byte past the end of the source buffer. */
	const uint8_t *src_end;
	/*
	 * Pointer to the next byte to write to destination buffer (this is
	 * advanced as we decode; dst_begin and dst_end describe the buffer and
	 * do not change).
	 */
	uint8_t *dst;
	/* Pointer to first byte of destination buffer. */
	uint8_t *dst_begin;
	/* Pointer to one byte past the end of the destination buffer. */
	uint8_t *dst_end;
	/* 1 if we have reached the end of the stream, 0 otherwise. */
	int end_of_stream;
	/*
	 * magic number of the current block if we are within a block,
	 * LZFSE_NO_BLOCK_MAGIC otherwise.
	 */
	uint32_t block_magic;
	lzfse_compressed_block_decoder_state compressed_lzfse_block_state;
	lzvn_compressed_block_decoder_state compressed_lzvn_block_state;
	uncompressed_block_decoder_state uncompressed_block_state;
} lzfse_decoder_state;

/* MARK: - Block header objects */

#define LZFSE_NO_BLOCK_MAGIC 0x00000000 /* 0    (invalid) */
#define LZFSE_ENDOFSTREAM_BLOCK_MAGIC 0x24787662 /* bvx$ (end of stream) */
#define LZFSE_UNCOMPRESSED_BLOCK_MAGIC 0x2d787662 /* bvx- (raw data) */
#define LZFSE_COMPRESSEDV1_BLOCK_MAGIC 0x31787662 /* bvx1 (lzfse compressed, uncompressed tables) */
#define LZFSE_COMPRESSEDV2_BLOCK_MAGIC 0x32787662 /* bvx2 (lzfse compressed, compressed tables) */
#define LZFSE_COMPRESSEDLZVN_BLOCK_MAGIC 0x6e787662 /* bvxn (lzvn compressed) */

/*! @abstract Uncompressed block header in encoder stream. */
typedef struct {
	/* Magic number, always LZFSE_UNCOMPRESSED_BLOCK_MAGIC. */
	uint32_t magic;
	/* Number of raw bytes in block. */
	uint32_t n_raw_bytes;
} uncompressed_block_header;

/*! @abstract Compressed block header with uncompressed tables. */
typedef struct {
	/* Magic number, always LZFSE_COMPRESSEDV1_BLOCK_MAGIC. */
	uint32_t magic;
	/* Number of decoded (output) bytes in block. */
	uint32_t n_raw_bytes;
	/* Number of encoded (source) bytes in block. */
	uint32_t n_payload_bytes;
	/* Number of literal bytes output by block (*not* the number of literals). */
	uint32_t n_literals;
	/* Number of matches in block (which is also the number of literals). */
	uint32_t n_matches;
	/* Number of bytes used to encode literals. */
	uint32_t n_literal_payload_bytes;
	/* Number of bytes used to encode matches. */
	uint32_t n_lmd_payload_bytes;

	/*
	 * Final encoder states for the block, which will be the initial states
	 * for the decoder:
	 */
	/* Final accum_nbits for literals stream. */
	int32_t literal_bits;
	/*
	 * There are four interleaved streams of literals, so there are four
	 * final states.
	 */
	uint16_t literal_state[4];
	/* accum_nbits for the l, m, d stream. */
	int32_t lmd_bits;
	/* Final L (literal length) state. */
	uint16_t l_state;
	/* Final M (match length) state. */
	uint16_t m_state;
	/* Final D (match distance) state. */
	uint16_t d_state;

	/*
	 * Normalized frequency tables for each stream. Sum of values in each
	 * array is the number of states.
	 */
	uint16_t l_freq[LZFSE_ENCODE_L_SYMBOLS];
	uint16_t m_freq[LZFSE_ENCODE_M_SYMBOLS];
	uint16_t d_freq[LZFSE_ENCODE_D_SYMBOLS];
	uint16_t literal_freq[LZFSE_ENCODE_LITERAL_SYMBOLS];
} lzfse_compressed_block_header_v1;

/*! @abstract Compressed block header with compressed tables. Note that because
 *  freq[] is compressed, the structure-as-stored-in-the-stream is *truncated*;
 *  we only store the used bytes of freq[]. This means that some extra care must
 *  be taken when reading one of these headers from the stream.
 */
typedef struct {
	/* Magic number, always LZFSE_COMPRESSEDV2_BLOCK_MAGIC. */
	uint32_t magic;
	/* Number of decoded (output) bytes in block. */
	uint32_t n_raw_bytes;
	/*
	 * The fields n_payload_bytes ... d_state from the
	 * lzfse_compressed_block_header_v1 object are packed into three 64-bit
	 * fields in the compressed header, as follows:
	 *
	 *    offset  bits  value
	 *    0       20    n_literals
	 *    20      20    n_literal_payload_bytes
	 *    40      20    n_matches
	 *    60      3     literal_bits
	 *    63      1     --- unused ---
	 *
	 *    0       10    literal_state[0]
	 *    10      10    literal_state[1]
	 *    20      10    literal_state[2]
	 *    30      10    literal_state[3]
	 *    40      20    n_lmd_payload_bytes
	 *    60      3     lmd_bits
	 *    63      1     --- unused ---
	 *
	 *    0       32    header_size (total header size in bytes; this does not
	 *                  correspond to a field in the uncompressed header version,
	 *                  but is required; we wouldn't know the size of the
	 *                  compresssed header otherwise.
	 *    32      10    l_state
	 *    42      10    m_state
	 *    52      10    d_state
	 *    62      2     --- unused ---
	 */
	uint64_t packed_fields[3];
	/*
	 * Variable size freq tables, using a Huffman-style fixed encoding.
	 * Size allocated here is an upper bound (all values stored on 16 bits).
	 */
	uint8_t freq[2 * (LZFSE_ENCODE_L_SYMBOLS + LZFSE_ENCODE_M_SYMBOLS + LZFSE_ENCODE_D_SYMBOLS +
			  LZFSE_ENCODE_LITERAL_SYMBOLS)];
} __packed __aligned(1) lzfse_compressed_block_header_v2;

/*! @abstract LZVN compressed block header. */
typedef struct {
	/* Magic number, always LZFSE_COMPRESSEDLZVN_BLOCK_MAGIC. */
	uint32_t magic;
	/* Number of decoded (output) bytes. */
	uint32_t n_raw_bytes;
	/* Number of encoded (source) bytes. */
	uint32_t n_payload_bytes;
} lzvn_compressed_block_header;

int lzfse_decode(lzfse_decoder_state *s);
size_t lzvn_decode_scratch_size(void);
size_t lzvn_decode_buffer(void *__restrict dst, size_t dst_size, const void *__restrict src,
			  size_t src_size);

/*! @abstract Signed offset in buffers, stored on either 32 or 64 bits. */
#if defined(_M_AMD64) || defined(__x86_64__) || defined(__arm64__)
typedef int64_t lzvn_offset;
#else
typedef int32_t lzvn_offset;
#endif

/* MARK: - LZFSE utility functions */

/*! @abstract Load bytes from memory location SRC. */
static __always_inline uint16_t load2(const void *ptr)
{
	uint16_t data;

	memcpy(&data, ptr, sizeof(data));
	return data;
}

static __always_inline uint32_t load4(const void *ptr)
{
	uint32_t data;

	memcpy(&data, ptr, sizeof(data));
	return data;
}

static __always_inline uint64_t load8(const void *ptr)
{
	uint64_t data;

	memcpy(&data, ptr, sizeof(data));
	return data;
}

/*! @abstract Store bytes to memory location DST. */
static __always_inline void store2(void *ptr, uint16_t data)
{
	memcpy(ptr, &data, sizeof(data));
}

static __always_inline void store4(void *ptr, uint32_t data)
{
	memcpy(ptr, &data, sizeof(data));
}

static __always_inline void store8(void *ptr, uint64_t data)
{
	memcpy(ptr, &data, sizeof(data));
}

/*! @abstract Load+store bytes from locations SRC to DST. Not intended for use
 * with overlapping buffers. Note that for LZ-style compression, you need
 * copies to behave like naive memcpy( ) implementations do, splatting the
 * leading sequence if the buffers overlap. This copy does not do that, so
 * should not be used with overlapping buffers.
 */
static __always_inline void copy8(void *dst, const void *src)
{
	store8(dst, load8(src));
}
static __always_inline void copy16(void *dst, const void *src)
{
	uint64_t m0 = load8(src);
	uint64_t m1 = load8((const unsigned char *)src + 8);

	store8(dst, m0);
	store8((unsigned char *)dst + 8, m1);
}

/*
 * ===============================================================
 * Bitfield Operations
 */

/*! @abstract Extracts \p width bits from \p container, starting with \p lsb; if
 * we view \p container as a bit array, we extract \c container[lsb:lsb+width].
 */
static __always_inline uintmax_t extract(uintmax_t container, unsigned int lsb, unsigned int width)
{
	static const size_t container_width = sizeof(container) * 8;

	if (width == container_width)
		return container;
	return (container >> lsb) & (((uintmax_t)1 << width) - 1);
}

/*! @abstract Inserts \p width bits from \p data into \p container, starting with \p lsb.
 *  Viewed as bit arrays, the operations is:
 * @code
 * container[:lsb] is unchanged
 * container[lsb:lsb+width] <-- data[0:width]
 * container[lsb+width:] is unchanged
 * @endcode
 */
static __always_inline uintmax_t insert(uintmax_t container, uintmax_t data, unsigned int lsb,
					unsigned int width)
{
	static const size_t container_width = sizeof(container) * 8;
	uintmax_t mask;

	if (width == container_width)
		return container;
	mask = ((uintmax_t)1 << width) - 1;
	return (container & ~(mask << lsb)) | (data & mask) << lsb;
}

/*! @abstract Perform sanity checks on the values of lzfse_compressed_block_header_v1.
 * Test that the field values are in the allowed limits, verify that the
 * frequency tables sum to value less than total number of states.
 * @return 0 if all tests passed.
 * @return negative error code with 1 bit set for each failed test.
 */
static __always_inline int
lzfse_check_block_header_v1(const lzfse_compressed_block_header_v1 *header)
{
	int tests_results = 0;
	uint16_t literal_state[4];
	int res;

	tests_results = tests_results |
			((header->magic == LZFSE_COMPRESSEDV1_BLOCK_MAGIC) ? 0 : (1 << 0));
	tests_results = tests_results |
			((header->n_literals <= LZFSE_LITERALS_PER_BLOCK) ? 0 : (1 << 1));
	tests_results = tests_results |
			((header->n_matches <= LZFSE_MATCHES_PER_BLOCK) ? 0 : (1 << 2));

	memcpy(literal_state, header->literal_state, sizeof(uint16_t) * 4);

	tests_results = tests_results |
			((literal_state[0] < LZFSE_ENCODE_LITERAL_STATES) ? 0 : (1 << 3));
	tests_results = tests_results |
			((literal_state[1] < LZFSE_ENCODE_LITERAL_STATES) ? 0 : (1 << 4));
	tests_results = tests_results |
			((literal_state[2] < LZFSE_ENCODE_LITERAL_STATES) ? 0 : (1 << 5));
	tests_results = tests_results |
			((literal_state[3] < LZFSE_ENCODE_LITERAL_STATES) ? 0 : (1 << 6));

	tests_results = tests_results | ((header->l_state < LZFSE_ENCODE_L_STATES) ? 0 : (1 << 7));
	tests_results = tests_results | ((header->m_state < LZFSE_ENCODE_M_STATES) ? 0 : (1 << 8));
	tests_results = tests_results | ((header->d_state < LZFSE_ENCODE_D_STATES) ? 0 : (1 << 9));

	res = fse_check_freq(header->l_freq, LZFSE_ENCODE_L_SYMBOLS, LZFSE_ENCODE_L_STATES);
	tests_results = tests_results | ((res == 0) ? 0 : (1 << 10));
	res = fse_check_freq(header->m_freq, LZFSE_ENCODE_M_SYMBOLS, LZFSE_ENCODE_M_STATES);
	tests_results = tests_results | ((res == 0) ? 0 : (1 << 11));
	res = fse_check_freq(header->d_freq, LZFSE_ENCODE_D_SYMBOLS, LZFSE_ENCODE_D_STATES);
	tests_results = tests_results | ((res == 0) ? 0 : (1 << 12));
	res = fse_check_freq(header->literal_freq, LZFSE_ENCODE_LITERAL_SYMBOLS,
			     LZFSE_ENCODE_LITERAL_STATES);
	tests_results = tests_results | ((res == 0) ? 0 : (1 << 13));

	if (tests_results) {
		/* each 1 bit is a test that failed (except for the sign bit) */
		return tests_results | 0x80000000;
	}

	return 0; /* OK */
}

/*! @abstract The L, M, D data streams are all encoded as a "base" value, which is
 * FSE-encoded, and an "extra bits" value, which is the difference between
 * value and base, and is simply represented as a raw bit value (because it
 * is the low-order bits of a larger number, not much entropy can be
 * extracted from these bits by more complex encoding schemes). The following
 * tables represent the number of low-order bits to encode separately and the
 * base values for each of L, M, and D.
 *
 * @note The inverse tables for mapping the other way are significantly larger.
 * Those tables have been split out to lzfse_encode_tables.h in order to keep
 * this file relatively small.
 */
static const uint8_t l_extra_bits[LZFSE_ENCODE_L_SYMBOLS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
							      0, 0, 0, 0, 0, 0, 2, 3, 5, 8 };
static const int32_t l_base_value[LZFSE_ENCODE_L_SYMBOLS] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 28, 60
};
static const uint8_t m_extra_bits[LZFSE_ENCODE_M_SYMBOLS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
							      0, 0, 0, 0, 0, 0, 3, 5, 8, 11 };
static const int32_t m_base_value[LZFSE_ENCODE_M_SYMBOLS] = { 0,  1,  2,  3,  4,  5,  6,
							      7,  8,  9,  10, 11, 12, 13,
							      14, 15, 16, 24, 56, 312 };
static const uint8_t d_extra_bits[LZFSE_ENCODE_D_SYMBOLS] = {
	0,  0,	0,  0,	1,  1,	1,  1,	2,  2,	2,  2,	3,  3,	3,  3,	4,  4,	4,  4,	5,  5,
	5,  5,	6,  6,	6,  6,	7,  7,	7,  7,	8,  8,	8,  8,	9,  9,	9,  9,	10, 10, 10, 10,
	11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15
};
static const int32_t d_base_value[LZFSE_ENCODE_D_SYMBOLS] = {
	0,     1,     2,     3,	    4,	    6,	    8,	    10,	    12,	   16,	  20,
	24,    28,    36,    44,    52,	    60,	    76,	    92,	    108,   124,	  156,
	188,   220,   252,   316,   380,    444,    508,    636,    764,   892,	  1020,
	1276,  1532,  1788,  2044,  2556,   3068,   3580,   4092,   5116,  6140,  7164,
	8188,  10236, 12284, 14332, 16380,  20476,  24572,  28668,  32764, 40956, 49148,
	57340, 65532, 81916, 98300, 114684, 131068, 163836, 196604, 229372
};

#endif /* LZFSE_INTERNAL_H */
