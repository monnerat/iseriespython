#ifndef _OS400_H
#define _OS400_H
#include <as400misc.h>

typedef struct {
    char type;
    int  len;
    int  digits;
    int  dec;
    int  offset;
    void * __ptr128 ptr;
    PyObject *obj;
} parmtype_t;

typedef struct {
    PyObject_HEAD
    char  name[11];
    char  lib[11];
    int   parmCount;
    parmtype_t  types[10];
    _SYSPTR ptr;
} OS400Program;

typedef struct {
    PyObject_HEAD
    char  name[11];
    char  lib[11];
    _SYSPTR ptr;
    int   actmark;
} OS400Srvpgm;

typedef struct {
    PyObject_HEAD
    void * __ptr128 obj;
    void (*destructor)(void * __ptr128);
} OS400ProcReturn;

typedef struct {
    PyObject_HEAD
    char name[256];
    int   parmCount;
    parmtype_t  retval;
    parmtype_t  types[10];
    OS400Srvpgm *srvpgm;
    _OPENPTR ptr;
} OS400Proc;

typedef struct {
    PyObject_HEAD
    char  name[15];
    int  ccsid;
    iconv_t  cdencode;
    iconv_t  cddecode;
} OS400Codec;

extern DL_IMPORT(PyTypeObject) OS400Program_Type;
extern DL_IMPORT(PyTypeObject) OS400Srvpgm_Type;
extern DL_IMPORT(PyTypeObject) OS400ProcReturn_Type;
extern DL_IMPORT(PyTypeObject) OS400Proc_Type;
extern DL_IMPORT(PyTypeObject) OS400Codec_Type;

#define OS400Program_Check(v)   ((v)->ob_type == &OS400Program_Type)
#define OS400Srvpgm_Check(v)    ((v)->ob_type == &OS400Srvpgm_Type)
#define OS400ProcReturn_Check(op) ((op)->ob_type == &OS400ProcReturn_Type)
#define OS400Proc_Check(v)  ((v)->ob_type == &OS400Proc_Type)
#define OS400Codec_Check(v)  ((v)->ob_type == &OS400Codec_Type)

#endif






