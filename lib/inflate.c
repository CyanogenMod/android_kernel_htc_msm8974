#define DEBG(x)
#define DEBG1(x)
/* inflate.c -- Not copyrighted 1992 by Mark Adler
   version c10p1, 10 January 1993 */




#include <linux/compiler.h>
#ifdef NO_INFLATE_MALLOC
#include <linux/slab.h>
#endif

#ifdef RCSID
static char rcsid[] = "#Id: inflate.c,v 0.14 1993/06/10 13:27:04 jloup Exp #";
#endif

#ifndef STATIC

#if defined(STDC_HEADERS) || defined(HAVE_STDLIB_H)
#  include <sys/types.h>
#  include <stdlib.h>
#endif

#include "gzip.h"
#define STATIC
#endif 

#ifndef INIT
#define INIT
#endif
	
#define slide window

struct huft {
  uch e;                
  uch b;                
  union {
    ush n;              
    struct huft *t;     
  } v;
};


STATIC int INIT huft_build OF((unsigned *, unsigned, unsigned, 
		const ush *, const ush *, struct huft **, int *));
STATIC int INIT huft_free OF((struct huft *));
STATIC int INIT inflate_codes OF((struct huft *, struct huft *, int, int));
STATIC int INIT inflate_stored OF((void));
STATIC int INIT inflate_fixed OF((void));
STATIC int INIT inflate_dynamic OF((void));
STATIC int INIT inflate_block OF((int *));
STATIC int INIT inflate OF((void));


#define wp outcnt
#define flush_output(w) (wp=(w),flush_window())

static const unsigned border[] = {    
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static const ush cplens[] = {         
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
        
static const ush cplext[] = {         
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99}; 
static const ush cpdist[] = {         
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
static const ush cpdext[] = {         
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13};




STATIC ulg bb;                         
STATIC unsigned bk;                    

STATIC const ush mask_bits[] = {
    0x0000,
    0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
    0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#define NEXTBYTE()  ({ int v = get_byte(); if (v < 0) goto underrun; (uch)v; })
#define NEEDBITS(n) {while(k<(n)){b|=((ulg)NEXTBYTE())<<k;k+=8;}}
#define DUMPBITS(n) {b>>=(n);k-=(n);}

#ifndef NO_INFLATE_MALLOC

static unsigned long malloc_ptr;
static int malloc_count;

static void *malloc(int size)
{
       void *p;

       if (size < 0)
		error("Malloc error");
       if (!malloc_ptr)
		malloc_ptr = free_mem_ptr;

       malloc_ptr = (malloc_ptr + 3) & ~3;     

       p = (void *)malloc_ptr;
       malloc_ptr += size;

       if (free_mem_end_ptr && malloc_ptr >= free_mem_end_ptr)
		error("Out of memory");

       malloc_count++;
       return p;
}

static void free(void *where)
{
       malloc_count--;
       if (!malloc_count)
		malloc_ptr = free_mem_ptr;
}
#else
#define malloc(a) kmalloc(a, GFP_KERNEL)
#define free(a) kfree(a)
#endif



STATIC const int lbits = 9;          
STATIC const int dbits = 6;          


#define BMAX 16         
#define N_MAX 288       


STATIC unsigned hufts;         


STATIC int INIT huft_build(
	unsigned *b,            
	unsigned n,             
	unsigned s,             
	const ush *d,           
	const ush *e,           
	struct huft **t,        
	int *m                  
	)
{
  unsigned a;                   
  unsigned f;                   
  int g;                        
  int h;                        
  register unsigned i;          
  register unsigned j;          
  register int k;               
  int l;                        
  register unsigned *p;         
  register struct huft *q;      
  struct huft r;                
  register int w;               
  unsigned *xp;                 
  int y;                        
  unsigned z;                   
  struct {
    unsigned c[BMAX+1];           
    struct huft *u[BMAX];         
    unsigned v[N_MAX];            
    unsigned x[BMAX+1];           
  } *stk;
  unsigned *c, *v, *x;
  struct huft **u;
  int ret;

DEBG("huft1 ");

  stk = malloc(sizeof(*stk));
  if (stk == NULL)
    return 3;			

  c = stk->c;
  v = stk->v;
  x = stk->x;
  u = stk->u;

  
  memzero(stk->c, sizeof(stk->c));
  p = b;  i = n;
  do {
    Tracecv(*p, (stderr, (n-i >= ' ' && n-i <= '~' ? "%c %d\n" : "0x%x %d\n"), 
	    n-i, *p));
    c[*p]++;                    
    p++;                      
  } while (--i);
  if (c[0] == n)                
  {
    *t = (struct huft *)NULL;
    *m = 0;
    ret = 2;
    goto out;
  }

DEBG("huft2 ");

  
  l = *m;
  for (j = 1; j <= BMAX; j++)
    if (c[j])
      break;
  k = j;                        
  if ((unsigned)l < j)
    l = j;
  for (i = BMAX; i; i--)
    if (c[i])
      break;
  g = i;                        
  if ((unsigned)l > i)
    l = i;
  *m = l;

DEBG("huft3 ");

  
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= c[j]) < 0) {
      ret = 2;                 
      goto out;
    }
  if ((y -= c[i]) < 0) {
    ret = 2;
    goto out;
  }
  c[i] += y;

DEBG("huft4 ");

  
  x[1] = j = 0;
  p = c + 1;  xp = x + 2;
  while (--i) {                 
    *xp++ = (j += *p++);
  }

DEBG("huft5 ");

  
  p = b;  i = 0;
  do {
    if ((j = *p++) != 0)
      v[x[j]++] = i;
  } while (++i < n);
  n = x[g];                   

DEBG("h6 ");

  
  x[0] = i = 0;                 
  p = v;                        
  h = -1;                       
  w = -l;                       
  u[0] = (struct huft *)NULL;   
  q = (struct huft *)NULL;      
  z = 0;                        
DEBG("h6a ");

  
  for (; k <= g; k++)
  {
DEBG("h6b ");
    a = c[k];
    while (a--)
    {
DEBG("h6b1 ");
      
      
      while (k > w + l)
      {
DEBG1("1 ");
        h++;
        w += l;                 

        
        z = (z = g - w) > (unsigned)l ? l : z;  
        if ((f = 1 << (j = k - w)) > a + 1)     
        {                       
DEBG1("2 ");
          f -= a + 1;           
          xp = c + k;
          if (j < z)
            while (++j < z)       
            {
              if ((f <<= 1) <= *++xp)
                break;            
              f -= *xp;           
            }
        }
DEBG1("3 ");
        z = 1 << j;             

        
        if ((q = (struct huft *)malloc((z + 1)*sizeof(struct huft))) ==
            (struct huft *)NULL)
        {
          if (h)
            huft_free(u[0]);
          ret = 3;             
	  goto out;
        }
DEBG1("4 ");
        hufts += z + 1;         
        *t = q + 1;             
        *(t = &(q->v.t)) = (struct huft *)NULL;
        u[h] = ++q;             

DEBG1("5 ");
        
        if (h)
        {
          x[h] = i;             
          r.b = (uch)l;         
          r.e = (uch)(16 + j);  
          r.v.t = q;            
          j = i >> (w - l);     
          u[h-1][j] = r;        
        }
DEBG1("6 ");
      }
DEBG("h6c ");

      
      r.b = (uch)(k - w);
      if (p >= v + n)
        r.e = 99;               
      else if (*p < s)
      {
        r.e = (uch)(*p < 256 ? 16 : 15);    
        r.v.n = (ush)(*p);             
	p++;                           
      }
      else
      {
        r.e = (uch)e[*p - s];   
        r.v.n = d[*p++ - s];
      }
DEBG("h6d ");

      
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      
      while ((i & ((1 << w) - 1)) != x[h])
      {
        h--;                    
        w -= l;
      }
DEBG("h6e ");
    }
DEBG("h6f ");
  }

DEBG("huft7 ");

  
  ret = y != 0 && g != 1;

  out:
  free(stk);
  return ret;
}



STATIC int INIT huft_free(
	struct huft *t         
	)
{
  register struct huft *p, *q;


  
  p = t;
  while (p != (struct huft *)NULL)
  {
    q = (--p)->v.t;
    free((char*)p);
    p = q;
  } 
  return 0;
}


STATIC int INIT inflate_codes(
	struct huft *tl,    
	struct huft *td,    
	int bl,             
	int bd              
	)
{
  register unsigned e;  
  unsigned n, d;        
  unsigned w;           
  struct huft *t;       
  unsigned ml, md;      
  register ulg b;       
  register unsigned k;  


  
  b = bb;                       
  k = bk;
  w = wp;                       

  
  ml = mask_bits[bl];           
  md = mask_bits[bd];
  for (;;)                      
  {
    NEEDBITS((unsigned)bl)
    if ((e = (t = tl + ((unsigned)b & ml))->e) > 16)
      do {
        if (e == 99)
          return 1;
        DUMPBITS(t->b)
        e -= 16;
        NEEDBITS(e)
      } while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
    DUMPBITS(t->b)
    if (e == 16)                
    {
      slide[w++] = (uch)t->v.n;
      Tracevv((stderr, "%c", slide[w-1]));
      if (w == WSIZE)
      {
        flush_output(w);
        w = 0;
      }
    }
    else                        
    {
      
      if (e == 15)
        break;

      
      NEEDBITS(e)
      n = t->v.n + ((unsigned)b & mask_bits[e]);
      DUMPBITS(e);

      
      NEEDBITS((unsigned)bd)
      if ((e = (t = td + ((unsigned)b & md))->e) > 16)
        do {
          if (e == 99)
            return 1;
          DUMPBITS(t->b)
          e -= 16;
          NEEDBITS(e)
        } while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
      DUMPBITS(t->b)
      NEEDBITS(e)
      d = w - t->v.n - ((unsigned)b & mask_bits[e]);
      DUMPBITS(e)
      Tracevv((stderr,"\\[%d,%d]", w-d, n));

      
      do {
        n -= (e = (e = WSIZE - ((d &= WSIZE-1) > w ? d : w)) > n ? n : e);
#if !defined(NOMEMCPY) && !defined(DEBUG)
        if (w - d >= e)         
        {
          memcpy(slide + w, slide + d, e);
          w += e;
          d += e;
        }
        else                      
#endif 
          do {
            slide[w++] = slide[d++];
	    Tracevv((stderr, "%c", slide[w-1]));
          } while (--e);
        if (w == WSIZE)
        {
          flush_output(w);
          w = 0;
        }
      } while (n);
    }
  }


  
  wp = w;                       
  bb = b;                       
  bk = k;

  
  return 0;

 underrun:
  return 4;			
}



STATIC int INIT inflate_stored(void)
{
  unsigned n;           
  unsigned w;           
  register ulg b;       
  register unsigned k;  

DEBG("<stor");

  
  b = bb;                       
  k = bk;
  w = wp;                       


  
  n = k & 7;
  DUMPBITS(n);


  
  NEEDBITS(16)
  n = ((unsigned)b & 0xffff);
  DUMPBITS(16)
  NEEDBITS(16)
  if (n != (unsigned)((~b) & 0xffff))
    return 1;                   
  DUMPBITS(16)


  
  while (n--)
  {
    NEEDBITS(8)
    slide[w++] = (uch)b;
    if (w == WSIZE)
    {
      flush_output(w);
      w = 0;
    }
    DUMPBITS(8)
  }


  
  wp = w;                       
  bb = b;                       
  bk = k;

  DEBG(">");
  return 0;

 underrun:
  return 4;			
}


STATIC int noinline INIT inflate_fixed(void)
{
  int i;                
  struct huft *tl;      
  struct huft *td;      
  int bl;               
  int bd;               
  unsigned *l;          

DEBG("<fix");

  l = malloc(sizeof(*l) * 288);
  if (l == NULL)
    return 3;			

  
  for (i = 0; i < 144; i++)
    l[i] = 8;
  for (; i < 256; i++)
    l[i] = 9;
  for (; i < 280; i++)
    l[i] = 7;
  for (; i < 288; i++)          
    l[i] = 8;
  bl = 7;
  if ((i = huft_build(l, 288, 257, cplens, cplext, &tl, &bl)) != 0) {
    free(l);
    return i;
  }

  
  for (i = 0; i < 30; i++)      
    l[i] = 5;
  bd = 5;
  if ((i = huft_build(l, 30, 0, cpdist, cpdext, &td, &bd)) > 1)
  {
    huft_free(tl);
    free(l);

    DEBG(">");
    return i;
  }


  
  if (inflate_codes(tl, td, bl, bd)) {
    free(l);
    return 1;
  }

  
  free(l);
  huft_free(tl);
  huft_free(td);
  return 0;
}


STATIC int noinline INIT inflate_dynamic(void)
{
  int i;                
  unsigned j;
  unsigned l;           
  unsigned m;           
  unsigned n;           
  struct huft *tl;      
  struct huft *td;      
  int bl;               
  int bd;               
  unsigned nb;          
  unsigned nl;          
  unsigned nd;          
  unsigned *ll;         
  register ulg b;       
  register unsigned k;  
  int ret;

DEBG("<dyn");

#ifdef PKZIP_BUG_WORKAROUND
  ll = malloc(sizeof(*ll) * (288+32));  
#else
  ll = malloc(sizeof(*ll) * (286+30));  
#endif

  if (ll == NULL)
    return 1;

  
  b = bb;
  k = bk;


  
  NEEDBITS(5)
  nl = 257 + ((unsigned)b & 0x1f);      
  DUMPBITS(5)
  NEEDBITS(5)
  nd = 1 + ((unsigned)b & 0x1f);        
  DUMPBITS(5)
  NEEDBITS(4)
  nb = 4 + ((unsigned)b & 0xf);         
  DUMPBITS(4)
#ifdef PKZIP_BUG_WORKAROUND
  if (nl > 288 || nd > 32)
#else
  if (nl > 286 || nd > 30)
#endif
  {
    ret = 1;             
    goto out;
  }

DEBG("dyn1 ");

  
  for (j = 0; j < nb; j++)
  {
    NEEDBITS(3)
    ll[border[j]] = (unsigned)b & 7;
    DUMPBITS(3)
  }
  for (; j < 19; j++)
    ll[border[j]] = 0;

DEBG("dyn2 ");

  
  bl = 7;
  if ((i = huft_build(ll, 19, 19, NULL, NULL, &tl, &bl)) != 0)
  {
    if (i == 1)
      huft_free(tl);
    ret = i;                   
    goto out;
  }

DEBG("dyn3 ");

  
  n = nl + nd;
  m = mask_bits[bl];
  i = l = 0;
  while ((unsigned)i < n)
  {
    NEEDBITS((unsigned)bl)
    j = (td = tl + ((unsigned)b & m))->b;
    DUMPBITS(j)
    j = td->v.n;
    if (j < 16)                 
      ll[i++] = l = j;          
    else if (j == 16)           
    {
      NEEDBITS(2)
      j = 3 + ((unsigned)b & 3);
      DUMPBITS(2)
      if ((unsigned)i + j > n) {
        ret = 1;
	goto out;
      }
      while (j--)
        ll[i++] = l;
    }
    else if (j == 17)           
    {
      NEEDBITS(3)
      j = 3 + ((unsigned)b & 7);
      DUMPBITS(3)
      if ((unsigned)i + j > n) {
        ret = 1;
	goto out;
      }
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
    else                        
    {
      NEEDBITS(7)
      j = 11 + ((unsigned)b & 0x7f);
      DUMPBITS(7)
      if ((unsigned)i + j > n) {
        ret = 1;
	goto out;
      }
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
  }

DEBG("dyn4 ");

  
  huft_free(tl);

DEBG("dyn5 ");

  
  bb = b;
  bk = k;

DEBG("dyn5a ");

  
  bl = lbits;
  if ((i = huft_build(ll, nl, 257, cplens, cplext, &tl, &bl)) != 0)
  {
DEBG("dyn5b ");
    if (i == 1) {
      error("incomplete literal tree");
      huft_free(tl);
    }
    ret = i;                   
    goto out;
  }
DEBG("dyn5c ");
  bd = dbits;
  if ((i = huft_build(ll + nl, nd, 0, cpdist, cpdext, &td, &bd)) != 0)
  {
DEBG("dyn5d ");
    if (i == 1) {
      error("incomplete distance tree");
#ifdef PKZIP_BUG_WORKAROUND
      i = 0;
    }
#else
      huft_free(td);
    }
    huft_free(tl);
    ret = i;                   
    goto out;
#endif
  }

DEBG("dyn6 ");

  
  if (inflate_codes(tl, td, bl, bd)) {
    ret = 1;
    goto out;
  }

DEBG("dyn7 ");

  
  huft_free(tl);
  huft_free(td);

  DEBG(">");
  ret = 0;
out:
  free(ll);
  return ret;

underrun:
  ret = 4;			
  goto out;
}



STATIC int INIT inflate_block(
	int *e                  
	)
{
  unsigned t;           
  register ulg b;       
  register unsigned k;  

  DEBG("<blk");

  
  b = bb;
  k = bk;


  
  NEEDBITS(1)
  *e = (int)b & 1;
  DUMPBITS(1)


  
  NEEDBITS(2)
  t = (unsigned)b & 3;
  DUMPBITS(2)


  
  bb = b;
  bk = k;

  
  if (t == 2)
    return inflate_dynamic();
  if (t == 0)
    return inflate_stored();
  if (t == 1)
    return inflate_fixed();

  DEBG(">");

  
  return 2;

 underrun:
  return 4;			
}



STATIC int INIT inflate(void)
{
  int e;                
  int r;                
  unsigned h;           

  
  wp = 0;
  bk = 0;
  bb = 0;


  
  h = 0;
  do {
    hufts = 0;
#ifdef ARCH_HAS_DECOMP_WDOG
    arch_decomp_wdog();
#endif
    r = inflate_block(&e);
    if (r)
	    return r;
    if (hufts > h)
      h = hufts;
  } while (!e);

  while (bk >= 8) {
    bk -= 8;
    inptr--;
  }

  
  flush_output(wp);


  
#ifdef DEBUG
  fprintf(stderr, "<%u> ", h);
#endif 
  return 0;
}


static ulg crc_32_tab[256];
static ulg crc;		
#define CRC_VALUE (crc ^ 0xffffffffUL)


static void INIT
makecrc(void)
{
/* Not copyrighted 1990 Mark Adler	*/

  unsigned long c;      
  unsigned long e;      
  int i;                
  int k;                

  
  static const int p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  
  e = 0;
  for (i = 0; i < sizeof(p)/sizeof(int); i++)
    e |= 1L << (31 - p[i]);

  crc_32_tab[0] = 0;

  for (i = 1; i < 256; i++)
  {
    c = 0;
    for (k = i | 256; k != 1; k >>= 1)
    {
      c = c & 1 ? (c >> 1) ^ e : c >> 1;
      if (k & 1)
        c ^= e;
    }
    crc_32_tab[i] = c;
  }

  
  crc = (ulg)0xffffffffUL; 
}

#define ASCII_FLAG   0x01 
#define CONTINUATION 0x02 
#define EXTRA_FIELD  0x04 
#define ORIG_NAME    0x08 
#define COMMENT      0x10 
#define ENCRYPTED    0x20 
#define RESERVED     0xC0 

static int INIT gunzip(void)
{
    uch flags;
    unsigned char magic[2]; 
    char method;
    ulg orig_crc = 0;       
    ulg orig_len = 0;       
    int res;

    magic[0] = NEXTBYTE();
    magic[1] = NEXTBYTE();
    method   = NEXTBYTE();

    if (magic[0] != 037 ||
	((magic[1] != 0213) && (magic[1] != 0236))) {
	    error("bad gzip magic numbers");
	    return -1;
    }

    
    if (method != 8)  {
	    error("internal error, invalid method");
	    return -1;
    }

    flags  = (uch)get_byte();
    if ((flags & ENCRYPTED) != 0) {
	    error("Input is encrypted");
	    return -1;
    }
    if ((flags & CONTINUATION) != 0) {
	    error("Multi part input");
	    return -1;
    }
    if ((flags & RESERVED) != 0) {
	    error("Input has invalid flags");
	    return -1;
    }
    NEXTBYTE();	
    NEXTBYTE();
    NEXTBYTE();
    NEXTBYTE();

    (void)NEXTBYTE();  
    (void)NEXTBYTE();  

    if ((flags & EXTRA_FIELD) != 0) {
	    unsigned len = (unsigned)NEXTBYTE();
	    len |= ((unsigned)NEXTBYTE())<<8;
	    while (len--) (void)NEXTBYTE();
    }

    
    if ((flags & ORIG_NAME) != 0) {
	    
	    while (NEXTBYTE() != 0)  ;
    } 

    
    if ((flags & COMMENT) != 0) {
	    while (NEXTBYTE() != 0)  ;
    }

    
    if ((res = inflate())) {
	    switch (res) {
	    case 0:
		    break;
	    case 1:
		    error("invalid compressed format (err=1)");
		    break;
	    case 2:
		    error("invalid compressed format (err=2)");
		    break;
	    case 3:
		    error("out of memory");
		    break;
	    case 4:
		    error("out of input data");
		    break;
	    default:
		    error("invalid compressed format (other)");
	    }
	    return -1;
    }
	    
    
    orig_crc = (ulg) NEXTBYTE();
    orig_crc |= (ulg) NEXTBYTE() << 8;
    orig_crc |= (ulg) NEXTBYTE() << 16;
    orig_crc |= (ulg) NEXTBYTE() << 24;
    
    orig_len = (ulg) NEXTBYTE();
    orig_len |= (ulg) NEXTBYTE() << 8;
    orig_len |= (ulg) NEXTBYTE() << 16;
    orig_len |= (ulg) NEXTBYTE() << 24;
    
    
    if (orig_crc != CRC_VALUE) {
	    error("crc error");
	    return -1;
    }
    if (orig_len != bytes_out) {
	    error("length error");
	    return -1;
    }
    return 0;

 underrun:			
    error("out of input data");
    return -1;
}


