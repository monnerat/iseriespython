/* Single level storage model + EBCDIC wrappers to Python API. */
/* Author: Patrick Monnerat, Datasphere S.A. */

/* vim: set expandtab ts=4 sw=4: */

#include "Python.h"

#include <string.h>

#include <except.h>
#include <qusec.h>
#include <qmhsndpm.h>
#include <qtqiconv.h>
#include <iconv.h>

typedef struct _frame   PyFrameObject;  /* Avoid including frameobject.h */


typedef int     (*Py_tracefunc128)(PyObject * __ptr128 obj,
                                   PyFrameObject * __ptr128 frame,
                                   int what, PyObject * __ptr128 arg);


/* Holds 128 pointers for callback wrapper. */
typedef struct {
        int     (* __ptr128 func)();        /* Parameters unspecified. */
        void    * __ptr128  arg;
}       funcandarg;


/* EBCDIC strings. */
#pragma convert(37)
static char     qceemsg[] = "QCEEMSG   QSYS      ";
static char     cee0813[] = "CEE0813";
static char     qcpfmsg[] = "QCPFMSG   QSYS      ";
static char     cpf247e[] = "CPF247E";
static char     escape[] = "*ESCAPE   ";
static char     curstkntry[] = "*         ";
static char     i5ImportParam_name[] =
                        "i5ImportParam                                    ";
static char     i5ExportResult_name[] =
                        "i5ExportResult                                   ";
static char     PySys_SetArgvExCCSID_name[] =
                        "PySys_SetArgvExCCSID                             ";
static char     settraceprofile_name[] =
                        "settraceprofile                                  ";
#pragma convert(0)


/* UTF-8 conversion identifier. */
static QtqCode_T    utfQtqCode = { 1208 };


/* Ensure pointed data is in teraspace. Copy if needed. */
static void *
i5ImportParam(void * __ptr128 ptr, size_t len,
                                    int ccsid, void * __ptr128 *allocated)
{
    volatile _INTRPT_Hndlr_Parms_T  excpData;
    void * __ptr128 p;
    size_t outlen;
    char msgkey[4];
    Qus_EC_t error;
    int mustconvert = ccsid >= 0 && ccsid != 65535 && ccsid != 1208;

    if (!mustconvert || !ptr) {
#pragma exception_handler(move_to_teraspace, excpData,                      \
                          _C1_ALL, _C2_ALL, _CTLA_HANDLE_NO_MSG)
        return ptr;
#pragma disable_handler
    }

move_to_teraspace:
    if (!len)
        len = strlen(ptr) + 1;
    outlen = mustconvert? 4 * len: len;
    p = allocated? malloc(outlen): NULL;
    if (!p) {
        error.Bytes_Provided = 0;
        QMHSNDPM(cee0813, qceemsg, i5ImportParam_name, 50, escape, curstkntry,
                 1, msgkey, (void *) &error);
        /* NOTREACHED */
    }
    *allocated = p;
    if (!mustconvert)
        memcpy(p, ptr, len);
    else {
        size_t inlen;
        char * __ptr128 inp;
        char * __ptr128 outp;
        QtqCode_T incode;
        iconv_t cd;

        memset((void *) &incode, 0, sizeof incode);
        incode.CCSID = ccsid;
        cd = QtqIconvOpen(&utfQtqCode, &incode);
        if (cd.return_value == -1) {
            error.Bytes_Provided = 0;
            QMHSNDPM(cpf247e, qcpfmsg, &ccsid, 4, escape, curstkntry, 1,
                     msgkey, (void *) &error);
            /* NOTREACHED */
        }
        inlen = len;
        inp = ptr;
        outp = p;
        iconv(cd, &inp, &inlen, &outp, &outlen);
        iconv_close(cd);
    }
    return p;
}


/* Export pointed result into dynamic storage, converting CCSID if needed. */
static void *
i5ExportResult(void * ptr, size_t len, int ccsid, void * __ptr128 *allocated)
{
    volatile _INTRPT_Hndlr_Parms_T  excpData;
    void * p;
    size_t outlen;
    char msgkey[4];
    Qus_EC_t error;
    int mustconvert = ccsid >= 0 && ccsid != 65535 && ccsid != 1208;

    *allocated = NULL;
    if (!ptr)
        return ptr;
    if (!len)
        len = strlen(ptr) + 1;
    outlen = mustconvert? 4 * len: len;
    p = malloc(outlen);
    if (!p) {
        error.Bytes_Provided = 0;
        QMHSNDPM(cee0813, qceemsg, i5ExportResult_name, 50, escape, curstkntry,
                 1, msgkey, (void *) &error);
        /* NOTREACHED */
    }
    *allocated = p;
    if (!mustconvert)
        memcpy(p, ptr, len);
    else {
        size_t inlen;
        char * __ptr128 inp;
        char * __ptr128 outp;
        QtqCode_T outcode;
        iconv_t cd;

        memset((void *) &outcode, 0, sizeof outcode);
        outcode.CCSID = ccsid;
        cd = QtqIconvOpen(&outcode, &utfQtqCode);
        if (cd.return_value == -1) {
            error.Bytes_Provided = 0;
            QMHSNDPM(cpf247e, qcpfmsg, &ccsid, 4, escape, curstkntry, 1,
                     msgkey, (void *) &error);
            /* NOTREACHED */
        }
        inlen = len;
        inp = ptr;
        outp = p;
        iconv(cd, &inp, &inlen, &outp, &outlen);
        iconv_close(cd);
    }
    return p;
}


/* Support for cleanout on exception. */

#define dynamicPointers(n)  struct {                                        \
                                size_t          count;                      \
                                void * __ptr128 pointers[n];                \
                            }
typedef dynamicPointers(1)  dynPtrs;
typedef dynPtrs * __ptr128  dynPtrsPtr;


static void
i5SafeFree(void *p)
{
    if (p)
        free(p);
}

static void
cleanout_dynptrs(dynPtrsPtr p)
{
    size_t i;

    if (p)
        for (i = p->count; i--;)
            i5SafeFree(p->pointers[i]);
}

static void
cleanout_handler(volatile _INTRPT_Hndlr_Parms_T * __ptr128 excpData)
{
    cleanout_dynptrs((dynPtrsPtr) excpData->Com_Area);
}

static int
pendingcallwrapper(void *tsarg)
{
    funcandarg *fna = (funcandarg *) tsarg;
    int (* __ptr128 func)(void * __ptr128) = fna->func;
    void * __ptr128 arg = fna->arg;

    free(tsarg);
    return (*func)(arg);
}

static int
tracefuncwrapper(PyObject *caps, PyFrameObject *frame, int what, PyObject *arg)
{
    funcandarg *fna = (funcandarg *) PyCapsule_GetPointer(caps, NULL);
    Py_tracefunc128 func = (Py_tracefunc128) fna->func;
    PyObject *obj = (PyObject *) fna->arg;

    return (func)(obj, frame, what, arg);
}

static void
freecaps(PyObject *caps)
{
    funcandarg *fna = (funcandarg *) PyCapsule_GetPointer(caps, NULL);
    PyObject *obj = (PyObject *) fna->arg;

    Py_XDECREF(obj);
    free((void *) fna);
}

static void
settraceprofile(Py_tracefunc128 func, PyObject *obj,
                void (*setfunc)(Py_tracefunc, PyObject *))
{
    funcandarg *fna;
    char msgkey[4];
    Qus_EC_t error;
    PyObject *caps;

    if (!func)
        (*setfunc)(NULL, NULL);
    else {
        fna = malloc(sizeof *fna);
        if (!fna) {
            error.Bytes_Provided = 0;
            QMHSNDPM(cee0813, qceemsg, settraceprofile_name, 50, escape,
                     curstkntry, 1, msgkey, (void *) &error);
        /* NOTREACHED */
        }
        else {
            fna->func = (int (* __ptr128)(void * __ptr128)) func;
            fna->arg = (void *) obj;
            Py_INCREF(obj);
            caps = PyCapsule_New((void *) fna, NULL, freecaps);
            (*setfunc)(tracefuncwrapper, caps);
        }
    }
}

/* For those languages not having macros, implement them as functions. */

#undef Py_XDECREF
void
Py_XDECREF(PyObject *o)
{
    if (o)
        Py_DECREF(o);
}

#undef Py_XINCREF
void
Py_XINCREF(PyObject *o)
{
    if (o)
        Py_INCREF(o);
}

#undef PyArg_NoArgs
int
PyArg_NoArgs(PyObject * args)
{
    return PyArg_Parse(args, "");
}


/* The 128-bit interface.
 *
 *   The following structures are supposed to always be allocated by the
 * Python run-time and thus are not copied to teraspace:
 *      PyObject
 *      PyFrameObject
 *      PyInterpreterState
 *      PyThreadState
 *   Other pointers are checked for pointing to teraspace and, if not, addressed
 * objects are temporarily copied to teraspace.
 */

#pragma datamodel(P128)


/* EBCDIC wrappers, 128-bit pointers only. */

void
Py_SetProgramNameCCSID(char * name, int ccsid)
{
    dynamicPointers(1) dptrs;

    memset((void *) &dptrs, 0, sizeof dptrs);
    dptrs.count = 1;
#pragma exception_handler(cleanout_handler, dptrs,                          \
                          _C1_ALL, _C2_ALL, _CTLA_INVOKE)
    Py_SetProgramName(i5ImportParam((void *) name, 0, ccsid, dptrs.pointers));
#pragma disable_handler

    cleanout_dynptrs((dynPtrsPtr) &dptrs);
}

static char *
getStringVoidCCSID(char *s, int ccsid)
{
    dynamicPointers(1) dptrs;
    char * result;

    dptrs.pointers[0] = NULL;
    dptrs.count = 1;
#pragma exception_handler(cleanout_handler, dptrs,                          \
                          _C1_ALL, _C2_ALL, _CTLA_INVOKE)
    return (char *) i5ExportResult(s, 0, ccsid, dptrs.pointers);
#pragma disable_handler
}

#define GETSTRINGVOIDCCSID(name, cst)                                       \
    cst char *name##CCSID(int ccsid)                                        \
    {                                                                       \
        return (cst char *) getStringVoidCCSID((char *) name(), ccsid);     \
    }

GETSTRINGVOIDCCSID(Py_GetProgramName,)
GETSTRINGVOIDCCSID(Py_GetPrefix,)
GETSTRINGVOIDCCSID(Py_GetExecPrefix,)
GETSTRINGVOIDCCSID(Py_GetProgramFullPath,)
GETSTRINGVOIDCCSID(Py_GetPath,)
GETSTRINGVOIDCCSID(Py_GetVersion, const)
GETSTRINGVOIDCCSID(Py_GetPlatform, const)
GETSTRINGVOIDCCSID(Py_GetCopyright, const)
GETSTRINGVOIDCCSID(Py_GetCompiler, const)
GETSTRINGVOIDCCSID(Py_GetBuildInfo, const)

void
PySys_SetArgvExCCSID(int argc, char **argv, int ccsid, int updatepath)
{
    dynPtrsPtr dptrs;
    char * __ptr64 *argv64 = NULL;
    size_t sz;
    Qus_EC_t error;
    char msgkey[4];

    sz = sizeof *dptrs + (argc + 2) * sizeof dptrs->pointers[0];
    dptrs = malloc(sz);
    if (dptrs) {
        memset((void *) dptrs, 0, sz);
        dptrs->count = argc + 2;
        dptrs->pointers[0] = dptrs;
        argv64 = malloc((argc + 1) * sizeof *argv64);
        dptrs->pointers[1] = argv64;
    }
    if (!argv64) {
        cleanout_dynptrs(dptrs);
        error.Bytes_Provided = 0;
        QMHSNDPM(cee0813, qceemsg, PySys_SetArgvExCCSID_name, 50, escape,
                 curstkntry, 1, msgkey, (void *) &error);
        /* NOTREACHED */
    }
#pragma exception_handler(cleanout_handler, dptrs,                          \
                          _C1_ALL, _C2_ALL, _CTLA_INVOKE)
    for (sz = 0; sz < argc; sz++)
        argv64[sz] = i5ImportParam((void *) argv[sz], 0, ccsid,
                                   dptrs->pointers + sz + 2);
    PySys_SetArgvEx(argc, argv64, updatepath);
#pragma disable_handler

    cleanout_dynptrs(dptrs);
}

void
PySys_SetArgvCCSID(int argc, char **argv, int ccsid)
{
    PySys_SetArgvExCCSID(argc, argv, -1, 1);
}

void
Py_SetPythonHomeCCSID(char *home, int ccsid)
{
    dynamicPointers(1) dptrs;

    memset((void *) &dptrs, 0, sizeof dptrs);
    dptrs.count = 1;
#pragma exception_handler(cleanout_handler, dptrs,                          \
                          _C1_ALL, _C2_ALL, _CTLA_INVOKE)
    Py_SetPythonHome(i5ImportParam((void *) home, 0, ccsid, dptrs.pointers));
#pragma disable_handler

    cleanout_dynptrs((dynPtrsPtr) &dptrs);
}

GETSTRINGVOIDCCSID(Py_GetPythonHome,)

int
PyRun_SimpleStringFlagsCCSID(const char *command, int ccsid,
                             PyCompilerFlags *flags)
{
    dynamicPointers(2) dptrs;
    int result;

    memset((void *) &dptrs, 0, sizeof dptrs);
    dptrs.count = 2;
#pragma exception_handler(cleanout_handler, dptrs,                          \
                          _C1_ALL, _C2_ALL, _CTLA_INVOKE)
    result = PyRun_SimpleStringFlags(i5ImportParam((void *) command, 0, ccsid,
                                                   dptrs.pointers),
                                     i5ImportParam((void *) flags,
                                                   sizeof *flags, -1,
                                                   dptrs.pointers + 1));
#pragma disable_handler

    if (dptrs.pointers[1])
        memcpy((void *) flags, (void *) dptrs.pointers[1], sizeof *flags);

    cleanout_dynptrs((dynPtrsPtr) &dptrs);
    return result;
}

int
PyRun_SimpleStringCCSID(const char *command, int ccsid)
{
    return PyRun_SimpleStringFlagsCCSID(command, ccsid, NULL);
}

PyObject *
PyString_FromStringCCSID(const char *v, int ccsid)
{
    dynamicPointers(1) dptrs;
    PyObject *result;

    memset((void *) &dptrs, 0, sizeof dptrs);
    dptrs.count = 1;
#pragma exception_handler(cleanout_handler, dptrs,                          \
                          _C1_ALL, _C2_ALL, _CTLA_INVOKE)
    result = PyString_FromString(i5ImportParam((void *) v, 0, ccsid,
                                                   dptrs.pointers));
#pragma disable_handler

    cleanout_dynptrs((dynPtrsPtr) &dptrs);
    return result;
}

PyObject *
PyObject_GetAttrStringCCSID(PyObject *o, const char *attr_name, int ccsid)
{
    dynamicPointers(1) dptrs;
    PyObject *result;

    memset((void *) &dptrs, 0, sizeof dptrs);
    dptrs.count = 1;
#pragma exception_handler(cleanout_handler, dptrs,                          \
                          _C1_ALL, _C2_ALL, _CTLA_INVOKE)
    result = PyObject_GetAttrString(o, i5ImportParam((void *) attr_name,
                                                     0, ccsid, dptrs.pointers));
#pragma disable_handler

    cleanout_dynptrs((dynPtrsPtr) &dptrs);
    return result;
}



/* The 128-bit pointer wrappers. */
/* They all have a _SLS_ prefix (Single-level safe). */

#define SLSGETSTRINGVOID(name, cst)                                         \
    cst char *_SLS_##name(void)                                             \
    {                                                                       \
        return (cst char *) name();                                         \
    }

void
_SLS_Py_SetProgramName(char *name)
{
    Py_SetProgramNameCCSID(name, -1);
}

SLSGETSTRINGVOID(Py_GetProgramName,)
SLSGETSTRINGVOID(Py_GetPrefix,)
SLSGETSTRINGVOID(Py_GetExecPrefix,)
SLSGETSTRINGVOID(Py_GetProgramFullPath,)
SLSGETSTRINGVOID(Py_GetPath,)
SLSGETSTRINGVOID(Py_GetVersion, const)
SLSGETSTRINGVOID(Py_GetPlatform, const)
SLSGETSTRINGVOID(Py_GetCopyright, const)
SLSGETSTRINGVOID(Py_GetCompiler, const)
SLSGETSTRINGVOID(Py_GetBuildInfo, const)

void
_SLS_PySys_SetArgvEx(int argc, char **argv, int updatepath)
{
    PySys_SetArgvExCCSID(argc, argv, -1, updatepath);
}

void
_SLS_PySys_SetArgv(int argc, char **argv)
{
    PySys_SetArgvExCCSID(argc, argv, -1, 1);
}

void
_SLS_Py_SetPythonHome(char *home)
{
    Py_SetPythonHomeCCSID(home, -1);
}

SLSGETSTRINGVOID(Py_GetPythonHome,)

PyThreadState *
_SLS_PyEval_SaveThread(void)
{
    return PyEval_SaveThread();
}

void
_SLS_PyEval_RestoreThread(PyThreadState *tstate)
{
    PyEval_RestoreThread(tstate);
}

PyThreadState *
_SLS_PyThreadState_Get(void)
{
    return PyThreadState_Get();
}

PyThreadState *
_SLS_PyThreadState_Swap(PyThreadState *tstate)
{
    return PyThreadState_Swap(tstate);
}

PyThreadState *
_SLS_PyGILState_GetThisThreadState(void)
{
    return PyGILState_GetThisThreadState();
}

PyInterpreterState *
_SLS_PyInterpreterState_New(void)
{
    return PyInterpreterState_New();
}

void
_SLS_PyInterpreterState_Clear(PyInterpreterState *interp)
{
    PyInterpreterState_Clear(interp);
}

void
_SLS_PyInterpreterState_Delete(PyInterpreterState *interp)
{
    PyInterpreterState_Delete(interp);
}

PyThreadState *
_SLS_PyThreadState_New(PyInterpreterState * interp)
{
    return PyThreadState_New(interp);
}

void
_SLS_PyThreadState_Clear(PyThreadState *tstate)
{
    PyThreadState_Clear(tstate);
}

void
_SLS_PyThreadState_Delete(PyThreadState *tstate)
{
    PyThreadState_Delete(tstate);
}

PyObject *
_SLS_PyThreadState_GetDict(void)
{
    return PyThreadState_GetDict();
}

int
_SLS_PyThreadState_SetAsyncExc(long id, PyObject *exc)
{
    return PyThreadState_SetAsyncExc(id, exc);
}

void
_SLS_PyEval_AcquireThread(PyThreadState *tstate)
{
    PyEval_AcquireThread(tstate);
}

void
_SLS_PyEval_ReleaseThread(PyThreadState *tstate)
{
    PyEval_ReleaseThread(tstate);
}

PyThreadState *
_SLS_Py_NewInterpreter(void)
{
    return Py_NewInterpreter();
}

void
_SLS_Py_EndInterpreter(PyThreadState *tstate)
{
    Py_EndInterpreter(tstate);
}

int
_SLS_Py_AddPendingCall(int (*func)(void *), void *arg)
{
    funcandarg *fna;
    int i;

    if (!func)
        return -1;
    fna = malloc(sizeof *fna);
    if (!fna)
        return -1;
    fna->func = func;
    fna->arg = arg;
    i = Py_AddPendingCall(pendingcallwrapper, (void *) fna);
    if (i)
        free(fna);
    return i;
}

void
_SLS_PyEval_SetProfile(Py_tracefunc128 func, PyObject *obj)
{
    settraceprofile(func, obj, PyEval_SetProfile);
}

void
_SLS_PyEval_SetTrace(Py_tracefunc128 func, PyObject *obj)
{
    settraceprofile(func, obj, PyEval_SetTrace);
}

PyObject *
_SLS_PyEval_GetCallStats(PyObject *self)
{
    return PyEval_GetCallStats(self);
}

PyInterpreterState *
_SLS_PyInterpreterState_Head()
{
    return PyInterpreterState_Head();
}

PyInterpreterState *
_SLS_PyInterpreterState_Next(PyInterpreterState *interp)
{
    return PyInterpreterState_Next(interp);
}

PyThreadState *
_SLS_PyInterpreterState_ThreadHead(PyInterpreterState *interp)
{
    return PyInterpreterState_ThreadHead(interp);
}

PyThreadState *
_SLS_PyThreadState_Next(PyThreadState *tstate)
{
    return PyThreadState_Next(tstate);
}

int
_SLS_PyRun_SimpleStringFlags(const char *command, PyCompilerFlags *flags)
{
    return PyRun_SimpleStringFlagsCCSID(command, -1, flags);
}

int
_SLS_PyRun_SimpleString(const char *command)
{
    return PyRun_SimpleStringFlagsCCSID(command, -1, NULL);
}

PyObject *
_SLS_PyString_FromString(const char *v)
{
    return PyString_FromStringCCSID(v, -1);
}

PyObject *
_SLS_PyImport_Import(PyObject *name)
{
    return PyImport_Import(name);
}

PyObject *
_SLS_PyObject_GetAttrString(PyObject *o, const char *attr_name)
{
    return PyObject_GetAttrStringCCSID(o, attr_name, -1);
}

void
_SLS_Py_DecRef(PyObject *o)
{
    Py_DecRef(o);
}

void
_SLS_Py_IncRef(PyObject *o)
{
    Py_IncRef(o);
}

void
_SLS_Py_XDECREF(PyObject *o)
{
    Py_XDECREF(o);
}

void
_SLS_Py_XINCREF(PyObject *o)
{
    Py_XINCREF(o);
}

int
_SLS_PyCallable_Check(PyObject *o)
{
    return PyCallable_Check(o);
}

PyObject *
_SLS_PyTuple_New(Py_ssize_t size)
{
    return PyTuple_New(size);
}

PyObject *
_SLS_PyInt_FromLong(long ival)
{
    return PyInt_FromLong(ival);
}

int
_SLS_PyTuple_SetItem(PyObject *p, Py_ssize_t pos, PyObject *o)
{
    return PyTuple_SetItem(p, pos, o);
}

PyObject *
_SLS_PyObject_CallObject(PyObject *callable_object, PyObject *args)
{
    return PyObject_CallObject(callable_object, args);
}

PyObject *
_SLS_PyErr_Occurred(void)
{
    return PyErr_Occurred();
}

char *
_SLS_PyString_AsString(PyObject *string)
{
    return PyString_AsString(string);
}


PyObject *
_SLS_Py_Mangle(PyObject *p, PyObject *name)
{
    return _Py_Mangle(p, name);
}

int
_SLS_PyArg_NoArgs(PyObject * args)
{
    return PyArg_NoArgs(args);
}


#pragma datamodel(pop)
