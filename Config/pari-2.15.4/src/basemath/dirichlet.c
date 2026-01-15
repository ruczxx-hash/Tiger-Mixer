/* Copyright (C) 2015  The PARI group.

This file is part of the PARI/GP package.

PARI/GP is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version. It is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY WHATSOEVER.

Check the License for details. You should have received a copy of it, along
with the package; see the file 'COPYING'. If not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

/********************************************************************/
/**                                                                **/
/**           Dirichlet series through Euler product               **/
/**                                                                **/
/********************************************************************/
#include "pari.h"
#include "paripriv.h"

static void
err_direuler(GEN x)
{ pari_err_DOMAIN("direuler","constant term","!=", gen_1,x); }

/* s = t_POL (tolerate t_SER of valuation 0) of constant term = 1
 * d = minimal such that p^d > X
 * V indexed by 1..X will contain the a_n
 * v[1..n] contains the indices nj such that V[nj] != 0 */
static long
dirmuleuler_small(GEN V, GEN v, long n, ulong p, GEN s, long d)
{
  long i, j, m = n, D = minss(d+2, lg(s));
  ulong q = 1, X = lg(V)-1;

  for (i = 3, q = p; i < D; i++, q *= p) /* q*p does not overflow */
  {
    GEN aq = gel(s,i);
    if (gequal0(aq)) continue;
    /* j = 1 */
    gel(V,q) = aq;
    v[++n] = q;
    for (j = 2; j <= m; j++)
    {
      ulong nj = umuluu_le(uel(v,j), q, X);
      if (!nj) continue;
      gel(V,nj) = gmul(aq, gel(V,v[j]));
      v[++n] = nj;
    }
  }
  return n;
}

/* ap != 0 for efficiency, p > sqrt(X) */
static void
dirmuleuler_large(GEN V, ulong p, GEN ap)
{
  long j, jp, X = lg(V)-1;
  gel(V,p) = ap;
  for (j = 2, jp = 2*p; jp <= X; j++, jp += p) gel(V,jp) = gmul(ap, gel(V,j));
}

static ulong
direulertou(GEN a, GEN fl(GEN))
{
  if (typ(a) != t_INT)
  {
    a = fl(a);
    if (typ(a) != t_INT) pari_err_TYPE("direuler", a);
  }
  return signe(a)<=0 ? 0: itou(a);
}

static GEN
direuler_Sbad(GEN V, GEN v, GEN Sbad, ulong *n)
{
  long i, l = lg(Sbad);
  ulong X = lg(V)-1;
  GEN pbad = gen_1;
  for (i = 1; i < l; i++)
  {
    GEN ai = gel(Sbad,i);
    ulong q;
    if (typ(ai) != t_VEC || lg(ai) != 3)
      pari_err_TYPE("direuler [bad primes]",ai);
    q = gtou(gel(ai,1));
    if (q <= X)
    {
      long d = ulogint(X, q) + 1;
      GEN s = direuler_factor(gel(ai,2), d);
      *n = dirmuleuler_small(V, v, *n, q, s, d);
      pbad = muliu(pbad, q);
    }
  }
  return pbad;
}

GEN
direuler_bad(void *E, GEN (*eval)(void *,GEN,long), GEN a,GEN b,GEN c, GEN Sbad)
{
  ulong au, bu, X, sqrtX, n, p;
  pari_sp av0 = avma;
  GEN gp, v, V;
  forprime_t T;
  au = direulertou(a, gceil);
  bu = direulertou(b, gfloor);
  X = c ? direulertou(c, gfloor): bu;
  if (X == 0) return cgetg(1,t_VEC);
  if (bu > X) bu = X;
  if (!u_forprime_init(&T, au, bu)) { set_avma(av0); return mkvec(gen_1); }
  v = vecsmall_ei(X, 1);
  V = vec_ei(X, 1);
  n = 1;
  if (Sbad) Sbad = direuler_Sbad(V, v, Sbad, &n);
  p = 1; gp = cgetipos(3); sqrtX = usqrt(X);
  while (p <= sqrtX && (p = u_forprime_next(&T)))
    if (!Sbad || umodiu(Sbad, p))
    {
      long d = ulogint(X, p) + 1; /* minimal d such that p^d > X */
      GEN s;
      gp[2] = p; s = eval(E, gp, d);
      n = dirmuleuler_small(V, v, n, p, s, d);
    }
  while ((p = u_forprime_next(&T))) /* sqrt(X) < p <= X */
    if (!Sbad || umodiu(Sbad, p))
    {
      GEN s;
      gp[2] = p; s = eval(E, gp, 2); /* s either t_POL or t_SER of val 0 */
      if (lg(s) > 3 && !gequal0(gel(s,3)))
        dirmuleuler_large(V, p, gel(s,3));
    }
  return gerepilecopy(av0,V);
}

/* return a t_SER or a truncated t_POL to precision n */
GEN
direuler_factor(GEN s, long n)
{
  long t = typ(s);
  if (is_scalar_t(t))
  {
    if (!gequal1(s)) err_direuler(s);
    return scalarpol_shallow(s,0);
  }
  switch(t)
  {
    case t_POL: break; /* no need to RgXn_red */
    case t_RFRAC:
    {
      GEN p = gel(s,1), q = gel(s,2);
      q = RgXn_red_shallow(q,n);
      s = RgXn_inv(q, n);
      if (typ(p) == t_POL && varn(p) == varn(q))
      {
        p = RgXn_red_shallow(p, n);
        s = RgXn_mul(s, p, n);
      }
      else
        if (!gequal1(p)) s = RgX_Rg_mul(s, p);
      if (!signe(s) || !gequal1(gel(s,2))) err_direuler(s);
      break;
    }
    case t_SER:
      if (!signe(s) || valp(s) || !gequal1(gel(s,2))) err_direuler(s);
      break;
    default: pari_err_TYPE("direuler", s);
  }
  return s;
}

struct eval_bad
{
  void *E;
  GEN (*eval)(void *, GEN);
};
static GEN
eval_bad(void *E, GEN p, long n)
{
  struct eval_bad *d = (struct eval_bad*) E;
  return direuler_factor(d->eval(d->E, p), n);
}
GEN
direuler(void *E, GEN (*eval)(void *, GEN), GEN a, GEN b, GEN c)
{
  struct eval_bad d;
  d.E= E; d.eval = eval;
  return direuler_bad((void*)&d, eval_bad, a, b, c, NULL);
}

static GEN
primelist(forprime_t *T, GEN Sbad, long n, long *running)
{
  GEN P = cgetg(n+1, t_VECSMALL);
  long i, j;
  for (i = 1, j = 1; i <= n; i++)
  {
    ulong p = u_forprime_next(T);
    if (!p) { *running = 0; break; }
    if (Sbad && umodiu(Sbad, p)==0) continue;
    uel(P,j++) = p;
  }
  setlg(P, j);
  return P;
}

GEN
pardireuler(GEN worker, GEN a, GEN b, GEN c, GEN Sbad)
{
  ulong au, bu, X, sqrtX, n, snX, nX;
  pari_sp av0 = avma;
  GEN v, V;
  forprime_t T;
  struct pari_mt pt;
  long running = 1, pending = 0;
  au = direulertou(a, gceil);
  bu = direulertou(b, gfloor);
  X = c ? direulertou(c, gfloor): bu;
  if (X == 0) return cgetg(1,t_VEC);
  if (bu > X) bu = X;
  if (!u_forprime_init(&T, au, bu)) { set_avma(av0); return mkvec(gen_1); }
  v = vecsmall_ei(X, 1);
  V = vec_ei(X, 1);
  n = 1;
  if (Sbad) Sbad = direuler_Sbad(V, v, Sbad, &n);
  sqrtX = usqrt(X); snX = uprimepi(sqrtX); nX = uprimepi(X);
  if (snX)
  {
    GEN P = primelist(&T, Sbad, snX, &running);
    GEN R = gel(closure_callgenvec(worker, mkvec2(P, utoi(X))), 2);
    long i, l = lg(P);
    for (i = 1; i < l; i++)
    {
      GEN s = gel(R,i);
      n = dirmuleuler_small(V, v, n, uel(P,i), s, lg(s));
    }
  } else snX = 1;
  mt_queue_start_lim(&pt, worker, (nX+snX-1)/snX);
  while (running || pending)
  {
    GEN done;
    GEN P = running? primelist(&T, Sbad, snX, &running): NULL;
    mt_queue_submit(&pt, 0, P ? mkvec2(P, utoi(X)): NULL);
    done = mt_queue_get(&pt, NULL, &pending);
    if (done)
    {
      GEN P = gel(done,1), R = gel(done,2);
      long j, l = lg(P);
      for (j=1; j<l; j++)
      {
        GEN F = gel(R,j);
        if (degpol(F) && !gequal0(gel(F,3)))
          dirmuleuler_large(V, uel(P,j), gel(F,3));
      }
    }
  }
  mt_queue_end(&pt);
  return gerepilecopy(av0,V);
}

/********************************************************************/
/**                                                                **/
/**                 DIRPOWERS and DIRPOWERSSUM                     **/
/**                                                                **/
/********************************************************************/

/* [1^B,...,N^B] */
GEN
vecpowuu(long N, ulong B)
{
  GEN v;
  long p, i;
  forprime_t T;

  if (B <= 8000)
  {
    if (!B) return const_vec(N,gen_1);
    v = cgetg(N+1, t_VEC); if (N == 0) return v;
    gel(v,1) = gen_1;
    if (B == 1)
      for (i = 2; i <= N; i++) gel(v,i) = utoipos(i);
    else if (B == 2)
    {
      ulong o, s;
      if (N & HIGHMASK)
        for (i = 2, o = 3; i <= N; i++, o += 2)
          gel(v,i) = addiu(gel(v,i-1), o);
      else
        for (i = 2, s = 1, o = 3; i <= N; i++, s += o, o += 2)
          gel(v,i) = utoipos(s + o);
    }
    else if (B == 3)
      for (i = 2; i <= N; i++) gel(v,i) = powuu(i, B);
    else
    {
      long k, Bk, e = expu(N);
      for (i = 3; i <= N; i += 2) gel(v,i) = powuu(i, B);
      for (k = 1; k <= e; k++)
      {
        N >>= 1; Bk = B * k;
        for (i = 1; i <= N; i += 2) gel(v, i << k) = shifti(gel(v, i), Bk);
      }
    }
    return v;
  }
  v = const_vec(N, NULL);
  u_forprime_init(&T, 3, N);
  while ((p = u_forprime_next(&T)))
  {
    long m, pk, oldpk;
    gel(v,p) = powuu(p, B);
    for (pk = p, oldpk = p; pk; oldpk = pk, pk = umuluu_le(pk,p,N))
    {
      if (pk != p) gel(v,pk) = mulii(gel(v,oldpk), gel(v,p));
      for (m = N/pk; m > 1; m--)
        if (gel(v,m) && m%p) gel(v, m*pk) = mulii(gel(v,m), gel(v,pk));
    }
  }
  gel(v,1) = gen_1;
  for (i = 2; i <= N; i+=2)
  {
    long vi = vals(i);
    gel(v,i) = shifti(gel(v,i >> vi), B * vi);
  }
  return v;
}

/* does n^s require log(x) ? */
static long
get_needlog(GEN s)
{
  switch(typ(s))
  {
    case t_REAL: return 2; /* yes but not powcx */
    case t_COMPLEX: return 1; /* yes using powcx */
    default: return 0; /* no */
  }
}
/* [1^B,...,N^B] */
GEN
vecpowug(long N, GEN B, long prec)
{
  GEN v, logp = NULL;
  long gp[] = {evaltyp(t_INT)|_evallg(3), evalsigne(1)|evallgefint(3),0};
  long p, precp = 2, prec0, prec1, needlog;
  forprime_t T;
  if (N == 1) return mkvec(gen_1);
  if (typ(B) == t_INT && lgefint(B) <= 3 && signe(B) >= 0)
    return vecpowuu(N, itou(B));
  needlog = get_needlog(B);
  prec1 = prec0 = prec;
  if (needlog == 1) prec1 = powcx_prec(log2((double)N), B, prec);
  u_forprime_init(&T, 2, N);
  v = const_vec(N, NULL);
  gel(v,1) = gen_1;
  while ((p = u_forprime_next(&T)))
  {
    long m, pk, oldpk;
    GEN u;
    gp[2] = p;
    if (needlog)
    {
      if (!logp)
        logp = logr_abs(utor(p, prec1));
      else
      { /* Assuming p and precp are odd,
         * log p = log(precp) + 2 atanh((p - precp) / (p + precp)) */
        ulong a = p >> 1, b = precp >> 1; /* p = 2a + 1, precp = 2b + 1 */
        GEN z = atanhuu(a - b, a + b + 1, prec1); /* avoid overflow */
        shiftr_inplace(z, 1); logp = addrr(logp, z);
      }
      u = needlog == 1? powcx(gp, logp, B, prec0)
                      : mpexp(gmul(B, logp));
      if (p == 2) logp = NULL; /* reset: precp must be odd */
    }
    else
      u = gpow(gp, B, prec0);
    precp = p;
    gel(v,p) = u; /* p^B */
    if (prec0 != prec) gel(v,p) = gprec_wtrunc(gel(v,p), prec);
    for (pk = p, oldpk = p; pk; oldpk = pk, pk = umuluu_le(pk,p,N))
    {
      if (pk != p) gel(v,pk) = gmul(gel(v,oldpk), gel(v,p));
      for (m = N/pk; m > 1; m--)
        if (gel(v,m) && m%p) gel(v, m*pk) = gmul(gel(v,m), gel(v,pk));
    }
  }
  return v;
}

GEN
dirpowers(long n, GEN x, long prec)
{
  pari_sp av;
  GEN v;
  if (n <= 0) return cgetg(1, t_VEC);
  av = avma; v = vecpowug(n, x, prec);
  if (typ(x) == t_INT && lgefint(x) <= 3 && signe(x) >= 0 && cmpiu(x, 2) <= 0)
    return v;
  return gerepilecopy(av, v);
}

/* P = prime divisors of (squarefree) n, V[i] = i^s for i <= sq.
 * Return NULL if n is not sq-smooth, else f(n)n^s */
static GEN
smallfact(ulong n, GEN P, ulong sq, GEN V)
{
  long i, l;
  ulong p, m, o;
  GEN c;
  if (n <= sq) return gel(V,n);
  l = lg(P); m = p = uel(P, l-1); if (p > sq) return NULL;
  for (i = l-2; i > 1; i--, m = o) { p = uel(P,i); o = m*p; if (o > sq) break; }
  c = gel(V,m); n /= m; /* m <= sq, o = m * p > sq */
  if (n > sq) { c = gmul(c, gel(V,p)); n /= p; }
  return gmul(c, gel(V,n));
}
static GEN
Qtor(GEN x, long prec)
{ return typ(x) == t_FRAC? fractor(x, prec): x; }
/* sum_{n <= N} f(n)n^s. */
GEN
dirpowerssumfun(ulong N, GEN s, void *E, GEN (*f)(void *, ulong, long),
                long prec)
{
  const ulong step = 2048;
  pari_sp av = avma, av2;
  GEN P, V, W, Q, c2, Q2, Q3, Q6, S, Z, logp;
  forprime_t T;
  long gp[] = {evaltyp(t_INT)|_evallg(3), evalsigne(1)|evallgefint(3),0};
  ulong a, b, c, e, q, x1, n, sq, p, precp;
  long prec0, prec1, needlog;

  if (!N) return gen_0;
  if (f)
  {
    if (N < 49)
    {
      V = vecpowug(N, s, prec);
      S = gen_1;
      for (n = 2; n <= N; n++)
        S = gadd(S, gmul(gel(V,n), f(E, n, prec)));
      return gerepileupto(av, Qtor(S, prec));
    }
  }
  else if (N < 1000UL)
    return gerepileupto(av, Qtor(RgV_sum(vecpowug(N, s, prec)), prec));
  sq = usqrt(N);
  V = cgetg(sq+1, t_VEC);
  W = cgetg(sq+1, t_VEC);
  Q = cgetg(sq+1, t_VEC);
  prec1 = prec0 = prec + EXTRAPRECWORD;
  s = gprec_w(s, prec0);
  needlog = get_needlog(s);
  if (needlog == 1) prec1 = powcx_prec(log2((double)N), s, prec);
  gel(V,1) = gel(W,1) = gel(Q,1) = gen_1;
  c2 = gpow(gen_2, s, prec0);
  if (f) c2 = gmul(c2, f(E, 2, prec));
  gel(V,2) = c2; /* f(2) 2^s */
  gel(W,2) = Qtor(gaddgs(c2, 1), prec0);
  gel(Q,2) = Qtor(gaddgs(gsqr(c2), 1), prec0);
  logp = NULL;
  for (n = 3; n <= sq; n++)
  {
    GEN u;
    if (odd(n))
    {
      gp[2] = n;
      if (needlog)
      {
        if (!logp)
          logp = logr_abs(utor(n, prec1));
        else
        { /* log n = log(n-2) + 2 atanh(1 / (n - 1)) */
          GEN z = atanhuu(1, n - 1, prec1);
          shiftr_inplace(z, 1); logp = addrr(logp, z);
        }
        u = needlog == 1? powcx(gp, logp, s, prec0)
                        : mpexp(gmul(s, logp));

      }
      else
        u = gpow(gp, s, prec0);
      if (f) u = gmul(u, f(E, n, prec0)); /* f(n) n^s */
    }
    else
      u = gmul(c2, gel(V, n>> 1));
    gel(V,n) = u; /* f(n) n^s */
    gel(W,n) = gadd(gel(W,n-1), gel(V,n));       /* = sum_{i<=n} f(i)i^s */
    gel(Q,n) = gadd(gel(Q,n-1), gsqr(gel(V,n))); /* = sum_{i<=n} f(i^2)i^2s */
  }
  Q2 = RgV_Rg_mul(Q, gel(V,2));
  Q3 = RgV_Rg_mul(Q, gel(V,3));
  Q6 = RgV_Rg_mul(Q, gel(V,6));
  precp = 0; logp = NULL; S = gen_0;
  u_forprime_init(&T, sq + 1, N);
  av2 = avma;
  while ((p = u_forprime_next(&T)))
  {
    GEN u;
    gp[2] = p;
    if (needlog)
    {
      if (!logp)
        logp = logr_abs(utor(p, prec1));
      else
      { /* log p = log(precp) + 2 atanh((p - precp) / (p + precp)) */
        ulong a = p >> 1, b = precp >> 1; /* p = 2a + 1, precp = 2b + 1 */
        GEN z = atanhuu(a - b, a + b + 1, prec1); /* avoid overflow */
        shiftr_inplace(z, 1); logp = addrr(logp, z);
      }
      u = needlog == 1? powcx(gp, logp, s, prec0)
                      : mpexp(gmul(s, logp));
    }
    else
      u = gpow(gp, s, prec0);
    if (f) u = gmul(u, f(E, p, prec0)); /* f(p) p^s */
    S = gadd(S, gmul(gel(W, N / p), u));
    precp = p;
    if ((p & 0x1ff) == 1)
    {
      if (!logp) S = gerepileupto(av2,S);
      else gerepileall(av2, 2, &S, &logp);
    }
  }
  P = mkvecsmall2(2, 3);
  Z = cgetg(sq+1, t_VEC);
  /* a,b,c,e = sqrt(q), sqrt(q/2), sqrt(q/3), sqrt(q/6)
   * Z[q] = Q[a] + 2^s Q[b] + 3^s Q[c] + 6^s Q[e], with Q[0] = 0 */
  gel(Z, 1) = gen_1;
  gel(Z, 2) = gel(W, 2);
  gel(Z, 3) = gel(W, 3);
  gel(Z, 4) = gel(Z, 5) = gel(W,4);
  gel(Z, 6) = gel(Z, 7) = gadd(gel(W,4), gel(V,6));
  a = 2; b = c = e = 1;
  for (q = 8; q <= sq; q++)
  { /* Gray code: at most one of a,b,c,d differs (by 1) from previous value */
    GEN z = gel(Z, q - 1);
    ulong na, nb, nc, ne;
    if ((na = usqrt(q)) != a)
    { a = na; z = gadd(z, gel(V, na * na)); }
    else if ((nb = usqrt(q / 2)) != b)
    { b = nb; z = gadd(z, gel(V, 2 * nb * nb)); }
    else if ((nc = usqrt(q / 3)) != c)
    { c = nc; z = gadd(z, gel(V, 3 * nc * nc)); }
    else if ((ne = usqrt(q / 6)) != e)
    { e = ne; z = gadd(z, gel(V, 6 * ne * ne)); }
    gel(Z,q) = z;
  }
  av2 = avma;
  for(x1 = 1;; x1 += step)
  { /* beware overflow, fuse last two bins (avoid a tiny remainder) */
    ulong j, lv, x2 = (N >= 2*step && N - 2*step >= x1)? x1-1 + step: N;
    GEN v = vecfactorsquarefreeu_coprime(x1, x2, P);
    lv = lg(v);
    for (j = 1; j < lv; j++) if (gel(v,j))
    {
      ulong d = x1-1 + j; /* squarefree, coprime to 6 */
      GEN t = smallfact(d, gel(v,j), sq, V), u; /* = d^s */
      if (!t) continue;
      /* S += d^s * Z[q] */
      q = N / d;
      if (q == 1) { S = gadd(S, t); continue; }
      if (q <= sq) u = gel(Z, q);
      else
      {
        a = usqrt(q); b = usqrt(q / 2); c = usqrt(q / 3); e = usqrt(q / 6);
        u = gadd(gadd(gel(Q,a), gel(Q2,b)), gadd(gel(Q3,c), gel(Q6,e)));
      }
      S = gadd(S, gmul(t, u));
    }
    if (x2 == N) break;
    S = gerepileupto(av2, S);
  }
  return gerepileupto(av, S);
}
GEN
dirpowerssum(ulong N, GEN s, long prec)
{ return dirpowerssumfun(N, s, NULL, NULL, prec); }
static GEN
gp_callUp(void *E, ulong x, long prec)
{
  long court[] = {evaltyp(t_INT)|_evallg(3), evalsigne(1)|evallgefint(3),0};
  court[2] = x; return gp_callprec(E, court, prec);
}
GEN
dirpowerssum0(GEN N, GEN s, GEN f, long prec)
{
  if (typ(N) != t_INT) pari_err_TYPE("dirpowerssum", N);
  if (signe(N) <= 0) return gen_0;
  if (!f) return dirpowerssum(itou(N), s, prec);
  if (typ(f) != t_CLOSURE) pari_err_TYPE("dirpowerssum", f);
  return dirpowerssumfun(itou(N), s, (void*)f, gp_callUp, prec);
}
