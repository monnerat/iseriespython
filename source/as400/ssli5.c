/* ssli5 module for I5/OS 
 
   SSL based on Global Secure ToolKit APIs (GSK) 

   Per Gummedal

*/

#include "Python.h"
/* Include symbols from _socket module */
#include "socketmodule.h"

#if defined(HAVE_POLL_H) 
#include <poll.h>
#elif defined(HAVE_SYS_POLL_H)
#include <sys/poll.h>
#endif

/* Include gskit header file */
#include <gskssl.h>

/* SSL environment object */
typedef struct {
    PyObject_HEAD
    gsk_handle env_handle;    /* secure environment handle */
} PySSLI5Env;

/* SSL session object */
typedef struct {
    PyObject_HEAD
    PySocketSockObject *socket; /* Socket on which we're layered */
    gsk_handle session_handle;    /* secure session handle */
    int status; 
} PySSLI5Session;

static PyObject * ssli5Error;
static PyTypeObject PySSLI5Env_Type;
static PyTypeObject PySSLI5Session_Type;
static PyObject * PySSLI5SessionWrite(PySSLI5Session *self, PyObject *args);
static PyObject * PySSLI5SessionRead(PySSLI5Session *self, PyObject *args);
static int check_socket_and_wait_for_timeout(PySocketSockObject *s, 
                         int writing);

#define PySSLI5Env_Check(v)  ((v)->ob_type == &PySSLI5Env_Type)
#define PySSLI5Session_Check(v)  ((v)->ob_type == &PySSLI5Session_Type)

typedef enum {
    SOCKET_IS_NONBLOCKING,
    SOCKET_IS_BLOCKING,
    SOCKET_HAS_TIMED_OUT,
    SOCKET_HAS_BEEN_CLOSED,
    SOCKET_TOO_LARGE_FOR_SELECT,
    SOCKET_OPERATION_OK
} timeout_state;

static PySSLI5Env *
newPySSLI5Env(char *applId, int sessionType, int authType)
{
    PySSLI5Env *self;
    int rc;
    validationCallBack valCallback;
    self = PyObject_New(PySSLI5Env, &PySSLI5Env_Type);
    Py_INCREF(self);
    if (self == NULL)
        return NULL;
    return self;
    /* open a gsk environment */
    rc = gsk_environment_open(&self->env_handle);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_environment_open failed, error = %d", rc);
        Py_DECREF(self);
        return NULL;
    }
    /* set the Application ID to use */
    if (applId != NULL && strlen(applId) > 0)
    {
        /* convert to job ccsid */
        PyObject *obj = PyString_FromStringAndSize(NULL, strlen(applId));
        char *s = PyString_AsString(obj);
        utfToStr(applId, s);
        rc = gsk_attribute_set_buffer(self->env_handle, GSK_OS400_APPLICATION_ID,
                                      s, sizeof(s));
        /* Py_DECREF(obj);*/
        if (rc != GSK_OK)
        {
            PyErr_Format(ssli5Error, "gsk_attribute_set_buffer failed, error = %d", rc);
            Py_DECREF(self);
            return NULL;
        }
    }
    /* set session type */
    rc = gsk_attribute_set_enum(self->env_handle, GSK_SESSION_TYPE, sessionType);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_attribute_set_enum failed, error = %d", rc);
        Py_DECREF(self);
        return NULL;
    }
    /* set client auth */
    if (authType != 0) {
        if (sessionType == GSK_CLIENT_SESSION)
            rc = gsk_attribute_set_enum(self->env_handle, GSK_SERVER_AUTH_TYPE, authType);
        else
            rc = gsk_attribute_set_enum(self->env_handle, GSK_CLIENT_AUTH_TYPE, authType);
        if (rc != GSK_OK)
        {
            PyErr_Format(ssli5Error, "gsk_attribute_set_enum failed, error = %d", rc);
            Py_DECREF(self);
            return NULL;
        }
    }
    rc = gsk_environment_init(self->env_handle);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_environment_init failed, error = %d", rc);
        Py_DECREF(self);
        return NULL;
    }
    return self;
}

static PySSLI5Session *
newPySSLI5Session(PySSLI5Env *env, PySocketSockObject *sock)
{
    PySSLI5Session *self;
    int rc, retcode;
    GSK_ENUM_VALUE sessionType;

    self = PyObject_New(PySSLI5Session, &PySSLI5Session_Type);
    if (self == NULL)
        return NULL;
    self->socket = NULL;
    self->status = 0;
    /* open a secure session */
    rc = gsk_secure_soc_open(env->env_handle, &self->session_handle);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_secure_soc_open failed, error = %d", rc);
        Py_DECREF(self);
        return NULL;
    }
    /* associate socket */
    rc = gsk_attribute_set_numeric_value(self->session_handle, GSK_FD, sock->sock_fd);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_attribute_set_numeric_value failed, error = %d", rc);
        Py_DECREF(self);
        return NULL;
    }
    /* the handshake */
    rc = gsk_secure_soc_init(self->session_handle);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_secure_soc_init failed, error = %d", rc);
        Py_DECREF(self);
        return NULL;
    }
    /* get certificate validation result */
    rc = gsk_attribute_get_enum(env->env_handle, GSK_SESSION_TYPE, &sessionType);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_attribute_get_enum failed, error = %d", rc);
        Py_DECREF(self);
        return NULL;
    }
    if (sessionType != GSK_SERVER_SESSION) {
        rc = gsk_attribute_get_numeric_value(self->session_handle,
                GSK_CERTIFICATE_VALIDATION_CODE, &retcode);
        if (rc != GSK_OK)
        {
            PyErr_Format(ssli5Error, "gsk_attribute_get_numeric_value failed, error = %d", rc);
            Py_DECREF(self);
            return NULL;
        }
        self->status = retcode;
    }
    self->socket = sock;
    Py_INCREF(self->socket);
    return self;
}

static PyObject *
PySSLI5Init(PyObject *self, PyObject *args)
{
    PySSLI5Env *env;
    char *applId = NULL;
    int sessionType = 0;
    int clientAuth = 0;

    if (!PyArg_ParseTuple(args, "si|i:sslInit",
                  &applId, &sessionType, &clientAuth))
        return NULL;

    env = newPySSLI5Env(applId, sessionType, clientAuth);
    if (env == NULL)
        return NULL;
    return (PyObject *)env;
}

PyDoc_STRVAR(PySSLI5Init_doc,
"sslInit(application_id, session_type, [auth_type]) -> SSLI5Environment");

static PyObject *
PySSLI5EnvOpen(PyObject *self, PyObject *args)
{
    PySSLI5Session *session;
    PySocketSockObject *sock;

	if (!PyArg_ParseTuple(args, "O:open", (PyObject*)&sock))
		return NULL;

    session = newPySSLI5Session((PySSLI5Env *)self, sock);
    if (session == NULL)
        return NULL;
    return (PyObject *)session;
}

PyDoc_STRVAR(PySSLI5EnvOpen_doc,
"open(socket) -> SSLI5Session");

static PyObject *
PySSLI5EnvApplId(PySSLI5Env *self)
{
    int rc, bufsize;
    char * __ptr128 buffer;
    if (!self->env_handle) {
        PyErr_SetString(ssli5Error, "Environment not initialized");
        return NULL;
    }
    rc = gsk_attribute_get_buffer(self->env_handle, GSK_OS400_APPLICATION_ID,
                &buffer, &bufsize);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_attribute_bet_buffer failed, error = %d", rc);
        return NULL;
    }
    return PyString_FromStringAndSize(buffer, bufsize);
}

static void PySSLI5Env_dealloc(PySSLI5Env *self)
{
    if (self->env_handle)
        gsk_environment_close(&self->env_handle);
    PyObject_Del(self);
}

/* SSLI5Session object methods */

static void PySSLI5Session_dealloc(PySSLI5Session *self)
{
    if (self->session_handle)
        gsk_secure_soc_close(self->session_handle);
    Py_XDECREF(self->socket);
    PyObject_Del(self);
}

/* If the socket has a timeout, do a select()/poll() on the socket.
   The argument writing indicates the direction.
   Returns one of the possibilities in the timeout_state enum (above).
 */

static int
check_socket_and_wait_for_timeout(PySocketSockObject *s, int writing)
{
    fd_set fds;
    struct timeval tv;
    int rc;

    /* Nothing to do unless we're in timeout mode (not non-blocking) */
    if (s->sock_timeout < 0.0)
        return SOCKET_IS_BLOCKING;
    else if (s->sock_timeout == 0.0)
        return SOCKET_IS_NONBLOCKING;

    /* Guard against closed socket */
    if (s->sock_fd < 0)
        return SOCKET_HAS_BEEN_CLOSED;

    /* Prefer poll, if available, since you can poll() any fd
     * which can't be done with select(). */
#ifdef HAVE_POLL
    {
        struct pollfd pollfd;
        int timeout;

        pollfd.fd = s->sock_fd;
        pollfd.events = writing ? POLLOUT : POLLIN;

        /* s->sock_timeout is in seconds, timeout in ms */
        timeout = (int)(s->sock_timeout * 1000 + 0.5);
        rc = poll(&pollfd, 1, timeout);

        goto normal_return;
    }
#endif

    /* Guard against socket too large for select*/
#ifndef Py_SOCKET_FD_CAN_BE_GE_FD_SETSIZE
    if (s->sock_fd >= FD_SETSIZE)
        return SOCKET_TOO_LARGE_FOR_SELECT;
#endif

    /* Construct the arguments to select */
    tv.tv_sec = (int)s->sock_timeout;
    tv.tv_usec = (int)((s->sock_timeout - tv.tv_sec) * 1e6);
    FD_ZERO(&fds);
    FD_SET(s->sock_fd, &fds);

    /* See if the socket is ready */
    if (writing)
        rc = select(s->sock_fd+1, NULL, &fds, NULL, &tv);
    else
        rc = select(s->sock_fd+1, &fds, NULL, NULL, &tv);

normal_return:
    /* Return SOCKET_TIMED_OUT on timeout, SOCKET_OPERATION_OK otherwise
       (when we are able to write or when there's something to read) */
    return rc == 0 ? SOCKET_HAS_TIMED_OUT : SOCKET_OPERATION_OK;
}

static PyObject *
PySSLI5SessionWrite(PySSLI5Session *self, PyObject *args)
{
    char *data;
    int len;
    int count;
    int sockstate;
    int rc;

    if (!PyArg_ParseTuple(args, "s#:write", &data, &count))
        return NULL;

    sockstate = check_socket_and_wait_for_timeout(self->socket, 1);
    if (sockstate == SOCKET_HAS_TIMED_OUT) {
        PyErr_SetString(ssli5Error, "The write operation timed out");
        return NULL;
    } else if (sockstate == SOCKET_HAS_BEEN_CLOSED) {
        PyErr_SetString(ssli5Error, "Underlying socket has been closed.");
        return NULL;
    } else if (sockstate == SOCKET_TOO_LARGE_FOR_SELECT) {
        PyErr_SetString(ssli5Error, "Underlying socket too large for select().");
        return NULL;
    }
    rc = gsk_secure_soc_write(self->session_handle, data, count, &len);
    if(PyErr_CheckSignals()) {
        return NULL;
    }
    if (rc != GSK_OK) {
        PyErr_Format(ssli5Error, "gsk_secure_soc_write failed, error = %d", rc);
        return NULL;
    }
    return PyInt_FromLong(len);
}

PyDoc_STRVAR(PySSLI5SessionWrite_doc,
"write(s) -> len\n\
\n\
Writes the string s into the SSL object.  Returns the number\n\
of bytes written.");

static PyObject *
PySSLI5SessionRead(PySSLI5Session *self, PyObject *args)
{
    PyObject *buf;
    int count = 0;
    int len = 1024;
    int sockstate;
    int rc;

    if (!PyArg_ParseTuple(args, "|i:read", &len))
        return NULL;
    if (!(buf = PyString_FromStringAndSize((char *) 0, len)))
        return NULL;
    rc = gsk_secure_soc_read(self->session_handle, PyString_AsString(buf), len, &count);
    if(PyErr_CheckSignals()) {
        Py_DECREF(buf);
        return NULL;
    }
    if (rc != GSK_OK) {
        PyErr_Format(ssli5Error, "gsk_secure_soc_read failed, error = %d", rc);
        Py_DECREF(buf);
        return NULL;
    }
    if (count != len)
        _PyString_Resize(&buf, count);
    return buf;
}

PyDoc_STRVAR(PySSLI5SessionRead_doc,
"read([len]) -> string\n\
\n\
Read up to len bytes from the SSL socket.");


static PyObject *
PySSLI5SessionStatus(PySSLI5Session *self)
{
    if (!self->session_handle) {
        PyErr_SetString(ssli5Error, "Session not initialized");
        return NULL;
    }
    return PyInt_FromLong(self->status);
}

PyDoc_STRVAR(PySSLI5SessionStatus_doc,
"validationStatus() -> int\n\
\n\
Returns 0 if the certificate validations was ok.");


static PyObject *
PySSLI5SessionCertificate(PySSLI5Session *self)
{
    int rc, i;
    int certElements;
    PyObject *dict, *k, *o;
    gsk_cert_data_elem * __ptr128 data_elem;

    if (!self->session_handle) {
        PyErr_SetString(ssli5Error, "Session not initialized");
        return NULL;
    }
    rc = gsk_attribute_get_cert_info(self->session_handle, GSK_PARTNER_CERT_INFO,
                &data_elem, &certElements);
    if (rc != GSK_OK)
    {
        PyErr_Format(ssli5Error, "gsk_attribute_bet_cert_info failed, error = %d", rc);
        return NULL;
    }
    /* put information in a list */
    dict = PyDict_New();
    if (dict == NULL)
        return NULL;
    for (i = 0; i < certElements; i++) {
        k = PyInt_FromLong(data_elem->cert_data_id);
        o = PyString_FromStringAndSize(data_elem->cert_data_p, data_elem->cert_data_l);
        if (k == NULL || o == NULL)
            return NULL;
        PyDict_SetItem(dict, k, o);
        Py_DECREF(k);
        Py_DECREF(o);
        data_elem++;
    }
    return dict;
}

PyDoc_STRVAR(PySSLI5SessionCertificate_doc,
"certificate() -> dict\n\
\n\
Get certificate information about the partner certificate,\n\
that may have been received during handshake.");


static PyMethodDef PySSLI5EnvMethods[] = {
    {"open", (PyCFunction)PySSLI5EnvOpen, METH_VARARGS, PySSLI5EnvOpen_doc},
    {"applicationId", (PyCFunction)PySSLI5EnvApplId, METH_NOARGS},
    {NULL, NULL}
};

static PyObject *
PySSLI5Env_getattr(PySSLI5Env *self, char *name)
{
    return Py_FindMethod(PySSLI5EnvMethods, (PyObject *)self, name);
}

static PyMethodDef PySSLI5SessionMethods[] = {
    {"write", (PyCFunction)PySSLI5SessionWrite, METH_VARARGS,
              PySSLI5SessionWrite_doc},
    {"read", (PyCFunction)PySSLI5SessionRead, METH_VARARGS,
              PySSLI5SessionRead_doc},
    {"certificate", (PyCFunction)PySSLI5SessionCertificate, METH_NOARGS,
              PySSLI5SessionCertificate_doc},
    {"validationStatus", (PyCFunction)PySSLI5SessionStatus, METH_NOARGS,
              PySSLI5SessionStatus_doc},
    {NULL, NULL}
};

static PyObject *
PySSLI5Session_getattr(PySSLI5Session *self, char *name)
{
    return Py_FindMethod(PySSLI5SessionMethods, (PyObject *)self, name);
}


/* List of functions exported by this module. */

static PyMethodDef PySSLI5_methods[] = {
    {"sslInit", PySSLI5Init, METH_VARARGS, PySSLI5Init_doc},
    {NULL,          NULL}        /* Sentinel */
};

static PyTypeObject PySSLI5Env_Type = {
    PyObject_HEAD_INIT(NULL)
    0,              /*ob_size*/
    "SSLI5Environment",     /*tp_name*/
    sizeof(PySSLI5Env),      /*tp_basicsize*/
    0,              /*tp_itemsize*/
    /* methods */
    (destructor)PySSLI5Env_dealloc,    /*tp_dealloc*/
    0,              /*tp_print*/
    (getattrfunc)PySSLI5Env_getattr,   /*tp_getattr*/
    0,              /*tp_setattr*/
    0,              /*tp_compare*/
    0,              /*tp_repr*/
    0,              /*tp_as_number*/
    0,              /*tp_as_sequence*/
    0,              /*tp_as_mapping*/
    0,              /*tp_hash*/
};

static PyTypeObject PySSLI5Session_Type = {
    PyObject_HEAD_INIT(NULL)
    0,              /*ob_size*/
    "SSLI5Session",           /*tp_name*/
    sizeof(PySSLI5Session),      /*tp_basicsize*/
    0,              /*tp_itemsize*/
    /* methods */
    (destructor)PySSLI5Session_dealloc,    /*tp_dealloc*/
    0,              /*tp_print*/
    (getattrfunc)PySSLI5Session_getattr,   /*tp_getattr*/
    0,              /*tp_setattr*/
    0,              /*tp_compare*/
    0,              /*tp_repr*/
    0,              /*tp_as_number*/
    0,              /*tp_as_sequence*/
    0,              /*tp_as_mapping*/
    0,              /*tp_hash*/
};


PyDoc_STRVAR(module_doc,
"Implementation of the Global Secure ToolKit.");

PyMODINIT_FUNC
initssli5(void)
{
    PyObject *m, *d;

    PySSLI5Env_Type.ob_type = &PyType_Type;
    PySSLI5Session_Type.ob_type = &PyType_Type;

    m = Py_InitModule3("ssli5", PySSLI5_methods, module_doc);
    if (m == NULL)
        return;
    d = PyModule_GetDict(m);
	/* Load _socket module and its C API */
/*	if (PySocketModule_ImportModuleAndAPI())
 	    	return;*/
    ssli5Error = PyErr_NewException("ssli5.error", NULL, NULL);
    PyDict_SetItemString(d, "error", ssli5Error);

    if (PyDict_SetItemString(d, "SSLEnvType",
                 (PyObject *)&PySSLI5Env_Type) != 0)
        return;
    if (PyDict_SetItemString(d, "SSLSessionType",
                 (PyObject *)&PySSLI5Session_Type) != 0)
        return;
    PyModule_AddIntConstant(m, "CLIENT_SESSION",
                GSK_CLIENT_SESSION);
    PyModule_AddIntConstant(m, "SERVER_SESSION",
                GSK_SERVER_SESSION);
    PyModule_AddIntConstant(m, "SERVER_SESSION_AUTH",
                GSK_SERVER_SESSION_WITH_CL_AUTH);
    PyModule_AddIntConstant(m, "CLIENT_AUTH_FULL",
                GSK_CLIENT_AUTH_FULL);
    PyModule_AddIntConstant(m, "CLIENT_AUTH_PASSTHRU",
                GSK_CLIENT_AUTH_PASSTHRU);
    PyModule_AddIntConstant(m, "CLIENT_AUTH_REQUIRED",
                GSK_OS400_CLIENT_AUTH_REQUIRED);
    PyModule_AddIntConstant(m, "SERVER_AUTH_FULL",
                GSK_SERVER_AUTH_FULL);
    PyModule_AddIntConstant(m, "SERVER_AUTH_PASSTHRU",
                GSK_SERVER_AUTH_PASSTHRU);
    PyModule_AddIntConstant(m, "ERROR_NO_CERTIFICATE",
                GSK_ERROR_NO_CERTIFICATE);
    PyModule_AddIntConstant(m, "ERROR_BAD_CERTIFICATE",
                GSK_ERROR_BAD_CERTIFICATE);
    PyModule_AddIntConstant(m, "OK",
                GSK_OK);
}
