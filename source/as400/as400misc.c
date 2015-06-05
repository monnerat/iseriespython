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

#include "Python.h"
#include <qleawi.h>
#include <qusec.h>
#include <qmhchgem.h>
#include <qwcrdtaa.h>
#include <iconv.h>
#include <qtqiconv.h>
#include <miptrnam.h>
#include "as400misc.h"
#include "osdefs.h"

static int utfInit = 0;
static iconv_t cdToUtf;
static iconv_t cdFromUtf;
/* hold conversion descriptors */
static long int convccsid_array[30];
static iconv_t convdesc_array[30];
static int conv_size = 0;
static char pyhome[256];
static char pypath[1024];
static char e_buf[1024];

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

/* convert form one ccsid to another, returns a PyString */
PyObject *
convertString(iconv_t cd, const char *in, int size)
{
    const char *__ptr128 p1;
    char *__ptr128 p2;
    size_t insize, outsize, newsize;
    int pos;
    PyObject *outobj;
    if (size <= 0)
        return PyString_FromStringAndSize("", 0);
    p1 = in;
    insize = outsize = newsize = size;
    outobj = PyString_FromStringAndSize(NULL, newsize);
    if (outobj == NULL) return NULL;
    p2 = PyString_AS_STRING(outobj);
    iconv(cd, &p1, &insize, &p2, &outsize);
    while (insize > 0) {
        pos = newsize - outsize;
        newsize += 10 + insize * 2;
        _PyString_Resize(&outobj, newsize);
        if (outobj == NULL) return NULL;
        p2 = PyString_AS_STRING(outobj) + pos;
        outsize = newsize - pos;
        iconv(cd, &p1, &insize, &p2, &outsize);
        /* if outsize still is same size there is a bug */
        if (outsize == newsize - pos)
            return NULL;
    }
    if (outsize > 0)
        _PyString_Resize(&outobj, newsize - outsize);
    return outobj;
}

/* convert from ebcdic ccsid to utf-8 */
PyObject *
ebcdicToString(int fromccsid, const char *in, int size)
{
    if (size <= 0)
        return PyString_FromStringAndSize("", 0);
    return convertString(getConvDesc(fromccsid, 1208), in, size);
}

/* convert from ebcdic ccsid to ucs2 */
PyObject *
ebcdicToUnicode(int fromccsid, const char *in, int size)
{
    const char *__ptr128 p1;
    char *__ptr128 p2;
    size_t insize, outsize, newsize;
    int pos;
    PyObject *outobj;
    if (size <= 0)
        return PyUnicode_FromUnicode((Py_UNICODE *)"\00\00\00\00", 0);
    p1 = in;
    insize = size;
    outsize = newsize = (size * 2);
    outobj = PyUnicode_FromUnicode(NULL, newsize / 2);
    if (outobj == NULL) return NULL;
    p2 = (char *)PyUnicode_AS_DATA(outobj);
    iconv(getConvDesc(fromccsid, 13488), &p1, &insize, &p2, &outsize);
    while (insize > 0) {
        pos = newsize - outsize;
        newsize += outsize * 2;
        PyUnicode_Resize(&outobj, newsize / 2);
        if (outobj == NULL) return NULL;
        p2 = (char *)PyUnicode_AS_DATA(outobj) + pos;
        outsize = newsize - pos;
        iconv(getConvDesc(fromccsid, 13488), &p1, &insize, &p2, &outsize);
        /* if outsize still is same size there is a bug */
        if (outsize == newsize - pos)
            return NULL;
    }
    if (outsize > 0)
        PyUnicode_Resize(&outobj, (newsize - outsize) / 2);
    return outobj;
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
    if (errno == E2BIG)
        errno = 0;
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
        if (errno == E2BIG)
            errno = 0;
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
        if (errno == E2BIG)
            errno = 0;
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
strToUtf(char *in, char *out, int maxlen) 
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
    sizefrom = strlen(in);
    sizeto = maxlen;
    iconv(cdToUtf, &p1, &sizefrom, &p2, &sizeto);
    *p2 = '\0';
    return out;
}

/* convert as/400 string to utf return a new Python object */   
PyObject *
strLenToUtfPy(char *in, int len) 
{
    char *__ptr128 p1, *__ptr128 p2;
    size_t sizefrom, sizeto, newsize;
    int pos;
    char *p;
    PyObject *o;
    /* remove trailing blanks */
    p = in + len - 1;
    while (p >= in && *p == 0x40)
        p--;
    newsize = sizefrom = sizeto = (p - in) + 1;
    if (sizefrom <= 0)
        return PyString_FromStringAndSize("", 0);
    o = PyString_FromStringAndSize(NULL, sizeto);
    if (utfInit == 0) initUtf();
    p1 = in;
    p2 = PyString_AS_STRING(o);
    iconv(cdToUtf, &p1, &sizefrom, &p2, &sizeto);
    while (sizefrom > 0) {
        pos = newsize - sizeto;
        newsize += 10 + sizefrom * 2;
        _PyString_Resize(&o, newsize);
        if (o == NULL) return NULL;
        p2 = PyString_AS_STRING(o) + pos;
        sizeto = newsize - pos;
        iconv(cdToUtf, &p1, &sizefrom, &p2, &sizeto);
        /* if sizeto still is same size there is a bug */
        if (sizeto == newsize - pos)
            return NULL;
    }
    if (sizeto > 0)
        _PyString_Resize(&o, newsize - sizeto);
    return o;
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
    if (errno == E2BIG)
        errno = 0;
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
        if (errno == E2BIG)
            errno = 0;
        if (sizeto > 0)
            memset(p2, 0x40, sizeto); 
    } else
        memset(out, 0x40, len);
    if (add_null == 1)
        out[len] = '\0';
    return out;
}

char *
utfToStrLenUpper(char *in, char *out, int len, int add_null)
{
    strncpy(out, in, len);  
    out[len] = '\0';
    strtoupper(out);  
    return utfToStrLen(out, out, len, add_null);
}

/* convert string to upper */
char *
strtoupper(char *str)
{
    int i, c;
    char *s;
    s = str;
    while (*s != '\0') {
        c = Py_CHARMASK(*s);
        if (islower(c))
            *s = toupper(c);               
        s++;
    }
    return str;
}

/* convert string to lower  - Added by Massimo to support ibm_db.c */
char *
strtolower(char *str)
{
    int i, c;
    char *s;
    s = str;
    while (*s != '\0') {
        c = Py_CHARMASK(*s);
        if (isupper(c))
            *s = tolower(c);               
        s++;
    }
    return str;
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

static void rmvmsg(_INTRPT_Hndlr_Parms_T *errmsg)
{
    error_code_struct Error_Code;
    Error_Code.ec_fields.Bytes_Provided = 0;
#pragma convert(37)
    QMHCHGEM(&(errmsg->Target), 0, (char *)&errmsg->Msg_Ref_Key,
             "*REMOVE   ", "", 0, &Error_Code);
#pragma convert(0)
} 

char *
getenv_as(const char *varname)
{
    PyObject *obj;
    char *s = getenv(utfToStr((char *)varname, e_buf));
    if (s == NULL)
        return s;
    strToUtf(s, e_buf, 1023);
    s = (char *)PyMem_Malloc(strlen(e_buf) + 1);
    strcpy(s, e_buf);
    return s;
}

char *
getpythonpath(void)
{
    char prog[20];
    char dtaara[21] = "PYTHONPATHQTEMP     ";
    char buf[1024], *p;
    Qus_EC_t error;
    Qwc_Rdtaa_Data_Returned_t *retData;

    p = (char *) buf;
    error.Bytes_Provided = sizeof(error);
    QWCRDTAA(p, 1024, utfToStrLen(dtaara, dtaara, 20, 1), 1, 1024, &error);
    if (error.Bytes_Available > 0) {
        strcpy(dtaara, "PYTHONPATHQTEMP     ");
        strcpy(prog, Py_GetProgramName());
        p = strchr(prog, '/');
        if (p) strncpy(dtaara + 10, prog, p - prog);
        p = (char *) buf;
        error.Bytes_Provided = sizeof(error);
        QWCRDTAA(p, 1024, utfToStrLen(dtaara, dtaara, 20, 1), 1, 1024, &error);
    }
    if (error.Bytes_Available <= 0) {
        retData = (Qwc_Rdtaa_Data_Returned_t *)p;
#pragma convert(37)
        if (!strncmp(retData->Type_Value_Returned, "*CHAR     ", 10)) {
#pragma convert(0)
            strLenToUtf(p + sizeof(*retData), retData->Length_Value_Returned, pypath);
            return pypath;
        }
    }
    p = getenv(utfToStr("PYTHONPATH", e_buf));
    if (p != NULL) {
        strToUtf(p, pypath, 1023);
        return pypath;
    }
    return NULL;
}

char *
getpythonhome(void)
{
    char prog[20];
    char dtaara[21] = "PYTHONHOMEQTEMP     ";
    char buf[256], *p;
    Qus_EC_t error;
    Qwc_Rdtaa_Data_Returned_t *retData;

    p = (char *) buf;
    error.Bytes_Provided = sizeof(error);
    QWCRDTAA(p, 256, utfToStrLen(dtaara, dtaara, 20, 1), 1, 256, &error);
    if (error.Bytes_Available > 0) {
        strcpy(dtaara, "PYTHONHOMEQTEMP     ");
        strcpy(prog, Py_GetProgramName());
        p = strchr(prog, '/');
        if (p) strncpy(dtaara + 10, prog, p - prog);
        p = (char *) buf;
        error.Bytes_Provided = sizeof(error);
        QWCRDTAA(p, 256, utfToStrLen(dtaara, dtaara, 20, 1), 1, 256, &error);
    }
    if (error.Bytes_Available <= 0) {
        retData = (Qwc_Rdtaa_Data_Returned_t *)p;
#pragma convert(37)
        if (!strncmp(retData->Type_Value_Returned, "*CHAR     ", 10)) {
#pragma convert(0)
            strLenToUtf(p + sizeof(*retData), retData->Length_Value_Returned, pyhome);
            return pyhome;
        }
    }
    p = getenv(utfToStr("PYTHONHOME", e_buf));
    if (p != NULL) {
        strToUtf(p, pyhome, 255);
        return pyhome;
    }
    return "/python27";
}

/* resolve program object */
_SYSPTR getPgm(char *name, char *lib) 
{
    char asname[11], aslib[11];
    volatile _INTRPT_Hndlr_Parms_T  excpData;
    utfToStrLenUpper(name, asname, 10, 1);
    utfToStrLenUpper(lib, aslib, 10, 1);
#pragma exception_handler(EXCP1, excpData, 0, _C2_MH_ESCAPE, _CTLA_HANDLE)
    return rslvsp(_Program, asname, aslib, _AUTH_OBJ_MGMT);
 EXCP1:
    rmvmsg((_INTRPT_Hndlr_Parms_T *)&excpData);
    return NULL;
}

/* resolve service program object */
int getSrvpgm(char *name, char *lib)
{
    char asname[11], aslib[11]; 
    volatile _INTRPT_Hndlr_Parms_T  excpData;
    _SYSPTR ptr;
    utfToStrLenUpper(name, asname, 10, 1);
    /* if name starts with _ replace it with Z */
#pragma convert(37)
    if (asname[0] == '_') asname[0] = 'Z';
#pragma convert(0)
    utfToStrLenUpper(lib, aslib, 10, 1);
#pragma exception_handler(EXCP1, excpData, 0, _C2_MH_ESCAPE, _CTLA_HANDLE)
    ptr = rslvsp(WLI_SRVPGM, asname, aslib, _AUTH_POINTER);
    return QleActBndPgm(&ptr, NULL, NULL, NULL, NULL);
 EXCP1:
    rmvmsg((_INTRPT_Hndlr_Parms_T *)&excpData);
    return 0;
}

/* get service program function pointer */
_OPENPTR getFunction(int actmark, char *name)
{
    volatile _INTRPT_Hndlr_Parms_T  excpData;
    _OPENPTR ptr;
    int expres;
    char asname[256];
#pragma exception_handler(EXCP1, excpData, 0, _C2_MH_ESCAPE, _CTLA_HANDLE)
    QleGetExp(&actmark, 0, 0, utfToStr(name, asname), &ptr, &expres, NULL);
    if (expres == 1)
        return ptr;
 EXCP1:
    return NULL;
}
