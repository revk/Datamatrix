// IEC16022 bar code generation library
// This software is provided under the terms of the GPL v2 or later.
// This software is provided free of charge with a full "Money back" guarantee.
// Use entirely at your own risk. We accept no liability. If you don't like that - don't use it.

// Adrian Kennard, Andrews & Arnold Ltd
// with help from Cliff Hones on the RS coding

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <popt.h>
#include <stdlib.h>
#include <err.h>
#include "reedsol.h"
#include "iec16022ecc200.h"

static struct ecc200matrix_s {
   int H,
    W;
   int FH,
    FW;
   int bytes;
   int datablock,
    rsblock;
} ecc200matrix[] = {
   {
    10, 10, 10, 10, 3, 3, 5 },  //
   {
    12, 12, 12, 12, 5, 5, 7 },  //
   {
    8, 18, 8, 18, 5, 5, 7 },    //
   {
    14, 14, 14, 14, 8, 8, 10 }, //
   {
    8, 32, 8, 16, 10, 10, 11 }, //
   {
    16, 16, 16, 16, 12, 12, 12 },       //
   {
    12, 26, 12, 26, 16, 16, 14 },       //
   {
    18, 18, 18, 18, 18, 18, 14 },       //
   {
    20, 20, 20, 20, 22, 22, 18 },       //
   {
    12, 36, 12, 18, 22, 22, 18 },       //
   {
    22, 22, 22, 22, 30, 30, 20 },       //
   {
    16, 36, 16, 18, 32, 32, 24 },       //
   {
    24, 24, 24, 24, 36, 36, 24 },       //
   {
    26, 26, 26, 26, 44, 44, 28 },       //
   {
    16, 48, 16, 24, 49, 49, 28 },       //
   {
    32, 32, 16, 16, 62, 62, 36 },       //
   {
    36, 36, 18, 18, 86, 86, 42 },       //
   {
    40, 40, 20, 20, 114, 114, 48 },     //
   {
    44, 44, 22, 22, 144, 144, 56 },     //
   {
    48, 48, 24, 24, 174, 174, 68 },     //
   {
    52, 52, 26, 26, 204, 102, 42 },     //
   {
    64, 64, 16, 16, 280, 140, 56 },     //
   {
    72, 72, 18, 18, 368, 92, 36 },      //
   {
    80, 80, 20, 20, 456, 114, 48 },     //
   {
    88, 88, 22, 22, 576, 144, 56 },     //
   {
    96, 96, 24, 24, 696, 174, 68 },     //
   {
    104, 104, 26, 26, 816, 136, 56 },   //
   {
    120, 120, 20, 20, 1050, 175, 68 },  //
   {
    132, 132, 22, 22, 1304, 163, 62 },  //
   {
    144, 144, 24, 24, 1558, 156, 62 },  // 156*4+155*2
   {
    0 }                         // terminate
};

 // simple checked response malloc
static void *safemalloc(int n)
{
   void *p = malloc(n);
   if (!p)
      err(1, "Malloc(%d) failed\n", n);
   return p;
}

// Annex M placement alorithm low level
static void ecc200placementbit(int *array, int NR, int NC, int r, int c, int p, char b)
{
   if (r < 0)
   {
      r += NR;
      c += 4 - ((NR + 4) % 8);
   }
   if (c < 0)
   {
      c += NC;
      r += 4 - ((NC + 4) % 8);
   }
   array[r * NC + c] = (p << 3) + b;
}

static void ecc200placementblock(int *array, int NR, int NC, int r, int c, int p)
{
   ecc200placementbit(array, NR, NC, r - 2, c - 2, p, 7);
   ecc200placementbit(array, NR, NC, r - 2, c - 1, p, 6);
   ecc200placementbit(array, NR, NC, r - 1, c - 2, p, 5);
   ecc200placementbit(array, NR, NC, r - 1, c - 1, p, 4);
   ecc200placementbit(array, NR, NC, r - 1, c - 0, p, 3);
   ecc200placementbit(array, NR, NC, r - 0, c - 2, p, 2);
   ecc200placementbit(array, NR, NC, r - 0, c - 1, p, 1);
   ecc200placementbit(array, NR, NC, r - 0, c - 0, p, 0);
}

static void ecc200placementcornerA(int *array, int NR, int NC, int p)
{
   ecc200placementbit(array, NR, NC, NR - 1, 0, p, 7);
   ecc200placementbit(array, NR, NC, NR - 1, 1, p, 6);
   ecc200placementbit(array, NR, NC, NR - 1, 2, p, 5);
   ecc200placementbit(array, NR, NC, 0, NC - 2, p, 4);
   ecc200placementbit(array, NR, NC, 0, NC - 1, p, 3);
   ecc200placementbit(array, NR, NC, 1, NC - 1, p, 2);
   ecc200placementbit(array, NR, NC, 2, NC - 1, p, 1);
   ecc200placementbit(array, NR, NC, 3, NC - 1, p, 0);
}

static void ecc200placementcornerB(int *array, int NR, int NC, int p)
{
   ecc200placementbit(array, NR, NC, NR - 3, 0, p, 7);
   ecc200placementbit(array, NR, NC, NR - 2, 0, p, 6);
   ecc200placementbit(array, NR, NC, NR - 1, 0, p, 5);
   ecc200placementbit(array, NR, NC, 0, NC - 4, p, 4);
   ecc200placementbit(array, NR, NC, 0, NC - 3, p, 3);
   ecc200placementbit(array, NR, NC, 0, NC - 2, p, 2);
   ecc200placementbit(array, NR, NC, 0, NC - 1, p, 1);
   ecc200placementbit(array, NR, NC, 1, NC - 1, p, 0);
}

static void ecc200placementcornerC(int *array, int NR, int NC, int p)
{
   ecc200placementbit(array, NR, NC, NR - 3, 0, p, 7);
   ecc200placementbit(array, NR, NC, NR - 2, 0, p, 6);
   ecc200placementbit(array, NR, NC, NR - 1, 0, p, 5);
   ecc200placementbit(array, NR, NC, 0, NC - 2, p, 4);
   ecc200placementbit(array, NR, NC, 0, NC - 1, p, 3);
   ecc200placementbit(array, NR, NC, 1, NC - 1, p, 2);
   ecc200placementbit(array, NR, NC, 2, NC - 1, p, 1);
   ecc200placementbit(array, NR, NC, 3, NC - 1, p, 0);
}

static void ecc200placementcornerD(int *array, int NR, int NC, int p)
{
   ecc200placementbit(array, NR, NC, NR - 1, 0, p, 7);
   ecc200placementbit(array, NR, NC, NR - 1, NC - 1, p, 6);
   ecc200placementbit(array, NR, NC, 0, NC - 3, p, 5);
   ecc200placementbit(array, NR, NC, 0, NC - 2, p, 4);
   ecc200placementbit(array, NR, NC, 0, NC - 1, p, 3);
   ecc200placementbit(array, NR, NC, 1, NC - 3, p, 2);
   ecc200placementbit(array, NR, NC, 1, NC - 2, p, 1);
   ecc200placementbit(array, NR, NC, 1, NC - 1, p, 0);
}

// Annex M placement alorithm main function
static void ecc200placement(int *array, int NR, int NC)
{
   int r,
    c,
    p;
   // invalidate
   for (r = 0; r < NR; r++)
      for (c = 0; c < NC; c++)
         array[r * NC + c] = 0;
   // start
   p = 1;
   r = 4;
   c = 0;
   do
   {
      // check corner
      if (r == NR && !c)
         ecc200placementcornerA(array, NR, NC, p++);
      if (r == NR - 2 && !c && NC % 4)
         ecc200placementcornerB(array, NR, NC, p++);
      if (r == NR - 2 && !c && (NC % 8) == 4)
         ecc200placementcornerC(array, NR, NC, p++);
      if (r == NR + 4 && c == 2 && !(NC % 8))
         ecc200placementcornerD(array, NR, NC, p++);
      // up/right
      do
      {
         if (r < NR && c >= 0 && !array[r * NC + c])
            ecc200placementblock(array, NR, NC, r, c, p++);
         r -= 2;
         c += 2;
      } while (r >= 0 && c < NC);
      r++;
      c += 3;
      // down/left
      do
      {
         if (r >= 0 && c < NC && !array[r * NC + c])
            ecc200placementblock(array, NR, NC, r, c, p++);
         r += 2;
         c -= 2;
      } while (r < NR && c >= 0);
      r += 3;
      c++;
   } while (r < NR || c < NC);
   // unfilled corner
   if (!array[NR * NC - 1])
      array[NR * NC - 1] = array[NR * NC - NC - 2] = 1;
}

// calculate and append ecc code, and if necessary interleave
static void ecc200(unsigned char *binary, int bytes, int datablock, int rsblock)
{
   int blocks = (bytes + 2) / datablock,
       b;
   rs_init(0x12d, rsblock, 1);
   for (b = 0; b < blocks; b++)
   {
      unsigned char buf[256],
       ecc[256];
      int n,
       p = 0;
      for (n = b; n < bytes; n += blocks)
         buf[p++] = binary[n];
      rs_encode(p, buf, ecc);
      p = 0;
      for (n = b; n < rsblock * blocks; n += blocks)
         binary[bytes + n] = ecc[p++];
   }
}

// perform encoding for ecc200, source s len sl, to target t len tl, using optional encoding control string e
// return 1 if OK, 0 if failed. Does all necessary padding to tl
char ecc200encode(unsigned char *t, int tl, unsigned char *s, int sl, char *encoding, unsigned int *lenp)
{
   char enc = 'a';              // start in ASCII encoding mode
   int tp = 0,
       sp = 0;
   if (strlen(encoding) < sl)
   {
      fprintf(stderr, "Encoding string too short\n");
      return 0;
   }
   // do the encoding
   while (sp < sl && tp < tl)
   {
      char newenc = enc;        // suggest new encoding
      newenc = tolower(encoding[sp]);
      switch (newenc)
      {                         // encode character
      case 'c':                // C40
      case 't':                // Text
      case 'x':                // X12
         {
            unsigned int out[6];
            int p = 0;
            const char *e = 0,
                *s2 = "!\"#$%&'()*+,-./:;<=>?@[\\]^_",
                *s3 = 0;
            if (newenc == 'c')
            {
               e = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
               s3 = "`abcdefghijklmnopqrstuvwxyz{|}~\177";
            }
            if (newenc == 't')
            {
               e = " 0123456789abcdefghijklmnopqrstuvwxyz";
               s3 = "`ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}~\177";
            }
            if (newenc == 'x')
               e = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\r*>";
            do
            {
               unsigned char c = s[sp++];
               char *w;
               if (c & 0x80)
               {
                  if (newenc == 'x')
                  {
                     fprintf(stderr, "Cannot encode char 0x%02X in X12\n", c);
                     return 0;
                  }
                  c &= 0x7f;
                  out[p++] = 1;
                  out[p++] = 30;
               }
               w = strchr(e, c);
               if (w)
                  out[p++] = ((w - e) + 3) % 40;
               else
               {
                  if (newenc == 'x')
                  {
                     fprintf(stderr, "Cannot encode char 0x%02X in X12\n", c);
                     return 0;
                  }
                  if (c < 32)
                  {             // shift 1
                     out[p++] = 0;
                     out[p++] = c;
                  } else
                  {
                     w = strchr(s2, c);
                     if (w)
                     {          // shift 2
                        out[p++] = 1;
                        out[p++] = (w - s2);
                     } else
                     {
                        w = strchr(s3, c);
                        if (w)
                        {
                           out[p++] = 2;
                           out[p++] = (w - s3);
                        } else
                        {
                           fprintf(stderr, "Could not encode 0x%02X, should not happen\n", c);
                           return 0;
                        }
                     }
                  }
               }
               if (p == 2 && tp + 2 == tl && sp == sl)
                  out[p++] = 0; // shift 1 pad at end
               while (p >= 3)
               {
                  unsigned int v = out[0] * 1600 + out[1] * 40 + out[2] + 1;
                  if (enc != newenc)
                  {
                     if (enc == 'c' || enc == 't' || enc == 'x')
                        t[tp++] = 254;  // escape C40/text/X12
                     else if (enc == 'e')
                        t[tp++] = 0x7C; // escape EDIFACT
                     if (newenc == 'c')
                        t[tp++] = 230;
                     if (newenc == 't')
                        t[tp++] = 239;
                     if (newenc == 'x')
                        t[tp++] = 238;
                     enc = newenc;
                  }
                  t[tp++] = (v >> 8);
                  t[tp++] = (v & 0xFF);
                  p -= 3;
                  out[0] = out[3];
                  out[1] = out[4];
                  out[2] = out[5];
               }
            } while (p && sp < sl);
            if (tp + 1 >= tl)
               enc = 'a';
         }
         break;
      case 'e':                // EDIFACT
         {
            unsigned char out[4],
             p = 0;
            if (enc != newenc)
            {
               if (enc == 'c' || enc == 't' || enc == 'x')
                  t[tp++] = 254;        // escape C40/text/X12
               else if (enc == 'a')
                  t[tp++] = 240;        // escape ASCII
               enc = 'e';
            }
            while (sp < sl && tolower(encoding[sp]) == 'e' && p < 4)
               out[p++] = s[sp++];
            if (p < 4)
            {
               out[p++] = 0x1F; // escape to ascii
               enc = 'a';
            }                   // termination
            t[tp] = ((out[0] & 0x3F) << 2);
            if (p == 1)
               tp++;
            else
            {
               t[tp++] |= ((out[1] & 0x30) >> 4);
               t[tp] = ((out[1] & 0x0F) << 4);
               if (p == 2)
                  tp++;
               else
               {
                  t[tp++] |= ((out[2] & 0x3C) >> 2);
                  t[tp] = ((out[2] & 0x03) << 6);
                  if (p == 3)
                     tp++;
                  else
                     t[tp++] |= (out[3] & 0x3F);
               }
            }
            if (tp + 2 >= tl)
               enc = 'a';       // automatically flips if not enough space
         }
         break;
      case 'a':                // ASCII
         if (enc != newenc)
         {
            if (enc == 'c' || enc == 't' || enc == 'x')
               t[tp++] = 254;   // escape C40/text/X12
            else if (enc == 'e')
               t[tp++] = 0x7C;  // escape EDIFACT
            enc = 'a';
         }
         if (sl - sp >= 2 && isdigit(s[sp]) && isdigit(s[sp + 1]))
         {
            t[tp++] = (s[sp] - '0') * 10 + s[sp + 1] - '0' + 130;
            sp += 2;
         } else if (s[sp] > 127)
         {
            t[tp++] = 235;
            t[tp++] = s[sp++] - 127;
         } else
            t[tp++] = s[sp++] + 1;
         break;
      case 'b':                // Binary
         {
            int l = 0;          // how much to encode
            if (encoding)
            {
               int p;
               for (p = sp; p < sl && tolower(encoding[p]) == 'b'; p++)
                  l++;
            }
            t[tp++] = 231;      // base256
            if (l < 250)
               t[tp++] = l;
            else
            {
               t[tp++] = 249 + (l / 250);
               t[tp++] = (l % 250);
            }
            while (l-- && tp < tl)
            {
               t[tp] = s[sp++] + (((tp + 1) * 149) % 255) + 1;  // see annex H
               tp++;
            }
            enc = 'a';          // reverts to ASCII at end
         }
         break;
      default:
         fprintf(stderr, "Unknown encoding %c\n", newenc);
         return 0;              // failed
      }
   }
   if (lenp)
      *lenp = tp;
   if (tp < tl && enc != 'a')
   {
      if (enc == 'c' || enc == 't' || enc == 'x')
         t[tp++] = 254;         // escape X12/C40/Text
      else if (enc == 'e')
         t[tp++] = 0x7C;        // escape EDIFACT
   }
   if (tp < tl)
      t[tp++] = 129;            // pad
   while (tp < tl)
   {                            // more padding
      int v = 129 + (((tp + 1) * 149) % 253) + 1;       // see Annex H
      if (v > 254)
         v -= 254;
      t[tp++] = v;
   }
   if (tp > tl || sp < sl)
      return 0;                 // did not fit
   //for (tp = 0; tp < tl; tp++) fprintf (stderr, "%02X ", t[tp]); fprintf (stderr, "\n");
   return 1;                    // OK 
}

// Auto encoding format functions
static char encchr[] = "ACTXEB";

enum {
   E_ASCII,
   E_C40,
   E_TEXT,
   E_X12,
   E_EDIFACT,
   E_BINARY,
   E_MAX
};

unsigned char switchcost[E_MAX][E_MAX] = {
   { 0, 1, 1, 1, 1, 2 },        // From E_ASCII
   { 1, 0, 2, 2, 2, 3 },        // From E_C40
   { 1, 2, 0, 2, 2, 3 },        // From E_TEXT
   { 1, 2, 2, 0, 2, 3 },        // From _X12
   { 1, 2, 2, 2, 0, 3 },        // From E_EDIFACT
   { 0, 1, 1, 1, 1, 0 },        // From E_BINARY
};

// Creates a encoding list (malloc)
// returns encoding string
// if lenp not null, target len stored
// if error, null returned
// if exact specified, then assumes shortcuts applicable for exact fit in target
// 1. No unlatch to return to ASCII for last encoded byte after C40 or Text or X12
// 2. No unlatch to return to ASCII for last 1 or 2 encoded bytes after EDIFACT
// 3. Final C40 or text encoding exactly in last 2 bytes can have a shift 0 to pad to make a triple
// Only use the encoding from an exact request if the len matches the target, otherwise free the result and try again with exact=0
static char *encmake(int l, unsigned char *s, int *lenp, char exact)
{
   char *encoding = NULL;
   int p = l;
   int e;
   struct {
      short s;                  // number of bytes of source that can be encoded in a row at this point using this encoding mode
      short t;                  // number of bytes of target generated encoding from this point to end if already in this encoding mode
   } enc[MAXBARCODE + 1][E_MAX];
   memset(&enc, 0, sizeof(enc));
   if (lenp)
      *lenp = 0;
   if (!l)
      return strdup("");        // no length
   if (l > MAXBARCODE)
      return NULL;              // not valid
   while (p--)
   {
      int b = 0,
          sub;
      int sl,
       tl,
       bl,
       t;
      // consider each encoding from this point
      // ASCII
      sl = tl = 1;
      if (isdigit(s[p]) && p + 1 < l && isdigit(s[p + 1]))
         sl = 2;                // double digit
      else if (s[p] & 0x80)
         tl = 2;                // high shifted
      bl = 0;
      if (p + sl < l)
         for (e = 0; e < E_MAX; e++)
            if (enc[p + sl][e].t && ((t = enc[p + sl][e].t + switchcost[E_ASCII][e]) < bl || !bl))
            {
               bl = t;
               b = e;
            }
      enc[p][E_ASCII].t = tl + bl;
      enc[p][E_ASCII].s = sl;
      if (bl && b == E_ASCII)
         enc[p][b].s += enc[p + sl][b].s;
      // C40
      sub = tl = sl = 0;
      do
      {
         unsigned char c = s[p + sl++];
         if (c & 0x80)
         {                      // shift + upper
            sub += 2;
            c &= 0x7F;
         }
         if (c != ' ' && !isdigit(c) && !isupper(c))
            sub++;              // shift
         sub++;
         while (sub >= 3)
         {
            sub -= 3;
            tl += 2;
         }
      } while (sub && p + sl < l);
      if (exact && sub == 2 && p + sl == l)
      {                         // special case, can encode last block with shift 0 at end
         sub = 0;
         tl += 2;
      }
      if (!sub)
      {                         // can encode C40
         bl = 0;
         if (p + sl < l)
            for (e = 0; e < E_MAX; e++)
               if (enc[p + sl][e].t && ((t = enc[p + sl][e].t + switchcost[E_C40][e]) < bl || !bl))
               {
                  bl = t;
                  b = e;
               }
         if (exact && enc[p + sl][E_ASCII].t == 1 && 1 <= bl)
         {                      // special case, switch to ASCII for last bytes
            bl = 1;
            b = E_ASCII;
         }
         enc[p][E_C40].t = tl + bl;
         enc[p][E_C40].s = sl;
         if (bl && b == E_C40)
            enc[p][b].s += enc[p + sl][b].s;
      }
      // Text
      sub = tl = sl = 0;
      do
      {
         unsigned char c = s[p + sl++];
         if (c & 0x80)
         {                      // shift + upper
            sub += 2;
            c &= 0x7F;
         }
         if (c != ' ' && !isdigit(c) && !islower(c))
            sub++;              // shift
         sub++;
         while (sub >= 3)
         {
            sub -= 3;
            tl += 2;
         }
      } while (sub && p + sl < l);
      if (exact && sub == 2 && p + sl == l)
      {                         // special case, can encode last block with shift 0 at end
         sub = 0;
         tl += 2;
      }
      if (!sub && sl)
      {                         // can encode Text
         bl = 0;
         if (p + sl < l)
            for (e = 0; e < E_MAX; e++)
               if (enc[p + sl][e].t && ((t = enc[p + sl][e].t + switchcost[E_TEXT][e]) < bl || !bl))
               {
                  bl = t;
                  b = e;
               }
         if (exact && enc[p + sl][E_ASCII].t == 1 && 1 <= bl)
         {                      // special case, switch to ASCII for last bytes
            bl = 1;
            b = E_ASCII;
         }
         enc[p][E_TEXT].t = tl + bl;
         enc[p][E_TEXT].s = sl;
         if (bl && b == E_TEXT)
            enc[p][b].s += enc[p + sl][b].s;
      }
      // X12
      sub = tl = sl = 0;
      do
      {
         unsigned char c = s[p + sl++];
         if (c != 13 && c != '*' && c != '>' && c != ' ' && !isdigit(c) && !isupper(c))
         {
            sl = 0;
            break;
         }
         sub++;
         while (sub >= 3)
         {
            sub -= 3;
            tl += 2;
         }
      } while (sub && p + sl < l);
      if (!sub && sl)
      {                         // can encode X12
         bl = 0;
         if (p + sl < l)
            for (e = 0; e < E_MAX; e++)
               if (enc[p + sl][e].t && ((t = enc[p + sl][e].t + switchcost[E_X12][e]) < bl || !bl))
               {
                  bl = t;
                  b = e;
               }
         if (exact && enc[p + sl][E_ASCII].t == 1 && 1 <= bl)
         {                      // special case, switch to ASCII for last bytes
            bl = 1;
            b = E_ASCII;
         }
         enc[p][E_X12].t = tl + bl;
         enc[p][E_X12].s = sl;
         if (bl && b == E_X12)
            enc[p][b].s += enc[p + sl][b].s;
      }
      // EDIFACT
      sl = bl = 0;
      if (s[p + 0] >= 32 && s[p + 0] <= 94)
      {                         // can encode 1
         if (p + 1 == l)
         {                      // last character
            bl = 1;
            sl = 1;
            if (exact)
               b = E_ASCII;
         } else
            for (e = 0; e < E_MAX; e++)
               if (e != E_EDIFACT && enc[p + 1][e].t && ((t = 2 + enc[p + 1][e].t + switchcost[E_ASCII][e]) < bl || !bl))       // E_ASCII as allowed for unlatch
               {
                  sl = 1;
                  bl = t;
                  b = e;
               }
         if (p + 1 < l && s[p + 1] >= 32 && s[p + 1] <= 94)
         {                      // can encode 2
            if (p + 2 == l)
            {                   // last 2 characters
               bl = 2;
               sl = 2;
               if (exact)
                  b = E_ASCII;
            } else
               for (e = 0; e < E_MAX; e++)
                  if (e != E_EDIFACT && enc[p + 2][e].t && ((t = 3 + enc[p + 2][e].t + switchcost[E_ASCII][e]) < bl || !bl))    // E_ASCII as allowed for unlatch
                  {
                     sl = 2;
                     bl = t;
                     b = e;
                  }
            if (p + 2 < l && s[p + 2] >= 32 && s[p + 2] <= 94)
            {                   // can encode 3
               if (p + 3 == l)
               {                // last 3 characters
                  bl = 3;
                  sl = 3;
               } else
                  for (e = 0; e < E_MAX; e++)
                     if (e != E_EDIFACT && enc[p + 3][e].t && ((t = 3 + enc[p + 3][e].t + switchcost[E_ASCII][e]) < bl || !bl)) // E_ASCII as allowed for unlatch
                     {
                        sl = 3;
                        bl = t;
                        b = e;
                     }
               if (p + 4 < l && s[p + 3] >= 32 && s[p + 3] <= 94)
               {                // can encode 4
                  if (p + 4 == l)
                  {             // last 4 characters
                     bl = 3;
                     sl = 4;
                  } else
                  {
                     for (e = 0; e < E_MAX; e++)
                        if (enc[p + 4][e].t && ((t = 3 + enc[p + 4][e].t + switchcost[E_EDIFACT][e]) < bl || !bl))
                        {
                           sl = 4;
                           bl = t;
                           b = e;
                        }
                     if (exact && enc[p + 4][E_ASCII].t && enc[p + 4][E_ASCII].t <= 2 && (t = 3 + enc[p + 4][E_ASCII].t) <= bl)
                     {          // special case, switch to ASCII for last 1 or two bytes
                        sl = 4;
                        bl = t;
                        b = E_ASCII;
                     }
                  }
               }
            }
         }
         enc[p][E_EDIFACT].t = bl;
         enc[p][E_EDIFACT].s = sl;
         if (bl && b == E_EDIFACT)
            enc[p][b].s += enc[p + sl][b].s;
      }
      // Binary
      bl = 0;
      for (e = 0; e < E_MAX; e++)
         if (enc[p + 1][e].t && ((t = enc[p + 1][e].t + switchcost[E_BINARY][e] + ((e == E_BINARY && enc[p + 1][e].t == 249) ? 1 : 0)) < bl || !bl))
         {
            bl = t;
            b = e;
         }
      enc[p][E_BINARY].t = 1 + bl;
      enc[p][E_BINARY].s = 1;
      if (bl && b == E_BINARY)
         enc[p][b].s += enc[p + 1][b].s;
      //fprintf (stderr, "%d:", p); for (e = 0; e < E_MAX; e++) fprintf (stderr, " %c*%d/%d", encchr[e], enc[p][e].s, enc[p][e].t); fprintf (stderr, "\n");
   }
   encoding = safemalloc(l + 1);
   p = 0;
   {
      int cur = E_ASCII;        // starts ASCII
      while (p < l)
      {
         int t,
          m = 0;
         int b = 0;
         for (e = 0; e < E_MAX; e++)
            if (enc[p][e].t && ((t = enc[p][e].t + switchcost[cur][e]) < m || (t == m && e == cur) || !m))
            {
               b = e;
               m = t;
            }
         if (exact && cur == E_EDIFACT && enc[p][E_ASCII].t <= 2)
            b = E_ASCII;
         if (exact && (cur == E_C40 || cur == E_X12 || cur == E_TEXT) && enc[p][E_ASCII].t <= 1)
            b = E_ASCII;
         if (!p && lenp)
            *lenp = enc[p][b].t + switchcost[cur][b];
         cur = b;
         m = enc[p][b].s;
         while (p < l && m--)
            encoding[p++] = encchr[b];
      }
   }
   encoding[p] = 0;
   return encoding;
}

// Main encoding function
// Returns the grid (malloced) containing the matrix. L corner at 0,0.
// Takes suggested size in *Wptr, *Hptr, or 0,0. Fills in actual size.
// Takes barcodelen and barcode to be encoded
// Note, if *encodingptr is null, then fills with auto picked (malloced) encoding
// If lenp not null, then the length of encoded data before any final unlatch or pad is stored
// If maxp not null, then the max storage of this size code is stored
// If eccp not null, then the number of ecc bytes used in this size is stored
// Returns 0 on error (writes to stderr with details).
// Wptr and Hptr non zero same pointer to request a square code only
unsigned char *iec16022ecc200_opts(iec16022ecc200_t o)
{
   unsigned char binary[3000];  // encoded raw data and ecc to place in barcode
   int W = 0,
       H = 0;
   char *encoding = 0;
   unsigned char *grid = 0;
   struct ecc200matrix_s *matrix;
   memset(binary, 0, sizeof(binary));
   if (o.encodingptr)
      encoding = *o.encodingptr;
   if (o.Wptr)
      W = *o.Wptr;
   if (o.Hptr)
      H = *o.Hptr;

   // encoding
   if (W)
   {                            // known size
      for (matrix = ecc200matrix; matrix->W && (matrix->W != W || matrix->H != H); matrix++);
      if (!matrix->W)
      {
         fprintf(stderr, "Invalid size %dx%d\n", W, H);
         return 0;
      }
      if (!encoding)
      {
         int len;
         char *e = encmake(o.barcodelen, o.barcode, &len, 1);
         if (e && len != matrix->bytes)
         {                      // try not an exact fit
            free(e);
            e = encmake(o.barcodelen, o.barcode, &len, 0);
            if (len > matrix->bytes)
            {
               fprintf(stderr, "Cannot make barcode fit %dx%d\n", W, H);
               return 0;
            }
         }
         encoding = e;
      }
   } else
   {                            // find size
      if (encoding)
      {                         // find one that fits chosen encoding
         for (matrix = ecc200matrix; matrix->W; matrix++)
            if (ecc200encode(binary, matrix->bytes, o.barcode, o.barcodelen, encoding, 0) && (matrix->W == matrix->H || (!o.square && (!o.Wptr || o.Wptr != o.Hptr))))
               break;
      } else
      {
         int len;
         char *e;
         // Try exact encoding
         e = encmake(o.barcodelen, o.barcode, &len, 1);
         for (matrix = ecc200matrix; matrix->W; matrix++)
            if (matrix->bytes == len && (matrix->W == matrix->H || (!o.square && (!o.Wptr || o.Wptr != o.Hptr))))
               break;
         if (e && !matrix->W)
         {                      // try for non exact fit
            char *e = encmake(o.barcodelen, o.barcode, &len, 0);
            for (matrix = ecc200matrix; matrix->W; matrix++)
               if (matrix->bytes >= len && (matrix->W == matrix->H || (!o.square && (!o.Wptr || o.Wptr != o.Hptr))))
                  break;
            if (e)
               free(e);
         }
         encoding = e;
      }
      if (!matrix->W)
      {
         fprintf(stderr, "Cannot find suitable size, barcode too long\n");
         return 0;
      }
      W = matrix->W;
      H = matrix->H;
   }
   int q = (o.noquiet ? 0 : 1);
   if (!ecc200encode(binary, matrix->bytes, o.barcode, o.barcodelen, encoding, o.lenp))
      fprintf(stderr, "Barcode too long for %dx%d %s %u bytes\n", W, H, encoding, matrix->bytes);
   else
   {
      // ecc code
      ecc200(binary, matrix->bytes, matrix->datablock, matrix->rsblock);
      {                         // placement
         int x,
          y,
          NC,
          NR,
         *places;
         NC = W - 2 * (W / matrix->FW);
         NR = H - 2 * (H / matrix->FH);
         places = safemalloc(NC * NR * sizeof(int));
         ecc200placement(places, NR, NC);
         grid = safemalloc((W + q + q) * (H + q + q));
         memset(grid, 0, (W + q + q) * (H + q + q));
         for (y = 0; y < H; y += matrix->FH)
         {
            for (x = 0; x < W; x++)
               grid[(y + q + matrix->FH - 1) * (W + q + q) + q + x] = 1;
            for (x = 0; x < W; x += 2)
               grid[(y + q) * (W + q + q) + q + x] = 1;
         }
         for (x = 0; x < W; x += matrix->FW)
         {
            for (y = 0; y < H; y++)
                grid[(H - 1 - y + q) * (W + q + q) + q + x] = 1;
            for (y = 0; y < H; y += 2)
               grid[(H - 1 - y + q) * (W + q + q) + q + x + matrix->FW - 1] = 1;
         }
         for (y = 0; y < NR; y++)
         {
            for (x = 0; x < NC; x++)
            {
               int v = places[y * NC + x];
               if (v == 1 || (v > 7 && (binary[(v >> 3) - 1] & (1 << (v & 7)))))
                  grid[(1 + y + q + 2 * (y / (matrix->FH - 2))) * (W + q + q) + q + 1 + x + 2 * (x / (matrix->FW - 2))] = 1;
            }
         }
         free(places);
      }
   }
   if (o.Wptr)
      *o.Wptr = W + q + q;
   if (o.Hptr)
      *o.Hptr = H + q + q;
   if (o.encodingptr)
      *o.encodingptr = encoding;
   if (o.maxp)
      *o.maxp = matrix->bytes;
   if (o.eccp)
      *o.eccp = (matrix->bytes + 2) / matrix->datablock * matrix->rsblock;
   return grid;
}
