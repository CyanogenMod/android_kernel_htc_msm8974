
#include <math-emu/soft-fp.h>

#undef count_leading_zeros
#define count_leading_zeros  __FP_CLZ

void
_fp_udivmodti4(_FP_W_TYPE q[2], _FP_W_TYPE r[2],
	       _FP_W_TYPE n1, _FP_W_TYPE n0,
	       _FP_W_TYPE d1, _FP_W_TYPE d0)
{
  _FP_W_TYPE q0, q1, r0, r1;
  _FP_I_TYPE b, bm;

  if (d1 == 0)
    {
#if !UDIV_NEEDS_NORMALIZATION
      if (d0 > n1)
	{
	  

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  
	}
      else
	{
	  

	  if (d0 == 0)
	    d0 = 1 / d0;	

	  udiv_qrnnd (q1, n1, 0, n1, d0);
	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  
	}

      r0 = n0;
      r1 = 0;

#else 

      if (d0 > n1)
	{
	  

	  count_leading_zeros (bm, d0);

	  if (bm != 0)
	    {

	      d0 = d0 << bm;
	      n1 = (n1 << bm) | (n0 >> (_FP_W_TYPE_SIZE - bm));
	      n0 = n0 << bm;
	    }

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  
	}
      else
	{
	  

	  if (d0 == 0)
	    d0 = 1 / d0;	

	  count_leading_zeros (bm, d0);

	  if (bm == 0)
	    {

	      n1 -= d0;
	      q1 = 1;
	    }
	  else
	    {
	      _FP_W_TYPE n2;

	      

	      b = _FP_W_TYPE_SIZE - bm;

	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q1, n1, n2, n1, d0);
	    }

	  

	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  
	}

      r0 = n0 >> bm;
      r1 = 0;
#endif 
    }
  else
    {
      if (d1 > n1)
	{
	  

	  q0 = 0;
	  q1 = 0;

	  
	  r0 = n0;
	  r1 = n1;
	}
      else
	{
	  

	  count_leading_zeros (bm, d1);
	  if (bm == 0)
	    {

	      if (n1 > d1 || n0 >= d0)
		{
		  q0 = 1;
		  sub_ddmmss (n1, n0, n1, n0, d1, d0);
		}
	      else
		q0 = 0;

	      q1 = 0;

	      r0 = n0;
	      r1 = n1;
	    }
	  else
	    {
	      _FP_W_TYPE m1, m0, n2;

	      

	      b = _FP_W_TYPE_SIZE - bm;

	      d1 = (d1 << bm) | (d0 >> b);
	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q0, n1, n2, n1, d1);
	      umul_ppmm (m1, m0, q0, d0);

	      if (m1 > n1 || (m1 == n1 && m0 > n0))
		{
		  q0--;
		  sub_ddmmss (m1, m0, m1, m0, d1, d0);
		}

	      q1 = 0;

	      
	      sub_ddmmss (n1, n0, n1, n0, m1, m0);
	      r0 = (n1 << b) | (n0 >> bm);
	      r1 = n1 >> bm;
	    }
	}
    }

  q[0] = q0; q[1] = q1;
  r[0] = r0, r[1] = r1;
}
