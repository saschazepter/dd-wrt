#pragma once
/*********************************************************************
* Filename:   sha256.h
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Defines the API for the corresponding SHA1 implementation.
*********************************************************************/

#ifndef SHA256_H
#define SHA256_H

#include "stdint.h"

/*************************** HEADER FILES ***************************/
#include <stddef.h>

/****************************** MACROS ******************************/
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

typedef struct {
	uint8_t data[64];
	uint32_t datalen;
	unsigned long long bitlen;
	uint32_t state[8];
} SHA256_CONTEXT;



/*********************** FUNCTION DECLARATIONS **********************/
void SHA256Init(SHA256_CONTEXT* ctx);
void SHA256Update(SHA256_CONTEXT* ctx, const uint8_t data[], size_t len);
void SHA256Final(SHA256_CONTEXT* ctx, uint8_t hash[]);

#endif   // SHA256_H