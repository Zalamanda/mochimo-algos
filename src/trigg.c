/* ****************************************************************
 * Trigg Algorithm, Original Mochimo POW mining algorithm (CPU).
 *  - trigg.c (5 May 2020)
 *
 * Copyright (c) 2020 by Adequate Systems, LLC.  All Rights Reserved.
 * See LICENSE.PDF   **** NO WARRANTY ****
 *
 *    a raindrop
 *    on sunrise air--
 *    drowned
 *
 * Emulate a PDP-10 running MACLISP (circa. 1971)...
 *
 *    Trigg's Algorithm uses classic AI techniques to establish
 *    proof of work.  By expanding a semantic grammar through
 *    heuristic search and combining that with material from the
 *    transaction array, we build the TRIGG chain and solve the
 *    block as evidenced by the output of haiku with the vibe of
 *    Basho...
 *
 * DEPENDENCIES:
 *    sha256.c - 256-bit Secure Hash Algorithm
 *
 * ****************************************************************/

#ifndef _MOCHIMO_TRIGG_C_
#define _MOCHIMO_TRIGG_C_  /* include guard */


#include <stddef.h>
#include <stdint.h>

#include "../hash/sha256.c"


/* *************************************************
   * Embeded version of `rand.c` for isolated high *
   * speed pseudo random number generation.        *
   ************************************************* */

   #ifdef EXCLUDE_THREADSAFE
   /****************************************************/
   /* ---------------- NOT Threadsafe ---------------- */

   /* Removeable function redefinitions (inactive) */
   #define trigg_rand_unlock()  /* do nothing */
   #define trigg_rand_lock()    /* do nothing */


   #else /* end NOT Threadsafe */
   /****************************/

   /************************************************/
   /* ---------------- Threadsafe ---------------- */

   #include "../util/thread.c"

   /* Removeable function redefinitions (active) */
   #define trigg_rand_unlock()  mutex_unlock(&Trigg_rand_mutex)
   #define trigg_rand_lock()    mutex_lock(&Trigg_rand_mutex)

   /* Restricted use Mutex guard for number generator */
   static volatile Mutex Trigg_rand_mutex;


   #endif /* end Threadsafe */
   /*************************/

   static volatile uint32_t Trigg_seed = 1;

   void trigg_srand(uint32_t x)
   {
      trigg_rand_lock();
      Trigg_seed = x;
      trigg_rand_unlock();
   }

   uint32_t trigg_rand(void)
   {
      uint32_t r;

      trigg_rand_lock();
      Trigg_seed = Trigg_seed * 69069L + 262145L;
      r = Trigg_seed >> 16;
      trigg_rand_unlock();

      return r;
   }

/* ************************************
   * End embedded version of `rand.c` *
   ************************************ */


/* The features for the semantic grammar are
 * adapted from systemic grammar (Winograd, 1972). */
#define F_ING     1
#define F_INF     2
#define F_MOTION  4
#define F_VB      ( F_INT | F_INT | F_MOTION )

#define F_NS      8
#define F_NPL     16
#define F_N       ( F_NS | F_NPL )
#define F_MASS    32
#define F_AMB     64
#define F_TIMED   128
#define F_TIMEY   256
#define F_TIME    ( F_TIMED | F_TIMEY )
#define F_AT      512
#define F_ON      1024
#define F_IN      2048
#define F_LOC     ( F_AT | F_ON | F_IN )
#define F_NOUN    ( F_NS | F_NPL | F_MASS | F_TIME | F_LOC )

#define F_PREP    0x1000
#define F_ADJ     0x2000
#define F_OP      0x4000
#define F_DETS    0x8000
#define F_DETPL   0x10000
#define F_XLIT    0x20000

#define S_NL      ( F_XLIT + 1 )
#define S_CO      ( F_XLIT + 2 )
#define S_MD      ( F_XLIT + 3 )
#define S_LIKE    ( F_XLIT + 4 )
#define S_A       ( F_XLIT + 5 )
#define S_THE     ( F_XLIT + 6 )
#define S_OF      ( F_XLIT + 7 )
#define S_NO      ( F_XLIT + 8 )
#define S_S       ( F_XLIT + 9 )
#define S_AFTER   ( F_XLIT + 10 )
#define S_BEFORE  ( F_XLIT + 11 )

#define S_AT      ( F_XLIT + 12 )
#define S_IN      ( F_XLIT + 13 )
#define S_ON      ( F_XLIT + 14 )
#define S_UNDER   ( F_XLIT + 15 )
#define S_ABOVE   ( F_XLIT + 16 )
#define S_BELOW   ( F_XLIT + 17 )

#define HAIKUSIZE 256
#define MAXDICT   256
#define MAXH      16
#define NFRAMES   10

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

/* Trigg algorithm struct */
typedef struct {
   /* the TRIGG chain... */
   uint64_t mroot[4];         /* input merkle root */
   uint8_t haiku[HAIKUSIZE];  /* expanded haiku */
   uint64_t haiku2[2];        /* secondary haiku */
   uint64_t bnum;             /* block number */
   /* ... end TRIGG chain */
   uint64_t haiku1[2];        /* primary haiku */
   uint32_t diff;             /* the block diff */
} TRIGG_ALGO;

/* Dictionary entry with semantic grammar features */
typedef struct {
  uint8_t tok[12];  /* word token */
  uint32_t fe;      /* semantic features */
} DICT;

static DICT Dict[MAXDICT] = {

/* Adverbs and function words */
   { "NIL", 0 },
   { "\n", F_OP },
   { "\b:", F_OP },
   { "\b--", F_OP },
   { "like", F_OP },
   { "a", F_OP },
   { "the", F_OP },
   { "of", F_OP },
   { "no", F_OP },
   { "\bs", F_OP },
   { "after", F_OP },
   { "before", F_OP },

/* Prepositions */
   { "at", F_PREP },
   { "in", F_PREP },
   { "on", F_PREP },
   { "under", F_PREP },
   { "above", F_PREP },
   { "below", F_PREP },
 
/* Verbs - intransitive ING and MOTION */
   { "arriving", F_ING | F_MOTION },
   { "departing", F_ING | F_MOTION },
   { "going", F_ING | F_MOTION },
   { "coming", F_ING | F_MOTION },
   { "creeping", F_ING | F_MOTION },
   { "dancing", F_ING | F_MOTION },
   { "riding", F_ING | F_MOTION },
   { "strutting", F_ING | F_MOTION },
   { "leaping", F_ING | F_MOTION },
   { "leaving", F_ING | F_MOTION },
   { "entering", F_ING | F_MOTION },
   { "drifting", F_ING | F_MOTION },
   { "returning", F_ING | F_MOTION },
   { "rising", F_ING | F_MOTION },
   { "falling", F_ING | F_MOTION },
   { "rushing", F_ING | F_MOTION },
   { "soaring", F_ING | F_MOTION },
   { "travelling", F_ING | F_MOTION },
   { "turning", F_ING | F_MOTION },
   { "singing", F_ING | F_MOTION },
   { "walking", F_ING | F_MOTION },
/* Verbs - intransitive ING */
   { "crying", F_ING },
   { "weeping", F_ING },
   { "lingering", F_ING },
   { "pausing", F_ING },
   { "shining", F_ING },
/* -------------- motion intransitive infinitive */
   { "fall", F_INF | F_MOTION },
   { "flow", F_INF | F_MOTION },
   { "wander", F_INF | F_MOTION },
   { "disappear", F_INF | F_MOTION },
/* -------------- intransitive infinitive */
   { "wait", F_INF },
   { "bloom", F_INF },
   { "doze", F_INF },
   { "dream", F_INF },
   { "laugh", F_INF },
   { "meditate", F_INF },
   { "listen", F_INF },
   { "sing", F_INF },
   { "decay", F_INF },
   { "cling", F_INF },
   { "grow", F_INF },
   { "forget", F_INF },
   { "remain", F_INF },
 
/* Adjectives - physical */
/* valences (e) based on Osgood's evaluation factor */
   { "arid", F_ADJ },
   { "abandoned", F_ADJ },
   { "aged", F_ADJ },
   { "ancient", F_ADJ },
   { "full", F_ADJ },
   { "glorious", F_ADJ },
   { "good", F_ADJ },
   { "beautiful", F_ADJ },
   { "first", F_ADJ },
   { "last", F_ADJ },
   { "forsaken", F_ADJ },
   { "sad", F_ADJ },
   { "mandarin", F_ADJ },
   { "naked", F_ADJ },
   { "nameless", F_ADJ },
   { "old", F_ADJ },

/* Ambient adjectives */
   { "quiet", F_ADJ | F_AMB },
   { "peaceful", F_ADJ },
   { "still", F_ADJ },
   { "tranquil", F_ADJ },
   { "bare", F_ADJ },

/* Time interval adjectives or nouns */
   { "evening", F_ADJ | F_TIMED },
   { "morning", F_ADJ | F_TIMED },
   { "afternoon", F_ADJ | F_TIMED },
   { "spring", F_ADJ | F_TIMEY },
   { "summer", F_ADJ | F_TIMEY },
   { "autumn", F_ADJ | F_TIMEY },
   { "winter", F_ADJ | F_TIMEY },

/* Adjectives - physical */
   { "broken", F_ADJ },
   { "thick", F_ADJ },
   { "thin", F_ADJ },
   { "little", F_ADJ },
   { "big", F_ADJ },
/* Physical + ambient adjectives */
   { "parched", F_ADJ | F_AMB },
   { "withered", F_ADJ | F_AMB },
   { "worn", F_ADJ | F_AMB },
/* Physical adj -- material things */
   { "soft", F_ADJ },
   { "bitter", F_ADJ },
   { "bright", F_ADJ },
   { "brilliant", F_ADJ },
   { "cold", F_ADJ },
   { "cool", F_ADJ },
   { "crimson", F_ADJ },
   { "dark", F_ADJ },
   { "frozen", F_ADJ },
   { "grey", F_ADJ },
   { "hard", F_ADJ },
   { "hot", F_ADJ },
   { "scarlet", F_ADJ },
   { "shallow", F_ADJ },
   { "sharp", F_ADJ },
   { "warm", F_ADJ },
   { "close", F_ADJ },
   { "calm", F_ADJ },
   { "cruel", F_ADJ },
   { "drowned", F_ADJ },
   { "dull", F_ADJ },
   { "dead", F_ADJ },
   { "sick", F_ADJ },
   { "deep", F_ADJ },
   { "fast", F_ADJ },
   { "fleeting", F_ADJ },
   { "fragrant", F_ADJ },
   { "fresh", F_ADJ },
   { "loud", F_ADJ },
   { "moonlit", F_ADJ | F_AMB },
   { "sacred", F_ADJ },
   { "slow", F_ADJ },
 
/* Nouns top-level */
/* Humans */
   { "traveller", F_NS },
   { "poet", F_NS },
   { "beggar", F_NS },
   { "monk", F_NS },
   { "warrior", F_NS },
   { "wife", F_NS },
   { "courtesan", F_NS },
   { "dancer", F_NS },
   { "daemon", F_NS },

/* Animals */
   { "frog", F_NS },
   { "hawks", F_NPL },
   { "larks", F_NPL },
   { "cranes", F_NPL },
   { "crows", F_NPL },
   { "ducks", F_NPL },
   { "birds", F_NPL },
   { "skylark", F_NS },
   { "sparrows", F_NPL },
   { "minnows", F_NPL },
   { "snakes", F_NPL },
   { "dog", F_NS },
   { "monkeys", F_NPL },
   { "cats", F_NPL },
   { "cuckoos", F_NPL },
   { "mice", F_NPL },
   { "dragonfly", F_NS },
   { "butterfly", F_NS },
   { "firefly", F_NS },
   { "grasshopper", F_NS },
   { "mosquitos", F_NPL },

/* Plants */
   { "trees", F_NPL | F_IN | F_AT },
   { "roses", F_NPL },
   { "cherries", F_NPL },
   { "flowers", F_NPL },
   { "lotuses", F_NPL },
   { "plums", F_NPL },
   { "poppies", F_NPL },
   { "violets", F_NPL },
   { "oaks", F_NPL | F_AT },
   { "pines", F_NPL | F_AT },
   { "chestnuts", F_NPL },
   { "clovers", F_NPL },
   { "leaves", F_NPL },
   { "petals", F_NPL },
   { "thorns", F_NPL },
   { "blossoms", F_NPL },
   { "vines", F_NPL },
   { "willows", F_NPL },

/* Things */
   { "mountain", F_NS | F_AT | F_ON },
   { "moor", F_NS | F_AT | F_ON | F_IN },
   { "sea", F_NS | F_AT | F_ON | F_IN },
   { "shadow", F_NS | F_IN },
   { "skies", F_NPL | F_IN },
   { "moon", F_NS },
   { "star", F_NS },
   { "stone", F_NS },
   { "cloud", F_NS },
   { "bridge", F_NS | F_ON | F_AT },
   { "gate", F_NS | F_AT },
   { "temple", F_NS | F_IN | F_AT },
   { "hovel", F_NS | F_IN | F_AT },
   { "forest", F_NS | F_IN | F_AT },
   { "grave", F_NS | F_IN | F_AT | F_ON },
   { "stream", F_NS | F_IN | F_AT | F_ON },
   { "pond", F_NS | F_IN | F_AT | F_ON },
   { "island", F_NS | F_ON | F_AT },
   { "bell", F_NS },
   { "boat", F_NS | F_IN | F_ON },
   { "sailboat", F_NS | F_IN | F_ON },
   { "bon fire", F_NS | F_AT },
   { "straw mat", F_NS | F_ON },
   { "cup", F_NS | F_IN },
   { "nest", F_NS | F_IN },
   { "sun", F_NS | F_IN },
   { "village", F_NS | F_IN },
   { "tomb", F_NS | F_IN | F_AT },
   { "raindrop", F_NS | F_IN },
   { "wave", F_NS | F_IN },
   { "wind", F_NS | F_IN },
   { "tide", F_NS | F_IN | F_AT },
   { "fan", F_NS },
   { "hat", F_NS },
   { "sandal", F_NS },
   { "shroud", F_NS },
   { "pole", F_NS },

/* Mass - substance */
   { "water", F_ON | F_IN | F_MASS | F_AMB },
   { "air", F_ON | F_IN | F_MASS | F_AMB },
   { "mud", F_ON | F_IN | F_MASS | F_AMB },
   { "rain", F_IN | F_MASS | F_AMB },
   { "thunder", F_IN | F_MASS | F_AMB },
   { "ice", F_ON | F_IN | F_MASS | F_AMB },
   { "snow", F_ON | F_IN | F_MASS | F_AMB },
   { "salt", F_ON | F_IN | F_MASS },
   { "hail", F_IN | F_MASS | F_AMB },
   { "mist", F_IN | F_MASS | F_AMB },
   { "dew", F_IN | F_MASS | F_AMB },
   { "foam", F_IN | F_MASS | F_AMB },
   { "frost", F_IN | F_MASS | F_AMB },
   { "smoke", F_IN | F_MASS | F_AMB },
   { "twilight", F_IN | F_AT | F_MASS | F_AMB },
   { "earth", F_ON | F_IN | F_MASS },
   { "grass", F_ON | F_IN | F_MASS },
   { "bamboo", F_MASS },
   { "gold", F_MASS },
   { "grain", F_MASS },
   { "rice", F_MASS },
   { "tea", F_IN | F_MASS },
   { "light", F_IN | F_MASS | F_AMB },
   { "darkness", F_IN | F_MASS | F_AMB },
   { "firelight", F_IN | F_MASS | F_AMB },
   { "sunlight", F_IN | F_MASS | F_AMB },
   { "sunshine", F_IN | F_MASS | F_AMB },

/* Abstract nouns and acts */
   { "journey", F_NS | F_ON },
   { "serenity", F_MASS },
   { "dusk", F_TIMED },
   { "glow", F_NS },
   { "scent", F_NS },
   { "sound", F_NS },
   { "silence", F_NS },
   { "voice", F_NS },
   { "day", F_NS | F_TIMED },
   { "night", F_NS | F_TIMED },
   { "sunrise", F_NS | F_TIMED },
   { "sunset", F_NS | F_TIMED },
   { "midnight", F_NS | F_TIMED },
   { "equinox", F_NS | F_TIMEY },
   { "noon", F_NS | F_TIMED }

};  /* end Dict[] */

/* Case frames for the semantic grammar with a vibe inspired by Basho... */
static uint32_t Frame[][MAXH] = {
   { F_PREP, F_ADJ, F_MASS, S_NL,            /* on a quiet moor */
     F_NPL, S_NL,                            /* raindrops       */
     F_INF | F_ING                           /* fall            */
   },
   { F_PREP, F_MASS, S_NL,
     F_ADJ, F_NPL, S_NL,
     F_INF | F_ING
   },
   { F_PREP, F_TIMED, S_NL,
     F_ADJ, F_NPL, S_NL,
     F_INF | F_ING
   },
   { F_PREP, F_TIMED, S_NL,
     S_A, F_NS, S_NL,
     F_ING
   },
   { F_TIME, F_AMB, S_NL,                    /* morning mist      */
     F_PREP, S_A, F_ADJ, F_NS, S_MD, S_NL,   /* on a worn field-- */
     F_ADJ | F_ING                           /* red               */
   },
   { F_TIME, F_AMB, S_NL,
     F_ADJ, F_MASS, S_NL,
     F_ING
   },
   { F_TIME, F_MASS, S_NL,                   /* morning mist */
     F_INF, S_S, S_CO, S_NL,                 /* remains:     */
     F_AMB                                   /* smoke        */
   },
   { F_ING, F_PREP, S_A, F_ADJ, F_NS, S_NL,  /* arriving at a parched gate */
     F_MASS, F_ING, S_MD, S_NL,              /* mist rises--               */
     S_A, F_ADJ, F_NS                        /* a moonlit sandal           */
   },
   { F_ING, F_PREP, F_TIME, F_MASS, S_NL,    /* pausing under a hot tomb */
     F_MASS, F_ING, S_MD, S_NL,              /* firelight shining--      */
     S_A, F_ADJ, F_NS                        /* a beautiful bon fire     */
   },
   { S_A, F_NS, S_NL,                        /* a wife              */
     F_PREP, F_TIMED, F_MASS, S_MD, S_NL,    /* in afternoon mist-- */
     F_ADJ                                   /* sad                 */
   },
/*
     ! increment NFRAMES if adding more frames...
 */
};

/* Generate a tokenized haiku into `out` using the number generator. */
void *trigg_gen(void *out)
{
   uint32_t *fp;
   uint8_t *hp;
   int j, widx;

   /* choose a random haiku frame */
   fp = &Frame[trigg_rand() % NFRAMES][0];
   hp = (uint8_t *) out;
   for(j = 0; j < MAXH; j++, fp++) {
      if(*fp == 0) {
         /* zero fill end of haiku */
         *(hp++) = 0;
         continue;
      }
      if(*fp & F_XLIT) {
         /* force S_* type semantic feature where required by frame */
         widx = *fp & 255;
      } else {
         /* randomly select next word suitable for frame */
         for(;;) {
            widx = trigg_rand() & (MAXDICT - 1);
            if(Dict[widx].fe & *fp) break;
         }
      }
      *(hp++) = (uint8_t) widx;
   }

   return out;
}

/* Expand a haiku to character format.
 * It must have the correct syntax and vibe. */
char *trigg_expand(const void *nonce, void *haiku)
{
   uint8_t *np, *bp, *lbp, *w;
   int i;

   np = (uint8_t *) nonce;
   bp = (uint8_t *) haiku;
   lbp = bp + HAIKUSIZE;
   /* step through all haiku words */
   for(i = 0; i < MAXH; i++, np++) {
      if(*np == 0) break;
      /* place word from dictionary into bp */
      w = Dict[*np].tok;
      while(*w) *(bp++) = *(w++);
      if(bp[-1] != '\n') *(bp++) = ' ';
   }
   /* zero fill remaining character space */
   i = (lbp - bp) & 7;
   while(i--) *(bp++) = 0;  /* 8-bit */
   while(bp < lbp) {  /* 64-bit */
      *((uint64_t *) bp) = 0;
      bp += 8;
   }

   return (char *) haiku;
}

/* Evaluate the TRIGG chain by using a heuristic estimate of the
 * final solution cost (Nilsson, 1971). Evaluate the relative
 * distance within the TRIGG chain to validate proof of work.
 * Return 1 if solved, else 0. */
int trigg_eval(void *hash, uint8_t diff)
{
   uint8_t *bp, n;

   n = diff >> 3;
   /* coarse check required bytes are zero */
   for(bp = (uint8_t *) hash; n; n--)
      if(*(bp++) != 0) return 0;
   if((diff & 7) == 0) return 1;
   /* fine check required bits are zero */
   if((*bp & (~(0xff >> (diff & 7)))) != 0)
      return 0;

   return 1;
}

/* Prepare a TRIGG context for solving and generate
 * initial tokenized haiku for context. */
void trigg_solve(TRIGG_ALGO *T, const BTRAILER *bt)
{
   const uint64_t *qmroot;

   qmroot = (const uint64_t *) bt->mroot;

   /* place merkle root in (T)chain */
   T->mroot[0] = qmroot[0];
   T->mroot[1] = qmroot[1];
   T->mroot[2] = qmroot[2];
   T->mroot[3] = qmroot[3];
   /* place bnum at end of (T)chain */
   T->bnum = *((const uint64_t *) bt->bnum);
   /* place block difficulty in diff */
   T->diff = *((const uint32_t *) bt->difficulty);
   /* generate initial haiku */
   trigg_gen(T->haiku2);
}

/* Generate the haiku output as proof of work.
 * Create the haiku inside the TRIGG chain using a semantic grammar
 * (Burton, 1976). The output must pass syntax checks, the entropy
 * check, and have the right vibe. Entropy is always preserved at
 * high difficulty levels. Place nonce into `out` on success.
 * Return 1 on success, else 0. */
int trigg_generate(TRIGG_ALGO *T, void *out)
{
   uint8_t hash[HASHLEN];

   /* determine next nonce attempt */
   T->haiku1[0] = T->haiku2[0];
   T->haiku1[1] = T->haiku2[1];
   trigg_gen(T->haiku2);
   /* expand haiku1 in to the TRIGG chain! */
   trigg_expand(T->haiku1, T->haiku);

   /* perform SHA256 hash on TRIGG chain */
   sha256(T, 312, hash);

   /* evaluate result against required difficulty */
   if(trigg_eval(hash, (uint8_t) T->diff)) {
      /* copy successful haiku to `out` */
      ((uint64_t *) out)[0] = T->haiku1[0];
      ((uint64_t *) out)[1] = T->haiku1[1];
      ((uint64_t *) out)[2] = T->haiku2[0];
      ((uint64_t *) out)[3] = T->haiku2[1];
      return 1;
   }

   return 0;
}

/* Check haiku syntax against semantic grammar.
 * It must have the correct syntax, semantics, and vibe.
 * Return 1 on correct syntax, else 0. */
int trigg_syntax(const void *nonce)
{
   uint32_t sf[MAXH], *fp;
   uint8_t *np;
   int j;

   /* load haiku's semantic frame */
   np = (uint8_t *) nonce;
   for(j = 0; j < MAXH; j++)
      sf[j] = Dict[np[j]].fe;

   /* check input for respective semantic features.
    * use unification on feature sets... */
   for(fp = &Frame[0][0]; fp < &Frame[NFRAMES][0]; fp += MAXH) {
      for(j = 0; j < MAXH; j++) {
        if(fp[j] == 0) {
          if(sf[j] == 0) return 1;
          break;
        }
        if(fp[j] & F_XLIT) {
           if((fp[j] & 255) != np[j]) break;
           continue;
        }
        if((sf[j] & fp[j]) == 0) break;
      }
      if(j >= MAXH) return 1;
   }

   return 0;
}

/* Check proof of work. The haiku must be syntactically correct
 * and have the right vibe. Also, entropy MUST match difficulty.
 * If non-NULL, place final hash in `out` on success.
 * Return 1 on success, else return 0. */
#define trigg_check(btp)  trigg_checkhash(btp, NULL)
int trigg_checkhash(const BTRAILER *bt, void *out)
{
   TRIGG_ALGO T;
   uint8_t hash[HASHLEN];
   const uint64_t *qmroot, *qnonce;

   qnonce = (const uint64_t *) bt->nonce;
   qmroot = (const uint64_t *) bt->mroot;

   /* check syntax, semantics, and vibe... */
   if(trigg_syntax(qnonce) == 0) return 0;
   if(trigg_syntax(&qnonce[2]) == 0) return 0;

   /* memcpy merkle root, nonce and block number into TRIGG chain */
   T.mroot[0] = qmroot[0];
   T.mroot[1] = qmroot[1];
   T.mroot[2] = qmroot[2];
   T.mroot[3] = qmroot[3];
   T.haiku2[0] = qnonce[2];
   T.haiku2[1] = qnonce[3];
   T.bnum = *((const uint64_t *) bt->bnum);
   /* re-linearise the haiku */
   trigg_expand(qnonce, T.haiku);

   /* check entropy */
   sha256(&T, 312, hash);
   
   /* pass final hash to `out` if != NULL */
   if(out != NULL)
      memcpy(out, hash, HASHLEN);

   /* return evaluation */
   return trigg_eval(hash, bt->difficulty[0]);
}


#endif  /* end _MOCHIMO_TRIGG_C_ */
