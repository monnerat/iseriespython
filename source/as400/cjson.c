/*
 * Copyright (C) 2006-2007 Dan Pascu. See LICENSE for details.
 * Author: Dan Pascu <dan@ag-projects.com>
 *
 * Copyright (C) 2008 Matt Billenstein. See LICENSE for details.
 * Author: Matt Billenstein <matt@vazor.com>
 *
 * Copyright (C) 2008 Per Gummedal. See LICENSE for details.
 * Changed to always assume utf-8 (for IBM iSeries)
 * Handle python objects
 * 
 * Fast JSON encoder/decoder implementation for Python
 *
 */

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "Python.h"
#include "structmember.h"

typedef struct JSONData {
    char *str; // the actual json string
    char *end; // pointer to the string end
    char *ptr; // pointer to the current parsing position
} JSONData;

static PyObject* encode_object(PyObject *object);
static PyObject* encode_string(PyObject *object);
static PyObject* encode_tuple(PyObject *object);
static PyObject* encode_list(PyObject *object);
static PyObject* get_object_dict(PyObject *object);
static PyObject* get_slots_dict(PyObject *object);
static PyObject* encode_dict(PyObject *object);
static PyObject* decode_json(JSONData *jsondata);
static PyObject* decode_null(JSONData *jsondata);
static PyObject* decode_bool(JSONData *jsondata);
static PyObject* decode_string(JSONData *jsondata);
static PyObject* decode_inf(JSONData *jsondata);
static PyObject* decode_nan(JSONData *jsondata);
static PyObject* decode_number(JSONData *jsondata);
static PyObject* decode_array(JSONData *jsondata);
static PyObject* decode_object(JSONData *jsondata);

static PyObject *JSON_Error;
static PyObject *JSON_EncodeError;
static PyObject *JSON_DecodeError;


#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN

#define SSIZE_T_F "%d"
#else
#define SSIZE_T_F "%zd"
#endif

#define True  1
#define False 0

#ifndef INFINITY
#define INFINITY HUGE_VAL
#endif

#ifndef NAN
#define NAN (HUGE_VAL - HUGE_VAL)
#endif

#ifndef Py_IS_NAN
#define Py_IS_NAN(X) ((X) != (X))
#endif

#define skipSpaces(d) while(*((d)->ptr) && isspace(*((d)->ptr))) (d)->ptr++


/* ------------------------------ Decoding ----------------------------- */

static PyObject*
decode_null(JSONData *jsondata)
{
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if (left >= 4 && strncmp(jsondata->ptr, "null", 4)==0) {
        jsondata->ptr += 4;
        Py_INCREF(Py_None);
        return Py_None;
    } else {
        PyErr_Format(JSON_DecodeError, "cannot parse JSON description: %.20s",
                     jsondata->ptr);
        return NULL;
    }
}


static PyObject*
decode_bool(JSONData *jsondata)
{
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if (left >= 4 && strncmp(jsondata->ptr, "true", 4)==0) {
        jsondata->ptr += 4;
        Py_INCREF(Py_True);
        return Py_True;
    } else if (left >= 5 && strncmp(jsondata->ptr, "false", 5)==0) {
        jsondata->ptr += 5;
        Py_INCREF(Py_False);
        return Py_False;
    } else {
        PyErr_Format(JSON_DecodeError, "cannot parse JSON description: %.20s",
                     jsondata->ptr);
        return NULL;
    }
}


static PyObject*
decode_escape(JSONData *jsondata, Py_ssize_t len)
{
	int c;
	char *s, *p, *buf;
	const char *end;
	PyObject *v;
	v = PyString_FromStringAndSize((char *)NULL, len);
	if (v == NULL)
		return NULL;
    s = jsondata->ptr + 1;
	p = buf = PyString_AsString(v);
	end = s + len;
	while (s < end) {
		if (*s != '\\') {
            *p++ = *s++;
			continue;
        }
        *s++;
        if (s==end) {
            PyErr_Format(JSON_DecodeError,
                         "Trailing \\ in string " SSIZE_T_F,
                         (Py_ssize_t)(jsondata->ptr - jsondata->str));
			goto failed;
		}
		switch (*s++) {
		case '\n': break;
		case '\\': *p++ = '\\'; break;
		case '\'': *p++ = '\''; break;
		case '\"': *p++ = '\"'; break;
		case '/': *p++ = '/'; break;
		case 'b': *p++ = '\b'; break;
		case 'f': *p++ = '\014'; break;
		case 't': *p++ = '\t'; break;
		case 'n': *p++ = '\n'; break;
		case 'r': *p++ = '\r'; break;
		case 'v': *p++ = '\013'; break;
		case 'a': *p++ = '\007'; break;
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			c = s[-1] - '0';
			if ('0' <= *s && *s <= '7') {
				c = (c<<3) + *s++ - '0';
				if ('0' <= *s && *s <= '7')
					c = (c<<3) + *s++ - '0';
			}
			*p++ = c;
			break;
		case 'u': case 'U':
			if (s[0] == '0' && s[1] == '0' && isxdigit(Py_CHARMASK(s[2]))
                    && isxdigit(Py_CHARMASK(s[3]))) {
				unsigned int x = 0;
				s++;
				s++;
				c = Py_CHARMASK(*s);
				s++;
				if (isdigit(c))
					x = c - '0';
				else if (islower(c))
					x = 10 + c - 'a';
				else
					x = 10 + c - 'A';
				x = x << 4;
				c = Py_CHARMASK(*s);
				s++;
				if (isdigit(c))
					x += c - '0';
				else if (islower(c))
					x += 10 + c - 'a';
				else
					x += 10 + c - 'A';
				*p++ = x;
				break;
			} else {
                PyErr_Format(JSON_DecodeError,
                             "u escape not valid " SSIZE_T_F,
                         (Py_ssize_t)(jsondata->ptr - jsondata->str));
				goto failed;
			}
		case 'x':
			if (isxdigit(Py_CHARMASK(s[0]))
			    && isxdigit(Py_CHARMASK(s[1]))) {
				unsigned int x = 0;
				c = Py_CHARMASK(*s);
				s++;
				if (isdigit(c))
					x = c - '0';
				else if (islower(c))
					x = 10 + c - 'a';
				else
					x = 10 + c - 'A';
				x = x << 4;
				c = Py_CHARMASK(*s);
				s++;
				if (isdigit(c))
					x += c - '0';
				else if (islower(c))
					x += 10 + c - 'a';
				else
					x += 10 + c - 'A';
				*p++ = x;
				break;
			} else {
                PyErr_Format(JSON_DecodeError,
                             "x escape not valid " SSIZE_T_F,
                             (Py_ssize_t)(jsondata->ptr - jsondata->str));
                goto failed;
			}
		default:
            PyErr_Format(JSON_DecodeError,
                         "escape not valid " SSIZE_T_F,
                         (Py_ssize_t)(jsondata->ptr - jsondata->str));
            goto failed;
		}
	}
	if (p-buf < len)
		_PyString_Resize(&v, p - buf);
	return v;
  failed:
      Py_DECREF(v);
      return NULL;
}

static PyObject*
decode_string(JSONData *jsondata)
{
    PyObject *object;
    int c, escaping, string_escape;
    Py_ssize_t len;
    char *ptr;

    // look for the closing quote
    escaping = string_escape = False;
    ptr = jsondata->ptr + 1;
    while (True) {
        c = *ptr;
        if (c == 0) {
            PyErr_Format(JSON_DecodeError,
                         "unterminated string starting at position " SSIZE_T_F,
                         (Py_ssize_t)(jsondata->ptr - jsondata->str));
            return NULL;
        }
        if (!escaping) {
            if (c == '\\') {
                escaping = True;
            } else if (c == '"') {
                break;
            }
        } else {
            switch(c) {
            case '"':
            case '/':
            case 'r':
            case 'n':
            case 't':
            case 'b':
            case 'f':
            case 'u':
            case '\\':
                string_escape = True;
                break;
            }
            escaping = False;
        }
        ptr++;
    }

    len = ptr - jsondata->ptr - 1;

    if (string_escape)
        object = decode_escape(jsondata, len);
    else
        object = PyString_FromStringAndSize(jsondata->ptr + 1, len);
    if (object != NULL)
        jsondata->ptr = ptr+1;
    return object;
}


static PyObject*
decode_inf(JSONData *jsondata)
{
    PyObject *object;
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if (left >= 8 && strncmp(jsondata->ptr, "Infinity", 8)==0) {
        jsondata->ptr += 8;
        object = PyFloat_FromDouble(INFINITY);
        return object;
    } else if (left >= 9 && strncmp(jsondata->ptr, "+Infinity", 9)==0) {
        jsondata->ptr += 9;
        object = PyFloat_FromDouble(INFINITY);
        return object;
    } else if (left >= 9 && strncmp(jsondata->ptr, "-Infinity", 9)==0) {
        jsondata->ptr += 9;
        object = PyFloat_FromDouble(-INFINITY);
        return object;
    } else {
        PyErr_Format(JSON_DecodeError, "cannot parse JSON description: %.20s",
                     jsondata->ptr);
        return NULL;
    }
}


static PyObject*
decode_nan(JSONData *jsondata)
{
    PyObject *object;
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if (left >= 3 && strncmp(jsondata->ptr, "NaN", 3)==0) {
        jsondata->ptr += 3;
        object = PyFloat_FromDouble(NAN);
        return object;
    } else {
        PyErr_Format(JSON_DecodeError, "cannot parse JSON description: %.20s",
                     jsondata->ptr);
        return NULL;
    }
}


static PyObject*
decode_number(JSONData *jsondata)
{
    PyObject *object, *str;
    int c, is_float, should_stop;
    char *ptr;

    // check if we got a floating point number
    ptr = jsondata->ptr;
    is_float = should_stop = False;
    while (True) {
        c = *ptr;
        if (c == 0)
            break;
        switch(c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
        case '+':
            break;
        case '.':
        case 'e':
        case 'E':
            is_float = True;
            break;
        default:
            should_stop = True;
        }
        if (should_stop) {
            break;
        }
        ptr++;
    }

    str = PyString_FromStringAndSize(jsondata->ptr, ptr - jsondata->ptr);
    if (str == NULL)
        return NULL;

    if (is_float) {
        object = PyFloat_FromString(str, NULL);
    } else {
        object = PyInt_FromString(PyString_AS_STRING(str), NULL, 10);
    }

    Py_DECREF(str);

    if (object == NULL) {
        PyErr_Format(JSON_DecodeError, "invalid number starting at position "
                     SSIZE_T_F, (Py_ssize_t)(jsondata->ptr - jsondata->str));
    } else {
        jsondata->ptr = ptr;
    }

    return object;
}


static PyObject*
decode_array(JSONData *jsondata)
{
    PyObject *object, *item;
    int c, expect_item, items, result;
    char *start;

    object = PyList_New(0);

    start = jsondata->ptr;
    jsondata->ptr++;
    expect_item = True;
    items = 0;
    while (True) {
        skipSpaces(jsondata);
        c = *jsondata->ptr;
        if (c == 0) {
            PyErr_Format(JSON_DecodeError, "unterminated array starting at "
                         "position " SSIZE_T_F,
                         (Py_ssize_t)(start - jsondata->str));
            goto failure;;
        } else if (c == ']') {
            if (expect_item && items>0) {
                PyErr_Format(JSON_DecodeError, "expecting array item at "
                             "position " SSIZE_T_F,
                             (Py_ssize_t)(jsondata->ptr - jsondata->str));
                goto failure;
            }
            jsondata->ptr++;
            break;
        } else if (c == ',') {
            if (expect_item) {
                PyErr_Format(JSON_DecodeError, "expecting array item at "
                             "position " SSIZE_T_F,
                             (Py_ssize_t)(jsondata->ptr - jsondata->str));
                goto failure;
            }
            expect_item = True;
            jsondata->ptr++;
            continue;
        } else {
            item = decode_json(jsondata);
            if (item == NULL)
                goto failure;
            result = PyList_Append(object, item);
            Py_DECREF(item);
            if (result == -1)
                goto failure;
            expect_item = False;
            items++;
        }
    }

    return object;

failure:
    Py_DECREF(object);
    return NULL;
}


static PyObject*
decode_object(JSONData *jsondata)
{
    PyObject *object, *key, *value;
    int c, expect_key, items, result;
    char *start;

    object = PyDict_New();

    expect_key = True;
    items = 0;
    start = jsondata->ptr;
    jsondata->ptr++;

    while (True) {
        skipSpaces(jsondata);
        c = *jsondata->ptr;
        if (c == 0) {
            PyErr_Format(JSON_DecodeError, "unterminated object starting at "
                         "position " SSIZE_T_F,
                         (Py_ssize_t)(start - jsondata->str));
            goto failure;;
        } else if (c == '}') {
            if (expect_key && items>0) {
                PyErr_Format(JSON_DecodeError, "expecting object property name"
                             " at position " SSIZE_T_F,
                             (Py_ssize_t)(jsondata->ptr - jsondata->str));
                goto failure;
            }
            jsondata->ptr++;
            break;
        } else if (c == ',') {
            if (expect_key) {
                PyErr_Format(JSON_DecodeError, "expecting object property name"
                             "at position " SSIZE_T_F,
                             (Py_ssize_t)(jsondata->ptr - jsondata->str));
                goto failure;
            }
            expect_key = True;
            jsondata->ptr++;
            continue;
        } else {
            if (c != '"') {
                PyErr_Format(JSON_DecodeError, "expecting property name in "
                             "object at position " SSIZE_T_F,
                             (Py_ssize_t)(jsondata->ptr - jsondata->str));
                goto failure;
            }

            key = decode_json(jsondata);
            if (key == NULL)
                goto failure;

            skipSpaces(jsondata);
            if (*jsondata->ptr != ':') {
                PyErr_Format(JSON_DecodeError, "missing colon after object "
                             "property name at position " SSIZE_T_F,
                             (Py_ssize_t)(jsondata->ptr - jsondata->str));
                Py_DECREF(key);
                goto failure;
            } else {
                jsondata->ptr++;
            }

            value = decode_json(jsondata);
            if (value == NULL) {
                Py_DECREF(key);
                goto failure;
            }

            result = PyDict_SetItem(object, key, value);
            Py_DECREF(key);
            Py_DECREF(value);
            if (result == -1)
                goto failure;
            expect_key = False;
            items++;
        }
    }

    return object;

failure:
    Py_DECREF(object);
    return NULL;
}


static PyObject*
decode_json(JSONData *jsondata)
{
    PyObject *object;

    skipSpaces(jsondata);
    switch(*jsondata->ptr) {
    case 0:
        PyErr_SetString(JSON_DecodeError, "empty JSON description");
        return NULL;
    case '{':
        object = decode_object(jsondata);
        break;
    case '[':
        object = decode_array(jsondata);
        break;
    case '"':
        object = decode_string(jsondata);
        break;
    case 't':
    case 'f':
        object = decode_bool(jsondata);
        break;
    case 'n':
        object = decode_null(jsondata);
        break;
    case 'N':
        object = decode_nan(jsondata);
        break;
    case 'I':
        object = decode_inf(jsondata);
        break;
    case '+':
    case '-':
        if (*(jsondata->ptr+1) == 'I') {
            object = decode_inf(jsondata);
        } else {
            object = decode_number(jsondata);
        }
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        object = decode_number(jsondata);
        break;
    default:
        PyErr_SetString(JSON_DecodeError, "cannot parse JSON description");
        return NULL;
    }

    return object;
}


/* ------------------------------ Encoding ----------------------------- */

/*
 * This function is an almost verbatim copy of PyString_Repr() from
 * Python's stringobject.c with the following differences:
 *
 * - it always quotes the output using double quotes.
 * - it also quotes \b, \f, and \/
 */
static PyObject*
encode_string(PyObject *string)
{
    register PyStringObject* op = (PyStringObject*) string;
    size_t newsize = 2 + 2 * op->ob_size;
    PyObject *v;

    if (op->ob_size > (PY_SSIZE_T_MAX-2)/2) {
        PyErr_SetString(PyExc_OverflowError,
                        "string is too large to make repr");
        return NULL;
    }
    v = PyString_FromStringAndSize((char *)NULL, newsize);
    if (v == NULL) {
        return NULL;
    }
    else {
        register Py_ssize_t i;
        register char c;
        register char *p;
        int quote;

        quote = '"';

        p = PyString_AS_STRING(v);
        *p++ = quote;
        for (i = 0; i < op->ob_size; i++) {
            c = op->ob_sval[i];
            if (c == quote || c == '\\')
                *p++ = '\\', *p++ = c;
            else if (c == '\t')
                *p++ = '\\', *p++ = 't';
            else if (c == '\n')
                *p++ = '\\', *p++ = 'n';
            else if (c == '\r')
                *p++ = '\\', *p++ = 'r';
            else if (c == '\f')
                *p++ = '\\', *p++ = 'f';
            else if (c == '\b')
                *p++ = '\\', *p++ = 'b';
            else if (c == '/')
                *p++ = '\\', *p++ = '/';
            else
                *p++ = c;
        }
        *p++ = quote;
        *p = '\0';
        _PyString_Resize(&v, (int) (p - PyString_AS_STRING(v)));
        return v;
    }
}


/*
 * This function is an almost verbatim copy of tuplerepr() from
 * Python's tupleobject.c with the following differences:
 *
 * - it uses encode_object() to get the object's JSON reprezentation.
 * - it uses [] as decorations isntead of () (to masquerade as a JSON array).
 */

static PyObject*
encode_tuple(PyObject *tuple)
{
    Py_ssize_t i, n;
    PyObject *s, *temp;
    PyObject *pieces, *result = NULL;
    PyTupleObject *v = (PyTupleObject*) tuple;

    n = v->ob_size;
    if (n == 0)
        return PyString_FromString("[]");

    pieces = PyTuple_New(n);
    if (pieces == NULL)
        return NULL;

    /* Do repr() on each element. */
    for (i = 0; i < n; ++i) {
        s = encode_object(v->ob_item[i]);
        if (s == NULL)
            goto Done;
        PyTuple_SET_ITEM(pieces, i, s);
    }

    /* Add "[]" decorations to the first and last items. */
    assert(n > 0);
    s = PyString_FromString("[");
    if (s == NULL)
        goto Done;
    temp = PyTuple_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyTuple_SET_ITEM(pieces, 0, s);
    if (s == NULL)
        goto Done;

    s = PyString_FromString("]");
    if (s == NULL)
        goto Done;
    temp = PyTuple_GET_ITEM(pieces, n-1);
    PyString_ConcatAndDel(&temp, s);
    PyTuple_SET_ITEM(pieces, n-1, temp);
    if (temp == NULL)
        goto Done;

    /* Paste them all together with ", " between. */
    s = PyString_FromString(", ");
    if (s == NULL)
        goto Done;
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_DECREF(pieces);
    return result;
}

/*
 * This function is an almost verbatim copy of list_repr() from
 * Python's listobject.c with the following differences:
 *
 * - it uses encode_object() to get the object's JSON reprezentation.
 * - it doesn't use the ellipsis to represent a list with references
 *   to itself, instead it raises an exception as such lists cannot be
 *   represented in JSON.
 */
static PyObject*
encode_list(PyObject *list)
{
    Py_ssize_t i;
    PyObject *s, *temp;
    PyObject *pieces = NULL, *result = NULL;
    PyListObject *v = (PyListObject*) list;

    i = Py_ReprEnter((PyObject*)v);
    if (i != 0) {
        if (i > 0) {
            PyErr_SetString(JSON_EncodeError, "a list with references to "
                            "itself is not JSON encodable");
        }
        return NULL;
    }

    if (v->ob_size == 0) {
        result = PyString_FromString("[]");
        goto Done;
    }

    pieces = PyList_New(0);
    if (pieces == NULL)
        goto Done;

    /* Do repr() on each element.  Note that this may mutate the list,
     * so must refetch the list size on each iteration. */
    for (i = 0; i < v->ob_size; ++i) {
        int status;
        s = encode_object(v->ob_item[i]);
        if (s == NULL)
            goto Done;
        status = PyList_Append(pieces, s);
        Py_DECREF(s);  /* append created a new ref */
        if (status < 0)
            goto Done;
    }

    /* Add "[]" decorations to the first and last items. */
    assert(PyList_GET_SIZE(pieces) > 0);
    s = PyString_FromString("[");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyList_SET_ITEM(pieces, 0, s);
    if (s == NULL)
        goto Done;

    s = PyString_FromString("]");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
    PyString_ConcatAndDel(&temp, s);
    PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
    if (temp == NULL)
        goto Done;

    /* Paste them all together with ", " between. */
    s = PyString_FromString(", ");
    if (s == NULL)
        goto Done;
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_XDECREF(pieces);
    Py_ReprLeave((PyObject *)v);
    return result;
}


static PyObject*
get_slots_dict(PyObject *obj)
{
	int i, n;
	PyObject *mro, *slots;
    PyTypeObject *type;
    slots = PyDict_New();
	mro = obj->ob_type->tp_mro;
	n = PyTuple_GET_SIZE(mro);
	for (i = 0; i < n; i++) {
		type = (PyTypeObject *)PyTuple_GET_ITEM(mro, i);
		if (!PyClass_Check(type)) {
            int i, n;
            PyMemberDef *mp;
            n = type->ob_size;
            mp = PyHeapType_GET_MEMBERS((PyHeapTypeObject *)type);
            for (i = 0; i < n; i++, mp++) {
                if (mp->type == T_OBJECT_EX && !(mp->flags & READONLY)) {
                    char *addr = (char *)obj + mp->offset;
                    PyObject *attr = *(PyObject **)addr;
                    if (attr != NULL && !PyDict_GetItemString(slots, mp->name)) {
                        PyDict_SetItemString(slots, mp->name, attr);
                    }
                }
            }
        }
    }
    return slots;
}

static PyObject*
get_object_dict(PyObject *obj)
{
    PyObject *d, *dict, *clsname, *name;
    if (obj->ob_type->tp_flags & Py_TPFLAGS_HEAPTYPE) {
        /* get __dict__ merged with slots */
        dict = get_slots_dict(obj);
        d = PyObject_GetAttrString(obj, "__dict__");
        if (d) {
            PyDict_Merge(dict, d, 0);
            Py_DECREF(d);
        } else
            PyErr_Clear();
        clsname = PyObject_GetAttrString((PyObject *)obj->ob_type, "__name__");
        name = PyObject_GetAttrString((PyObject *)obj->ob_type, "__module__");
    } else {
        PyClassObject *cls = ((PyInstanceObject *)obj)->in_class;
        d = PyObject_GetAttrString(obj, "__dict__");
        dict = PyDict_Copy(d);
        Py_DECREF(d);
        clsname = PyString_FromString(PyString_AsString(cls->cl_name));
        name = PyObject_GetAttrString(obj, "__module__");
    }
    if (name == NULL || clsname == NULL) {
        PyErr_SetString(JSON_EncodeError, "could not find class of object.");
        return NULL;
    }
    /* add __class__ to dict */
    PyString_ConcatAndDel(&name, PyString_FromString("."));
    PyString_ConcatAndDel(&name, clsname);
    PyDict_SetItemString(dict, "__class__", name);
    Py_DECREF(name);
    return dict;
}

/*
 * This function is an almost verbatim copy of dict_repr() from
 * Python's dictobject.c with the following differences:
 *
 * - it uses encode_object() to get the object's JSON reprezentation.
 * - only accept strings for keys.
 * - it doesn't use the ellipsis to represent a dictionary with references
 *   to itself, instead it raises an exception as such dictionaries cannot
 *   be represented in JSON.
 */
static PyObject*
encode_dict(PyObject *dict)
{
    Py_ssize_t i;
    PyObject *s, *temp, *colon = NULL;
    PyObject *pieces = NULL, *result = NULL;
    PyObject *key, *value, *v1;
    PyDictObject *mp = (PyDictObject*) dict;

    i = Py_ReprEnter((PyObject *)mp);
    if (i != 0) {
        if (i > 0) {
            PyErr_SetString(JSON_EncodeError, "a dict with references to "
                            "itself is not JSON encodable");
        }
        return NULL;
    }

    if (mp->ma_used == 0) {
        result = PyString_FromString("{}");
        goto Done;
    }

    pieces = PyList_New(0);
    if (pieces == NULL)
        goto Done;

    colon = PyString_FromString(": ");
    if (colon == NULL)
        goto Done;

    /* Do repr() on each key+value pair, and insert ": " between them.
     * Note that repr may mutate the dict. */
    i = 0;
    while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
        int status;

        if (!PyString_Check(key) && !PyUnicode_Check(key)) {
            PyErr_SetString(JSON_EncodeError, "JSON encodable dictionaries "
                            "must have string/unicode keys");
            goto Done;
        }

        /* Prevent repr from deleting value during key format. */
        Py_INCREF(value);
        s = encode_object(key);
        PyString_Concat(&s, colon);
        PyString_ConcatAndDel(&s, encode_object(value));
        Py_DECREF(value);
        if (s == NULL)
            goto Done;
        status = PyList_Append(pieces, s);
        Py_DECREF(s);  /* append created a new ref */
        if (status < 0)
            goto Done;
    }

    /* Add "{}" decorations to the first and last items. */
    assert(PyList_GET_SIZE(pieces) > 0);
    s = PyString_FromString("{");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyList_SET_ITEM(pieces, 0, s);
    if (s == NULL)
        goto Done;

    s = PyString_FromString("}");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
    PyString_ConcatAndDel(&temp, s);
    PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
    if (temp == NULL)
        goto Done;

    /* Paste them all together with ", " between. */
    s = PyString_FromString(", ");
    if (s == NULL)
        goto Done;
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_XDECREF(pieces);
    Py_XDECREF(colon);
    Py_ReprLeave((PyObject *)mp);
    return result;
}

static PyObject*
encode_object(PyObject *object)
{
    PyObject *v1, *v2;
    if (object == Py_True) {
        return PyString_FromString("true");
    } else if (object == Py_False) {
        return PyString_FromString("false");
    } else if (object == Py_None) {
        return PyString_FromString("null");
    } else if (PyString_Check(object)) {
        return encode_string(object);
    } else if (PyUnicode_Check(object)) {
        v1 = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(object),
                    PyUnicode_GET_SIZE(object), NULL);
        v2 = encode_string(v1);
        Py_DECREF(v1);
        return v2;
    } else if (PyFloat_Check(object)) {
        double val = PyFloat_AS_DOUBLE(object);
        if (Py_IS_NAN(val)) {
            return PyString_FromString("NaN");
        } else if (Py_IS_INFINITY(val)) {
            if (val > 0) {
                return PyString_FromString("Infinity");
            } else {
                return PyString_FromString("-Infinity");
            }
        } else {
            return PyObject_Str(object);
        }
    } else if (PyInt_Check(object) || PyLong_Check(object)) {
        return PyObject_Str(object);
    } else if (PyList_Check(object)) {
        return encode_list(object);
    } else if (PyTuple_Check(object)) {
        return encode_tuple(object);
    } else if (PyDict_Check(object)) {
        return encode_dict(object);
    } else if (PyInstance_Check(object) || 
            object->ob_type->tp_flags & Py_TPFLAGS_HEAPTYPE) {
        v1 = get_object_dict(object); 
        if (v1 == NULL)
            return NULL;
        v2 = encode_dict(v1);
        Py_DECREF(v1);
        return v2;
    } else {
        PyErr_SetString(JSON_EncodeError, "object is not JSON encodable");
        return NULL;
    }
}


/* Encode object into its JSON representation */

static PyObject*
JSON_encode(PyObject *self, PyObject *object)
{
    return encode_object(object);
}


/* Decode JSON representation into pyhton objects */

static PyObject*
JSON_decode(PyObject *self, PyObject *args)
{
    PyObject *object, *string, *str;
    JSONData jsondata;

    if (!PyArg_ParseTuple(args, "O:decode", &string))
        return NULL;

    if (PyUnicode_Check(string)) {
        str = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(string),
                    PyUnicode_GET_SIZE(string), NULL);
        if (str == NULL)
            return NULL;
    } else if (PyString_Check(string)) {
        Py_INCREF(string);
        str = string;
    } else {
        PyErr_SetString(JSON_DecodeError, "Argument should be astring/unicode");
		return NULL;
    }

    if (PyString_AsStringAndSize(str, &(jsondata.str), NULL) == -1) {
        Py_DECREF(str);
        return NULL; // not a string object or it contains null bytes
    }

    jsondata.ptr = jsondata.str;
    jsondata.end = jsondata.str + strlen(jsondata.str);

    object = decode_json(&jsondata);

    if (object != NULL) {
        skipSpaces(&jsondata);
        if (jsondata.ptr < jsondata.end) {
            PyErr_Format(JSON_DecodeError, "extra data after JSON description"
                         " at position " SSIZE_T_F,
                         (Py_ssize_t)(jsondata.ptr - jsondata.str));
            Py_DECREF(str);
            Py_DECREF(object);
            return NULL;
        }
    }

    Py_DECREF(str);

    return object;
}


/* List of functions defined in the module */

static PyMethodDef cjson_methods[] = {
    {"encode", (PyCFunction)JSON_encode,  METH_O,
    PyDoc_STR("encode(object) -> generate the JSON representation for object.")},

    {"decode", (PyCFunction)JSON_decode,  METH_VARARGS,
    PyDoc_STR("decode(string) -> parse the JSON representation into\n"
              "python objects.")},

    {NULL, NULL}  // sentinel
};

PyDoc_STRVAR(module_doc,
"Fast JSON encoder/decoder module."
);

/* Initialization function for the module (*must* be called initcjson) */

PyMODINIT_FUNC
initcjson(void)
{
    PyObject *m;

    m = Py_InitModule3("cjson", cjson_methods, module_doc);

    if (m == NULL)
        return;

    JSON_Error = PyErr_NewException("cjson.Error", NULL, NULL);
    if (JSON_Error == NULL)
        return;
    Py_INCREF(JSON_Error);
    PyModule_AddObject(m, "Error", JSON_Error);

    JSON_EncodeError = PyErr_NewException("cjson.EncodeError", JSON_Error, NULL);
    if (JSON_EncodeError == NULL)
        return;
    Py_INCREF(JSON_EncodeError);
    PyModule_AddObject(m, "EncodeError", JSON_EncodeError);

    JSON_DecodeError = PyErr_NewException("cjson.DecodeError", JSON_Error, NULL);
    if (JSON_DecodeError == NULL)
        return;
    Py_INCREF(JSON_DecodeError);
    PyModule_AddObject(m, "DecodeError", JSON_DecodeError);

    // Module version (the MODULE_VERSION macro is defined by setup.py)
    PyModule_AddStringConstant(m, "__version__", "1.0.6");

}


