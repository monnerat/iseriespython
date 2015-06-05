/*
 * i5pgm
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
 * appear in supporting documentation, and that the name of FIGU DATA AS
 * or the author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 * 
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL FIGU DATA AS OR THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *--------------------------------------------------------------------*/

#include "Python.h"
#include <qusec.h>
#include <qmhsndpm.h>
#include <qusrjobi.h>
#include "as400misc.h"

static PyObject *initdict = NULL;
static PyObject *hdict = NULL;
static int hcounter = 0;
static int invcounter = 0;

/* Initialize the program call */
void
close_pgm(void)
{
    invcounter -= 1;
    if (invcounter <= 0)
    {
        Py_XDECREF(initdict);
        initdict = NULL;
        hcounter = 0;
        Py_XDECREF(hdict);
        hdict = NULL;
        invcounter = 0;
    }
}

void
init_pgm(PyObject *idict)
{
    if (invcounter == 0)
    {
        hdict = PyDict_New();
        if (idict != NULL)
            Py_INCREF(idict);
        initdict = idict;
        hcounter = 1;
    }
    invcounter += 1;
}

#pragma datamodel(P128)

PyObject *
getObjectFromHandle(int handle)
{
    if (handle == 1)
        return initdict;
    else {
        PyObject *v, *o;
        v = PyInt_FromLong(handle);
        o = PyDict_GetItem(hdict, v);
        Py_DECREF(v);
        return o;
    }
}

int
newHandle(PyObject *obj)
{
    hcounter += 1;
    PyDict_SetItem(hdict, PyInt_FromLong(hcounter), obj);
    return hcounter;
}

void
initpython()
{
    /*
     * Getting job info and override if interactive job
     * or set QIBM_QSH_CMD_OUTPUT
     * Not realy needed
     *
    struct Qwc_JOBI0100 jobinfo;
    char qp1[9], qp2[27], qp3[17]; 
    Qus_EC_t error;
    char s[128];
    error.Bytes_Provided = sizeof(error);
    QUSRJOBI(&jobinfo, sizeof(jobinfo), utfToStrLen("JOBI0100",qp1,8,1),
             utfToStrLen("*",qp2,26,1), utfToStrLen(" ",qp3,16,1), &error);
    if (strToUtf(jobinfo.Job_Type, s)[0] == 'I')
    {
        system("ovrdbf stdout qprint ovrscope(*actgrpdfn)");
        system("ovrdbf stderr qprint ovrscope(*actgrpdfn)");
    }
    char *p;
    time_t ltime;
    struct tm *ts;
    ltime = time(&ltime);
    ts = localtime(&ltime);
    p = (char *)s;
    p += strftime(p, sizeof(s), "QIBM_QSH_CMD_OUTPUT=/var/pylog/%Y%m%d_", ts);
    utfToStr(s, s);
    strncat(s + strlen(s), jobinfo.Job_Number, 6);
    putenv(s);
     *
     */

    system("ovrdbf stdout qprint ovrscope(*actgrpdfn)");
    system("ovrdbf stderr qprint ovrscope(*actgrpdfn)");

    Py_SetProgramName("PYTHON27/PYTHON");
    Py_Initialize();
    init_pgm(NULL);
}

void
closepython()
{
    close_pgm();
}

/* send escape message */    
static void
error_msg(char *s)
{
    char errmsgkey[4];
    char errmsg[101];
    Qus_EC_t error;

    utfToStrLen(s, errmsg, 100, 1);
#pragma convert(37)
    error.Bytes_Provided = sizeof(error);
    QMHSNDPM("CPF9898", "QCPFMSG   *LIBL     ", errmsg, strlen(errmsg),
           "*INFO     ", "*PGMBDY   ", 0, errmsgkey, &error); 
#pragma convert(0)
    return;
}

static PyObject *
from_json(PyObject *s) {
    PyObject *mod, *dict; 
    PyRun_SimpleString("import json");
    mod = PyImport_AddModule("__main__");
    dict = PyModule_GetDict(mod);
    PyDict_SetItemString(dict, "s", s);
    PyRun_SimpleString("plist = json.loads(s)");
    return PyDict_GetItemString(dict, "plist");
}

/* Run python statement.
 * impstr: "from module_a import func_1"
 * stmt:   "x = func1()"
 * rtnvar: "x"
 */
int
runstatement_ts(const char *impstr, const char *stmt, const char *rtnvar)
{
    PyObject *sobj, *mod, *dict, *result; 
    int r;

    /* Create __main___ module */
    mod = PyImport_AddModule("__main__");
    dict = PyModule_GetDict(mod);
    /* run import string */
    if (impstr != NULL && strlen(impstr) > 0) {
        sobj = strLenToUtfPy((char *)impstr, strlen(impstr));
        r = PyRun_SimpleString(PyString_AS_STRING(sobj));
        Py_XDECREF(sobj);
        if (r == -1)
            return -1;
    }
    /* run statements */
    if (stmt != NULL && strlen(stmt) > 0) {
        sobj = strLenToUtfPy((char *)stmt, strlen(stmt));
        r = PyRun_SimpleString(PyString_AS_STRING(sobj));
        Py_XDECREF(sobj);
        if (r == -1)
            return -1;
    }
    /* return variable */
    if (rtnvar != NULL && strlen(rtnvar) > 0) {
        sobj = strLenToUtfPy((char *)rtnvar, strlen(rtnvar));
        result = PyDict_GetItem(dict, sobj);
        if (result == NULL)
            return -1;
        return newHandle(result);
    }
    return 0;
}

int
runpython_ts(const char *modname, const char * clsname, const char * funcname, const int paramhandle)
{
    PyObject *modstr, *mod, *clsstr, *cls, *funcstr, *func, *plist, *args, *retval; 
    /* convert module, class and func to utf-8 */
    modstr = strLenToUtfPy((char *)modname, strlen(modname));
    plist = getObjectFromHandle(paramhandle);
    /* parameters */
    if (PyString_Check(plist)) {
        /* parameter should be a json string */
        plist = from_json(plist); 
        if (plist == NULL) {
            error_msg("Error in json parameter string");
            return -1;
        }
    }
    if (!PyList_Check(plist)) {
        error_msg("Error in runpython: parameters must be a list");
        return -1;
    }
    /* import modules */
    mod = PyImport_Import(modstr);
    if (mod == NULL)
    {
        Py_DECREF(modstr);
        error_msg("Error in runpython: Module not found");
        return -1;
    }
    if (clsname != NULL)
        clsstr = strLenToUtfPy((char *)clsname, strlen(clsname));
    funcstr = strLenToUtfPy((char *)funcname, strlen(funcname));
    if (clsname != NULL && strlen(clsname) > 0)
    {
        cls = PyObject_GetAttr(mod, clsstr);
        if (cls != NULL)
            func = PyObject_GetAttr(cls, funcstr);
    } else {
        func = PyObject_GetAttr(mod, funcstr);
    }
    if (!func || !PyCallable_Check(func))
    {
        Py_XDECREF(cls);
        Py_XDECREF(mod);
        error_msg("Error in runpython: Function not found");
        return -1;
    }
    args = PyList_AsTuple(plist);
    retval = PyObject_CallObject(func, args);
    Py_XDECREF(args);
    Py_XDECREF(func);
    Py_XDECREF(funcstr);
    Py_XDECREF(cls);
    Py_XDECREF(clsstr);
    Py_XDECREF(mod);
    Py_XDECREF(modstr);
    if (!retval) {
        PyErr_Print();
        error_msg("Error in runpython: See QPRINT for details");
        return -1;
    }
    /* keep the return value */
    return newHandle(retval);
}

int
getparams(void)
{
    PyObject *o = PyDict_GetItemString(initdict, "params");
    return newHandle(o);
}

void
setresult(const int objhandle)
{
    PyObject *o = getObjectFromHandle(objhandle);
    PyDict_SetItemString(initdict, "result", o);
}

void
seterror(const int objhandle)
{
    PyObject *o = getObjectFromHandle(objhandle);
    PyDict_SetItemString(initdict, "error", o);
}

int
dictnew(void)
{
    PyObject *v = PyDict_New();
    return newHandle(v);
}

int
dictget_ts(const int handle, const char * key)
{
    PyObject *v;
    PyObject *obj = getObjectFromHandle(handle);
    PyObject *k = strLenToUtfPy((char *)key, strlen(key));
    v = PyDict_GetItem(obj, k);
    Py_DECREF(k);
    if (v == NULL)
        return 0;
    Py_INCREF(v);
    return newHandle(v);
}

void
dictadd_ts(const int dicthandle, const char *key, const int objhandle)
{
    PyObject *d = getObjectFromHandle(dicthandle);
    PyObject *o = getObjectFromHandle(objhandle);
    PyObject *k = strLenToUtfPy((char *)key, strlen(key));
    PyDict_SetItem(d, k, o);
    Py_DECREF(k);
}

int
dictkeys(const int handle)
{
    PyObject *v;
    PyObject *obj = getObjectFromHandle(handle);
    v = PyDict_Keys(obj);
    return newHandle(v);
}

char *
handletype(const int handle)
{
    PyObject *obj = getObjectFromHandle(handle);
    if (PyList_Check(obj) || PyTuple_Check(obj))
        return "list";
    else if PyDict_Check(obj)
        return "dict";
    else if PyString_Check(obj)
        return "string";
    else if PyInt_Check(obj)
        return "int";
    else if PyFloat_Check(obj)
        return "float";
    else
        return "";
}

int
stringnew_ts(const char *ns)
{
    PyObject *s = strLenToUtfPy((char *)ns, strlen(ns));
    return newHandle(s);
}

char *
stringbuffer(const int handle)
{
    PyObject *obj = getObjectFromHandle(handle);
    return PyString_AsString(obj);
}

int
stringlen(const int handle)
{
    PyObject *obj = getObjectFromHandle(handle);
    return PyString_Size(obj);
}

char *
stringget(const int handle)
{
    int len;
    PyObject *s, *obj = getObjectFromHandle(handle);
    if (!PyString_Check(obj))
        obj = PyObject_Str(obj);
    len = PyString_Size(obj);
    s = PyString_FromStringAndSize(NULL, len);
    /* save reference */
    newHandle(s);
    return utfToStr(PyString_AsString(obj), PyString_AsString(s));
}


int
listnew(void)
{
    PyObject *o = PyList_New(0);
    return newHandle(o);
}

int
listlen(const int handle)
{
    PyObject *obj = getObjectFromHandle(handle);
    if (PyTuple_Check(obj))
        return PyTuple_Size(obj);
    return PyList_Size(obj);
}

int
listget(const int handle, const int pos)
{
    PyObject *v;
    PyObject *obj = getObjectFromHandle(handle);
    if (PyTuple_Check(obj))
        v = PyTuple_GetItem(obj, pos);
    else
        v = PyList_GetItem(obj, pos);
    Py_INCREF(v);
    return newHandle(v);
}

void
listadd(const int handle, const int objhandle)
{
    PyObject *v;
    PyObject *l = getObjectFromHandle(handle);
    PyObject *o = getObjectFromHandle(objhandle);
    PyList_Append(l, o);
}

int
intnew(const int v)
{
    return newHandle(PyInt_FromLong(v));
}

int
intget(const int handle)
{
    PyObject *obj = getObjectFromHandle(handle);
    return PyInt_AsLong(obj);
}

int
floatnew(const double v)
{
    return newHandle(PyFloat_FromDouble(v));
}

double
floatget(const int handle)
{
    PyObject *obj = getObjectFromHandle(handle);
    return PyFloat_AsDouble(obj);
}

#pragma datamodel(pop)


