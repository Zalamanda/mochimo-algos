/* ****************************************************************
 * Peach Algorithm, Mochimo FPGA-tough POW Mining Algorithm.
 *  - peach.c (5 May 2020)
 *
 * Copyright (c) 2020 by Adequate Systems, LLC.  All Rights Reserved.
 * See LICENSE.PDF   **** NO WARRANTY ****
 *
 * The Peach algorithm appears to be designed with the intention of
 * permitting a "mining advantage" to modern GPUs with >1GiB memory
 * capacity where it can cache and re-obtain a large amount of data
 * faster than it would take to re-compute it.
 *
 * The cache is made of 1048576 x 1KiB (1024 bytes) blocks of data.
 * The cache is deterministically generated from the previous block
 * hash on the Mochimo blockchain, thus making it unique per block.
 * This generation uses a process, dubbed as Nighthash, which takes
 * a seed to perform single precision floating point operations on,
 * which optionally transforms the seed and performs various random
 * repeating memory transformations, before selecting a random hash
 * function to finalise a 256-bit hash. This digest is then used as
 * input, repeating the process until the block is fully generated.
 *
 * Peach also utilizes the nonce restrictions designed for use with
 * the Trigg algorithm, to retain haiku syntax on the blockchain.
 *
 *    a raindrop
 *    on sunrise air--
 *    drowned
 *
 * DEPENDENCIES:
 *    trigg.c   - nonce generation and hash difficulty evaluation
 *    md2.c     - 128-bit Message Digest Algorithm
 *    md5.c     - 128-bit Message Digest Algorithm
 *    sha1.c    - 160-bit Secure Hash Algorithm
 *    sha256.c  - 256-bit Secure Hash Algorithm
 *    sha3.c    - 256-bit Secure Hash Algorithm
 *    blake2b.c - 256-bit Cryptographic Hash Algorithm
 *
 * MOTES:
 * - It may be desireable to completely avoid malloc() in software.
 *   In such cases, define STATIC_PEACH_MAP before the file include
 *   to compile with a restricted use semaphore map and cache.
 *
 * ****************************************************************/

#ifndef _MOCHIMO_PEACH_C_
#define _MOCHIMO_PEACH_C_  /* include guard */


#include <stddef.h>
#include <stdint.h>
#include <math.h>

#include "trigg.c"
#include "../hash/md2.c"
#include "../hash/md5.c"
#include "../hash/sha1.c"
#include "../hash/sha256.c"
#include "../hash/sha3.c"
#include "../hash/blake2b.c"

#define PEACH_NEXT    1060               /* (HASHLEN + 4 + PEACH_TILE) */
#define PEACH_GEN     36                 /* (HASHLEN + 4) */
#define PEACH_SIZE    1073741824  /* 1 GiB, (PEACH_MAP * PEACH_TILE) */
#define PEACH_MAP     1048576     /* 1 MiB, (PEACH_TILE * PEACH_TILE) */
#define PEACH_TILE    1024        /* 1 KiB, (PEACH_ROW * HASHLEN) */
#define PEACH_TILE32  256         /* 32-bit variation to PEACH_TILE */
#define PEACH_ROW     32          /*  32 B, HASHLEN */
#define PEACH_RNDS    8
#define PEACH_JUMP    8

#ifndef HASHLEN
#define HASHLEN  32
#endif

#ifndef BTSIZE
#define BTSIZE  160
typedef struct {  /* The block trailer struct... */
   uint8_t phash[HASHLEN];  /* previous block hash (32) */
   uint8_t bnum[8];         /* this block number */
   uint8_t mfee[8];         /* minimum transaction fee */
   uint8_t tcount[4];       /* transaction count */
   uint8_t time0[4];        /* to compute next difficulty */
   uint8_t difficulty[4];   /* difficulty of block */
   uint8_t mroot[HASHLEN];  /* hash of all TXQENTRY's */
   uint8_t nonce[HASHLEN];  /* haiku */
   uint8_t stime[4];        /* unsigned solve time GMT seconds */
   uint8_t bhash[HASHLEN];  /* hash of all block less bhash[] */
} BTRAILER;
#endif  /* ... end block trailer struct */

typedef struct {  /* Peach algorithm struct */
   const BTRAILER *bt;        /* pointer to block trailer */
   uint8_t *map;              /* map data, malloc for use */
   uint8_t *cache;            /* cache data, malloc for use */
   uint8_t tile[PEACH_TILE];  /* temporary tile, for validation */
   uint64_t nonce[4];         /* primary and secondarey haiku */
   uint32_t diff;             /* the block diff */
} PEACH_ALGO;

#ifdef STATIC_PEACH_MAP
/* Restricted use Peach semaphores */
static uint8_t Map_peach[PEACH_SIZE];
static uint8_t Cache_peach[PEACH_MAP];
#endif

/* The floating point operation function. Deterministically performs
 * (single precision) floating point operations on a set length of data
 * (in 4 byte chunks). Operations are only guarenteed "deterministic"
 * for IEEE-754 compliant hardware.
 * Returns an operation identifier as a 32-bit unsigned integer. */
static uint32_t peach_dflop(void *data, size_t len, uint32_t index, int txf)
{
   uint32_t op;
   int32_t operand;
   float *flp, temp, flv;
   uint8_t *bp, shift;
   int i;

   /* process entire length of input data; limit to 4 byte multiples */
   len = len - (len & 3);
   for(i = op = 0; i < len; i += 4, bp += 4) {
      bp = &((uint8_t *) data)[i];
      if(txf) {
         /* input data is modified directly */
         flp = (float *) bp;
      } else {
         /* temp variable is modified, input data is unchanged */
         temp = *((float *) bp);
         flp = &temp;
      }

      /* first byte allocated to determine shift amount */
      shift = ((*bp & 7) + 1) << 1;

      /* remaining bytes are selected for 3 different operations based on
       * the first bytes resulting shift on precomputed contants to...
       * ... 1) determine the floating point operation type */
      op += bp[((0x26C34 >> shift) & 3)];
      /* ... 2) determine the value of the operand */
      operand = bp[((0x14198 >> shift) & 3)];
      /* ... 3) determine the sign of the operand
       *        NOTE: must be performed AFTER the allocation of the operand */
      if(bp[((0x3D6EC >> shift) & 3)] & 1)
         operand ^= 0x80000000;

      /* cast operand to float */
      flv = (float) operand;

      /* Replace pre-operation NaN with index */
      if(isnan(*flp))
         *flp = (float) index;

      /**************************************************************
       * PEACHv2 thought: The separation of the addition/subtraction
       * operations are not contributing to the complexity of the
       * floating point operations. Perhaps...

      switch(op & 3) {
         case 3:  *flp /= operand;  break;
         case 2:  *flp *= operand;  break;
         case 1:  *flp = sqrtf(operand);  break; // requires <math.h>
         default: *flp += operand;
      }

       * ... adding the sqrt operation may help increase
       * the complexity of the *_dflop function while maintaining
       * deterministic intention with IEEE-754 compliant hardware.
       * ***********************************************************/

      /* Perform predetermined floating point operation */
      switch(op & 3) {
         case 0:  *flp += flv;  break;
         case 1:  *flp -= flv;  break;
         case 2:  *flp *= flv;  break;
         case 3:  *flp /= flv;  break;
      }

      /* Replace post-operation NaN with index */
      if(isnan(*flp))
         *flp = (float) index;

      /* Add result of the operation to `op` as an array of bytes */
      bp = (uint8_t *) flp;
      op += bp[0];
      op += bp[1];
      op += bp[2];
      op += bp[3];
   }  /* end for(i = 0; ... */

   return op;
}

/* The memory transformation function. Deterministically performs various
 * memory transformations on a set length of data .
 * Returns the modified `op` as a 32-bit unsigned integer. */
static uint32_t peach_dmemtx(void *data, size_t len, uint32_t op)
{
   size_t halflen, len32, len64, y;
   uint64_t *qp;
   uint32_t *dp;
   uint8_t *bp, temp;
   int i, z;

   bp = (uint8_t *) data;
   dp = (uint32_t *) data;
   qp = (uint64_t *) data;
   halflen = len >> 1;
   len32 = len >> 2;
   len64 = len >> 3;

   /* perform memory transformations multiple times */
   for(i = 0; i < PEACH_RNDS; i++) {
      /* Determine operation to use this iteration */
      op += bp[i & 31];

      /* select random transformation based on value of `op` */
      switch(op & 7) {
         case 0:  /* flip the first and last bit in every byte */
            for(z = 0; z < len64; z++)
               qp[z] ^= 0x8181818181818181ULL;
            for(z <<= 1; z < len32; z++)
               dp[z] ^= 0x81818181;
            break;
         case 1: /* Swap bytes */
            for(y = halflen, z = 0; z < halflen; y++, z++) {
               temp = bp[z];
               bp[z] = bp[y];
               bp[y] = temp;
            }
            break;
         case 2: /* 1's complement, all bytes */
            for(z = 0; z < len64; z++)
               qp[z] = ~qp[z];
            for(z <<= 1; z < len32; z++)
               dp[z] = ~dp[z];
            break;
         case 3: /* Alternate +1 and -1 on all bytes */
            for(z = 0; z < len; z++)
               bp[z] += ((z & 1) == 0) ? 1 : -1;
            break;
         case 4: /* Alternate -i and +i on all bytes */
            for(z = 0; z < len; z++)
               bp[z] += (uint8_t) (((z & 1) == 0) ? -i : i);
            break;
         case 5: /* Replace every occurrence of 104 with 72 */ 
            for(z = 0; z < len; z++)
               if(bp[z] == 104) bp[z] = 72;
            break;
         case 6: /* If byte a is > byte b, swap them. */
            for(y = halflen, z = 0; z < halflen; y++, z++)
               if(bp[z] > bp[y]) {
                  temp = bp[z];
                  bp[z] = bp[y];
                  bp[y] = temp;
               }
            break;
         case 7: /* XOR all bytes */
            for(y = 0, z = 1; z < len; y++, z++)
               bp[z] ^= bp[y];
            break;
      } /* end switch(op & 7)... */
   } /* end for(i = 0; ... */

   return op;
}

/* The nighthash function. Makes use of (single precision) deterministic
 * floating point operations and memory transformations. */
void peach_nighthash(void *in, size_t inlen, uint32_t index, int hashindex,
                     int txf, void *out)
{
   BLAKE2B_CTX blake2b_ctx;
   SHA1_CTX sha1_ctx;
   SHA256_CTX sha256_ctx;
   SHA3_CTX sha3_ctx;
/* KECCAK_CTX keccak_ctx;  // same as SHA3_CTX... */
   MD2_CTX md2_ctx;
   MD5_CTX md5_ctx;

   uint64_t key[8];
   uint32_t algo_type;

   /* perform flops to determine initial algo type. `txf` flag allows
    * transformation of input data. */
   algo_type = peach_dflop(in, inlen, index, txf);

   /* if `txf` is set, perform extra memory transformations to further
    * modify algo type and input data */
   if(txf) algo_type = peach_dmemtx(in, inlen, algo_type);

   /**************************************************************
    * PEACHv2 thought: Blake2b and Sha3 are reused as 2 additional
    * algorithms which doesn't really contribute to the hype and
    * complexity of an 8 algorithm nighthash. Additionally, there
    * are 3 other algorithms which don't utilise the entire 256
    * bits of entropy in a row.
    * ... sugestions for consideration are to include additional
    * algorithms to replace the reused ones, and either perform a
    * secondary pass of an algorithm using the initial output as
    * input or extending the hash in some fashion to fill out the
    * 256 bit width with (somewhat random) data instead of zeros.
    * ***********************************************************/

   /* reduce algorithm selection to 1 of 8 choices */
   algo_type &= 7;
   switch(algo_type) {
      case 0:  /* Blake2b w/ 32 byte key */
      case 1:  /* Blake2b w/ 64 byte key */
         /* set the value of key as repeating algo_type */
         memset(key, algo_type, 64);
         /* clear context and perform blake2b w/ 32 byte key */
         memset(&blake2b_ctx, 0, sizeof(BLAKE2B_CTX));
         blake2b_init(&blake2b_ctx, key, algo_type ? 64 : 32,
                      BLAKE2B_256_LENGTH);
         blake2b_update(&blake2b_ctx, in, inlen);
         if(hashindex)
            blake2b_update(&blake2b_ctx, &index, 4);
         blake2b_final(&blake2b_ctx, out);
         return;
      case 2:  /* SHA1 */
         /* clear context and perform algorithm */
         memset(&sha1_ctx, 0, sizeof(SHA1_CTX));
         sha1_init(&sha1_ctx);
         sha1_update(&sha1_ctx, in, inlen);
         if(hashindex)
            sha1_update(&sha1_ctx, &index, 4);
         sha1_final(&sha1_ctx, out);
         /* SHA1 hash is only 20 bytes long,
          * so set remaining bytes consistantly */
         ((uint32_t *) out)[5] = 0;
         ((uint64_t *) out)[3] = 0;
         return;
      case 3:  /* SHA256 */
         /* clear context and perform algorithm */
         memset(&sha256_ctx, 0, sizeof(SHA256_CTX));
         sha256_init(&sha256_ctx);
         sha256_update(&sha256_ctx, in, inlen);
         if(hashindex)
            sha256_update(&sha256_ctx, &index, 4);
         sha256_final(&sha256_ctx, out);
         return;
      case 4:  /* SHA3 */
      case 5:  /* Keccak */
         /* clear context and perform algorithm */
         memset(&sha3_ctx, 0, sizeof(SHA3_CTX));
         sha3_init(&sha3_ctx, SHA3_256_LENGTH);
         sha3_update(&sha3_ctx, in, inlen);
         if(hashindex)
            sha3_update(&sha3_ctx, &index, 4);
         if(algo_type == 4)
            sha3_final(&sha3_ctx, out);
         else
            keccak_final(&sha3_ctx, out);
         return;
      case 6:  /* MD2 */
         /* clear context and perform algorithm */
         memset(&md2_ctx, 0, sizeof(MD2_CTX));
         md2_init(&md2_ctx);
         md2_update(&md2_ctx, in, inlen);
         if(hashindex)
            md2_update(&md2_ctx, &index, 4);
         md2_final(&md2_ctx, out);
         /* MD2 hash is only 16 bytes long,
          * so set remaining bytes consistantly */
         ((uint64_t *) out)[2] = 0;
         ((uint64_t *) out)[3] = 0;
         return;
      case 7: /* MD5 */
         /* clear context and perform algorithm */
         memset(&md5_ctx, 0, sizeof(MD5_CTX));
         md5_init(&md5_ctx);
         md5_update(&md5_ctx, in, inlen);
         if(hashindex)
            md5_update(&md5_ctx, &index, 4);
         md5_final(&md5_ctx, out);
         /* MD5 hash is only 16 bytes long,
          * so set remaining bytes consistantly */
         ((uint64_t *) out)[2] = 0;
         ((uint64_t *) out)[3] = 0;
         return;
   }
}

/* Perform an index jump using the result hash of the nighthash function.
 * Returns the next index as a 32-bit unsigned integer. */
uint32_t peach_next(uint32_t index, uint32_t *dtile, const uint64_t *qnonce)
{
   uint8_t seed[PEACH_NEXT];
   uint8_t hash[HASHLEN];
   uint64_t *qseed;
   uint32_t *dseed, *dhash;
   int i;

   dhash = (uint32_t *) hash;
   dseed = (uint32_t *) seed;
   qseed = (uint64_t *) seed;

   /* construct data for use in nighthash for this index on the map */
   qseed[0] = qnonce[0];
   qseed[1] = qnonce[1];
   qseed[2] = qnonce[2];
   qseed[3] = qnonce[3];
   dseed[8] = index;

   /* adjust 32-bit pointer for data index synchronization */
   dseed = &dseed[9];
   for(i = 0; i < PEACH_TILE32; i++)
      dseed[i] = dtile[i];

   /* perform nighthash */
   peach_nighthash(seed, PEACH_NEXT, index, 0, 0, hash);

   /* add hash onto index as 8x 32-bit unsigned integers */
   index = dhash[0] + dhash[1] + dhash[2] + dhash[3] +
           dhash[4] + dhash[5] + dhash[6] + dhash[7];

   return index & (PEACH_MAP - 1);
}

/* Generate a tile of data on the PEACH map and cache (if setup).
 * Returns a pointer to the beginning of the tile. */
uint32_t *peach_gen(PEACH_ALGO *P, uint32_t index)
{
   uint8_t *tilep, seed[PEACH_GEN];
   uint32_t *dseed, *dhash;

   /* check for cached tile */
   if(P->cache && P->cache[index])
      return (uint32_t *) &(P->map[index * PEACH_TILE]);

   /* setup pointers */
   if(P->map) {
      tilep = P->map + (index * PEACH_TILE);
      P->cache[index] = 1;
   } else tilep = P->tile;
   dseed = (uint32_t *) seed;
   dhash = (uint32_t *) (P->bt)->phash;

   /* Create nighthash seed for this index on the map */
   dseed[0] = index;
   dseed[1] = dhash[0];
   dseed[2] = dhash[1];
   dseed[3] = dhash[2];
   dseed[4] = dhash[3];
   dseed[5] = dhash[4];
   dseed[6] = dhash[5];
   dseed[7] = dhash[6];
   dseed[8] = dhash[7];

   /* perform initial nighthash */
   peach_nighthash(seed, PEACH_GEN, index, 0, 1, tilep);

   /* continue to use nighthash to fill tile */
   peach_nighthash(&tilep[0], HASHLEN, index, 1, 1, &tilep[32]);
   peach_nighthash(&tilep[32], HASHLEN, index, 1, 1, &tilep[64]);
   peach_nighthash(&tilep[64], HASHLEN, index, 1, 1, &tilep[96]);
   peach_nighthash(&tilep[96], HASHLEN, index, 1, 1, &tilep[128]);
   peach_nighthash(&tilep[128], HASHLEN, index, 1, 1, &tilep[160]);
   peach_nighthash(&tilep[160], HASHLEN, index, 1, 1, &tilep[192]);
   peach_nighthash(&tilep[192], HASHLEN, index, 1, 1, &tilep[224]);
   peach_nighthash(&tilep[224], HASHLEN, index, 1, 1, &tilep[256]);
   peach_nighthash(&tilep[256], HASHLEN, index, 1, 1, &tilep[288]);
   peach_nighthash(&tilep[288], HASHLEN, index, 1, 1, &tilep[320]);
   peach_nighthash(&tilep[320], HASHLEN, index, 1, 1, &tilep[352]);
   peach_nighthash(&tilep[352], HASHLEN, index, 1, 1, &tilep[384]);
   peach_nighthash(&tilep[384], HASHLEN, index, 1, 1, &tilep[416]);
   peach_nighthash(&tilep[416], HASHLEN, index, 1, 1, &tilep[448]);
   peach_nighthash(&tilep[448], HASHLEN, index, 1, 1, &tilep[480]);
   peach_nighthash(&tilep[480], HASHLEN, index, 1, 1, &tilep[512]);
   peach_nighthash(&tilep[512], HASHLEN, index, 1, 1, &tilep[544]);
   peach_nighthash(&tilep[544], HASHLEN, index, 1, 1, &tilep[576]);
   peach_nighthash(&tilep[576], HASHLEN, index, 1, 1, &tilep[608]);
   peach_nighthash(&tilep[608], HASHLEN, index, 1, 1, &tilep[640]);
   peach_nighthash(&tilep[640], HASHLEN, index, 1, 1, &tilep[672]);
   peach_nighthash(&tilep[672], HASHLEN, index, 1, 1, &tilep[704]);
   peach_nighthash(&tilep[704], HASHLEN, index, 1, 1, &tilep[736]);
   peach_nighthash(&tilep[736], HASHLEN, index, 1, 1, &tilep[768]);
   peach_nighthash(&tilep[768], HASHLEN, index, 1, 1, &tilep[800]);
   peach_nighthash(&tilep[800], HASHLEN, index, 1, 1, &tilep[832]);
   peach_nighthash(&tilep[832], HASHLEN, index, 1, 1, &tilep[864]);
   peach_nighthash(&tilep[864], HASHLEN, index, 1, 1, &tilep[896]);
   peach_nighthash(&tilep[896], HASHLEN, index, 1, 1, &tilep[928]);
   peach_nighthash(&tilep[928], HASHLEN, index, 1, 1, &tilep[960]);
   peach_nighthash(&tilep[960], HASHLEN, index, 1, 1, &tilep[992]);

   return (uint32_t *) tilep;
}

/* Free any memory allocated in the peach context. */
void peach_free(PEACH_ALGO *P)
{
#ifndef STATIC_PEACH_MAP
   if(P->map) free(P->map);
   if(P->cache) free(P->cache);
#endif

   P->map = P->cache = NULL;
}

/* Prepare a PEACH context for solving. */
int peach_solve(PEACH_ALGO *P, const BTRAILER *bt)
{
   uint64_t *zp;
   int i, len;

   memset(P, 0, sizeof(PEACH_ALGO));

#ifdef STATIC_PEACH_MAP
   /* assign static semaphores */
   P->map = Map_peach;
   P->cache = Cache_peach;
#else
   /* allocate memory for map and cache */
   P->map = malloc(PEACH_SIZE);
   P->cache = malloc(PEACH_MAP);
#endif

   if(P->map && P->cache) {
      /* zero allocated memory */
      len = PEACH_SIZE >> 3;
      for(i = 0, zp = (uint64_t *) P->map; i < len; zp[i++] = 0);
      len = PEACH_MAP >> 3;
      for(i = 0, zp = (uint64_t *) P->cache; i < len; zp[i++] = 0);
   } else {
      peach_free(P);
      return 1;
   }

   /* set btp and difficulty */
   P->bt = bt;
   P->diff = *((uint32_t *) bt->difficulty);

   /* generate initial haiku */
   trigg_gen(&(P->nonce[2]));

   return 0;
}

/* Combine haiku protocols implemented in the Trigg Algorithm with the
 * memory intensive protocols of the Peach algorithm to generate haiku
 * output as proof of work. Place nonce into `out` on success.
 * Return 1 on success, else 0. */
int peach_generate(PEACH_ALGO *P, void *out)
{
   SHA256_CTX ictx;
   uint8_t hash[HASHLEN], bt_hash[HASHLEN];
   uint32_t *tilep, mario;
   int i;

   /* advance nonce */
   P->nonce[0] = P->nonce[2];
   P->nonce[1] = P->nonce[3];
   trigg_gen(&(P->nonce[2]));

   /* obtain a starting sha256 hash of the "known" block trailer */
   sha256_init(&ictx);
   sha256_update(&ictx, P->bt, 92);
   sha256_update(&ictx, P->nonce, 32);
   sha256_final(&ictx, bt_hash);

   /**************************************************************
    * PEACHv2 thought: multiplying `mario` by every byte of a hash
    * has a 1 in 8 chance of having a final result of 0, which is
    * an arguable complexity setback...
    * .. suggest using a non-multiplication type operation on
    * 32-bit (un)signed integers of the hash instead.
    * ***********************************************************/

   /* determine mario's initial position */
   mario = bt_hash[0];
   for(i = 1; i < HASHLEN; i++)
      mario *= bt_hash[i];

   /* map boundary protection */
   mario &= PEACH_MAP - 1;

   /* move across the map, in search of the princess */
   tilep = peach_gen(P, mario);
   for(i = 0; i < PEACH_JUMP; i++) {
      mario = peach_next(mario, tilep, P->nonce);
      tilep = peach_gen(P, mario);
   }

   /* perform final sha256 hash for validation */
   sha256_init(&ictx);
   sha256_update(&ictx, bt_hash, HASHLEN);
   sha256_update(&ictx, tilep, PEACH_TILE);
   sha256_final(&ictx, hash);

   /* evaluate result against required difficulty */
   if(trigg_eval(hash, (uint8_t) P->diff)) {
      /* copy successful haiku to `out` */
      ((uint64_t *) out)[0] = P->nonce[0];
      ((uint64_t *) out)[1] = P->nonce[1];
      ((uint64_t *) out)[2] = P->nonce[2];
      ((uint64_t *) out)[3] = P->nonce[3];
      return 1;
   }

   return 0;
}

/* Check proof of work. The haiku must be syntactically correct
 * and have the right vibe. Also, entropy MUST match difficulty.
 * If non-NULL, place final hash in `out` on success.
 * Return 1 on success, else return 0. */
#define peach_check(btp)  peach_checkhash(btp, NULL)
int peach_checkhash(const BTRAILER *bt, void *out)
{
   PEACH_ALGO P;
   SHA256_CTX ictx;
   uint8_t hash[HASHLEN], bt_hash[HASHLEN];
   uint32_t *tilep, mario;
   int i;


   /* check syntax, semantics, and vibe... */
   if(trigg_syntax(bt->nonce) == 0) return 0;
   if(trigg_syntax(&bt->nonce[16]) == 0) return 0;

   /* clear peach and copy btp */
   memset(&P, 0, sizeof(PEACH_ALGO));
   P.bt = bt;

   /* `peach_generate()` without haiku generation... */
   sha256_init(&ictx);
   sha256_update(&ictx, bt, 124);
   sha256_final(&ictx, bt_hash);

   mario = bt_hash[0];
   for(i = 1; i < HASHLEN; i++)
      mario *= bt_hash[i];
   mario &= PEACH_MAP - 1;

   tilep = peach_gen(&P, mario);
   for(i = 0; i < PEACH_JUMP; i++) {
      mario = peach_next(mario, tilep, (const uint64_t *) bt->nonce);
      tilep = peach_gen(&P, mario);
   }

   sha256_init(&ictx);
   sha256_update(&ictx, bt_hash, HASHLEN);
   sha256_update(&ictx, tilep, PEACH_TILE);
   sha256_final(&ictx, hash);

   /* pass final hash to `out` if != NULL */
   if(out != NULL)
      memcpy(out, hash, HASHLEN);

   /* return evaluation */
   return trigg_eval(hash, bt->difficulty[0]);
}


#endif  /* end _MOCHIMO_PEACH_C */
