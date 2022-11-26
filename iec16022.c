// IEC16022 bar code generation
// Copyright (c) 2007 Adrian Kennard Andrews & Arnold Ltd
// This software is provided under the terms of the GPL v2 or later.
// This software is provided free of charge with a full "Money back" guarantee.
// Use entirely at your own risk. We accept no liability. If you don't like that - don't use it.

// TBA, structured append, and ECI options.
// TBA, library for odd size ECC000-140 codes

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <popt.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include "image.h"
#include "iec16022ecc200.h"

 // simple checked response malloc
void *safemalloc(int n)
{
   void *p = malloc(n);
   if (!p)
      errx(1, "Malloc(%d) failed", n);
   return p;
}

// hex dump - bottom left pixel first
void dumphex(unsigned char *grid, int W, int H, unsigned char p, int S, int B)
{
   int c = 0,
       y;
   for (y = -B * S; y < (H + B) * S; y++)
   {
      int v = 0,
          x,
          b = 128;
      for (x = -B * S; x < (W + B) * S; x++)
      {
         if (x >= 0 && x < W * S && y >= 0 && y < H * S && (grid[(H - 1 - y / S) * W + (x / S)] & 1))
            v |= b;
         b >>= 1;
         if (!b)
         {
            printf("%02X", v ^ p);
            v = 0;
            b = 128;
            c++;
         }
      }
      if (b != 128)
      {
         printf("%02X", v ^ p);
         c++;
      }
      printf(" ");
      c++;
      if (c >= 40)
      {
         printf("\n");
         c = 0;
      }
   }
   if (c)
      printf("\n");
}

int main(int argc, const char *argv[])
{
   int c;
   int W = 0,
       H = 0;
   int ecc = 0;
   int barcodelen = 0;
   char *encoding = NULL;
   char *outfile = NULL;
   char *infile = NULL;
   char *barcode = NULL;
   char *format = NULL;
   char *size = NULL;
   char *eccstr = NULL;
   int len = 0,
       maxlen = 0,
       ecclen = 0,
       square = 0,
       noquiet = 0;;
   int formatcode = 0;
   double scale = -1,
       dpi = -1;
   int S = -1;
   unsigned char *grid = 0;
   poptContext optCon;          // context for parsing command-line options
   const struct poptOption optionsTable[] = {
      { "fixed", 's', POPT_ARG_STRING, &size, 0, "Size", "WxH" },
      { "square", 'q', POPT_ARG_NONE, &square, 0, "Square", 0 },
      {
       "barcode", 'c', POPT_ARG_STRING, &barcode, 0, "Barcode", "text" },
      {
       "ecc", 0, POPT_ARG_STRING, &eccstr, 0, "ECC", "000/050/080/100/140/200" },
      {
       "infile", 'i', POPT_ARG_STRING, &infile, 0, "Barcode file", "filename" },
      {
       "outfile", 'o', POPT_ARG_STRING, &outfile, 0, "Output filename", "filename or -" },
      {
       "encoding", 'e', POPT_ARG_STRING, &encoding, 0, "Encoding template", "[CTXEAB]* for ecc200 or 11/27/41/37/128/256" },
      { "svg", 0, POPT_ARG_VAL, &formatcode, 'v', "SVG" },
      { "path", 0, POPT_ARG_VAL, &formatcode, 'V', "SVG path" },
      { "png", 0, POPT_ARG_VAL, &formatcode, 'p', "PNG" },
      { "data", 0, POPT_ARG_VAL, &formatcode, 'd', "PNG Data URI" },
      { "eps", 0, POPT_ARG_VAL, &formatcode, 'e', "EPS" },
      { "ps", 0, POPT_ARG_VAL, &formatcode, 'g', "Postscript" },
      { "text", 0, POPT_ARG_VAL, &formatcode, 't', "Text" },
      { "binary", 0, POPT_ARG_VAL, &formatcode, 'b', "Binary" },
      { "hex", 0, POPT_ARG_VAL, &formatcode, 'h', "Hex" },
      { "stamp", 0, POPT_ARG_VAL, &formatcode, 's', "Stamp" },
      { "info", 0, POPT_ARG_VAL, &formatcode, 'i', "Info" },
      { "size", 0, POPT_ARG_VAL, &formatcode, 'x', "Size" },
      { "scale", 0, POPT_ARG_INT, &S, 0, "Scale", "pixels" },
      { "mm", 0, POPT_ARG_DOUBLE, &scale, 0, "Size of pixels", "mm" },
      { "dpi", 0, POPT_ARG_DOUBLE, &dpi, 0, "Size of pixels", "dpi" },
      { "no-quiet", 'Q', POPT_ARG_NONE, &noquiet, 0, "No quiet area" },
      { "format", 'f', POPT_ARGFLAG_DOC_HIDDEN | POPT_ARG_STRING, &format, 0, "Output format",
       "x=size/t[s]=text/e[s]=EPS/b=bin/h[s]=hex/p[s]=PNG/g[s]=ps/v[s]=svg/s=stamp" },

      POPT_AUTOHELP {
                     NULL, 0, 0, NULL, 0 }
   };
   optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
   poptSetOtherOptionHelp(optCon, "[barcode]");
   if ((c = poptGetNextOpt(optCon)) < -1)
      errx(1, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));

   if (poptPeekArg(optCon) && !barcode && !infile)
      barcode = (char *) poptGetArg(optCon);
   if (poptPeekArg(optCon) || (!barcode && !infile) || (barcode && infile))
   {
      poptPrintUsage(optCon, stderr, 0);
      return -1;
   }
   if (outfile && !strcmp(outfile, "data:"))
   {                            // Legacy format for data:
      if (*format != 'p')
         errx(1, "data: only for png");
      outfile = NULL;
      *format = 'd';
   }
   if (outfile && strcmp(outfile, "-") && !freopen(outfile, "w", stdout))
      err(1, "%s", outfile);
   if (formatcode && format)
      errx(1, "--format is deprecated");
   char formatspace[2] = { };
   if (formatcode)
      *(format = formatspace) = formatcode;
   if (!format)
      format = "t";             // Default

   if (scale >= 0 && dpi >= 0)
      errx(1, "--mm or --dpi");
   if (dpi > 0)
      scale = 25.4 / dpi;
   if (scale >= 0 && S >= 0 && *format != 'e' && *format != 'g')
      errx(1, "--scale or --mm/--dpi");

   if (format && *format && format[1])  // Old scale after format
      S = atoi(format + 1);     // scale
   if (S < 0)
      S = scale;
   if (S <= 0)
      S = 1;
   if (scale < 0)
      scale = 0;


   if (infile)
   {                            // read from file
      FILE *f = stdin;
      if (strcmp(infile, "-"))
         f = fopen(infile, "rb");
      barcode = safemalloc(4001);
      if (!f)
         err(1, "%s", infile);
      barcodelen = fread(barcode, 1, 4000, f);
      if (barcodelen < 0)
         err(1, "%s", infile);
      barcode[barcodelen] = 0;  // null terminate anyway
      fclose(f);
   } else
      barcodelen = strlen(barcode);
   // check parameters
   if (size)
   {
      char *x = strchr(size, 'x');
      W = atoi(size);
      if (x)
         H = atoi(x + 1);
      if (!H)
         W = H;
   }
   if (eccstr)
      ecc = atoi(eccstr);
   if (W & 1)
   {                            // odd size
      if (W != H || W < 9 || W > 49)
         errx(1, "Invalid size %dx%d\n", W, H);
      if (!eccstr)
      {
         if (W >= 17)
            ecc = 140;
         else if (W >= 13)
            ecc = 100;
         else if (W >= 11)
            ecc = 80;
         else
            ecc = 0;
      }
      if ((ecc && ecc != 50 && ecc != 80 && ecc != 100 && ecc != 140) || (ecc == 50 && W < 11) || (ecc == 80 && W < 13) || (ecc == 100 && W < 13) || (ecc == 140 && W < 17))
         errx(1, "ECC%03d invalid for %dx%d\n", ecc, W, H);

   } else if (W)
   {                            // even size
      if (W < H)
      {
         int t = W;
         W = H;
         H = t;
      }
      if (!eccstr)
         ecc = 200;
      if (ecc != 200)
         errx(1, "ECC%03d invalid for %dx%d\n", ecc, W, H);
   }

   else
   {                            // auto size
      if (!eccstr)
         ecc = 200;             // default is even sizes only unless explicit ecc set to force odd sizes
   }

   if (tolower(*format) == 's')
   {                            // special stamp format checks & defaults
      if (!W)
         W = H = 32;
      if (ecc != 200 || W != 32 || H != 32)
         errx(1, "Stamps must be 32x32\n");
      if (encoding)
         errx(1, "Stamps should use auto encoding\n");
      else
      {
         int n;
         for (n = 0; n < barcodelen && (barcode[n] == ' ' || isdigit(barcode[n]) || isupper(barcode[n])); n++);
         if (n < barcodelen)
            errx(1, "Has invalid characters for a stamp\n");
         else
         {                      // Generate simplistic encoding rules as used by the windows app
            // TBA - does not always match the windows app...
            n = 0;
            encoding = safemalloc(barcodelen + 1);
            while (n < barcodelen)
            {
               // ASCII
               while (1)
               {
                  if (n == barcodelen || (n + 3 <= barcodelen && (!isdigit(barcode[n]) || !isdigit(barcode[n + 1]))))
                     break;
                  encoding[n++] = 'A';
                  if (n < barcodelen && isdigit(barcode[n - 1]) && isdigit(barcode[n]))
                     encoding[n++] = 'A';
               }
               // C40
               while (1)
               {
                  int r = 0;
                  while (n + r < barcodelen && isdigit(barcode[n + r]))
                     r++;
                  if (n + 3 > barcodelen || r >= 6)
                     break;
                  encoding[n++] = 'C';
                  encoding[n++] = 'C';
                  encoding[n++] = 'C';
               }
            }
            encoding[n] = 0;
         }
      }
   }
   // processing stamps
   if ((W & 1) || ecc < 200)
      errx(1, "Not done odd sizes yet, sorry\n");
   else
    grid = iec16022ecc200(&W, &H, &encoding, barcodelen, (unsigned char *) barcode, &len, &maxlen, &ecclen, square: square, noquiet:noquiet);

   // output
   if (tolower(*format) != 'i' && (!grid || !W))
      errx(1, "No barcode produced\n");
   switch (tolower(*format))
   {
   case 'x':                   // Size in pixels including quite border
      if (square)
         printf("%d", W);       // unless specifically requested square, print two fields
      else
         printf("%d %d", W, H);
      break;
   case 'i':                   // info
      printf("Size    : %dx%d\n", W, H);
      printf("Encoded : %d of %d bytes with %d bytes of ecc\n", len, maxlen, ecclen);
      printf("Barcode : %s\n", barcode);
      printf("Encoding: %s\n", encoding ? : "");
      break;
   case 'h':                   // hex
      dumphex(grid, W, H, 0, S, 0);
      break;
   case 'b':                   // bin
      {
         int y;
         for (y = 0; y < H * S; y++)
         {
            int v = 0,
                x,
                b = 128;
            for (x = 0; x < W * S; x++)
            {
               if (grid[(y / S) * W + (x / S)])
                  v |= b;
               b >>= 1;
               if (!b)
               {
                  putchar(v);
                  v = 0;
                  b = 128;
               }
            }
            if (b != 128)
               putchar(v);
         }
      }
      break;
   case 't':                   // text
      {
         int y;
         for (y = 0; y < H * S; y++)
         {
            int x;
            for (x = 0; x < (W * S); x++)
               printf("%s", y < H * S && y >= 0 && x < W * S && x >= 0 && grid[W * (y / S) + (x / S)] ? " " : "█");
            printf("\n");
         }
      }
      break;
   case 'e':                   // EPS
      printf("%%!PS-Adobe-3.0 EPSF-3.0\n" "%%%%Creator: IEC18004 barcode/stamp generator\n" "%%%%BarcodeData: %s\n"
             "%%%%BarcodeSize: %dx%d\n" "%%%%DocumentData: Clean7Bit\n" "%%%%LanguageLevel: 1\n" "%%%%Pages: 1\n" "%%%%BoundingBox: 0 0 %d %d\n" "%%%%EndComments\n" "%%%%Page: 1 1\n", barcode, W * S, H * S, (int) ((double) W * (scale * 72 / 25.4 ? : S) + .99), (int) ((double) H * (scale * 72 / 25.4 ? : S) + 0.99));
   case 'g':                   // PS
      if (scale)
         printf("%.4f dup scale ", (scale * 72 / 25.4 / S));
      printf("%d %d 1[1 0 0 1 0 0]{<\n", W * S, H * S);
      dumphex(grid, W, H, 0xFF, S, 0);
      printf(">}image\n");
      break;
   case 's':                   // Stamp
      {
         char temp[74],
          c;
         time_t now;
         struct tm t = {
            0
         };
         int v;
         if (barcodelen < 74)
            errx(1, "Does not look like a stamp barcode\n");
         memmove(temp, barcode, 74);
         c = temp[5];
         temp[56] = 0;
         t.tm_year = atoi(temp + 54) + 100;
         t.tm_mday = 1;
         now = mktime(&t);
         temp[54] = 0;
         now += 86400 * (atoi(temp + 51) - 1);
         t = *gmtime(&now);
         temp[46] = 0;
         v = atoi(temp + 36);
         printf("%%!PS-Adobe-3.0 EPSF-3.0\n" "%%%%Creator: IEC16022 barcode/stamp generator\n" "%%%%BarcodeData: %s\n"
                "%%%%BarcodeSize: %dx%d\n" "%%%%DocumentData: Clean7Bit\n" "%%%%LanguageLevel: 1\n"
                "%%%%Pages: 1\n" "%%%%BoundingBox: 0 0 190 80\n" "%%%%EndComments\n" "%%%%Page: 1 1\n"
                "save 10 dict begin/f{findfont exch scalefont setfont}bind def/rm/rmoveto load def/m/moveto load def/rl/rlineto load def\n"
                "/l/lineto load def/cp/closepath load def/c{dup stringwidth pop -2 div 0 rmoveto show}bind def\n" "72 25.4 div dup scale 0 0 m 67 0 rl 0 28 rl -67 0 rl cp clip 1 setgray fill 0 setgray 0.5 0 translate 0.3 setlinewidth\n" "%d %d 1[1 0 0 1 0 -11]{<\n", barcode, W, H, W, H);
         dumphex(grid, W, H, 0xFF, 1, 0);
         printf(">}image\n" "3.25/Helvetica-Bold f 8 25.3 m(\\243%d.%02d)c\n" "2.6/Helvetica f 8 22.3 m(%.4s %.4s)c\n" "1.5/Helvetica f 8 3.3 m(POST BY)c\n" "3.3/Helvetica f 8 0.25 m(%02d.%02d.%02d)c\n", v / 100, v % 100, temp + 6, temp + 10, t.tm_mday, t.tm_mon + 1, t.tm_year % 100);
         if (c == '1' || c == '2' || c == 'A' || c == 'S')
         {
            if (c == '2')
               printf("42 0 m 10 0 rl 0 28 rl -10 0 rl cp 57 0 m 5 0 rl 0 28 rl -5 0 rl cp");
            else
               printf("42 0 m 5 0 rl 0 28 rl -5 0 rl cp 52 0 m 10 0 rl 0 28 rl -10 0 rl cp");
            printf(" 21 0 m 16 0 rl 0 28 rl -16 0 rl cp fill\n" "21.3 0.3 m 15.4 0 rl 0 13 rl -15.4 0 rl cp 1 setgray fill gsave 21.3 0.3 15.4 27.4 rectclip newpath\n");
            switch (c)
            {
            case '1':
               printf("27/Helvetica-Bold f 27 8.7 m(1)show grestore 0 setgray 1.5/Helvetica-Bold f 22 3.3 m(POSTAGE PAID GB)show 1.7/Helvetica f 29 1.5 m(SmartStamp.co.uk)c\n");
               break;
            case '2':
               printf("21/Helvetica-Bold f 23.5 13 m(2)1.25 1 scale show grestore 0 setgray 1.5/Helvetica-Bold f 22 3.3 m(POSTAGE PAID GB)show 1.7/Helvetica f 29 1.5 m(SmartStamp.co.uk)c\n");
               break;
            case 'A':
               printf("16/Helvetica-Bold f 29 14.75 m 1.1 1 scale(A)c grestore 0 setgray 1.5/Helvetica-Bold f 22 3.3 m(POSTAGE PAID GB)show 1.7/Helvetica f 22 1.5 m(Par Avion)show\n");
               break;
            case 'S':
               printf("10/Helvetica-Bold f 29 17 m(SU)c grestore 0 setgray 1.5/Helvetica-Bold f 22 1.5 m(POSTAGE PAID GB)show\n");
               break;
            }
            printf("2.3/Helvetica-Bold f 29 10 m(ROYAL MAIL)c\n");
         } else if (c == 'P')
         {                      // Standard Parcels
            printf("21 0 m 41 0 rl 0 28 rl -41 0 rl cp fill\n"
                   "37.7 0.3 m 24 0 rl 0 27.4 rl -24 0 rl cp 1 setgray fill gsave 21.3 0.3 16.4 27.4 rectclip newpath\n"
                   "22.5/Helvetica-Bold f 37.75 -1.25 m 90 rotate(SP)show grestore 0 setgray\n" "3.5/Helvetica-Bold f 49.7 21.5 m(ROYAL MAIL)c\n" "2.3/Helvetica-Bold f 49.7 7 m(POSTAGE PAID GB)c\n" "2.6/Helveica f 49.7 4.25 m(SmartStamp.co.uk)c\n");
         } else if (c == '3' || c == '9')
            printf("21.15 0.15 40.7 27.7 rectstroke\n"
                   "21 0 m 41 0 rl 0 5 rl -41 0 rl cp fill\n"
                   "0 1 2{0 1 18{dup 1.525 mul 22.9 add 24 3 index 1.525 mul add 3 -1 roll 9 add 29 div 0 360 arc fill}for pop}for\n"
                   "50.5 23.07 m 11.5 0 rl 0 5 rl -11.5 0 rl cp fill\n"
                   "5.85/Helvetica f 23.7 15.6 m(Royal Mail)show\n"
                   "4.75/Helvetica-Bold f 24 11 m(special)show 4.9/Helvetica f(delivery)show\n" "gsave 1 setgray 3.2/Helvetica-Bold f 24 1.6 m(%s)show 26 10.15 m 2 0 rl stroke grestore\n" "21.15 9.9 m 53.8 9.9 l stroke 53.8 9.9 0.4 0 360 arc fill\n", c == '3' ? "next day" : "9.00am");
         printf("end restore\n");
      }
      break;
   case 'v':                   // svg
      {
         int x,
          y;
         Image *i;
         i = ImageNew(W, H, 2);
         i->Colour[0] = 0xFFFFFF;
         i->Colour[1] = 0;
         for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
               if (grid[y * W + x] & 1)
                  ImagePixel(i, x, y) = 1;
         if (isupper(*format))
            ImageSVGPath(i, stdout, 1);
         else
            ImageWriteSVG(i, fileno(stdout), 0, -1, barcode, scale);
         ImageFree(i);
      }
      break;
   case 'd':                   // data: URI
   case 'p':                   // png
      {
         int x,
          y;
         Image *i = ImageNew(W * S, H * S, 2);
         i->Colour[0] = 0xFFFFFF;
         i->Colour[1] = 0;
         for (y = 0; y < H * S; y++)
            for (x = 0; x < W * S; x++)
               if (grid[(y / S) * W + (x / S)])
                  ImagePixel(i, x, y) = 1;
         if (*format == 'd')
         {
            char tmp[] = "/tmp/XXXXXX";
            int fh = mkstemp(tmp);
            if (fh < 0)
               err(1, "Fail %s", tmp);
            unlink(tmp);
            ImageWritePNG(i, fh, 0, -1, barcode);
            lseek(fh, 0, SEEK_SET);
            printf("data:image/png;base64,");
            static const char BASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            int b = 0,
                v = 0;
            while (1)
            {
               unsigned char c;
               if (read(fh, &c, 1) != 1)
                  break;
               b += 8;
               v = (v << 8) | c;
               while (b >= 6)
               {
                  b -= 6;
                  putchar(BASE64[(v >> b) & 0x3F]);
               }
            }
            if (b)
            {
               b += 8;
               v = (v << 8);
               b -= 6;
               putchar(BASE64[(v >> b) & 0x3F]);
            }
            while (b)
            {
               if (b < 6)
                  b += 8;
               b -= 6;
               putchar('=');
            }
            close(fh);
         } else
            ImageWritePNG(i, fileno(stdout), 0, -1, barcode);
         ImageFree(i);
      }
      break;
   default:
      errx(1, "Unknown output format %s\n", format);
      break;
   }
   
   free(encoding);
   free(grid);

   return 0;
}
