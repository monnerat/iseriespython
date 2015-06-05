/*
 * as400misc  Misc routines 
 * 
 *--------------------------------------------------------------------
 * Copyright (c) 2010 by Per Gummedal.
 *
 * per.gummedal@gmail.com
 * 
 * By obtaining, using, and/or copying this software and/or its
 * associated documentation, you agree that you have read, understood,
 * and will comply with the following terms and conditions:
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * associated documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appears in all
 * copies, and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the author
 * not to be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 * 
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *------------------------------------------------------------------------
 */

#include <string.h>
#include <qleawi.h>
#include <qusec.h>
#include <qmhchgem.h>
#include <qwcrdtaa.h>
#include <iconv.h>
#include <qtqiconv.h>
#include <miptrnam.h>
#include "osdefs.h"
#include "imisc.h"

static int utfInit = 0;
static iconv_t cdToUtf;
static iconv_t cdFromUtf;
/* hold conversion descriptors */
static long int convccsid_array[30];
static iconv_t convdesc_array[30];
static int conv_size = 0;
static char e_buf[MAXPATHLEN + 1];

typedef struct { 
    Qus_EC_t ec_fields;
    char     exception_data[100];
} error_code_struct;

/* returns conversion descriptors  */
static iconv_t
initConvert(int fromccsid, int toccsid) {
    QtqCode_T qtfrom, qtto;
    memset(&qtfrom, 0x00, sizeof(qtfrom));
    memset(&qtto, 0x00, sizeof(qtto));
    qtfrom.CCSID = fromccsid;
    qtto.CCSID = toccsid;
    return QtqIconvOpen(&qtto, &qtfrom);
}

iconv_t
getConvDesc(int fromccsid, int toccsid) {
    long int ccsid;
    int i;
    ccsid = (fromccsid << 16) | toccsid;
    for (i = 0; i < conv_size; i++) {
        if (ccsid == convccsid_array[i])
            return convdesc_array[i];
    }
    if (i < 30) {
        conv_size += 1;
        convdesc_array[i] = initConvert(fromccsid, toccsid);
        convccsid_array[i] = ccsid;
        return convdesc_array[i];
    } else
        return initConvert(fromccsid, toccsid);
}

/* convert form one ccsid to another, returns bytes converted */
int
convertstr(iconv_t cd, const char *in, int in_size, char *out, int out_size)
{
    const char *__ptr128 p1;
    char *__ptr128 p2;
    size_t insize, outsize;
    p1 = in;
    p2 = out;
    insize = in_size;
    outsize = out_size;
    iconv(cd, &p1, &insize, &p2, &outsize);
    return (out_size - outsize);
}


/* convert from ebcdic ccsid to utf-8 */
int
ebcdicToStringBuffer(int fromccsid, const char *in, int size, char *out)
{
    const char *__ptr128 p1;
    char *__ptr128 p2;
    size_t insize, outsize;
    p1 = in;
    p2 = out;
    insize = outsize = size;
    iconv(getConvDesc(fromccsid, 1208), &p1, &insize, &p2, &outsize);
    *p2 = '\0';
    return (size - outsize);
}

/* convert string to ebcdic ccsid padded with blank up to len */
char *
stringToEbcdic(int toccsid, const char *in, int in_len, char *out, int out_len)
{
    const char *__ptr128 p1;
    char *__ptr128 p2;
    size_t sizefrom, sizeto;
    if (in_len > 0) {
        p1 = in;
        p2 = out;
        sizefrom = in_len;
        sizeto = out_len;
        iconv(getConvDesc(1208, toccsid), &p1, &sizefrom, &p2, &sizeto);
        if (sizeto > 0)
            memset(p2, 0x40, sizeto); 
    } else
        memset(out, 0x40, out_len);
    return out;
}

/* convert string to ebcdic ccsid padded with blank up to len */
char *
unicodeToEbcdic(int toccsid, const char *in, int in_len, char *out, int out_len)
{
    const char *__ptr128 p1;
    char *__ptr128 p2;
    size_t sizefrom, sizeto;
    if (in_len > 0) {
        p1 = in;
        p2 = out;
        sizefrom = in_len;
        sizeto = out_len;
        iconv(getConvDesc(13488, toccsid), &p1, &sizefrom, &p2, &sizeto);
        if (sizeto > 0)
            memset(p2, 0x40, sizeto); 
    } else
        memset(out, 0x40, out_len);
    return out;
}

static int
ebcdicLen(char *buf, int len) {
    char *p;
    if (len <= 0)
        p = buf + strlen(buf) - 1;
    else
        p = buf + len - 1;
    while (p >= buf && *p == 0x40)
        p--;
    return buf - p + 1;
}

void
initUtf(void)
{
    cdFromUtf = initConvert(1208, 0);
    cdToUtf = initConvert(0, 1208);
    utfInit = 1;
}

/* convert as/400 string to utf */   
char *
strToUtf(char *in, char *out) 
{
    char *__ptr128 p1, *__ptr128 p2;
    size_t sizefrom, sizeto;
    if (*in == '\0') {
        *out = '\0';
        return out;
    }
    if (utfInit == 0) initUtf();
    p1 = in;
    p2 = out;
    sizefrom = sizeto = strlen(in);
    iconv(cdToUtf, &p1, &sizefrom, &p2, &sizeto);
    *p2 = '\0';
    return out;
}

/* convert as/400 string to utf, returns length not including ending NULL */   
int
strLenToUtf(char *in, int len, char *out) 
{
    char *__ptr128 p1, *__ptr128 p2;
    size_t sizefrom, sizeto;
    char *p;
    /* remove trailing blanks */
    p = in + len - 1;
    while (p >= in && *p == 0x40)
        p--;
    sizefrom = (p - in) + 1;
    if (sizefrom <= 0) {
        *out = '\0';
        return 0;
    }
    if (utfInit == 0) initUtf();
    p1 = in;
    p2 = out;
    sizeto = len;
    iconv(cdToUtf, &p1, &sizefrom, &p2, &sizeto);
    *p2 = '\0';
    return len - sizeto;
}

/* convert string from utf to job ccsid */
char *
utfToStr(char *in, char *out)
{
    char *__ptr128 p1, *__ptr128 p2;
    size_t sizefrom, sizeto;
    if (*in == '\0') {
        *out = '\0';
        return out;
    }
    if (utfInit == 0) initUtf();
    p1 = in;
    p2 = out;
    sizefrom = sizeto = strlen(in);
    iconv(cdFromUtf, &p1, &sizefrom, &p2, &sizeto);
    *p2 = '\0';
    return out;
}

/* convert string from utf to job ccsid padded with blank up to len */
char *
utfToStrLen(char *in, char *out, int len, int add_null)
{
    char *__ptr128 p1, *__ptr128 p2;
    size_t sizefrom, sizeto;
    if (*in != '\0') {
        if (utfInit == 0) initUtf();
        p1 = in;
        p2 = out;
        sizefrom = strlen(in);
        sizeto = len;
        iconv(cdFromUtf, &p1, &sizefrom, &p2, &sizeto);
        if (sizeto > 0)
            memset(p2, 0x40, sizeto); 
    } else
        memset(out, 0x40, len);
    if (add_null == 1)
        out[len] = '\0';
    return out;
}

/* convert zoned to string (ascii)*/
char *
zonedtostr(char *buf, char *p, int digits, int dec, char decsep)
{
    unsigned char c;
    int decstart, i, j;
    decstart = digits - dec;
    /* check if negative */
    j = 0;
    if (((p[digits - 1] >> 4) | 0xF0) != 0xFF) {                
        buf[j] = '-';
        j++;
    }           
    /* get all digits */
    for (i = 0; i < digits; i++) {              
        c = (p[i] & 0x0f) | 0x30;
        /* decimal point */
        if (i == decstart) {
            if (j == 0 || (j == 1 && buf[0] == '-')) {
                buf[j] = '0';
                j++;
            }
            buf[j] = decsep;
            j++;
        }
        if (c != 0x30 || j > 1 || (j > 0 && buf[0] != '-'))  {
            buf[j] = c;
            j++;
        }
    }
    if (j == 0) {
        buf[j] = '0';
        j++;
    }
    buf[j] = '\0';
    return buf;
}

/* convert packed to string (ascii) */
char *
packedtostr(char *buf, char *p, int digits, int dec, char decsep)
{
    unsigned char c;
    int decstart, i, j;
    /* size should be odd */
    if ((digits % 2) == 0)
        digits++;
    decstart = digits - dec;
    /* check if negative */
    j = 0;
    if ((p[digits/2] | 0xF0) != 0xFF) {             
        buf[j] = '-';
        j++;
    }           
    /* get all digits */
    for (i = 0; i < digits; i++) {              
        if (i % 2)
            c = (p[i / 2] & 0x0F) | 0x30;
        else
            c = (p[i / 2] >> 4) | 0x30;
        /* decimal point */
        if (i == decstart) {
            if (j == 0 || (j == 1 && buf[0] == '-')) {
                buf[j] = '0';
                j++;
            }
            buf[j] = decsep;
            j++;
        }
        if (c != 0x30 || j > 1 || (j > 0 && buf[0] != '-'))  {
            buf[j] = c;
            j++;
        }
    }
    if (j == 0) {
        buf[j] = '0';
        j++;
    }
    buf[j] = '\0';
    return buf;
}

