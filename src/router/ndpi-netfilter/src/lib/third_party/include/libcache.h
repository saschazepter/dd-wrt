/**
 * libcache.h
 *
Copyright (c) 2017 William Guglielmo <william@deselmo.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif


#ifndef __DESELMO_LIBCACHE_H__
#define __DESELMO_LIBCACHE_H__

#ifndef __KERNEL__
#include <stdint.h>
#endif

/**
 * @brief Codes representing the result of some functions
 *
 */
typedef enum cache_result {
  CACHE_NO_ERROR = 0,         /**< Returned by a function if no error occurs. */
  CACHE_CONTAINS_FALSE = 0,   /**< Returned by function cache_contains if item is not present. */
  CACHE_CONTAINS_TRUE,        /**< Returned by function cache_contains if item is present. */
  CACHE_INVALID_INPUT,        /**< Returned by a function if it is called with invalid input parameters. */
  CACHE_REMOVE_NOT_FOUND,     /**< Returned by function cache_remove if item is not present. */
  CACHE_MALLOC_ERROR          /**< Returned by a function if a malloc fail. */
} cache_result;


typedef struct cache *cache_t;
typedef int (*cache_item_compare)(const void *item1, size_t item1_size,
                                  const void *item2, size_t item2_size);
typedef uint32_t (*cache_item_hash)(const void *item, size_t item_size);

/**
 * @brief Returns a new cache_t
 * 
 * @par    cache_max_size  = max number of item that the new cache_t can contain
 * @par    cache_hash_size = size of hash table. May be 0
 * @return a new cache_t, or NULL if an error occurred
 *
 */
NDPI_STATIC cache_t cache_new(uint32_t cache_max_size, uint32_t cache_hash_size);


/**
 * @brief Add an item in the specified cache_t
 * 
 * @par    cache      = the cache_t
 * @par    item       = pointer to the item to add
 * @par    item_size  = size of the item
 * @return a code representing the result of the function
 *
 */
NDPI_STATIC cache_result cache_add(cache_t cache, void *item, size_t item_size);


/**
 * @brief Check if an item is in the specified cache_t
 * 
 * @par    cache      = the cache_t
 * @par    item       = pointer to the item to check
 * @par    item_size  = size of the item
 * @return a pointer to found item
 *
 */
NDPI_STATIC void *cache_contains(cache_t cache, void *item, size_t item_size);


/**
 * @brief Remove an item in the specified cache_t
 * 
 * @par    cache      = the cache_t
 * @par    item       = pointer to the item to remove
 * @par    item_size  = size of the item
 * @return a code representing the result of the function
 *
 */
NDPI_STATIC cache_result cache_remove(cache_t cache, void *item, size_t item_size);

/**
 * @brief Free the specified cache_t
 * 
 * @par alist  = the cache
 *
 */
NDPI_STATIC void cache_free(cache_t cache);

#ifndef __KERNEL__
NDPI_STATIC void cache_dump(cache_t cache);
#endif

#endif

#ifdef __cplusplus
}
#endif
