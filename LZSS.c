/*
  LZSS implementation    by    YarnSalesman
  
  
  Description:
  As I've understood it, LZSS compresses data using 2-byte POS-LEN codes that represent a previously
  seen sequence of bytes in a sliding window. Typically allocating 11 bits for POS, 4 bits for LEN, 
  and 1 to identify it as a POS-LEN match code. As for non-matches, 9 bits are allocated where 8 bits 
  are the literal non-matched byte and 1 bit that says it's a literal.
  
  In an attempt to improve compression this implementation does not use fixed bit lengths for matches
  and does away with a bit-per-non-match. I reckoned if you had 256 non-matched bytes, that ends up 
  amounting to 256 bits to represent each of those, which is an additional 32 bytes. Why not just use
  a single byte to say there are n non-matched bytes ahead?
  
 
  Well not always one byte, but for non-matches, depending on how many, codes may be 1/2/3/5 bytes long:
  
   _Identifier
  /
  0 00 11111 [B|W|D] ___Overflow (31 + overflow)
    \  \
     \  \_Count (max=31)
      \
       \_Overflow Size (00=No Overflow; 01=BYTE; 10=WORD; 11=DWORD)


  POSLEN match codes can also vary in size depending on the POS and LEN:
  
     _Identifier
    /
   /      _LEN-POS
  /      /
  1 0000 000 0000 0000 [B|W|D] ___Overflow (LEN)
    \
     \_POS bit length (>9 these then represent the overflow size; LEN-POS then holds only POS)

 
  These modifications may not be so beneficial in some instances mind you.
  The WINDOWSIZE. Yeah I don't think it should be larger than 4 KB.
  
  COMPILE: gcc -Wall LZSS.c

*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MINMATCH   0x3
#define WINDOWSIZE 0x1000

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;


void * open_file_mem(char *fpath, u32 *fsize)
{
  u8    *fmem;
  FILE  *in;
  
  in     = fopen(fpath, "rb");
  fseek(in, 0, SEEK_END);
  *fsize = ftell(in);
  fseek(in, 0, SEEK_SET);
  fmem   = calloc(*fsize, 1);
  fread(fmem, 1, *fsize, in);
  fclose(in);
  
  return fmem;
}

int write_file(char *opath, u8 *odata, u32 fsize, u32 csize, u32 type)
{
  FILE  *out;
  
  out = fopen(opath, "wb");
  
  if (type)
  {
    fwrite(&fsize, sizeof(u32), 1, out);
    fwrite(&csize, sizeof(u32), 1, out);
    fwrite(odata, 1, csize, out);
  }
  else
  {
    fwrite(odata, 1, fsize, out);
  }
  
  fclose(out);
  
  return 0;
}

void find_longest_match(u8 *start, u8 *end, u8 *wnd, u32 *pos, u32 *len)
{
  *pos = 0;
  *len = 1;
  
  u8  *ltemp, *lptr = wnd;
  u8  *stemp, *sptr = wnd-1;
  u32  spos, mlen;
  
  for (spos=0,mlen=0; sptr > start+1; sptr--)
  {
    spos++;
    if (*sptr == *lptr)
    {
      mlen++;
      for (stemp=sptr+1,ltemp=lptr+1; ltemp != end && *stemp==*ltemp; stemp++,ltemp++,mlen++);
      if (mlen > *len)
      {
        *pos = spos;
        *len = mlen;
      }
      mlen = 0;
    }
  }
}

// Thanks Stack Overflow
u32 POS32 [] = {  0,  1, 28,  2, 29, 14, 24, 3,
                 30, 22, 20, 15, 25, 17,  4, 8,
                 31, 27, 13, 23, 21, 19, 16, 7,
                 26, 12, 18,  6, 11,  5, 10, 9  };

#define MSBi32(n)                     \
  n |= n >> 1;                        \
  n |= n >> 2;                        \
  n |= n >> 4;                        \
  n |= n >> 8;                        \
  n |= n >> 16;                       \
  n  = (n >> 1) + 1;                  \
  n  = POS32[(n * 0x077CB531) >> 27]; \

// check for when there actually is compression vs not.
u8 * write_code(u32 pos, u32 len, u8 *wnd, u8 *cptr, u32 *nm)
{
  if (*nm)
  {
    wnd  -= *nm;
    *cptr = 0;                                                                      // 0 00 00000
    if (*nm < 32) *cptr |= *nm;                                                     // 0 00 -----
    else
    {
      u32 _nm = *nm - 31;

      *cptr |= 31;                                                                  // 0 -- 11111
      if      (_nm <= 0xFF)   { *cptr |= 32; *(u8  *)(cptr+1) = _nm; cptr += 1; }   // 0 01 11111  8
      else if (_nm <= 0xFFFF) { *cptr |= 64; *(u16 *)(cptr+1) = _nm; cptr += 2; }   // 0 10 11111  16
      else                    { *cptr |= 96; *(u32 *)(cptr+1) = _nm; cptr += 4; }   // 0 11 11111  32
    }
    for (cptr++; *nm; (*nm)--,*cptr=*wnd,cptr++,wnd++);
  }

  if (len >= MINMATCH)
  {
    u16  cinfo  = 0x8000;
    u32  poslen = pos;
    u32  lenlen = len;

    MSBi32(poslen) // returns 0 if bitlen is 1
    MSBi32(lenlen)

    if (poslen+lenlen < 11-1)
    {
      cinfo |= ((len<<(poslen+1))|pos);
      cinfo |= ((poslen) << 11);
      *(u16 *)cptr = (cinfo << 8) | (cinfo >> 8);
    }
    else
    {
      poslen  = (len <= 0xFFFF ? len <= 0xFF ? 10 : 12 : 14) + (poslen == 12-1 ? 1 : 0);
      cinfo  |= ((poslen) << 11);
      cinfo  |= pos;
      *(u16 *)cptr = (cinfo << 8) | (cinfo >> 8);

      if      (poslen<12) { *(u8  *)(cptr+2) = len; cptr += 1; }
      else if (poslen<14) { *(u16 *)(cptr+2) = len; cptr += 2; }
      else                { *(u32 *)(cptr+2) = len; cptr += 4; }
    }
    cptr += 2;
  }

  return cptr;
}

int compress(u8 *uc, u8 *c, u32 ucsize)
{
  u8  *wnd   = uc;
  u8  *start = uc-1;
  u8  *end   = uc+ucsize;
  u8  *cptr  = c;
  u32  i, nm;
  u32  len, pos;

  for (nm=0,i=0; i<ucsize; i+=len, wnd+=len)
  {
    if (i>WINDOWSIZE) start += len;

    find_longest_match(start, end, wnd, &pos, &len);
    
    if (len >= MINMATCH)
      cptr = write_code(pos, len, wnd, cptr, &nm);
    else
      nm += len;
  }

  if (nm) cptr = write_code(pos, len, wnd, cptr, &nm);

  return cptr-c;
}

int decompress(u8 *c, u8 *uc)
{
  u16  info;
  u32  lcount, poslen, pos, len;
  u8  *wptr;
  u8  *cptr  = c + 8;
  u8  *wnd   = uc;
  u32  csize = *(u32 *)(c+4);

  c += 8;

  while (csize)
  {
    if (*cptr & 0x80)
    {
      info = *(u16 *)cptr;
      info = (info >> 8) | (info << 8);

      poslen = (info >> 11) & 0xF;
      if (poslen < 11-1)
      {
        len   = (info & 0x7FF) >> (poslen+1);
        pos   = info & (0xFFFF >> (16-(poslen+1)));
      }
      else
      {
        poslen = poslen & 0xE;
        pos    = info & 0xFFF;
        if      (poslen == 14) { len = *(u32 *)(cptr+2); cptr += 4; }
        else if (poslen == 12) { len = *(u16 *)(cptr+2); cptr += 2; }
        else                   { len = *(u8  *)(cptr+2); cptr += 1; }
      }
      
      for (cptr+=2, wptr=wnd-pos; len--; *wnd=*wptr,wnd++,wptr++);
    }
    else
    {
      if      ((*cptr & 0x60) == 0x60) { lcount = 31 + *(u32 *)(cptr+1); cptr += 4; } // dword
      else if ((*cptr & 0x60) == 0x40) { lcount = 31 + *(u16 *)(cptr+1); cptr += 2; } // word
      else if ((*cptr & 0x60) == 0x20) { lcount = 31 + *(u8  *)(cptr+1); cptr += 1; } // byte
      else                               lcount = *cptr;

      for (cptr++; lcount--; *wnd=*cptr,wnd++,cptr++);
    }

    csize -= (cptr-c);
    c      = cptr;
  }

  return 0;
}

int lzss_compress(char *fpath, char *opath)
{
  u32   fsize, csize;
  u8   *idata, *odata;
  
  idata = open_file_mem(fpath, &fsize);
  odata = calloc((fsize/8) + fsize + 1, 1); // allocate some extra just in case

  csize = compress(idata, odata, fsize);

  //wprintf(L"compressed size = %d\n", csize);

  write_file(opath, odata, fsize, csize, 1);

  free(idata);
  free(odata);
  return 0;
}

int lzss_decompress(char *fpath, char *opath)
{
  u32  dummy, ucsize;
  u8   *cdata, *ucdata;

  cdata  = open_file_mem(fpath, &dummy);
  ucsize = *(u32 *)cdata;
  ucdata = calloc(ucsize, 1);

  decompress(cdata, ucdata);

  write_file(opath, ucdata, ucsize, 0, 0);

  return 0;
}

int main(int argc, char *argv[])
{
  if (argc > 2)
  {
    if      (*argv[1] == 'c') lzss_compress(argv[2], "compressed.bin");
    else if (*argv[1] == 'd') lzss_decompress(argv[2], "uncompressed.bin");
  }

  return 0;
}
