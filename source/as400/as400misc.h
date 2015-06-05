#ifndef AS400MISC_H
#define AS400MISC_H
#include <python.h>
#include <qleawi.h>
#include <iconv.h>

extern DL_IMPORT(iconv_t)getConvDesc(int fromccsid, int toccsid);
extern DL_IMPORT(int)convertstr(iconv_t cd, const char *in, int in_size, char *out, int out_size);
extern DL_IMPORT(PyObject *)convertString(iconv_t cd, const char *in, int size);
extern DL_IMPORT(PyObject *)ebcdicToString(int fromccsid, const char *in, int size);
extern DL_IMPORT(PyObject *)ebcdicToUnicode(int fromccsid, const char *in, int size);
extern DL_IMPORT(int)ebcdicToStringBuffer(int fromccsid, const char *in, int size, char *out);
extern DL_IMPORT(char *)stringToEbcdic(int toccsid, const char *in, int in_size, char *out, int out_size);
extern DL_IMPORT(char *)unicodeToEbcdic(int toccsid, const char *in, int in_size, char *out, int out_size);
extern DL_IMPORT(int)ebcdicLen(const char *buf, int len);
extern DL_IMPORT(char *)strToUtf(char *in, char *out, int maxlen);
extern DL_IMPORT(int)strLenToUtf(char *in, int len, char *out);
extern DL_IMPORT(PyObject *)strLenToUtfPy(char *in, int len);
extern DL_IMPORT(char *)utfToStr(char *in, char *out);
extern DL_IMPORT(char *)utfToStrLen(char *in, char *out, int len, int add_null);
extern DL_IMPORT(char *)utfToStrLenUpper(char *in, char *out, int len, int add_null);
extern DL_IMPORT(char *) strtoupper(char *str);
extern DL_IMPORT(char *) zonedtostr(char *buf, char *p, int digits, int dec, char decsep);
extern DL_IMPORT(char *) packedtostr(char *buf, char *p, int digits, int dec, char decsep);
extern DL_IMPORT(char *) getenv_as(const char *);
extern DL_IMPORT(char *) getpythonpath(void);
extern DL_IMPORT(char *) getpythonhome(void);
extern DL_IMPORT(_SYSPTR) getPgm(char *name, char *lib);
extern DL_IMPORT(int) getSrvpgm(char *name, char *lib);
extern DL_IMPORT(_SYSPTR) getFunction(int actmark, char *name);


#endif /* AS400MISC.H */
