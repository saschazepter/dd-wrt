/*
 * $Id: patricia.c,v 1.7 2005/12/07 20:46:41 dplonka Exp $
 * Dave Plonka <plonka@doit.wisc.edu>
 *
 * This product includes software developed by the University of Michigan,
 * Merit Network, Inc., and their contributors. 
 *
 * This file had been called "radix.c" in the MRT sources.
 *
 * I renamed it to "patricia.c" since it's not an implementation of a general
 * radix trie.  Also I pulled in various requirements from "prefix.c" and
 * "demo.c" so that it could be used as a standalone API.


 https://github.com/deepfield/MRT/blob/master/COPYRIGHT

 Copyright (c) 1999-2013
 
 The Regents of the University of Michigan ("The Regents") and Merit
 Network, Inc.
 
 Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 
 Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/

#ifndef __KERNEL__
#include <assert.h> /* assert */
#include <errno.h> /* errno */
#include <math.h> /* sin */
#include <stddef.h> /* NULL */
#include <stdio.h> /* sprintf, fprintf, stderr */
#include <stdlib.h> /* free, atol, calloc */
#include <string.h> /* memcpy, strchr, strlen */
#include <sys/types.h> /* BSD: for inet_addr */
#if !defined(WIN32) && !defined(_MSC_VER)
#include <sys/socket.h> /* BSD, Linux: for inet_addr */
#include <netinet/in.h> /* BSD, Linux: for inet_addr */
#include <arpa/inet.h> /* BSD, Linux, Solaris: for inet_addr */
#endif
#else
#include <ndpi_kernel_compat.h>
#endif

#include "ndpi_patricia.h"

static void ndpi_DeleteEntry(void *a) {
  ndpi_free(a);
}

/* { from prefix.c */

/* ndpi_prefix_tochar
 * convert prefix information to bytes
 */
static u_char* ndpi_prefix_tochar (ndpi_prefix_t * prefix)
{
  unsigned short family;

  if(prefix == NULL)
    return (NULL);

  family = prefix->family;

  if(family == AF_INET) return ((u_char *) & prefix->add.sin);
  else if(family == AF_INET6) return ((u_char *) & prefix->add.sin6);
  else /* if(family == AF_MAC) */ return ((u_char *) & prefix->add.mac);
}

static int ndpi_comp_with_mask (void *addr, void *dest, u_int mask) {
  uint32_t *pa = addr;
  uint32_t *pd = dest;
  uint32_t m;
  for(;mask >= 32; mask -= 32, pa++,pd++)
    if(*pa != *pd) return 0;
  if(!mask) return 1;
  m = htonl((~0u) << (32-mask));
  return (*pa & m) == (*pd &m);
}

/* this allows incomplete prefix */
#ifndef __KERNEL__
static int ndpi_my_inet_pton (int af, const char *src, void *dst)
{
  if(af == AF_INET) {
    int i;
    u_char xp[sizeof(struct in_addr)] = {0, 0, 0, 0};

    for (i = 0; ; i++) {
      int c, val;

      c = *src++;
      if(!ndpi_isdigit (c))
	return (-1);
      val = 0;
      do {
	val = val * 10 + c - '0';
	if(val > 255)
	  return (0);
	c = *src++;
      } while (c && ndpi_isdigit (c));
      xp[i] = (u_char)val;
      if(c == '\0')
	break;
      if(c != '.')
	return (0);
      if(i >= 3)
	return (0);
    }
    memcpy (dst, xp, sizeof(struct in_addr));
    return (1);
  } else if(af == AF_INET6) {
    return (inet_pton (af, src, dst));
  } else {
#ifndef NT
    errno = EAFNOSUPPORT;
#endif /* NT */
    return -1;
  }
}
#else
#define ndpi_my_inet_pton(A,S,D) inet_pton(A,S,D)
#endif

#if 0
#define PATRICIA_MAX_THREADS		16

/* 
 * convert prefix information to ascii string with length
 * thread safe and (almost) re-entrant implementation
 */
static char * ndpi_prefix_toa2x (ndpi_prefix_t *prefix, char *buff, int with_len)
{
  if(prefix == NULL)
    return ((char*)"(Null)");
  assert (prefix->ref_count >= 0);
  if(buff == NULL) {

    struct buffer {
      char buffs[PATRICIA_MAX_THREADS][48+5];
      u_int i;
    } *buffp;

#    if 0
    THREAD_SPECIFIC_DATA (struct buffer, buffp, 1);
#    else
    { /* for scope only */
      static struct buffer local_buff;
      buffp = &local_buff;
    }
#    endif
    if(buffp == NULL) {
      /* XXX should we report an error? */
      return (NULL);
    }

    buff = buffp->buffs[buffp->i++%PATRICIA_MAX_THREADS];
  }
  if(prefix->family == AF_INET) {
    u_char *a;
    assert (prefix->bitlen <= sizeof(struct in_addr) * 8);
    a = ndpi_prefix_touchar (prefix);
    if(with_len) {
      sprintf (buff, "%d.%d.%d.%d/%d", a[0], a[1], a[2], a[3],
	       prefix->bitlen);
    }
    else {
      sprintf (buff, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
    }
    return (buff);
  }
  else if(prefix->family == AF_INET6) {
    char *r;
    r = (char *) inet_ntop (AF_INET6, &prefix->add.sin6, buff, 48 /* a guess value */ );
    if(r && with_len) {
      assert (prefix->bitlen <= sizeof(struct in6_addr) * 8);
      sprintf (buff + strlen (buff), "/%d", prefix->bitlen);
    }
    return (buff);
  }
  else
    return (NULL);
}

/* ndpi_prefix_toa2
 * convert prefix information to ascii string
 */
static  char * ndpi_prefix_toa2 (ndpi_prefix_t *prefix, char *buff)
{
  return (ndpi_prefix_toa2x (prefix, buff, 0));
}

/* ndpi_prefix_toa
 */
static char * ndpi_prefix_toa (ndpi_prefix_t * prefix)
{
  return (ndpi_prefix_toa2 (prefix, (char *) NULL));
}
#endif

static ndpi_prefix_t * ndpi_New_Prefix2 (int family, void *dest, int bitlen, ndpi_prefix_t *prefix)
{
  int dynamic_allocated = 0;
  int default_bitlen = sizeof(struct in_addr) * 8;

  if(family == AF_INET6) {
    default_bitlen = sizeof(struct in6_addr) * 8;
    if(prefix == NULL) {
      prefix = (ndpi_prefix_t*)ndpi_calloc(1, sizeof (ndpi_prefix_t));
      if(!prefix)
        return (NULL);
      dynamic_allocated++;
    }
    memcpy (&prefix->add.sin6, dest, sizeof(struct in6_addr));
  }
  else
    if(family == AF_INET) {
      if(prefix == NULL) {
#ifndef NT
	prefix = (ndpi_prefix_t*)ndpi_calloc(1, sizeof (prefix4_t));
#else
	//for some reason, compiler is getting
	//prefix4_t size incorrect on NT
	prefix = ndpi_calloc(1, sizeof (ndpi_prefix_t)); 
#endif /* NT */
	if(!prefix)
	  return (NULL);

	dynamic_allocated++;
      }
      memcpy (&prefix->add.sin, dest, sizeof(struct in_addr));
    }
    else if(family == AF_MAC) {
      default_bitlen = 48;
      if(prefix == NULL) {
        prefix = (ndpi_prefix_t*)ndpi_calloc(1, sizeof (ndpi_prefix_t));
        if(!prefix)
          return (NULL);
        dynamic_allocated++;
      }
      memcpy (prefix->add.mac, dest, 6);
    }
    else {
      return (NULL);
    }

  prefix->bitlen = (u_int16_t)((bitlen >= 0) ? bitlen : default_bitlen);
  prefix->family = (u_int16_t)family;
  prefix->ref_count = 0;
  if(dynamic_allocated) {
    prefix->ref_count++;
  }
  /* fprintf(stderr, "[C %s, %d]\n", ndpi_prefix_toa (prefix), prefix->ref_count); */
  return (prefix);
}

#if 0
static ndpi_prefix_t * ndpi_New_Prefix (int family, void *dest, int bitlen)
{
  return (ndpi_New_Prefix2 (family, dest, bitlen, NULL));
}
#endif

ndpi_prefix_t *
ndpi_Ref_Prefix (ndpi_prefix_t * prefix)
{
  if(prefix == NULL)
    return (NULL);
  if(prefix->ref_count == 0) {
    /* make a copy in case of a static prefix */
    return (ndpi_New_Prefix2 (prefix->family, &prefix->add, prefix->bitlen, NULL));
  }
  prefix->ref_count++;
  /* fprintf(stderr, "[A %s, %d]\n", ndpi_prefix_toa (prefix), prefix->ref_count); */
  return (prefix);
}

void 
ndpi_Deref_Prefix (ndpi_prefix_t * prefix)
{
  if(prefix == NULL)
    return;
  /* for secure programming, raise an assert. no static prefix can call this */
  assert (prefix->ref_count > 0);

  prefix->ref_count--;
  assert (prefix->ref_count >= 0);
  if(prefix->ref_count <= 0) {
    ndpi_DeleteEntry (prefix);
    return;
  }
}

// #define PATRICIA_DEBUG 1

static int num_active_patricia = 0;

/* these routines support continuous mask only */

ndpi_patricia_tree_t *
ndpi_patricia_new (u_int16_t maxbits)
{
  ndpi_patricia_tree_t *patricia = (ndpi_patricia_tree_t*)ndpi_calloc(1, sizeof *patricia);
  if(!patricia)
    return (NULL);

  patricia->maxbits = maxbits;
  patricia->head = NULL;
  patricia->num_active_node = 0;
  assert((u_int16_t)maxbits <= PATRICIA_MAXBITS); /* XXX */
  num_active_patricia++;
  return (patricia);
}


/*
 * if func is supplied, it will be called as func(node->data)
 * before deleting the node
 */
void
ndpi_Clear_Patricia (ndpi_patricia_tree_t *patricia, 
		ndpi_patricia_node_t *Xrn,
		ndpi_void_fn_t func)
{
  if(!patricia)
    return;

  if(!Xrn)
	Xrn = patricia->head;

  if(Xrn) {
    ndpi_patricia_node_t *Xstack[PATRICIA_MAXBITS+1];
    ndpi_patricia_node_t **Xsp = Xstack;
    ndpi_patricia_node_t *parent;
    parent = Xrn->parent;
    if(parent) {
	    Xrn->parent = NULL;
	    if(parent->l == Xrn) {
		parent->l = NULL;
	    } else if(parent->r == Xrn)
		    	parent->r = NULL;
    } else {
	if(Xrn == patricia->head)
		patricia->head = NULL;
    }
    while (Xrn) {
      ndpi_patricia_node_t *l = Xrn->l;
      ndpi_patricia_node_t *r = Xrn->r;

      if(Xrn->prefix) {
	ndpi_Deref_Prefix (Xrn->prefix);
	if(Xrn->data && func)
	  func (Xrn->data);
      }
      else {
	assert (Xrn->data == NULL);
      }
      ndpi_DeleteEntry (Xrn);
      patricia->num_active_node--;

      if(l) {
	if(r) {
	  *Xsp++ = r;
	}
	Xrn = l;
      } else if(r) {
	Xrn = r;
      } else if(Xsp != Xstack) {
	Xrn = *(--Xsp);
      } else {
	Xrn = NULL;
      }
    }
  }
  assert (!patricia->head && patricia->num_active_node == 0);
  /* ndpi_DeleteEntry (patricia); */
}

void
ndpi_patricia_destroy (ndpi_patricia_tree_t *patricia, ndpi_void_fn_t func)
{
  ndpi_Clear_Patricia (patricia, NULL, func);
  ndpi_DeleteEntry (patricia);
  num_active_patricia--;
}


/*
 * if func is supplied, it will be called as func(node->prefix, node->data)
 */
void
ndpi_patricia_process (ndpi_patricia_tree_t *patricia, ndpi_void_fn2_t func)
{
  ndpi_patricia_node_t *node;

  if (!patricia)
    return;
  assert (func);

  PATRICIA_WALK (patricia->head, node) {
    func (node->prefix, node->data);
  } PATRICIA_WALK_END;
}

static size_t
ndpi_patricia_clone_walk(ndpi_patricia_node_t *node, ndpi_patricia_tree_t *dst)
{
  ndpi_patricia_node_t *cloned_node;
  size_t n = 0;

  if(node->l) {
    n += ndpi_patricia_clone_walk(node->l, dst);
  }

  if(node->prefix) {
    cloned_node = ndpi_patricia_lookup(dst, node->prefix);

    if(cloned_node) {
      /* Note: this is not cloning clone referenced void * data / user_data */
      memcpy(&cloned_node->value, &node->value, sizeof(cloned_node->value));
    }

    n++;
  }

  if(node->r) {
    n += ndpi_patricia_clone_walk(node->r, dst);
  }

  return n;
}

ndpi_patricia_tree_t *
ndpi_patricia_clone (const ndpi_patricia_tree_t * const from)
{
  ndpi_patricia_tree_t *patricia;
  if(!from) return (NULL);

  patricia = ndpi_patricia_new(from->maxbits);

  if(!patricia) return (NULL);

  if(from->head)
    ndpi_patricia_clone_walk(from->head, patricia);

  return (patricia);
}

size_t
ndpi_patricia_walk_inorder(ndpi_patricia_node_t *node, ndpi_void_fn3_t func, void *data)
{
  size_t n = 0;
  assert(func);

  if(node->l) {
    n += ndpi_patricia_walk_inorder(node->l, func, data);
  }

  if(node->prefix) {
    func(node, node->data, data);
    n++;
  }
	
  if(node->r) {
    n += ndpi_patricia_walk_inorder(node->r, func, data);
  }

  return n;
}

size_t
ndpi_patricia_walk_tree_inorder(ndpi_patricia_tree_t *patricia, ndpi_void_fn3_t func, void *data) {
  if (patricia == NULL || patricia->head == NULL)
    return 0;

  return ndpi_patricia_walk_inorder(patricia->head, func, data);
}

ndpi_patricia_node_t *
ndpi_patricia_search_exact (ndpi_patricia_tree_t *patricia, ndpi_prefix_t *prefix)
{
  ndpi_patricia_node_t *node;
  u_char *addr;
  u_int16_t bitlen;

  if (!patricia)
    return (NULL);
  assert (prefix);
  assert (prefix->bitlen <= patricia->maxbits);

  patricia->stats.n_search++;

  if(patricia->head == NULL)
    return (NULL);

  node = patricia->head;
  addr = ndpi_prefix_touchar (prefix);
  bitlen = prefix->bitlen;

  while (node->bit < bitlen) {

    if(BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
#ifdef PATRICIA_DEBUG
      if(node->prefix)
	fprintf (stderr, "patricia_search_exact: take right %s/%d\n", 
		 ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
      else
	fprintf (stderr, "patricia_search_exact: take right at %u\n", 
		 node->bit);
#endif /* PATRICIA_DEBUG */
      node = node->r;
    }
    else {
#ifdef PATRICIA_DEBUG
      if(node->prefix)
	fprintf (stderr, "patricia_search_exact: take left %s/%d\n", 
		 ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
      else
	fprintf (stderr, "patricia_search_exact: take left at %u\n", 
		 node->bit);
#endif /* PATRICIA_DEBUG */
      node = node->l;
    }

    if(node == NULL)
      return (NULL);
  }

#ifdef PATRICIA_DEBUG
  if(node->prefix)
    fprintf (stderr, "patricia_search_exact: stop at %s/%d [node->bit: %u][bitlen: %u]\n", 
	     ndpi_prefix_toa (node->prefix), node->prefix->bitlen, node->bit, bitlen);
  else
    fprintf (stderr, "patricia_search_exact: stop at %u\n", node->bit);
#endif /* PATRICIA_DEBUG */
  
  if(node->bit > bitlen || node->prefix == NULL)
    return (NULL);

  assert (node->bit == bitlen);
  assert (node->bit == node->prefix->bitlen);
  if(ndpi_comp_with_mask (ndpi_prefix_tochar (node->prefix), ndpi_prefix_tochar (prefix),
			  bitlen)) {
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_search_exact: found %s/%d\n", 
	     ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
    patricia->stats.n_found++;
    return (node);
  }
  return (NULL);
}


/* if inclusive != 0, "best" may be the given prefix itself */
ndpi_patricia_node_t *
ndpi_patricia_search_best2 (ndpi_patricia_tree_t *patricia, ndpi_prefix_t *prefix, int inclusive)
{
  ndpi_patricia_node_t *node;
  ndpi_patricia_node_t *stack[PATRICIA_MAXBITS + 1];
  u_char *addr;
  u_int16_t bitlen;
  int cnt = 0;

  if(patricia == NULL)
    return (NULL);

  assert (prefix);
  assert (prefix->bitlen <= patricia->maxbits);

  patricia->stats.n_search++;

  if(patricia->head == NULL)
    return (NULL);

  node = patricia->head;
  addr = ndpi_prefix_touchar (prefix);
  bitlen = prefix->bitlen;

  while (node->bit < bitlen) {
    if(node->prefix) {
#ifdef PATRICIA_DEBUG
      fprintf (stderr, "patricia_search_best: push %s/%d\n", 
	       ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
      stack[cnt++] = node;
    }

    if(BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
#ifdef PATRICIA_DEBUG
      if(node->prefix)
	fprintf (stderr, "patricia_search_best: take right %s/%d\n", 
		 ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
      else
	fprintf (stderr, "patricia_search_best: take right at %u\n", 
		 node->bit);
#endif /* PATRICIA_DEBUG */
      node = node->r;
    } else {
#ifdef PATRICIA_DEBUG
      if(node->prefix)
	fprintf (stderr, "patricia_search_best: take left %s/%d\n", 
		 ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
      else
	fprintf (stderr, "patricia_search_best: take left at %u\n", 
		 node->bit);
#endif /* PATRICIA_DEBUG */
      node = node->l;
    }

    if(node == NULL)
      break;
  }

  if(inclusive && node && node->prefix) {
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_search_best: found node %s/%d [searching %s/%d]\n",
	     ndpi_prefix_toa (node->prefix), node->prefix->bitlen,
	     ndpi_prefix_toa (prefix), prefix->bitlen); 
#endif
    stack[cnt++] = node;
  }
  
#ifdef PATRICIA_DEBUG
  if(node == NULL)
    fprintf (stderr, "patricia_search_best: stop at null\n");
  else if(node->prefix)
    fprintf (stderr, "patricia_search_best: stop at %s/%d\n", 
	     ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
  else
    fprintf (stderr, "patricia_search_best: stop at %u\n", node->bit);
#endif /* PATRICIA_DEBUG */

  if(cnt <= 0)
    return (NULL);

  while (--cnt >= 0) {
    node = stack[cnt];
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_search_best: pop %s/%d\n", 
	     ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
    if(ndpi_comp_with_mask (ndpi_prefix_tochar (node->prefix), 
			    ndpi_prefix_tochar (prefix),
			    node->prefix->bitlen) && node->prefix->bitlen <= bitlen) {
#ifdef PATRICIA_DEBUG
      fprintf (stderr, "patricia_search_best: found %s/%d\n", 
	       ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
      patricia->stats.n_found++;
      return (node);
    }
  }
  return (NULL);
}


ndpi_patricia_node_t *
ndpi_patricia_search_best (ndpi_patricia_tree_t *patricia, ndpi_prefix_t *prefix)
{
  return (ndpi_patricia_search_best2 (patricia, prefix, 1));
}


ndpi_patricia_node_t *
ndpi_patricia_lookup (ndpi_patricia_tree_t *patricia, ndpi_prefix_t *prefix)
{
  ndpi_patricia_node_t *node, *new_node, *parent, *glue;
  u_char *addr, *test_addr;
  u_int16_t bitlen, check_bit, differ_bit;
  int i, j;

  if(!patricia)
    return (NULL);

#ifdef PATRICIA_DEBUG
  fprintf (stderr, "patricia_lookup() %s/%d (head)\n", 
	   ndpi_prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */

  assert (prefix);
  assert (prefix->bitlen <= patricia->maxbits);

  if(patricia->head == NULL) {
    node = (ndpi_patricia_node_t*)ndpi_calloc(1, sizeof *node);
    if(!node)
      return NULL;
    node->bit = prefix->bitlen;
    node->prefix = ndpi_Ref_Prefix (prefix);
    if(!node->prefix) {
      ndpi_free(node);
      return NULL;
    }
    node->parent = NULL;
    node->l = node->r = NULL;
    node->data = NULL;
    patricia->head = node;
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_lookup: new_node #0 %s/%d (head)\n", 
	     ndpi_prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
    patricia->num_active_node++;
    return (node);
  }

  addr = ndpi_prefix_touchar (prefix);
  bitlen = prefix->bitlen;
  node = patricia->head;

  while (node->bit < bitlen || node->prefix == NULL) {
    if(node->bit < patricia->maxbits &&
       BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
      if(node->r == NULL)
	break;
#ifdef PATRICIA_DEBUG
      if(node->prefix)
	fprintf (stderr, "patricia_lookup: take right %s/%d\n", 
		 ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
      else
	fprintf (stderr, "patricia_lookup: take right at %u\n", node->bit);
#endif /* PATRICIA_DEBUG */
      node = node->r;
    }
    else {
      if(node->l == NULL)
	break;
#ifdef PATRICIA_DEBUG
      if(node->prefix)
	fprintf (stderr, "patricia_lookup: take left %s/%d\n", 
		 ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
      else
	fprintf (stderr, "patricia_lookup: take left at %u\n", node->bit);
#endif /* PATRICIA_DEBUG */
      node = node->l;
    }

    assert (node);
  }

  assert (node->prefix);
#ifdef PATRICIA_DEBUG
  fprintf (stderr, "patricia_lookup: stop at %s/%d\n", 
	   ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */

  test_addr = ndpi_prefix_touchar (node->prefix);
  /* find the first bit different */
  check_bit = (node->bit < bitlen)? node->bit: bitlen;
  differ_bit = 0;
  for (i = 0; (u_int)i*8 < check_bit; i++) {
    int r;

    if((r = (addr[i] ^ test_addr[i])) == 0) {
      differ_bit = (i + 1) * 8;
      continue;
    }
    /* I know the better way, but for now */
    for (j = 0; j < 8; j++) {
      if(BIT_TEST (r, (0x80 >> j)))
	break;
    }
    /* must be found */
    assert (j < 8);
    differ_bit = i * 8 + j;
    break;
  }
  
  if(differ_bit > check_bit)
    differ_bit = check_bit;
#ifdef PATRICIA_DEBUG
  fprintf (stderr, "patricia_lookup: differ_bit %d\n", differ_bit);
#endif /* PATRICIA_DEBUG */

  parent = node->parent;
  while (parent && parent->bit >= differ_bit) {
    node = parent;
    parent = node->parent;
#ifdef PATRICIA_DEBUG
    if(node->prefix)
      fprintf (stderr, "patricia_lookup: up to %s/%d\n", 
	       ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
    else
      fprintf (stderr, "patricia_lookup: up to %u\n", node->bit);
#endif /* PATRICIA_DEBUG */
  }

  if(differ_bit == bitlen && node->bit == bitlen) {
    if(node->prefix) {
#ifdef PATRICIA_DEBUG 
      fprintf (stderr, "patricia_lookup: found %s/%d\n", 
	       ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
      return (node);
    }
    node->prefix = ndpi_Ref_Prefix (prefix);
    if(!node->prefix) {
      return NULL;
    }
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_lookup: new node #1 %s/%d (glue mod)\n",
	     ndpi_prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
    assert (node->data == NULL);
    return (node);
  }

  new_node = (ndpi_patricia_node_t*)ndpi_calloc(1, sizeof *new_node);
  if(!new_node) return NULL;
  new_node->bit = prefix->bitlen;
  new_node->prefix = ndpi_Ref_Prefix (prefix);
  if(!new_node->prefix) {
    ndpi_free(new_node);
    return NULL;
  }
  new_node->parent = NULL;
  new_node->l = new_node->r = NULL;
  new_node->data = NULL;
  patricia->num_active_node++;

  if(node->bit == differ_bit) {
    new_node->parent = node;
    if(node->bit < patricia->maxbits &&
       BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
      assert (node->r == NULL);
      node->r = new_node;
    }
    else {
      assert (node->l == NULL);
      node->l = new_node;
    }
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_lookup: new_node #2 %s/%d (child)\n", 
	     ndpi_prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
    return (new_node);
  }

  if(bitlen == differ_bit) {
    if(bitlen < patricia->maxbits &&
       BIT_TEST (test_addr[bitlen >> 3], 0x80 >> (bitlen & 0x07))) {
      new_node->r = node;
    }
    else {
      new_node->l = node;
    }
    new_node->parent = node->parent;
    if(node->parent == NULL) {
      assert (patricia->head == node);
      patricia->head = new_node;
    }
    else if(node->parent->r == node) {
      node->parent->r = new_node;
    }
    else {
      node->parent->l = new_node;
    }
    node->parent = new_node;
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_lookup: new_node #3 %s/%d (parent)\n", 
	     ndpi_prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
  }
  else {
    glue = (ndpi_patricia_node_t*)ndpi_calloc(1, sizeof *glue);

    if(!glue) {
      ndpi_Deref_Prefix(new_node->prefix);
      ndpi_DeleteEntry (new_node);
      patricia->num_active_node--;
      return(NULL);
    }
    glue->bit = differ_bit;
    glue->prefix = NULL;
    glue->parent = node->parent;
    glue->data = NULL;
    patricia->num_active_node++;
    if(differ_bit < patricia->maxbits &&
       BIT_TEST (addr[differ_bit >> 3], 0x80 >> (differ_bit & 0x07))) {
      glue->r = new_node;
      glue->l = node;
    }
    else {
      glue->r = node;
      glue->l = new_node;
    }
    new_node->parent = glue;

    if(node->parent == NULL) {
      assert (patricia->head == node);
      patricia->head = glue;
    }
    else if(node->parent->r == node) {
      node->parent->r = glue;
    }
    else {
      node->parent->l = glue;
    }
    node->parent = glue;
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_lookup: new_node #4 %s/%d (glue+node)\n", 
	     ndpi_prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */    
  }
  return (new_node);
}


void
ndpi_patricia_remove (ndpi_patricia_tree_t *patricia, ndpi_patricia_node_t *node)
{
  ndpi_patricia_node_t *parent, *child;

  if(!patricia)
    return;
  assert (node);

  if(node->r && node->l) {
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_remove: #0 %s/%d (r & l)\n", 
	     ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
	
    /* this might be a placeholder node -- have to check and make sure
     * there is a prefix associated with it ! */
    if(node->prefix != NULL) 
      ndpi_Deref_Prefix (node->prefix);
    node->prefix = NULL;
    /* Also I needed to clear data pointer -- masaki */
    node->data = NULL;
    return;
  }

  if(node->r == NULL && node->l == NULL) {
#ifdef PATRICIA_DEBUG
    fprintf (stderr, "patricia_remove: #1 %s/%d (!r & !l)\n", 
	     ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
    parent = node->parent;
    ndpi_Deref_Prefix (node->prefix);
    ndpi_DeleteEntry (node);
    patricia->num_active_node--;

    if(parent == NULL) {
      assert (patricia->head == node);
      patricia->head = NULL;
      return;
    }

    if(parent->r == node) {
      parent->r = NULL;
      child = parent->l;
    }
    else {
      assert (parent->l == node);
      parent->l = NULL;
      child = parent->r;
    }

    if(parent->prefix)
      return;

    /* we need to remove parent too */

    if(parent->parent == NULL) {
      assert (patricia->head == parent);
      patricia->head = child;
    }
    else if(parent->parent->r == parent) {
      parent->parent->r = child;
    }
    else {
      assert (parent->parent->l == parent);
      parent->parent->l = child;
    }
    child->parent = parent->parent;
    ndpi_DeleteEntry (parent);
    patricia->num_active_node--;
    return;
  }

#ifdef PATRICIA_DEBUG
  fprintf (stderr, "patricia_remove: #2 %s/%d (r ^ l)\n", 
	   ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* PATRICIA_DEBUG */
  if(node->r) {
    child = node->r;
  }
  else {
    assert (node->l);
    child = node->l;
  }
  parent = node->parent;
  child->parent = parent;

  ndpi_Deref_Prefix (node->prefix);
  ndpi_DeleteEntry (node);
  patricia->num_active_node--;

  if(parent == NULL) {
    assert (patricia->head == node);
    patricia->head = child;
    return;
  }

  if(parent->r == node) {
    parent->r = child;
  }
  else {
    assert (parent->l == node);
    parent->l = child;
  }
}

/* { from demo.c */

/* ndpi_ascii2prefix */
ndpi_prefix_t * ndpi_ascii2prefix (int family, char *string)
{
  long bitlen;
  long maxbitlen = 0;
  char *cp;
  struct in_addr sin;
  struct in6_addr sin6;
  char save[MAXLINE];

  if(string == NULL)
    return (NULL);

  /* easy way to handle both families */
  if(family == 0) {
    family = AF_INET;
    if(strchr (string, ':')) family = AF_INET6;
  }

  if(family == AF_INET) {
    maxbitlen = sizeof(struct in_addr) * 8;
  }
  else if(family == AF_INET6) {
    maxbitlen = sizeof(struct in6_addr) * 8;
  }

  if((cp = strchr (string, '/')) != NULL) {
    bitlen = atol (cp + 1);
    /* *cp = '\0'; */
    /* copy the string to save. Avoid destroying the string */
    assert (cp - string < MAXLINE);
    memcpy (save, string, cp - string);
    save[cp - string] = '\0';
    string = save;
    if((bitlen < 0) || (bitlen > maxbitlen))
      bitlen = maxbitlen;
  } else {
    bitlen = maxbitlen;
  }

  if(family == AF_INET) {
    if(ndpi_my_inet_pton (AF_INET, string, &sin) <= 0)
      return (NULL);
    if(htonl(sin.s_addr) & (0xfffffffful >> bitlen))
      return (NULL);
    return (ndpi_New_Prefix2 (AF_INET, &sin, bitlen, NULL));
  }
  else if(family == AF_INET6) {
    // Get rid of this with next IPv6 upgrade
#if defined(NT) && !defined(HAVE_INET_NTOP)
    inet6_addr(string, &sin6);
    return (ndpi_New_Prefix2 (AF_INET6, &sin6, bitlen, NULL));
#else
    if(inet_pton (AF_INET6, string, &sin6) <= 0)
      return (NULL);
#endif /* NT */
    /* FIXME check network mask! */
    return (ndpi_New_Prefix2 (AF_INET6, &sin6, bitlen, NULL));
  }
  else
    return (NULL);
}

#if 0

ndpi_patricia_node_t *
ndpi_make_and_lookup (ndpi_patricia_tree_t *tree, char *string)
{
  ndpi_prefix_t *prefix;
  ndpi_patricia_node_t *node;

  prefix = ndpi_ascii2prefix (AF_INET, string);
  printf ("make_and_lookup: %s/%d\n", ndpi_prefix_toa (prefix), prefix->bitlen);
  node = ndpi_patricia_lookup (tree, prefix);
  ndpi_Deref_Prefix (prefix);
  return (node);
}

ndpi_patricia_node_t *
ndpi_try_search_exact (ndpi_patricia_tree_t *tree, char *string)
{
  ndpi_prefix_t *prefix;
  ndpi_patricia_node_t *node;

  prefix = ndpi_ascii2prefix (AF_INET, string);
  printf ("try_search_exact: %s/%d\n", ndpi_prefix_toa (prefix), prefix->bitlen);
  if((node = patricia_search_exact (tree, prefix)) == NULL) {
    printf ("try_search_exact: not found\n");
  }
  else {
    printf ("try_search_exact: %s/%d found\n", 
	    ndpi_prefix_toa (node->prefix), node->prefix->bitlen);
  }
  ndpi_Deref_Prefix (prefix);
  return (node);
}

void
ndpi_lookup_then_remove (ndpi_patricia_tree_t *tree, char *string)
{
  ndpi_patricia_node_t *node;

  if((node = try_search_exact (tree, string)))
    patricia_remove (tree, node);
}
#endif
