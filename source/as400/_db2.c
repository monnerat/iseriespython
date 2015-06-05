/*
 * _db2  Database access
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
#include <sqlcli.h>
#define STR_LEN 255

/* Error Objects */
static PyObject *dbError;
static PyObject *dbWarning;

/* Factory functions for date/time etc */
static PyObject * DateFunc = NULL;
static PyObject * TimeFunc = NULL;
static PyObject * DatetimeFunc = NULL;
static PyObject * FieldtypeFunc = NULL;

/* Field information structure */
typedef struct {
	char        name[STR_LEN];
    SQLSMALLINT type;
    SQLINTEGER  prec;
    SQLSMALLINT scale;
    SQLSMALLINT nulls;
    int         ctype;
	int         offset;
	int         size;
} fieldInfoStruct;

/* Parameter structure */
typedef struct {
    SQLSMALLINT type;
    SQLINTEGER  prec;
    SQLSMALLINT scale;
    SQLSMALLINT nulls;
    SQLINTEGER  ctype;
    SQLPOINTER  data;
    PyObject    *obj;
	SQLINTEGER  size;
} paramInfoStruct;

/* Connection object */
typedef struct {
    PyObject_HEAD
    SQLHENV henv;
    SQLHDBC hdbc;
    SQLRETURN rc;
    char *dsn;
    char *user;
    char *pwd;
    char *database;
} ConnectionObject;

/* Cursor description Object */
typedef struct {
    PyObject_HEAD
    fieldInfoStruct *fieldArr;
    SQLSMALLINT numCols;
    int totalsize;
} CursordescObject;

/* row Object */
typedef struct {
    PyObject_VAR_HEAD
    int       conv_utf16;
    CursordescObject *curdesc;
	char      buffer[1];
} RowObject;

/* Cursor Object */
typedef struct {
    PyObject_HEAD
    PyObject *con;
    PyObject *stmt;
    SQLHSTMT hstmt;
    CursordescObject *curdesc;
    paramInfoStruct *paraminfo;
    PyObject **paramobj;
    int     arraysize;
    int     conv_utf16;
    SQLINTEGER rowcount;
    SQLSMALLINT numCols;
    SQLSMALLINT numParams;
	RowObject    *row;
    PyObject *buflist;
} CursorObject;

extern PyTypeObject Connection_Type;
extern PyTypeObject Cursordesc_Type;
extern PyTypeObject Row_Type;
extern PyTypeObject Cursor_Type;

#define ConnectionObject_Check(v) ((v)->ob_type == &Connection_Type)
#define CursordescObject_Check(v) ((v)->ob_type == &Cursordesc_Type)
#define RowObject_Check(v) ((v)->ob_type == &Row_Type)
#define CursorObject_Check(v) ((v)->ob_type == &Cursor_Type)

static PyObject *
f_error(SQLSMALLINT htype, SQLINTEGER handle) {
    SQLINTEGER  errorcode;
    SQLCHAR     state[5];
    SQLCHAR     msg[STR_LEN];
    SQLSMALLINT msgLen;
    char errorstr[STR_LEN];
    SQLRETURN rc;
    rc = SQLGetDiagRec(htype, handle, (SQLSMALLINT)1, state,
            &errorcode, msg, sizeof(msg), &msgLen);
    if ( rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
        msg[msgLen] = '\0';
        sprintf(errorstr, "SQLState: %s, Error code: %d\n%s",
            state, (int)errorcode, (char *)msg);
    } else {
        strcpy(errorstr, "No error information found.");
    }
    PyErr_SetString(dbError, errorstr);
    return NULL;
}

/* connection methods */

char connection_doc[] =
"Connection([dsn, user, pwd, database, autocommit, sysnaming, servermode]) -> Connection object\n\
\n\
Creates a new Connection object. All parameters are optional.\n\
Use the command dsprdbdire if you don't know the name to use for dsn.\n\
database , default library (not valid with sysnaming).\n\
autocommit (default = False), statments are committed as it is executed.\n\
sysnaming (default = False), system naming mode, use of *LIBL and / delimiter.\n\
servermode (default = False), run all sql commands in a server job,\n\
this is required if you need more than one open connection to the same dsn.";

static PyObject*
connection_new(PyTypeObject *type, PyObject *args, PyObject *keywds)
{
    ConnectionObject *o;
	o = (ConnectionObject *)(type->tp_alloc(type, 0));
    if (o != NULL) {
        o->dsn = NULL;
        o->user = NULL;
        o->pwd = NULL;
        o->database = NULL;
    }
    return (PyObject *)o;
}

static int
connection_init(PyObject *self, PyObject *args, PyObject *keywds)
{
    char    *dsn = NULL;
    char    *user = NULL;
    char    *pwd = NULL;
    char    *database = NULL;
    int     autocommit = 0;
    int     sysnaming = 0;
    int     servermode = 0;
    static char *kwlist[] = {"dsn","user","pwd","database","autocommit","sysnaming",
                             "servermode", NULL};
    SQLRETURN rc;
    int vparm;
    ConnectionObject *c = (ConnectionObject *)self;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|ssssiii:connect", kwlist, 
                                     &dsn, &user, &pwd, &database, &autocommit, 
                                     &sysnaming, &servermode))
        return -1;
    rc = SQLAllocEnv(&c->henv);
    if (rc == SQL_SUCCESS) {
        /* use UTF8 */
        vparm = SQL_TRUE;
        rc = SQLSetEnvAttr(c->henv, SQL_ATTR_UTF8, &vparm, 0);
        if (dsn != NULL)
        {
            c->dsn = PyMem_Malloc(strlen(dsn) + 1);
            strcpy(c->dsn, dsn);
        }
        if (user != NULL)
        {
            c->user = PyMem_Malloc(strlen(user) + 1);
            strcpy(c->user, user);
        }
        if (pwd != NULL)
        {
            c->pwd = PyMem_Malloc(strlen(pwd) + 1);
            strcpy(c->pwd, pwd);
        }
        if (database != NULL)
        {
            c->database = PyMem_Malloc(strlen(database) + 1);
            strcpy(c->database, database);
        }
        /* run in server mode */
        if (servermode) {
            vparm = SQL_TRUE;
            rc = SQLSetEnvAttr(c->henv, SQL_ATTR_SERVER_MODE, &vparm, 0);
        }
        /* sort sequence job */
        vparm = SQL_TRUE;
        rc = SQLSetEnvAttr(c->henv, SQL_ATTR_JOB_SORT_SEQUENCE, &vparm, 0);
        /* naming convension */
        vparm = sysnaming ? SQL_TRUE: SQL_FALSE;
        rc = SQLSetEnvAttr(c->henv, SQL_ATTR_SYS_NAMING, &vparm, 0);
        if (rc == SQL_SUCCESS && database != NULL) {
            /* default database */
            rc = SQLSetEnvAttr(c->henv, SQL_ATTR_DEFAULT_LIB, c->database,
                               strlen(c->database));
        }
        /* allocate connection handle */
        rc = SQLAllocConnect(c->henv, &c->hdbc);
    }
    if (rc != SQL_SUCCESS) {
        f_error(SQL_HANDLE_ENV, c->henv);
        return -1;
    }
    /* Autocommit */
    vparm = autocommit ? SQL_TXN_NO_COMMIT: SQL_TXN_READ_UNCOMMITTED;
    rc = SQLSetConnectOption(c->hdbc, SQL_ATTR_COMMIT, &vparm);
    if (rc == SQL_SUCCESS)
        /* connect */
        rc = SQLConnect (c->hdbc, c->dsn, SQL_NTS, c->user, SQL_NTS, c->pwd, SQL_NTS);
    if (rc != SQL_SUCCESS) {
        f_error(SQL_HANDLE_DBC, c->hdbc);
        return -1;
    }
    return 0;
}

char con_cursor_doc[] =
"cursor([conv_utf16]) -> Cursor object\n\
\n\
Creates a new Cursor object. Optional parameter conv_utf16 to request text to be utf16.";

static PyObject *
con_cursor(PyObject *self, PyObject *args)
{
    CursorObject *cursor;
    SQLHSTMT hstmt;
    SQLRETURN rc;
    SQLHDBC hdbc;
    int conv_utf16 = 0;
    if (!PyArg_ParseTuple(args, "|i:cursor", &conv_utf16))
        return NULL;
    hdbc = ((ConnectionObject *)self)->hdbc;
    if (!hdbc) {
        PyErr_SetString(dbError, "Connection is closed.");
        return NULL;
    }
    rc = SQLAllocStmt(hdbc, &hstmt);
    if (rc != SQL_SUCCESS)
        return f_error(SQL_HANDLE_DBC, hdbc);
    cursor = PyObject_New(CursorObject, &Cursor_Type);
    Py_INCREF(self);
    cursor->con = self;
    cursor->hstmt = hstmt;
    cursor->stmt = NULL;
    cursor->curdesc = NULL;
    cursor->paraminfo = NULL;
    cursor->arraysize = 10;
    cursor->conv_utf16 = conv_utf16;
    cursor->rowcount = 0;
    cursor->numCols = 0;
    cursor->numParams = 0;
    cursor->row = NULL;
    cursor->buflist = NULL;
    return (PyObject *)cursor;
}

char con_commit_doc[] =
"commit() -> None.\n\
\n\
Commit pending transactions.";

static PyObject *
con_commit(PyObject *self)
{
    SQLRETURN rc;
    SQLHDBC hdbc = ((ConnectionObject *)self)->hdbc;
    if (!hdbc) {
        PyErr_SetString(dbError, "Connection is closed.");
        return NULL;
    }
    rc = SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT_HOLD);
    if (rc != SQL_SUCCESS)
        return f_error(SQL_HANDLE_DBC, hdbc);
    Py_INCREF(Py_None);
    return Py_None;
}

char con_rollback_doc[] =
"rollback() -> None.\n\
\n\
rollback pending transactions.";

static PyObject *
con_rollback(PyObject *self)
{
    SQLRETURN rc;
    SQLHDBC hdbc = ((ConnectionObject *)self)->hdbc;
    if (!hdbc) {
        PyErr_SetString(dbError, "Connection is closed.");
        return NULL;
    }
    rc = SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_ROLLBACK_HOLD);
    if (rc != SQL_SUCCESS)
        return f_error(SQL_HANDLE_DBC, hdbc);
    Py_INCREF(Py_None);
    return Py_None;
}

char con_close_doc[] =
"close() -> Close connection\n\
\n\
Close a connection, open cursors will be invalid.";

static PyObject *
con_close(PyObject *self)
{
    SQLRETURN rc;
    ConnectionObject *c = (ConnectionObject *)self;
    if (c->hdbc) {
        rc = SQLDisconnect(c->hdbc);
        rc = SQLFreeConnect(c->hdbc);
        rc = SQLFreeEnv(c->henv);
        c->hdbc = 0;
        c->henv = 0;
        if (c->dsn) PyMem_Free(c->dsn);
        if (c->user) PyMem_Free(c->user);
        if (c->pwd) PyMem_Free(c->pwd);
        if (c->database) PyMem_Free(c->database);
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static void
con_dealloc(PyObject *self)
{
    con_close(self);
    PyObject_Del(self);
}

static PyObject *
con_repr(PyObject *self)
{
    return PyString_FromString("connection");
}

static void
setColumnType(fieldInfoStruct *fi)
{
    /* ctype and size */
    if (fi->type == SQL_DECIMAL || fi->type == SQL_NUMERIC) {
        if (fi->scale == 0 && fi->prec < 10) {
            fi->ctype = SQL_INTEGER;
            fi->size = sizeof(SQLINTEGER);
        } else if (fi->scale == 0 && fi->prec < 19) {
            fi->ctype = SQL_BIGINT;
            fi->size = sizeof(long long);
        } else {
            fi->ctype = SQL_DOUBLE;
            fi->size = sizeof(SQLDOUBLE);
        }
    } else if (fi->type == SQL_FLOAT || fi->type == SQL_REAL || 
               fi->type == SQL_DOUBLE) {
        fi->ctype = SQL_DOUBLE;
        fi->size = sizeof(SQLDOUBLE);
    } else if (fi->type == SQL_SMALLINT || fi->type == SQL_INTEGER) {
        fi->ctype = SQL_INTEGER;
        fi->size = sizeof(SQLINTEGER);
    } else if (fi->type == SQL_BIGINT) {
        fi->ctype = SQL_BIGINT;
        fi->size = sizeof(long long);
    } else {
        fi->ctype = SQL_CHAR;
        if (fi->type == SQL_GRAPHIC || fi->type == SQL_VARGRAPHIC ||
            fi->type == SQL_DBCLOB)
            fi->size = fi->prec * 2 + 1;
        else
            fi->size = fi->prec + 1;
    }
}

/* bind parameters from supplied python tuple/list */  
static PyObject *
bindParams(CursorObject *self, PyObject *args)
{
    volatile _INTRPT_Hndlr_Parms_T  excpData;
    int i;
    paramInfoStruct *pi;
    SQLRETURN rc;
    char errbuf[255];
#pragma exception_handler(EXCP1, excpData, 0, _C2_MH_ESCAPE, _CTLA_HANDLE)
    pi = self->paraminfo;
    for (i = 0; i < self->numParams; i++) {
        pi->obj = PySequence_GetItem(args, i);
        if (pi->obj == Py_None) {
            pi->ctype = SQL_CHAR;
            pi->size = SQL_NULL_DATA;
        } else {
            switch (pi->type) {
            case SQL_INTEGER:
            case SQL_SMALLINT:
            case SQL_BIGINT:
            case SQL_FLOAT:
            case SQL_DECIMAL:
            case SQL_NUMERIC:
            case SQL_REAL:
                if (PyInt_Check(pi->obj)) {
                    pi->ctype = SQL_INTEGER;
                    pi->size = sizeof(SQLINTEGER);
                    pi->data = (SQLPOINTER)&(PyInt_AS_LONG(pi->obj));
                } else if (PyLong_Check(pi->obj)) {
                    Py_DECREF(pi->obj);
                    pi->obj = PyObject_Str(pi->obj);
                    if (PyErr_Occurred())
                        return NULL;
                    pi->size = PyString_GET_SIZE(pi->obj);
                    pi->ctype = SQL_CHAR;
                    pi->data = (SQLPOINTER)(PyString_AS_STRING(pi->obj));
                } else if (PyFloat_Check(pi->obj)) {
                    pi->ctype = SQL_DOUBLE;
                    pi->size = sizeof(SQLDOUBLE);
                    pi->data = (SQLPOINTER)&(PyFloat_AS_DOUBLE(pi->obj));
                } else {
                    sprintf(errbuf, "Param %d: Number expected.", i);
                    PyErr_SetString(dbError, errbuf);
                    return NULL;
                }
                break;
            case SQL_GRAPHIC:
            case SQL_VARGRAPHIC:
            case SQL_DBCLOB:
                /* unicode */
                if (PyUnicode_Check(pi->obj)) {
                    pi->data = (SQLPOINTER)(PyUnicode_AS_DATA(pi->obj));
                    pi->ctype = SQL_GRAPHIC;
                    pi->size = PyUnicode_GET_SIZE(pi->obj);
                } else {
                    sprintf(errbuf, "Param %d: Unicode object expected.", i);
                    PyErr_SetString(dbError, errbuf);
                    return NULL;
                }
                break;
            case SQL_CHAR:
            case SQL_VARCHAR:
            case SQL_CLOB:
                if (PyString_Check(pi->obj)) { 
                    pi->size = PyString_GET_SIZE(pi->obj);
                    if (pi->size == 0) {
                        pi->size = 1;
                        pi->data = " ";
                    } else
                        pi->data = (SQLPOINTER)(PyString_AS_STRING(pi->obj));
                    pi->ctype = SQL_CHAR;
                } else {
                    sprintf(errbuf, "Param %d: String expected.", i);
                    PyErr_SetString(dbError, errbuf);
                    return NULL;
                }
                break;
            case SQL_DATE:
            case SQL_TIME:
            case SQL_TIMESTAMP:
                if (PyString_Check(pi->obj)) {
                    pi->size = PyString_GET_SIZE(pi->obj);
                    if (pi->size == 0) {
                        pi->size = 1;
                        pi->data = " ";
                    } else
                        pi->data = (SQLPOINTER)(PyString_AS_STRING(pi->obj));
                    pi->ctype = SQL_CHAR;
                } else {
                    Py_DECREF(pi->obj);
                    pi->obj = PyObject_Str(pi->obj);
                    if (PyErr_Occurred())
                        return NULL;
                    pi->size = PyString_GET_SIZE(pi->obj);
                    pi->ctype = SQL_CHAR;
                    pi->data = (SQLPOINTER)(PyString_AS_STRING(pi->obj));
                }
                /* the format of the string is checked when executed */
                break;

            sprintf(errbuf, "Param %d: Unknown type.", i);
            PyErr_SetString(dbError, errbuf);
            return NULL;
            }
        }
        rc = SQLBindParameter(self->hstmt, i + 1, SQL_PARAM_INPUT,
                             pi->ctype, pi->type, pi->prec, pi->scale,
                             pi->data, 0, &pi->size);
        if (rc != SQL_SUCCESS)
            return f_error(SQL_HANDLE_STMT, self->hstmt);
        pi++;
    }
    Py_INCREF(Py_None);
    return Py_None;
 EXCP1:
    PyErr_SetString(dbError, "Error when binding parameters.");
    return NULL;
}

/* copy row object (to return) */  
static PyObject *
row_copy(RowObject *self)
{
    RowObject *o;
    int size = self->ob_size;
    o = PyObject_NewVar(RowObject, &Row_Type, size);
    if (o == NULL)
        return NULL;
    o->curdesc = self->curdesc;
    o->conv_utf16 = self->conv_utf16;
    Py_INCREF(o->curdesc);
    memcpy(o->buffer, self->buffer, size);
    return (PyObject *)o;
}

/* cursor methods */

static void
cur_unbind(CursorObject *self)
{
    int i;
    if (self->hstmt) {
        SQLFreeStmt(self->hstmt, SQL_RESET_PARAMS);
        Py_XDECREF(self->curdesc);
        Py_XDECREF(self->row);
        if (self->numParams > 0) {
            for (i = 0; i < self->numParams; i++)
                Py_XDECREF(self->paraminfo[i].obj);
            PyMem_Free(self->paraminfo);
            self->paraminfo = NULL;
            self->numParams = 0;
        }
        self->curdesc = NULL;
        self->row = NULL;
        SQLFreeStmt(self->hstmt, SQL_UNBIND);
    }
}

char cur_execute_doc[] =
"execute(string,[Parameters]) -> None.\n\
\n\
Execute sql statement. Parameters should be a tuple or list.";

static PyObject *
cur_execute(PyObject *self, PyObject *args)
{
    CursorObject *c = (CursorObject *)self;
    PyObject *stmt, *params = NULL;
    SQLRETURN rc;
    int i;
    if (!PyArg_ParseTuple(args, "O!|O:execute", &PyString_Type, &stmt, &params))
        return NULL;
    if (params && (!PyTuple_Check(params) && !PyList_Check(params))) {
        PyErr_SetString(dbError, "Parameters must be a tuple or list.");
        return NULL;
    }
    if (!c->hstmt) {
        PyErr_SetString(dbError, "Cursor is closed.");
        return NULL;
    }
    rc = SQLFreeStmt(c->hstmt, SQL_CLOSE);
    /* check if identical statement */
    if (!c->stmt || strcmp(PyString_AsString(c->stmt),
                          PyString_AsString(stmt)) != 0)
    {
        cur_unbind(c);
        /* prepare */
        Py_BEGIN_ALLOW_THREADS
        rc = SQLPrepare(c->hstmt, PyString_AsString(stmt), SQL_NTS);
        Py_END_ALLOW_THREADS
        if (rc != SQL_SUCCESS)
            return f_error(SQL_HANDLE_STMT, c->hstmt);
        /* number of columns */
        rc = SQLNumResultCols(c->hstmt, &c->numCols);
        if (rc != SQL_SUCCESS)
            return f_error(SQL_HANDLE_STMT, c->hstmt);
        if (c->numCols > 0) {
            char *buffer;
            fieldInfoStruct *fi;
            int totalsize = c->numCols * sizeof(SQLINTEGER);
            c->curdesc = PyObject_New(CursordescObject, &Cursordesc_Type);
            /* column information */
            c->curdesc->fieldArr = fi = PyMem_Malloc(c->numCols * 
                                                        sizeof(fieldInfoStruct));
            for (i = 0; i < c->numCols; i++) {
                SQLSMALLINT nameLength;
                rc = SQLDescribeCol(c->hstmt, i + 1, fi->name, sizeof(fi->name),
                                    &nameLength, &fi->type, &fi->prec, &fi->scale,
                                    &fi->nulls);
                setColumnType(fi);
                fi->offset = totalsize; 
                totalsize += fi->size;
                fi++;
            }
            c->curdesc->numCols = c->numCols;
            c->curdesc->totalsize = totalsize;
            /* allocate one row object for buffer */
            c->row = PyObject_NewVar(RowObject, &Row_Type, totalsize);
            c->row->curdesc = c->curdesc;
            c->row->conv_utf16 = c->conv_utf16;
            Py_INCREF(c->curdesc);
            buffer = c->row->buffer;
            fi = c->curdesc->fieldArr;
            for (i = 0; i < c->numCols; i++) {
                /* bind column */
                SQLBindCol(c->hstmt, i + 1, fi->ctype,
                           buffer + fi->offset, fi->size,
                           (SQLINTEGER *)(buffer + sizeof(SQLINTEGER) * i)); 
                fi++;
            }
        }
        /* get parameter information */
        rc = SQLNumParams(c->hstmt, &c->numParams);
        if (rc != SQL_SUCCESS)
            return f_error(SQL_HANDLE_STMT, c->hstmt);
        if (c->numParams > 0) {
            paramInfoStruct *pi;
            c->paraminfo = pi = PyMem_Malloc(c->numParams * sizeof(paramInfoStruct));
            for (i = 0; i < c->numParams; i++) {
                rc = SQLDescribeParam(c->hstmt, i + 1, &pi->type,
                                      &pi->prec, &pi->scale, &pi->nulls);
                pi->obj = NULL;
                pi++;
            }
        }
        Py_INCREF(stmt);
        c->stmt = stmt;
    }
    if (c->numParams > 0) {
        if (!params || PySequence_Size(params) != c->numParams) {
            PyErr_SetString(dbError, "Number of parameters don't match.");
            return NULL;
        }
        if (bindParams(c, params) == NULL)
            return NULL;
    }
    /* execute */
    Py_BEGIN_ALLOW_THREADS
    rc = SQLExecute(c->hstmt);
    Py_END_ALLOW_THREADS
    if (rc == SQL_SUCCESS)
        rc = SQLRowCount(c->hstmt, &c->rowcount);
    else if (rc == SQL_NO_DATA_FOUND)
        c->rowcount = 0;
    else
        return f_error(SQL_HANDLE_STMT, c->hstmt);
    /* return self for iteration */
    Py_INCREF(self);
    return self;
}/* end cur_execute */

char cur_fetchone_doc[] =
"fetchone() -> Row object (sequence)\n\
\n\
Fetch one row.";

static PyObject *
cur_fetchone(PyObject *self)
{
    CursorObject *c = (CursorObject *)self;
    SQLRETURN rc;
    if (!c->hstmt) {
        PyErr_SetString(dbError, "Cursor is closed.");
        return NULL;
    }
    if (c->numCols == 0) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    Py_BEGIN_ALLOW_THREADS
    rc = SQLFetch(c->hstmt);
    Py_END_ALLOW_THREADS
    if (rc == SQL_NO_DATA_FOUND) {
        Py_INCREF(Py_None);
        return Py_None;
    } else if (rc != SQL_SUCCESS)
        return f_error(SQL_HANDLE_STMT, c->hstmt);
    else
        return row_copy(c->row);
}

char cur_fetchmany_doc[] =
"fetchmany([size]) -> Tuple of Row objects.\n\
\n\
Fetch size number of rows. If size is not given,\n\
arraysize number of rows is fetched.";

static PyObject *
cur_fetchmany(PyObject *self, PyObject *args)
{
    CursorObject *c = (CursorObject *)self;
    SQLRETURN rc;
    int i, size = c->arraysize;
    PyObject *t, *o;
    if (!PyArg_ParseTuple(args, "|i:fetchmany", &size))
        return NULL;
    if (!c->hstmt) {
        PyErr_SetString(dbError, "Cursor is closed.");
        return NULL;
    }
    t = PyList_New(0);
    if (t == NULL)
        return NULL;
    for (i = 0; i < size; i++) {
        Py_BEGIN_ALLOW_THREADS
        rc = SQLFetch(c->hstmt);
        Py_END_ALLOW_THREADS
        if (rc == SQL_NO_DATA_FOUND)
            break;
        else if (rc != SQL_SUCCESS)
            return f_error(SQL_HANDLE_STMT, c->hstmt);
        o = row_copy(c->row);
        if (o == NULL)
            return NULL;
        if (PyList_Append(t, o) == -1)
            return NULL;
        Py_DECREF(o);
    }
    return t;
}

char cur_fetchall_doc[] =
"fetchall() -> List of Row objects.\n\
\n\
Fetch all(remaining) rows.";

static PyObject *
cur_fetchall(PyObject *self)
{
    CursorObject *c = (CursorObject *)self;
    SQLRETURN rc;
    int i;
    PyObject *t, *o;
    if (!c->hstmt) {
        PyErr_SetString(dbError, "Cursor is closed.");
        return NULL;
    }
    t = PyList_New(0);
    if (t == NULL)
        return NULL;
    while (1) {
        Py_BEGIN_ALLOW_THREADS
        rc = SQLFetch(c->hstmt);
        Py_END_ALLOW_THREADS
        if (rc == SQL_NO_DATA_FOUND)
            break;
        else if (rc != SQL_SUCCESS)
            return f_error(SQL_HANDLE_STMT, c->hstmt);
        o = row_copy(c->row);
        if (o == NULL)
            return NULL;
        if (PyList_Append(t, o) == -1)
            return NULL;
        Py_DECREF(o);
    }
    return t;
}

char cur_close_doc[] =
"close() -> None.\n\
\n\
Close the cursor. It can not be reopened.";

static PyObject *
cur_close(PyObject *self)
{
    CursorObject *c = (CursorObject *)self;
    if (c->hstmt) {
        cur_unbind(c);
        SQLFreeStmt(c->hstmt, SQL_DROP);
        c->hstmt = 0;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

char cur_nextset_doc[] =
"nextset() -> None.\n\
\n\
Make cursor skip to the next set. (not yet implemented.)";

static PyObject *
cur_nextset(PyObject *self)
{
    CursorObject *c = (CursorObject *)self;
    if (c->hstmt) {
    }
    Py_INCREF(Py_None);
    return Py_None;
}

char cur_executemany_doc[] =
"executemany(string,seq of parameters) -> Tuple of result sets.\n\
\n\
Run a statement agains many parameter sequences.";

static PyObject *
cur_executemany(PyObject *self, PyObject *args)
{
    CursorObject *c = (CursorObject *)self;
    PyObject *stmt, *params, *res, *v, *p;
    int i, size;
    if (!PyArg_ParseTuple(args, "O!O:executemany", &PyString_Type, &stmt, &params))
        return NULL;
    if (params && (!PyTuple_Check(params) && !PyList_Check(params))) {
        PyErr_SetString(dbError, "Parameters must be a tuple or list of tuple or list.");
        return NULL;
    }
    size = PySequence_Size(params);
    res = PyTuple_New(size);
    for (i = 0; i < size; i++) {
        p = Py_BuildValue("ON", stmt, PySequence_GetItem(params, i));
        v = cur_execute(self, p);
        Py_XDECREF(p);
        if (v == NULL)
            return NULL;
        /* fetch the results if result set exists */
        if (c->numCols > 0)
            PyTuple_SET_ITEM(res, i, cur_fetchall(self));
    }
    if (c->numCols > 0)
        return res;
    else {
        Py_DECREF(res);
        Py_INCREF(Py_None);
        return Py_None;
    }
}

char cur_callproc_doc[] =
"callproc(procname,[parameters]) -> None.\n\
\n\
Call a stored database procedure (not yet implemented).";

static PyObject *
cur_callproc(PyObject *self)
{
    CursorObject *c = (CursorObject *)self;
    if (c->hstmt) {
    }
    Py_INCREF(Py_None);
    return Py_None;
}

char cur_setinputsizes_doc[] =
"setinputsizes(seq of sizes) -> None.\n\
\n\
Set size of input parameters.";

static PyObject *
cur_setinputsizes(PyObject *self)
{
    CursorObject *c = (CursorObject *)self;
    if (c->hstmt) {
    }
    Py_INCREF(Py_None);
    return Py_None;
}

char cur_setoutputsizes_doc[] =
"setoutputsizes(seq of sizes) -> None.\n\
\n\
Set size of output parameters.";

static PyObject *
cur_setoutputsizes(PyObject *self)
{
    CursorObject *c = (CursorObject *)self;
    if (c->hstmt) {
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
cur_description(CursorObject *self)
{
    PyObject *t, *v, *o;
    int i;
    fieldInfoStruct *fi;
    if (self->curdesc) {
        t = PyTuple_New(self->curdesc->numCols);
        fi = self->curdesc->fieldArr;
        for (i = 0; i < self->curdesc->numCols; i++) {
            if (FieldtypeFunc)
                o = PyObject_CallFunction(FieldtypeFunc, "i", fi->type);
            else
                o = PyInt_FromLong(fi->type);
            if (o == NULL)
                return NULL;
            v = Py_BuildValue("sNOOiii", fi->name, o, Py_None,
                              Py_None, fi->prec, fi->scale, fi->nulls);
            if (v == NULL)
                return NULL;
            PyTuple_SET_ITEM(t, i, v);
            fi++;
        }
        return t;
    } else {
        PyErr_SetString(dbError, "No description exists.");
        return NULL;
    }
}

static void
cursor_dealloc(PyObject *self)
{
    cur_close(self);
    Py_XDECREF(((CursorObject *)self)->con);
    PyObject_Del(self);
}

static PyObject *
cursor_repr(CursorObject *self)
{
    if (self->stmt) {
        Py_INCREF(self->stmt);
        return self->stmt;
    } else
        return PyString_FromString("Cursor object");
}

static PyObject *
cursor_iter(PyObject *self)
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
cursor_next(PyObject *self)
{
    PyObject *row;
    CursorObject *c = (CursorObject *)self;
    if (c->buflist == NULL) {
        PyObject *parm, *res;
        parm = PyTuple_New(0);
        res = cur_fetchmany(self, parm);
        Py_DECREF(parm);
        if (res == Py_None || PyList_Size(res) == 0) {
            PyErr_SetObject(PyExc_StopIteration, Py_None);
            return NULL;
        }
        PyObject_CallMethod(res, "reverse", NULL);
        c->buflist = res;
    }
    row = PyObject_CallMethod(c->buflist, "pop", NULL);
    if (PyList_Size(c->buflist) == 0) {
        Py_DECREF(c->buflist);
        c->buflist = NULL;
    }
    return row;
}

/* row methods       */

/* convert one row field to python object */  
static PyObject *
cvtToPy(unsigned char *buf, fieldInfoStruct fi, int i, int conv_utf16)
{
    volatile _INTRPT_Hndlr_Parms_T  excpData;
    int size;
    PyObject *o;
    char *p;
#pragma exception_handler(EXCP1, excpData, 0, _C2_MH_ESCAPE, _CTLA_HANDLE)
    size = *(int *)(buf + sizeof(int) * i);
    p = buf + fi.offset;
    if (size == SQL_NULL_DATA) {
        Py_INCREF(Py_None);
        return Py_None;
    } else if (fi.ctype == SQL_INTEGER) {
        return PyInt_FromLong(*(long *)p);
    } else if (fi.ctype == SQL_BIGINT) {
        return PyLong_FromLongLong(*(long long *)p);
    } else if (fi.ctype == SQL_DOUBLE) {
        return PyFloat_FromDouble(*(double *)p);
    } else if (fi.ctype == SQL_CHAR) {
        if (size == SQL_NTS) {
            size = strlen(p);
        } else if (size == 0) {
            if (fi.type == SQL_VARCHAR) size = strlen(p);
            else size = fi.size - 1;
        }
        while (size > 0 && isspace(Py_CHARMASK(p[size - 1])))
            size--;
        if (fi.type == SQL_DATE && DateFunc)
            return PyObject_CallFunction(DateFunc, "s#", p, size);
        else if (fi.type == SQL_TIME && TimeFunc)
            return PyObject_CallFunction(TimeFunc, "s#", p, size);
        else if (fi.type == SQL_TIMESTAMP && TimeFunc)
            return PyObject_CallFunction(DatetimeFunc, "s#", p, size);
        else if (conv_utf16)
            return PyUnicode_FromStringAndSize(p, size);
        else
            return PyString_FromStringAndSize(p, size);
    } else if (fi.ctype == SQL_GRAPHIC) {
        if (size == SQL_NTS)
            size = strlen(p);
        if (conv_utf16 > 0)
            return PyUnicode_FromStringAndSize(p, size);
        else
            return PyString_FromStringAndSize(p, size);
    }
 EXCP1:
    PyErr_SetString(dbError, "Data conversion error.");
    return NULL;
}

static int
row_length(RowObject *self)
{
	return self->curdesc->numCols;
}

static PyObject *
row_item(RowObject *self, int i)
{
	if (i < 0 || i >= self->curdesc->numCols) {
		PyErr_SetString(PyExc_IndexError, "row index out of range");
		return NULL;
	}
    return cvtToPy(self->buffer, self->curdesc->fieldArr[i], i, self->conv_utf16);
}

static PyObject *
row_slice(RowObject *self, int ilow, int ihigh)
{
	PyObject *t, *v;
	int i, size = self->curdesc->numCols;
	if (ilow < 0)
		ilow = 0;
	if (ihigh > size)
		ihigh = size;
	if (ihigh < ilow)
		ihigh = ilow;
	t = PyTuple_New(ihigh - ilow);
	if (t == NULL)
		return NULL;
	for (i = ilow; i < ihigh; i++) {
        v = cvtToPy(self->buffer, self->curdesc->fieldArr[i], i, self->conv_utf16);
        if (v == NULL)
            return NULL;
		PyTuple_SET_ITEM(t, i - ilow, v);
	}
	return t;
}

static void
row_dealloc(PyObject *self)
{
    Py_XDECREF(((RowObject*)self)->curdesc);
    PyObject_Del(self);
}

static PyObject *
row_repr(RowObject *self)
{
    fieldInfoStruct *fi;
    PyObject *s;
    char *s0, *p;
    int i, j, size, s_size = self->curdesc->totalsize + 100;
    s = PyString_FromStringAndSize(NULL, s_size);
    s0 = PyString_AsString(s);
    j = 0;
    fi = self->curdesc->fieldArr;
	for (i = 0; i < self->curdesc->numCols; i++) {
        size = *(int *)(self->buffer + sizeof(int) * i);
        p = self->buffer + fi->offset;
        if (fi->size > s_size - j - 100) {
            s_size += size;
            _PyString_Resize(&s, s_size);
            s0 = PyString_AsString(s);
        }
        if (size != SQL_NULL_DATA) {
            if (fi->ctype == SQL_INTEGER)
                j += sprintf(s0 + j, "%ld", *(long *)p);
            else if (fi->ctype == SQL_BIGINT)
                j += sprintf(s0 + j, "%lld", *(long long *)p);
            else if (fi->ctype == SQL_DOUBLE)
                j += sprintf(s0 + j, "%.*f", fi->scale, *(double *)p);
            else if (fi->ctype == SQL_CHAR) {
                if (size == SQL_NTS) {
                    size = strlen(p);
                } else if (size == 0) {
                    if (fi->type == SQL_VARCHAR) size = strlen(p);
                    else size = fi->size - 1;
                }
                while (size > 0 && isspace(Py_CHARMASK(p[size - 1])))
                    size--;
                if (size > 0) {
                    memcpy(s0 + j, p, size);
                    j += size;
                }
            } else if (fi->ctype == SQL_GRAPHIC) {
                wcstombs(s0 + j, (wchar_t *)p, size);
                j += size * sizeof(wchar_t);
            }
        }
        memcpy(s0 + j, "\t", 1);
        j++;
        fi++;
	}
    _PyString_Resize(&s, j - 1);
	return s;
}

static PyMethodDef connection_methods[] = {
    {"cursor", (PyCFunction)con_cursor, METH_VARARGS, con_cursor_doc},
    {"commit", (PyCFunction)con_commit, METH_NOARGS, con_commit_doc},
    {"rollback", (PyCFunction)con_rollback, METH_NOARGS, con_rollback_doc},
    {"close", (PyCFunction)con_close, METH_NOARGS, con_close_doc},
    {NULL, NULL}       /* sentinel */
};

PyTypeObject Connection_Type = {
    PyObject_HEAD_INIT(NULL)
    0,          /*ob_size*/
    "Connection",   /*tp_name*/
    sizeof(ConnectionObject),  /*tp_basicsize*/
    0,          /*tp_itemsize*/
    /* methods */
    (destructor)con_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
   (reprfunc)con_repr,   /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash*/
    0,          /*tp_call*/
    0,          /*tp_str*/
	PyObject_GenericGetAttr, /* tp_getattro */
	0,          /* tp_setattro */
	0,			/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT, /* tp_flags */
	connection_doc,      	/* tp_doc */
	0,			/* tp_traverse */
	0,			/* tp_clear */
	0,			/* tp_richcompare */
	0,			/* tp_weaklistoffset */
	0,			/* tp_iter */
	0,			/* tp_iternext */
	connection_methods, /* tp_methods */
    0,      	/* tp_members */
	0,	       	/* tp_getset */
	0,			/* tp_base */
	0,			/* tp_dict */
	0,			/* tp_descr_get */
	0,			/* tp_descr_set */
	0,	        /* tp_dictoffset */
	connection_init, /* tp_init */
	PyType_GenericAlloc, /* tp_alloc */
	connection_new, /* tp_new */
};

static void
cursordesc_dealloc(PyObject *self)
{
    PyMem_Free(((CursordescObject *)self)->fieldArr);
    PyObject_Del(self);
}

PyTypeObject Cursordesc_Type = {
    PyObject_HEAD_INIT(NULL)
    0,          /*ob_size*/
    "Cursordesc",   /*tp_name*/
    sizeof(CursordescObject),  /*tp_basicsize*/
    0,          /*tp_itemsize*/
    /* methods */
    (destructor)cursordesc_dealloc, /*tp_dealloc*/
};

static PyMethodDef cursor_methods[] = {
    {"execute", (PyCFunction)cur_execute, METH_VARARGS, cur_execute_doc},
    {"executemany", (PyCFunction)cur_executemany, METH_VARARGS, cur_executemany_doc},
    {"fetchone", (PyCFunction)cur_fetchone, METH_NOARGS, cur_fetchone_doc},
    {"fetchmany", (PyCFunction)cur_fetchmany, METH_VARARGS, cur_fetchmany_doc},
    {"fetchall", (PyCFunction)cur_fetchall, METH_NOARGS, cur_fetchall_doc},
    {"close", (PyCFunction)cur_close, METH_NOARGS, cur_close_doc},
    {"nextset", (PyCFunction)cur_nextset, METH_NOARGS, cur_nextset_doc},
    {"callproc", (PyCFunction)cur_callproc, METH_VARARGS, cur_callproc_doc},
    {"setinputsizes", (PyCFunction)cur_setinputsizes, METH_VARARGS, cur_setinputsizes_doc},
    {"setoutputsizes", (PyCFunction)cur_setoutputsizes, METH_VARARGS, cur_setoutputsizes_doc},
    {NULL, NULL}       /* sentinel */
};

static PyObject *
cursor_getattr(CursorObject *self, char *name)
{
    if (!strcmp(name, "arraysize"))
        return PyInt_FromLong(self->arraysize);
    else if (!strcmp(name, "connection")) {
        Py_XINCREF(self->con);
        return self->con;
    } else if (!strcmp(name, "description"))
        return cur_description(self);
    else if (!strcmp(name, "rowcount"))
        return PyInt_FromLong(self->rowcount);
    else if (!strcmp(name, "__members__"))
        return Py_BuildValue("[ssss]", "arraysize","connection", 
                             "description","rowcount");
    else
        return Py_FindMethod(cursor_methods, (PyObject *)self, name);
}

static int
cursor_setattr(CursorObject *self, char *name, PyObject *v)
{
    if (!strcmp(name, "arraysize")) {
        if (!PyInt_Check(v)) {
            PyErr_SetString(dbError, "Argument must be an int value.");
            return -1;
        }
        self->arraysize = PyInt_AsLong(v);
    } else if (!strcmp(name, "conv_utf16")) {
        if (!PyInt_Check(v)) {
            PyErr_SetString(dbError, "Argument must be an int value.");
            return -1;
        }
        self->conv_utf16 = PyInt_AsLong(v);
    } else {
        PyErr_Format(PyExc_AttributeError, "Not valid to set attribute: %s", name);
        return -1;
    }
}

PyTypeObject Cursor_Type = {
    PyObject_HEAD_INIT(NULL)
    0,          /*ob_size*/
    "Cursor",   /*tp_name*/
    sizeof(CursorObject),  /*tp_basicsize*/
    0,          /*tp_itemsize*/
    /* methods */
    (destructor)cursor_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    (getattrfunc)cursor_getattr, /*tp_getattr*/
    (setattrfunc)cursor_setattr, /*tp_setattr*/
    0,          /*tp_compare*/
   (reprfunc)cursor_repr,   /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash*/
    0,          /*tp_call*/
    0,          /*tp_str*/
	0,          /* tp_getattro */
	0,          /* tp_setattro */
	0,			/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT, /* tp_flags */
	NULL,      	/* tp_doc */
	0,			/* tp_traverse */
	0,			/* tp_clear */
	0,			/* tp_richcompare */
	0,			/* tp_weaklistoffset */
	cursor_iter, /* tp_iter */
	cursor_next, /* tp_iternext */
	cursor_methods, /* tp_methods */
    0,	/* tp_members */
	0,	       	/* tp_getset */
	0,			/* tp_base */
	0,			/* tp_dict */
	0,			/* tp_descr_get */
	0,			/* tp_descr_set */
	0,	        /* tp_dictoffset */
	0,			/* tp_init */
	0,			/* tp_alloc */
	0,	        /* tp_new */
};

static PyMethodDef row_methods[] = {
    /*{"get",  (PyCFunction)row_get,  METH_VARARGS, row_get_doc},*/
    {NULL,      NULL}       /* sentinel */
};

static PySequenceMethods row_as_sequence = {
	(inquiry)row_length,
	0,          /* sq_concat */
	0,         	/* sq_repeat */
	(intargfunc)row_item,		/* sq_item */
	(intintargfunc)row_slice,		/* sq_slice */
	0,					/* sq_ass_item */
	0,					/* sq_ass_slice */
	0,                  /* sq_contains */
};

PyTypeObject Row_Type = {
    PyObject_HEAD_INIT(NULL)
    0,          /*ob_size*/
    "Row",      /*tp_name*/
    sizeof(RowObject),  /*tp_basicsize*/
    1,          /*tp_itemsize*/
    /* methods */
    (destructor)row_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    0,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
   (reprfunc)row_repr,   /*tp_repr*/
    0,          /*tp_as_number*/
    &row_as_sequence,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash*/
    0,          /*tp_call*/
    0,          /*tp_str*/
	0,             		/* tp_getattro */
	0,                 	/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT, /* tp_flags */
	0,	 		        /* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,      			/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	row_methods,   		/* tp_methods */
    0,	             	/* tp_members */
	0,		          	/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,                  /* tp_dictoffset */
	0,					/* tp_init */
	0,					/* tp_alloc */
	0,  				/* tp_new */
};

static char setFieldtype_doc[] =
"setFieldtypeFunction(func) -> None.\n\
\n\
Set factory function for field types.";

static PyObject*
setFieldtype(PyObject* self, PyObject* args)
{
    PyObject *func;
    if (!PyArg_ParseTuple(args, "O", &func))
        return NULL;
    if (func != Py_None && !PyCallable_Check(func)) {
        PyErr_SetString(dbError, "Function is not callable.");
        return NULL;
    }
    Py_XDECREF(FieldtypeFunc);
    if (func != Py_None) {
        FieldtypeFunc = func;
        Py_INCREF(FieldtypeFunc);
    } else
        FieldtypeFunc = NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static char setDatetime_doc[] =
"setDatetimeFunctions(datefunc, timefunc, datetimefunc) -> None.\n\
\n\
Set date and time factory functions for module.";

static PyObject*
setDatetime(PyObject* self, PyObject* args)
{
    PyObject *datef, *timef, *datetimef;
    if (!PyArg_ParseTuple(args, "OOO", &datef, &timef, &datetimef))
        return NULL;
    if (datef != Py_None && !PyCallable_Check(datef)) {
        PyErr_SetString(dbError, "Date function is not callable.");
        return NULL;
    }
    if (timef != Py_None && !PyCallable_Check(timef)) {
        PyErr_SetString(dbError, "Time function is not callable.");
        return NULL;
    }
    if (datetimef != Py_None && !PyCallable_Check(datetimef)) {
        PyErr_SetString(dbError, "Datetime function is not callable.");
        return NULL;
    }
    Py_XDECREF(DateFunc);
    Py_XDECREF(TimeFunc);
    Py_XDECREF(DatetimeFunc);
    DateFunc = TimeFunc = DatetimeFunc = NULL;
    if (datef != Py_None) {
        DateFunc = datef;
        Py_INCREF(DateFunc);
    }
    if (timef != Py_None) {
        TimeFunc = timef;
        Py_INCREF(TimeFunc);
    }
    if (datetimef != Py_None) {
        DatetimeFunc = datetimef;
        Py_INCREF(DatetimeFunc);
    }
    Py_INCREF(Py_None);
    return Py_None;
}

/* List of functions defined in the module */
static PyMethodDef db_memberlist[] = {
    /* TODO methods to set date/time/timestamp factory functions */
    {"setFieldtypeFunction", setFieldtype, METH_VARARGS, setFieldtype_doc},   
    {"setDatetimeFunctions", setDatetime, METH_VARARGS, setDatetime_doc},   
    {NULL,      NULL}
};

/* Doc string */
static char db_doc[] = "Python Database API v2.0 for DB2/400";

void
init_db2(void)
{
    PyObject *d, *m;
    /* Initialize the type of the new type object here; doing it here
     * is required for portability to Windows without requiring C++. */
    Connection_Type.ob_type = &PyType_Type;
    Cursordesc_Type.ob_type = &PyType_Type;
    Cursor_Type.ob_type = &PyType_Type;
    Row_Type.ob_type = &PyType_Type;
    
    m = Py_InitModule3("_db2", db_memberlist, db_doc);
    d = PyModule_GetDict(m);
    dbError = PyErr_NewException("_db2.Error", NULL, NULL);
    dbWarning = PyErr_NewException("_db2.Warning", NULL, NULL);
    PyDict_SetItemString(d, "Error", dbError);
    PyDict_SetItemString(d, "Warning", dbWarning);
    PyDict_SetItemString(d, "Connection", (PyObject *)&Connection_Type);
    if (PyErr_Occurred() ) {
        Py_FatalError("Can not initialize _db2");
    }
    return;
}
