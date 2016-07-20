//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// This is a derivative work based on Zlib, copyright below:
/*
    Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
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

    Jean-loup Gailly        Mark Adler
    jloup@gzip.org          madler@alumni.caltech.edu

    The data format used by the zlib library is described by RFCs (Request for
    Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
    (zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).
*/

#ifndef BEAST_CORE_DETAIL_ZLIB_IMPL_DEFLATE_STREAM_IPP
#define BEAST_CORE_DETAIL_ZLIB_IMPL_DEFLATE_STREAM_IPP

namespace beast {

/*
 *  ALGORITHM
 *
 *      The "deflation" process uses several Huffman trees. The more
 *      common source values are represented by shorter bit sequences.
 *
 *      Each code tree is stored in a compressed form which is itself
 * a Huffman encoding of the lengths of all the code strings (in
 * ascending order by source values).  The actual code strings are
 * reconstructed from the lengths in the inflate process, as described
 * in the deflate specification.
 *
 *  REFERENCES
 *
 *      Deutsch, L.P.,"'Deflate' Compressed Data Format Specification".
 *      Available in ftp.uu.net:/pub/archiving/zip/doc/deflate-1.1.doc
 *
 *      Storer, James A.
 *          Data Compression:  Methods and Theory, pp. 49-50.
 *          Computer Science Press, 1988.  ISBN 0-7167-8156-5.
 *
 *      Sedgewick, R.
 *          Algorithms, p290.
 *          Addison-Wesley, 1983. ISBN 0-201-06672-6.
 */

template<class _>
deflate_stream_t<_>::deflate_stream_t()
    : lut_(detail::get_deflate_tables())
{
    // default level 6
    //deflateInit2(this, 6, Z_DEFLATED, 15, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
}

template<class _>
deflate_stream_t<_>::~deflate_stream_t()
{
    deflateEnd(this);
}
















#ifndef DEBUG
#  define send_code(s, c, tree) send_bits(s, tree[c].fc, tree[c].dl)
   /* Send a code of the given tree. c and tree must not have side effects */

#else /* DEBUG */
#  define send_code(s, c, tree) \
     { if (z_verbose>2) fprintf(stderr,"\ncd %3d ",(c)); \
       send_bits(s, tree[c].fc, tree[c].dl); }
#endif

/* ===========================================================================
 * Output a short LSB first on the stream.
 * IN assertion: there is enough room in pendingBuf.
 */
#define put_short(s, w) { \
    put_byte(s, (std::uint8_t)((w) & 0xff)); \
    put_byte(s, (std::uint8_t)((std::uint16_t)(w) >> 8)); \
}

/* ===========================================================================
 * Send a value on a given number of bits.
 * IN assertion: length <= 16 and value fits in length bits.
 */
#define send_bits(s, value, length) \
{ int len = length;\
  if (s->bi_valid_ > (int)Buf_size - len) {\
    int val = value;\
    s->bi_buf_ |= (std::uint16_t)val << s->bi_valid_;\
    put_short(s, s->bi_buf_);\
    s->bi_buf_ = (std::uint16_t)val >> (Buf_size - s->bi_valid_);\
    s->bi_valid_ += len - Buf_size;\
  } else {\
    s->bi_buf_ |= (std::uint16_t)(value) << s->bi_valid_;\
    s->bi_valid_ += len;\
  }\
}

/* ===========================================================================
 * Initialize the tree data structures for a new zlib stream.
 */
template<class _>
void
deflate_stream_t<_>::_tr_init(
    deflate_stream_t *s)
{
    s->l_desc_.dyn_tree = s->dyn_ltree_;
    s->l_desc_.stat_desc = &s->lut_.l_desc;

    s->d_desc_.dyn_tree = s->dyn_dtree_;
    s->d_desc_.stat_desc = &s->lut_.d_desc;

    s->bl_desc_.dyn_tree = s->bl_tree_;
    s->bl_desc_.stat_desc = &s->lut_.bl_desc;

    s->bi_buf_ = 0;
    s->bi_valid_ = 0;
#ifdef DEBUG
    s->compressed_len_ = 0L;
    s->bits_sent_ = 0L;
#endif

    /* Initialize the first block of the first file: */
    init_block(s);
}

/* ===========================================================================
 * Initialize a new block.
 */
template<class _>
void
deflate_stream_t<_>::init_block(
    deflate_stream_t *s)
{
    int n; /* iterates over tree elements */

    /* Initialize the trees. */
    for (n = 0; n < limits::lCodes;  n++) s->dyn_ltree_[n].fc = 0;
    for (n = 0; n < limits::dCodes;  n++) s->dyn_dtree_[n].fc = 0;
    for (n = 0; n < limits::blCodes; n++) s->bl_tree_[n].fc = 0;

    s->dyn_ltree_[END_BLOCK].fc = 1;
    s->opt_len_ = s->static_len_ = 0L;
    s->last_lit_ = s->matches_ = 0;
}

#define SMALLEST 1
/* Index within the heap array of least frequent node in the Huffman tree */


/* ===========================================================================
 * Remove the smallest element from the heap and recreate the heap with
 * one less element. Updates heap and heap_len.
 */
#define pqremove(s, tree, top) \
{\
    top = s->heap_[SMALLEST]; \
    s->heap_[SMALLEST] = s->heap_[s->heap_len_--]; \
    pqdownheap(s, tree, SMALLEST); \
}

/* ===========================================================================
 * Compares to subtrees, using the tree depth as tie breaker when
 * the subtrees have equal frequency. This minimizes the worst case length.
 */
#define smaller(tree, n, m, depth) \
   (tree[n].fc < tree[m].fc || \
   (tree[n].fc == tree[m].fc && depth[n] <= depth[m]))

/* ===========================================================================
 * Restore the heap property by moving down the tree starting at node k,
 * exchanging a node with the smallest of its two sons if necessary, stopping
 * when the heap property is re-established (each father smaller than its
 * two sons).
 */
template<class _>
void
deflate_stream_t<_>::pqdownheap(
    deflate_stream_t *s,
    detail::ct_data *tree,  /* the tree to restore */
    int k)               /* node to move down */
{
    int v = s->heap_[k];
    int j = k << 1;  /* left son of k */
    while (j <= s->heap_len_) {
        /* Set j to the smallest of the two sons: */
        if (j < s->heap_len_ &&
            smaller(tree, s->heap_[j+1], s->heap_[j], s->depth_)) {
            j++;
        }
        /* Exit if v is smaller than both sons */
        if (smaller(tree, v, s->heap_[j], s->depth_)) break;

        /* Exchange v with the smallest son */
        s->heap_[k] = s->heap_[j];  k = j;

        /* And continue down the tree, setting j to the left son of k */
        j <<= 1;
    }
    s->heap_[k] = v;
}

/* ===========================================================================
 * Compute the optimal bit lengths for a tree and update the total bit length
 * for the current block.
 * IN assertion: the fields freq and dad are set, heap[heap_max] and
 *    above are the tree nodes sorted by increasing frequency.
 * OUT assertions: the field len is set to the optimal bit length, the
 *     array bl_count contains the frequencies for each bit length.
 *     The length opt_len is updated; static_len is also updated if stree is
 *     not null.
 */
template<class _>
void
deflate_stream_t<_>::gen_bitlen(
    deflate_stream_t *s,
    tree_desc *desc)    /* the tree descriptor */
{
    detail::ct_data *tree        = desc->dyn_tree;
    int max_code         = desc->max_code;
    const detail::ct_data *stree = desc->stat_desc->static_tree;
    std::uint8_t const *extra    = desc->stat_desc->extra_bits;
    int base             = desc->stat_desc->extra_base;
    int max_length       = desc->stat_desc->max_length;
    int h;              /* heap index */
    int n, m;           /* iterate over the tree elements */
    int bits;           /* bit length */
    int xbits;          /* extra bits */
    std::uint16_t f;              /* frequency */
    int overflow = 0;   /* number of elements with bit length too large */

    for (bits = 0; bits <= limits::maxBits; bits++) s->bl_count_[bits] = 0;

    /* In a first pass, compute the optimal bit lengths (which may
     * overflow in the case of the bit length tree).
     */
    tree[s->heap_[s->heap_max_]].dl = 0; /* root of the heap */

    for (h = s->heap_max_+1; h < HEAP_SIZE; h++) {
        n = s->heap_[h];
        bits = tree[tree[n].dl].dl + 1;
        if (bits > max_length) bits = max_length, overflow++;
        tree[n].dl = (std::uint16_t)bits;
        /* We overwrite tree[n].dl which is no longer needed */

        if (n > max_code) continue; /* not a leaf node */

        s->bl_count_[bits]++;
        xbits = 0;
        if (n >= base) xbits = extra[n-base];
        f = tree[n].fc;
        s->opt_len_ += (std::uint32_t)f * (bits + xbits);
        if (stree) s->static_len_ += (std::uint32_t)f * (stree[n].dl + xbits);
    }
    if (overflow == 0) return;

    Trace((stderr,"\nbit length overflow\n"));
    /* This happens for example on obj2 and pic of the Calgary corpus */

    /* Find the first bit length which could increase: */
    do {
        bits = max_length-1;
        while (s->bl_count_[bits] == 0) bits--;
        s->bl_count_[bits]--;      /* move one leaf down the tree */
        s->bl_count_[bits+1] += 2; /* move one overflow item as its brother */
        s->bl_count_[max_length]--;
        /* The brother of the overflow item also moves one step up,
         * but this does not affect bl_count[max_length]
         */
        overflow -= 2;
    } while (overflow > 0);

    /* Now recompute all bit lengths, scanning in increasing frequency.
     * h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
     * lengths instead of fixing only the wrong ones. This idea is taken
     * from 'ar' written by Haruhiko Okumura.)
     */
    for (bits = max_length; bits != 0; bits--) {
        n = s->bl_count_[bits];
        while (n != 0) {
            m = s->heap_[--h];
            if (m > max_code) continue;
            if ((unsigned) tree[m].dl != (unsigned) bits) {
                Trace((stderr,"code %d bits %d->%d\n", m, tree[m].dl, bits));
                s->opt_len_ += ((long)bits - (long)tree[m].dl)
                              *(long)tree[m].fc;
                tree[m].dl = (std::uint16_t)bits;
            }
            n--;
        }
    }
}

/* ===========================================================================
 * Generate the codes for a given tree and bit counts (which need not be
 * optimal).
 * IN assertion: the array bl_count contains the bit length statistics for
 * the given tree and the field len is set for all tree elements.
 * OUT assertion: the field code is set for all tree elements of non
 *     zero code length.
 */
template<class _>
void
deflate_stream_t<_>::gen_codes (
    detail::ct_data *tree,             /* the tree to decorate */
    int max_code,              /* largest code with non zero frequency */
    std::uint16_t *bl_count)            /* number of codes at each bit length */
{
    std::uint16_t next_code[limits::maxBits+1]; /* next code value for each bit length */
    std::uint16_t code = 0;              /* running code value */
    int bits;                  /* bit index */
    int n;                     /* code index */

    /* The distribution counts are first used to generate the code values
     * without bit reversal.
     */
    for (bits = 1; bits <= limits::maxBits; bits++) {
        next_code[bits] = code = (code + bl_count[bits-1]) << 1;
    }
    /* Check that the bit counts in bl_count are consistent. The last code
     * must be all ones.
     */
    Assert (code + bl_count[limits::maxBits]-1 == (1<<limits::maxBits)-1,
            "inconsistent bit counts");
    Tracev((stderr,"\ngen_codes: max_code %d ", max_code));

    for (n = 0;  n <= max_code; n++) {
        int len = tree[n].dl;
        if (len == 0) continue;
        /* Now reverse the bits */
        tree[n].fc = bi_reverse(next_code[len]++, len);

        Tracecv(tree != static_ltree, (stderr,"\nn %3d %c l %2d c %4x (%x) ",
             n, (isgraph(n) ? n : ' '), len, tree[n].fc, next_code[len]-1));
    }
}

/* ===========================================================================
 * Construct one Huffman tree and assigns the code bit strings and lengths.
 * Update the total bit length for the current block.
 * IN assertion: the field freq is set for all tree elements.
 * OUT assertions: the fields len and code are set to the optimal bit length
 *     and corresponding code. The length opt_len is updated; static_len is
 *     also updated if stree is not null. The field max_code is set.
 */
template<class _>
void
deflate_stream_t<_>::build_tree(
    deflate_stream_t *s,
    tree_desc *desc) /* the tree descriptor */
{
    detail::ct_data *tree         = desc->dyn_tree;
    const detail::ct_data *stree  = desc->stat_desc->static_tree;
    int elems             = desc->stat_desc->elems;
    int n, m;          /* iterate over heap elements */
    int max_code = -1; /* largest code with non zero frequency */
    int node;          /* new node being created */

    /* Construct the initial heap, with least frequent element in
     * heap[SMALLEST]. The sons of heap[n] are heap[2*n] and heap[2*n+1].
     * heap[0] is not used.
     */
    s->heap_len_ = 0, s->heap_max_ = HEAP_SIZE;

    for (n = 0; n < elems; n++) {
        if (tree[n].fc != 0) {
            s->heap_[++(s->heap_len_)] = max_code = n;
            s->depth_[n] = 0;
        } else {
            tree[n].dl = 0;
        }
    }

    /* The pkzip format requires that at least one distance code exists,
     * and that at least one bit should be sent even if there is only one
     * possible code. So to avoid special checks later on we force at least
     * two codes of non zero frequency.
     */
    while (s->heap_len_ < 2) {
        node = s->heap_[++(s->heap_len_)] = (max_code < 2 ? ++max_code : 0);
        tree[node].fc = 1;
        s->depth_[node] = 0;
        s->opt_len_--; if (stree) s->static_len_ -= stree[node].dl;
        /* node is 0 or 1 so it does not have extra bits */
    }
    desc->max_code = max_code;

    /* The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree,
     * establish sub-heaps of increasing lengths:
     */
    for (n = s->heap_len_/2; n >= 1; n--) pqdownheap(s, tree, n);

    /* Construct the Huffman tree by repeatedly combining the least two
     * frequent nodes.
     */
    node = elems;              /* next internal node of the tree */
    do {
        pqremove(s, tree, n);  /* n = node of least frequency */
        m = s->heap_[SMALLEST]; /* m = node of next least frequency */

        s->heap_[--(s->heap_max_)] = n; /* keep the nodes sorted by frequency */
        s->heap_[--(s->heap_max_)] = m;

        /* Create a new node father of n and m */
        tree[node].fc = tree[n].fc + tree[m].fc;
        s->depth_[node] = (std::uint8_t)((s->depth_[n] >= s->depth_[m] ?
                                s->depth_[n] : s->depth_[m]) + 1);
        tree[n].dl = tree[m].dl = (std::uint16_t)node;
#ifdef DUMP_BL_TREE
        if (tree == s->bl_tree_) {
            fprintf(stderr,"\nnode %d(%d), sons %d(%d) %d(%d)",
                    node, tree[node].fc, n, tree[n].fc, m, tree[m].fc);
        }
#endif
        /* and insert the new node in the heap */
        s->heap_[SMALLEST] = node++;
        pqdownheap(s, tree, SMALLEST);

    } while (s->heap_len_ >= 2);

    s->heap_[--(s->heap_max_)] = s->heap_[SMALLEST];

    /* At this point, the fields freq and dad are set. We can now
     * generate the bit lengths.
     */
    gen_bitlen(s, (tree_desc *)desc);

    /* The field len is now set, we can generate the bit codes */
    gen_codes ((detail::ct_data *)tree, max_code, s->bl_count_);
}

/* ===========================================================================
 * Scan a literal or distance tree to determine the frequencies of the codes
 * in the bit length tree.
 */
template<class _>
void
deflate_stream_t<_>::scan_tree (
    deflate_stream_t *s,
    detail::ct_data *tree,   /* the tree to be scanned */
    int max_code)    /* and its largest code of non zero frequency */
{
    int n;                     /* iterates over all tree elements */
    int prevlen = -1;          /* last emitted length */
    int curlen;                /* length of current code */
    int nextlen = tree[0].dl; /* length of next code */
    int count = 0;             /* repeat count of the current code */
    int max_count = 7;         /* max repeat count */
    int min_count = 4;         /* min repeat count */

    if (nextlen == 0) max_count = 138, min_count = 3;
    tree[max_code+1].dl = (std::uint16_t)0xffff; /* guard */

    for (n = 0; n <= max_code; n++) {
        curlen = nextlen; nextlen = tree[n+1].dl;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            s->bl_tree_[curlen].fc += count;
        } else if (curlen != 0) {
            if (curlen != prevlen) s->bl_tree_[curlen].fc++;
            s->bl_tree_[REP_3_6].fc++;
        } else if (count <= 10) {
            s->bl_tree_[REPZ_3_10].fc++;
        } else {
            s->bl_tree_[REPZ_11_138].fc++;
        }
        count = 0; prevlen = curlen;
        if (nextlen == 0) {
            max_count = 138, min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6, min_count = 3;
        } else {
            max_count = 7, min_count = 4;
        }
    }
}

/* ===========================================================================
 * Send a literal or distance tree in compressed form, using the codes in
 * bl_tree.
 */
template<class _>
void
deflate_stream_t<_>::send_tree (
    deflate_stream_t *s,
    detail::ct_data *tree, /* the tree to be scanned */
    int max_code)       /* and its largest code of non zero frequency */
{
    int n;                     /* iterates over all tree elements */
    int prevlen = -1;          /* last emitted length */
    int curlen;                /* length of current code */
    int nextlen = tree[0].dl; /* length of next code */
    int count = 0;             /* repeat count of the current code */
    int max_count = 7;         /* max repeat count */
    int min_count = 4;         /* min repeat count */

    /* tree[max_code+1].dl = -1; */  /* guard already set */
    if (nextlen == 0) max_count = 138, min_count = 3;

    for (n = 0; n <= max_code; n++) {
        curlen = nextlen; nextlen = tree[n+1].dl;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            do { send_code(s, curlen, s->bl_tree_); } while (--count != 0);

        } else if (curlen != 0) {
            if (curlen != prevlen) {
                send_code(s, curlen, s->bl_tree_); count--;
            }
            Assert(count >= 3 && count <= 6, " 3_6?");
            send_code(s, REP_3_6, s->bl_tree_); send_bits(s, count-3, 2);

        } else if (count <= 10) {
            send_code(s, REPZ_3_10, s->bl_tree_); send_bits(s, count-3, 3);

        } else {
            send_code(s, REPZ_11_138, s->bl_tree_); send_bits(s, count-11, 7);
        }
        count = 0; prevlen = curlen;
        if (nextlen == 0) {
            max_count = 138, min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6, min_count = 3;
        } else {
            max_count = 7, min_count = 4;
        }
    }
}

/* ===========================================================================
 * Construct the Huffman tree for the bit lengths and return the index in
 * bl_order of the last bit length code to send.
 */
template<class _>
int
deflate_stream_t<_>::build_bl_tree(deflate_stream_t *s)
{
    int max_blindex;  /* index of last bit length code of non zero freq */

    /* Determine the bit length frequencies for literal and distance trees */
    scan_tree(s, (detail::ct_data *)s->dyn_ltree_, s->l_desc_.max_code);
    scan_tree(s, (detail::ct_data *)s->dyn_dtree_, s->d_desc_.max_code);

    /* Build the bit length tree: */
    build_tree(s, (tree_desc *)(&(s->bl_desc_)));
    /* opt_len now includes the length of the tree representations, except
     * the lengths of the bit lengths codes and the 5+5+4 bits for the counts.
     */

    /* Determine the number of bit length codes to send. The pkzip format
     * requires that at least 4 bit length codes be sent. (appnote.txt says
     * 3 but the actual value used is 4.)
     */
    for (max_blindex = limits::blCodes-1; max_blindex >= 3; max_blindex--) {
        if (s->bl_tree_[s->lut_.bl_order[max_blindex]].dl != 0) break;
    }
    /* Update opt_len to include the bit length tree and counts */
    s->opt_len_ += 3*(max_blindex+1) + 5+5+4;
    Tracev((stderr, "\ndyn trees: dyn %ld, stat %ld",
            s->opt_len_, s->static_len_));

    return max_blindex;
}

/* ===========================================================================
 * Send the header for a block using dynamic Huffman trees: the counts, the
 * lengths of the bit length codes, the literal tree and the distance tree.
 * IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
 */
template<class _>
void
deflate_stream_t<_>::send_all_trees(
    deflate_stream_t *s,
    int lcodes,
    int dcodes,
    int blcodes) /* number of codes for each tree */
{
    int rank;                    /* index in bl_order */

    Assert (lcodes >= 257 && dcodes >= 1 && blcodes >= 4, "not enough codes");
    Assert (lcodes <= limits::lCodes && dcodes <= limits::dCodes && blcodes <= limits::blCodes,
            "too many codes");
    Tracev((stderr, "\nbl counts: "));
    send_bits(s, lcodes-257, 5); /* not +255 as stated in appnote.txt */
    send_bits(s, dcodes-1,   5);
    send_bits(s, blcodes-4,  4); /* not -3 as stated in appnote.txt */
    for (rank = 0; rank < blcodes; rank++) {
        Tracev((stderr, "\nbl code %2d ", bl_order[rank]));
        send_bits(s, s->bl_tree_[s->lut_.bl_order[rank]].dl, 3);
    }
    Tracev((stderr, "\nbl tree: sent %ld", s->bits_sent_));

    send_tree(s, (detail::ct_data *)s->dyn_ltree_, lcodes-1); /* literal tree */
    Tracev((stderr, "\nlit tree: sent %ld", s->bits_sent_));

    send_tree(s, (detail::ct_data *)s->dyn_dtree_, dcodes-1); /* distance tree */
    Tracev((stderr, "\ndist tree: sent %ld", s->bits_sent_));
}

/* ===========================================================================
 * Send a stored block
 */
template<class _>
void
deflate_stream_t<_>::_tr_stored_block(
    deflate_stream_t *s,
    char *buf,       /* input block */
    std::uint32_t stored_len,   /* length of input block */
    int last)         /* one if this is the last block for a file */
{
    send_bits(s, (STORED_BLOCK<<1)+last, 3);    /* send block type */
#ifdef DEBUG
    s->compressed_len_ = (s->compressed_len_ + 3 + 7) & (std::uint32_t)~7L;
    s->compressed_len_ += (stored_len + 4) << 3;
#endif
    copy_block(s, buf, (unsigned)stored_len, 1); /* with header */
}

/* ===========================================================================
 * Flush the bits in the bit buffer to pending output (leaves at most 7 bits)
 */
template<class _>
void
deflate_stream_t<_>::_tr_flush_bits(deflate_stream_t *s)
{
    bi_flush(s);
}

/* ===========================================================================
 * Send one empty static block to give enough lookahead for inflate.
 * This takes 10 bits, of which 7 may remain in the bit buffer.
 */
template<class _>
void
deflate_stream_t<_>::_tr_align(deflate_stream_t *s)
{
    send_bits(s, STATIC_TREES<<1, 3);
    send_code(s, END_BLOCK, s->lut_.ltree);
#ifdef DEBUG
    s->compressed_len_ += 10L; /* 3 for block type, 7 for EOB */
#endif
    bi_flush(s);
}

/* ===========================================================================
 * Determine the best encoding for the current block: dynamic trees, static
 * trees or store, and output the encoded block to the zip file.
 */
template<class _>
void
deflate_stream_t<_>::_tr_flush_block(
    deflate_stream_t *s,
    char *buf,       /* input block, or NULL if too old */
    std::uint32_t stored_len,   /* length of input block */
    int last)         /* one if this is the last block for a file */
{
    std::uint32_t opt_lenb, static_lenb; /* opt_len and static_len in bytes */
    int max_blindex = 0;  /* index of last bit length code of non zero freq */

    /* Build the Huffman trees unless a stored block is forced */
    if (s->level_ > 0) {

        /* Check if the file is binary or text */
        if (s->data_type == Z_UNKNOWN)
            s->data_type = detect_data_type(s);

        /* Construct the literal and distance trees */
        build_tree(s, (tree_desc *)(&(s->l_desc_)));
        Tracev((stderr, "\nlit data: dyn %ld, stat %ld", s->opt_len_,
                s->static_len_));

        build_tree(s, (tree_desc *)(&(s->d_desc_)));
        Tracev((stderr, "\ndist data: dyn %ld, stat %ld", s->opt_len_,
                s->static_len_));
        /* At this point, opt_len and static_len are the total bit lengths of
         * the compressed block data, excluding the tree representations.
         */

        /* Build the bit length tree for the above two trees, and get the index
         * in bl_order of the last bit length code to send.
         */
        max_blindex = build_bl_tree(s);

        /* Determine the best encoding. Compute the block lengths in bytes. */
        opt_lenb = (s->opt_len_+3+7)>>3;
        static_lenb = (s->static_len_+3+7)>>3;

        Tracev((stderr, "\nopt %lu(%lu) stat %lu(%lu) stored %lu lit %u ",
                opt_lenb, s->opt_len_, static_lenb, s->static_len_, stored_len,
                s->last_lit_));

        if (static_lenb <= opt_lenb) opt_lenb = static_lenb;

    } else {
        Assert(buf != (char*)0, "lost buf");
        opt_lenb = static_lenb = stored_len + 5; /* force a stored block */
    }

#ifdef FORCE_STORED
    if (buf != (char*)0) { /* force stored block */
#else
    if (stored_len+4 <= opt_lenb && buf != (char*)0) {
                       /* 4: two words for the lengths */
#endif
        /* The test buf != NULL is only necessary if LIT_BUFSIZE > WSIZE.
         * Otherwise we can't have processed more than WSIZE input bytes since
         * the last block flush, because compression would have been
         * successful. If LIT_BUFSIZE <= WSIZE, it is never too late to
         * transform a block into a stored block.
         */
        _tr_stored_block(s, buf, stored_len, last);

#ifdef FORCE_STATIC
    } else if (static_lenb >= 0) { /* force static trees */
#else
    } else if (s->strategy_ == Z_FIXED || static_lenb == opt_lenb) {
#endif
        send_bits(s, (STATIC_TREES<<1)+last, 3);
        compress_block(s, s->lut_.ltree, s->lut_.dtree);
#ifdef DEBUG
        s->compressed_len_ += 3 + s->static_len_;
#endif
    } else {
        send_bits(s, (DYN_TREES<<1)+last, 3);
        send_all_trees(s, s->l_desc_.max_code+1, s->d_desc_.max_code+1,
                       max_blindex+1);
        compress_block(s, (const detail::ct_data *)s->dyn_ltree_,
                       (const detail::ct_data *)s->dyn_dtree_);
#ifdef DEBUG
        s->compressed_len_ += 3 + s->opt_len_;
#endif
    }
    Assert (s->compressed_len_ == s->bits_sent_, "bad compressed size");
    /* The above check is made mod 2^32, for files larger than 512 MB
     * and uLong implemented on 32 bits.
     */
    init_block(s);

    if (last) {
        bi_windup(s);
#ifdef DEBUG
        s->compressed_len_ += 7;  /* align on byte boundary */
#endif
    }
    Tracev((stderr,"\ncomprlen %lu(%lu) ", s->compressed_len_>>3,
           s->compressed_len_-7*last));
}

/* ===========================================================================
 * Save the match info and tally the frequency counts. Return true if
 * the current block must be flushed.
 */
template<class _>
int
deflate_stream_t<_>::_tr_tally (
    deflate_stream_t *s,
    unsigned dist,  /* distance of matched string */
    unsigned lc)    /* match length-limits::minMatch or unmatched char (if dist==0) */
{
    s->d_buf_[s->last_lit_] = (std::uint16_t)dist;
    s->l_buf_[s->last_lit_++] = (std::uint8_t)lc;
    if (dist == 0) {
        /* lc is the unmatched char */
        s->dyn_ltree_[lc].fc++;
    } else {
        s->matches_++;
        /* Here, lc is the match length - limits::minMatch */
        dist--;             /* dist = match distance - 1 */
        Assert((std::uint16_t)dist < (std::uint16_t)MAX_DIST(s) &&
               (std::uint16_t)lc <= (std::uint16_t)(limits::maxMatch-limits::minMatch) &&
               (std::uint16_t)d_code(dist) < (std::uint16_t)limits::dCodes,  "_tr_tally: bad match");

        s->dyn_ltree_[s->lut_.length_code[lc]+limits::literals+1].fc++;
        s->dyn_dtree_[d_code(dist)].fc++;
    }

#ifdef TRUNCATE_BLOCK
    /* Try to guess if it is profitable to stop the current block here */
    if ((s->last_lit_ & 0x1fff) == 0 && s->level > 2) {
        /* Compute an upper bound for the compressed length */
        std::uint32_t out_length = (std::uint32_t)s->last_lit_*8L;
        std::uint32_t in_length = (std::uint32_t)((long)s->strstart - s->block_start);
        int dcode;
        for (dcode = 0; dcode < limits::dCodes; dcode++) {
            out_length += (std::uint32_t)s->dyn_dtree_[dcode].fc *
                (5L+extra_dbits[dcode]);
        }
        out_length >>= 3;
        Tracev((stderr,"\nlast_lit %u, in %ld, out ~%ld(%ld%%) ",
               s->last_lit_, in_length, out_length,
               100L - out_length*100L/in_length));
        if (s->matches_ < s->last_lit_/2 && out_length < in_length/2) return 1;
    }
#endif
    return (s->last_lit_ == s->lit_bufsize_-1);
    /* We avoid equality with lit_bufsize because of wraparound at 64K
     * on 16 bit machines and because stored blocks are restricted to
     * 64K-1 bytes.
     */
}

/* ===========================================================================
 * Send the block data compressed using the given Huffman trees
 */
template<class _>
void
deflate_stream_t<_>::compress_block(
    deflate_stream_t *s,
    const detail::ct_data *ltree, /* literal tree */
    const detail::ct_data *dtree) /* distance tree */
{
    unsigned dist;      /* distance of matched string */
    int lc;             /* match length or unmatched char (if dist == 0) */
    unsigned lx = 0;    /* running index in l_buf */
    unsigned code;      /* the code to send */
    int extra;          /* number of extra bits to send */

    if (s->last_lit_ != 0) do {
        dist = s->d_buf_[lx];
        lc = s->l_buf_[lx++];
        if (dist == 0) {
            send_code(s, lc, ltree); /* send a literal byte */
            Tracecv(isgraph(lc), (stderr," '%c' ", lc));
        } else {
            /* Here, lc is the match length - limits::minMatch */
            code = s->lut_.length_code[lc];
            send_code(s, code+limits::literals+1, ltree); /* send the length code */
            extra = s->lut_.extra_lbits[code];
            if (extra != 0) {
                lc -= s->lut_.base_length[code];
                send_bits(s, lc, extra);       /* send the extra length bits */
            }
            dist--; /* dist is now the match distance - 1 */
            code = d_code(dist);
            Assert (code < limits::dCodes, "bad d_code");

            send_code(s, code, dtree);       /* send the distance code */
            extra = s->lut_.extra_dbits[code];
            if (extra != 0) {
                dist -= s->lut_.base_dist[code];
                send_bits(s, dist, extra);   /* send the extra distance bits */
            }
        } /* literal or match pair ? */

        /* Check that the overlay between pending_buf and d_buf+l_buf is ok: */
        Assert((uInt)(s->pending_) < s->lit_bufsize_ + 2*lx,
               "pendingBuf overflow");

    } while (lx < s->last_lit_);

    send_code(s, END_BLOCK, ltree);
}

/* ===========================================================================
 * Check if the data type is TEXT or BINARY, using the following algorithm:
 * - TEXT if the two conditions below are satisfied:
 *    a) There are no non-portable control characters belonging to the
 *       "black list" (0..6, 14..25, 28..31).
 *    b) There is at least one printable character belonging to the
 *       "white list" (9 {TAB}, 10 {LF}, 13 {CR}, 32..255).
 * - BINARY otherwise.
 * - The following partially-portable control characters form a
 *   "gray list" that is ignored in this detection algorithm:
 *   (7 {BEL}, 8 {BS}, 11 {VT}, 12 {FF}, 26 {SUB}, 27 {ESC}).
 * IN assertion: the fields fc of dyn_ltree are set.
 */
template<class _>
int
deflate_stream_t<_>::detect_data_type(deflate_stream_t *s)
{
    /* black_mask is the bit mask of black-listed bytes
     * set bits 0..6, 14..25, and 28..31
     * 0xf3ffc07f = binary 11110011111111111100000001111111
     */
    unsigned long black_mask = 0xf3ffc07fUL;
    int n;

    /* Check for non-textual ("black-listed") bytes. */
    for (n = 0; n <= 31; n++, black_mask >>= 1)
        if ((black_mask & 1) && (s->dyn_ltree_[n].fc != 0))
            return Z_BINARY;

    /* Check for textual ("white-listed") bytes. */
    if (s->dyn_ltree_[9].fc != 0 || s->dyn_ltree_[10].fc != 0
            || s->dyn_ltree_[13].fc != 0)
        return Z_TEXT;
    for (n = 32; n < limits::literals; n++)
        if (s->dyn_ltree_[n].fc != 0)
            return Z_TEXT;

    /* There are no "black-listed" or "white-listed" bytes:
     * this stream either is empty or has tolerated ("gray-listed") bytes only.
     */
    return Z_BINARY;
}

/* ===========================================================================
 * Reverse the first len bits of a code, using straightforward code (a faster
 * method would use a table)
 * IN assertion: 1 <= len <= 15
 */
template<class _>
unsigned
deflate_stream_t<_>::bi_reverse(
    unsigned code, /* the value to invert */
    int len)       /* its bit length */
{
    unsigned res = 0;
    do {
        res |= code & 1;
        code >>= 1, res <<= 1;
    } while (--len > 0);
    return res >> 1;
}

/* ===========================================================================
 * Flush the bit buffer, keeping at most 7 bits in it.
 */
template<class _>
void
deflate_stream_t<_>::bi_flush(
    deflate_stream_t *s)
{
    if (s->bi_valid_ == 16) {
        put_short(s, s->bi_buf_);
        s->bi_buf_ = 0;
        s->bi_valid_ = 0;
    } else if (s->bi_valid_ >= 8) {
        put_byte(s, (Byte)s->bi_buf_);
        s->bi_buf_ >>= 8;
        s->bi_valid_ -= 8;
    }
}

/* ===========================================================================
 * Flush the bit buffer and align the output on a byte boundary
 */
template<class _>
void
deflate_stream_t<_>::bi_windup(deflate_stream_t *s)
{
    if (s->bi_valid_ > 8) {
        put_short(s, s->bi_buf_);
    } else if (s->bi_valid_ > 0) {
        put_byte(s, (Byte)s->bi_buf_);
    }
    s->bi_buf_ = 0;
    s->bi_valid_ = 0;
#ifdef DEBUG
    s->bits_sent_ = (s->bits_sent_+7) & ~7;
#endif
}

/* ===========================================================================
 * Copy a stored block, storing first the length and its
 * one's complement if requested.
 */
template<class _>
void
deflate_stream_t<_>::copy_block(
    deflate_stream_t *s,
    char    *buf,    /* the input data */
    unsigned len,     /* its length */
    int      header)  /* true if block header must be written */
{
    bi_windup(s);        /* align on byte boundary */

    if (header) {
        put_short(s, (std::uint16_t)len);
        put_short(s, (std::uint16_t)~len);
#ifdef DEBUG
        s->bits_sent_ += 2*16;
#endif
    }
#ifdef DEBUG
    s->bits_sent_ += (std::uint32_t)len<<3;
#endif
    while (len--) {
        put_byte(s, *buf++);
    }
}

} // beast

#endif