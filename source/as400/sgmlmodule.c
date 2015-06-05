/*
 * sgmlmodule
 * 
 *--------------------------------------------------------------------
 * Copyright (c) 2001 by Per Gummedal.
 *
 * p.g@figu.no
 * 
 *----------------------------------------------------------------------
 * Initially copied from sgmlop.
 *
 * Copyright (c) 1998-2000 by Secret Labs AB
 * Copyright (c) 1998-2000 by Fredrik Lundh
 * 
 * fredrik@pythonware.com
 * http://www.pythonware.com
 *
 * By obtaining, using, and/or copying this software and/or its
 * associated documentation, you agree that you have read, understood,
 * and will comply with the following terms and conditions:
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * associated documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appears in all
 * copies, and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Secret Labs
 * AB or the author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 * 
 * SECRET LABS AB AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS.  IN NO EVENT SHALL SECRET LABS AB OR THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *------------------------------------------------------------------------
 */

#include "Python.h"
#include "sgmlmodule.h"
#include <ctype.h>

#define CHAR_T  char

#define ISALNUM isalnum
#define ISSPACE isspace
#define TOLOWER tolower

typedef struct {
	char *id;
	char c;
} entityStruct;

static entityStruct xmlEntities[] = {
	{"lt", '<'},
	{"gt", '>'},
	{"amp", '&'},
	{0,0}
};

typedef int (cfDoc) (void *o);
typedef int (cfTag) (void *o, char *s);
typedef int (cfStartTag) (void *o, char *s, char *a);
typedef int (cfProc) (void *o, char *s, char *a);
typedef int (cfData) (void *o, char *s, int len);

/* ==================================================================== */
/* parser data type */

/* state flags */
#define MAYBE 1
#define SURE 2

/* forward declarations */
static int fastfeed(SGMLParserObject* self);
static PyObject* attrparse(const CHAR_T *p, int len, int xml);

/* -------------------------------------------------------------------- */
/* create parser */

static PyObject*
sgml_new(int xml, int fromc, void *statobj)
{
    SGMLParserObject* self;

    self = PyObject_NEW(SGMLParserObject, &SGMLParser_Type);
    if (self == NULL)
        return NULL;

    self->xml = xml;
    self->c = fromc;

    self->feed = 0;
    self->shorttag = 0;
    self->doctype = 0;
    self->attrasstring = 0;

    self->buffer = NULL;
    self->bufferlen = 0;
    self->buffertotal = 0;
    self->parsing = 0;

	self->obj = statobj;

    self->handle_startdoc = NULL;
    self->handle_enddoc = NULL;
    self->handle_starttag = NULL;
    self->handle_endtag = NULL;
    self->handle_proc = NULL;
    self->handle_special = NULL;
    self->handle_charref = NULL;
    self->handle_entityref = NULL;
    self->handle_data = NULL;
    self->handle_cdata = NULL;
    self->handle_comment = NULL;

    return (PyObject*) self;
}

static PyObject*
sgml_sgmlparser(PyObject* self, PyObject* args)
{
    if (!PyArg_NoArgs(args))
        return NULL;

    return sgml_new(0, 0, NULL);
}

static PyObject*
sgml_xmlparser(PyObject* self, PyObject* args)
{
    if (!PyArg_NoArgs(args))
        return NULL;

    return sgml_new(1, 0, NULL);
}

static void
sgml_dealloc(SGMLParserObject* self)
{
    if (self->buffer)
        free(self->buffer);
	if (self->c == 0) {
		Py_XDECREF((PyObject *)self->handle_startdoc);
		Py_XDECREF((PyObject *)self->handle_enddoc);
		Py_XDECREF((PyObject *)self->handle_starttag);
		Py_XDECREF((PyObject *)self->handle_endtag);
		Py_XDECREF((PyObject *)self->handle_proc);
		Py_XDECREF((PyObject *)self->handle_special);
		Py_XDECREF((PyObject *)self->handle_charref);
		Py_XDECREF((PyObject *)self->handle_entityref);
		Py_XDECREF((PyObject *)self->handle_data);
		Py_XDECREF((PyObject *)self->handle_cdata);
		Py_XDECREF((PyObject *)self->handle_comment);
	}
    PyObject_DEL(self);
}

static char setContentHandler_doc[] =
"p.setContentHandler(contenthandler) -> None.\n\
\n\
Use a sax content handler.\n\
The following methods are used:\n\
startDocument, endDocument, startElement, endElement, characters.";  

static PyObject*
sgml_setContentHandler(SGMLParserObject* self, PyObject* args)
{
    /* set content handler */
    PyObject *o;
    if (!PyArg_ParseTuple(args, "O", &o))
        return NULL;
    Py_XDECREF((PyObject *)self->handle_startdoc);
    Py_XDECREF((PyObject *)self->handle_enddoc);
    Py_XDECREF((PyObject *)self->handle_starttag);
    Py_XDECREF((PyObject *)self->handle_endtag);
    Py_XDECREF((PyObject *)self->handle_data);
	self->handle_startdoc = PyObject_GetAttrString(o, "startDocument");
	self->handle_enddoc = PyObject_GetAttrString(o, "endDocument");
	self->handle_starttag = PyObject_GetAttrString(o, "startElement");
	self->handle_endtag = PyObject_GetAttrString(o, "endElement");
	self->handle_data = PyObject_GetAttrString(o, "characters");
    PyErr_Clear();
    Py_INCREF(Py_None);
    return Py_None;
}

static char setHandlerMethod_doc[] =
"p.setHandlerMethod(name, function) -> None.\n\
\n\
Set handler method.\n\
The following handler names are valid:\n\
startDocument, endDocument, startElement, endElement, characters,\n\
proc, special, charref, entityref, cdata, comment.";  

static PyObject*
sgml_setHandlerMethod(SGMLParserObject* self, PyObject* args)
{
	char *name; 	
    PyObject* item;
    if (!PyArg_ParseTuple(args, "sO", &name, &item))
        return NULL;
	Py_INCREF(item);
	if (!strcmp(name, "startDocument"))
		self->handle_startdoc = item;
	else if (!strcmp(name, "endDocument"))
		self->handle_enddoc = item;
	else if (!strcmp(name, "startElement"))
		self->handle_starttag = item;
	else if (!strcmp(name, "endElement"))
		self->handle_endtag = item;
	else if (!strcmp(name, "characters"))
		self->handle_data = item;
	else if (!strcmp(name, "proc"))
		self->handle_proc = item;
	else if (!strcmp(name, "special"))
		self->handle_special = item;
	else if (!strcmp(name, "charref"))
		self->handle_charref = item;
	else if (!strcmp(name, "entityref"))
		self->handle_entityref = item;
	else if (!strcmp(name, "cdata"))
		self->handle_cdata = item;
	else if (!strcmp(name, "comment"))
		self->handle_comment = item;
	else {
		Py_DECREF(item);
        PyErr_SetString(PyExc_AttributeError, name);
		return NULL;
	}
    PyErr_Clear();
    Py_INCREF(Py_None);
    return Py_None;
}


/* -------------------------------------------------------------------- */
/* feed data to parser.  the parser processes as much of the data as
   possible, and keeps the rest in a local buffer. */

static int
feed(SGMLParserObject* self, char* string, int stringlen, int last)
{
    /* common subroutine for SGMLParser.feed and SGMLParser.close */

    int length;

    if (self->feed) {
        /* dealing with recursive feeds isn's exactly trivial, so
           let's just bail out before the parser messes things up */
        PyErr_SetString(PyExc_AssertionError, "recursive feed");
        return -1;
    }
	/* start document */
	if (!self->parsing) {
		if (self->handle_startdoc) {
			if (self->c) {
				if (((cfDoc *)self->handle_startdoc)(self->obj) == -1)
                    return -1;
			} else {
				PyObject* res;
				res = PyEval_CallObject((PyObject *)self->handle_startdoc, NULL);
				if (!res)
					return -1;
				Py_DECREF(res);
			}
		}
		self->parsing = 1;
	}
    /* append new text block to local buffer */
    if (!self->buffer) {
        length = stringlen;
        self->buffer = malloc(length);
        self->buffertotal = stringlen;
    } else {
        length = self->bufferlen + stringlen;
        if (length > self->buffertotal) {
            self->buffer = realloc(self->buffer, length);
            self->buffertotal = length;
        }
    }
    if (!self->buffer) {
        PyErr_NoMemory();
        return -1;
    }
    memcpy(self->buffer + self->bufferlen, string, stringlen);
    self->bufferlen = length;

    self->feed = 1;

    length = fastfeed(self);

    self->feed = 0;

    if (length < 0)
        return -1;

    if (length > self->bufferlen) {
        /* ran beyond the end of the buffer (internal error)*/
        PyErr_SetString(PyExc_AssertionError, "buffer overrun");
        return -1;
    }

    if (length > 0 && length < self->bufferlen)
        /* adjust buffer */
        memmove(self->buffer, self->buffer + length,
                self->bufferlen - length);

    self->bufferlen = self->bufferlen - length;

	/* if last of document */
	if (last) {
		if (self->handle_enddoc) {
			if (self->c) {
				if (((cfDoc *)self->handle_enddoc)(self->obj) == -1)
                    return -1;
			} else {
				PyObject* res;
				res = PyEval_CallObject(self->handle_enddoc, NULL);
				if (!res)
					return -1;
				Py_DECREF(res);
			}
		}
		self->bufferlen = 0;
		self->parsing = 0;
	}
	return self->bufferlen;
}

static char feed_doc[] =
"p.feed(string) -> int.\n\
\n\
Feed a chunk of data to the parser.\n\
Returns number of bytes parsed.";  

static PyObject*
sgml_feed(SGMLParserObject* self, PyObject* args)
{
    /* feed a chunk of data to the parser */

    char* string;
    int stringlen;
    if (!PyArg_ParseTuple(args, "s#", &string, &stringlen))
        return NULL;

    return Py_BuildValue("i", feed(self, string, stringlen, 0));
}

static char close_doc[] =
"p.close() -> None.\n\
\n\
Close the parsing and reset values.";  

static PyObject*
sgml_close(SGMLParserObject* self, PyObject* args)
{
    /* flush parser buffers */

    if (!PyArg_NoArgs(args))
        return NULL;

    return Py_BuildValue("i", feed(self, "", 0, 1));
}

static char parse_doc[] =
"p.parse(string) -> int.\n\
\n\
Parse data in one step.\n\
Returns number of bytes parsed.";  

static PyObject*
sgml_parse(SGMLParserObject* self, PyObject* args)
{
    /* feed a single chunk of data to the parser */

    char* string;
    int stringlen;
    if (!PyArg_ParseTuple(args, "s#", &string, &stringlen))
        return NULL;

    return Py_BuildValue("i", feed(self, string, stringlen, 1));
}


/* -------------------------------------------------------------------- */
/* type interface */

static PyMethodDef sgml_methods[] = {
    /* set document handler */
    {"setContentHandler", (PyCFunction) sgml_setContentHandler, METH_VARARGS, setContentHandler_doc},
    /* set document handler */
    {"setHandlerMethod", (PyCFunction) sgml_setHandlerMethod, METH_VARARGS, setHandlerMethod_doc},
    /* incremental parsing */
    {"feed", (PyCFunction) sgml_feed, METH_VARARGS, feed_doc},
    {"close", (PyCFunction) sgml_close, METH_VARARGS, close_doc},
    /* one-shot parsing */
    {"parse", (PyCFunction) sgml_parse, METH_VARARGS, parse_doc},
    {NULL, NULL}
};

static PyObject*  
sgml_getattr(SGMLParserObject* self, char* name)
{
    return Py_FindMethod(sgml_methods, (PyObject*) self, name);
}

PyTypeObject SGMLParser_Type = {
    PyObject_HEAD_INIT(NULL)
    0, /* ob_size */
    "SGMLParser", /* tp_name */
    sizeof(SGMLParserObject), /* tp_size */
    0, /* tp_itemsize */
    /* methods */
    (destructor)sgml_dealloc, /* tp_dealloc */
    0, /* tp_print */
    (getattrfunc)sgml_getattr, /* tp_getattr */
    0 /* tp_setattr */
};


/* ==================================================================== */
/* python module interface */

static PyMethodDef _functions[] = {
    {"SGMLParser", sgml_sgmlparser, 0},
    {"XMLParser", sgml_xmlparser, 0},
    {NULL, NULL}
};

DL_EXPORT(void)
initsgml(void)
{
    /* Patch object type */
    SGMLParser_Type.ob_type = &PyType_Type;
    Py_InitModule("sgml", _functions);
}

/* -------------------------------------------------------------------- */
/* Exported functions for C */

PyObject *
sgmlXMLParser(void *statobj)
{
	return sgml_new(1, 1, statobj);
}

PyObject *
sgmlSGMLParser(void *statobj)
{
	return sgml_new(0, 1, statobj);
}

int 
sgmlFeed(PyObject *self, char *s, int len)
{
    return feed((SGMLParserObject*)self, s, strlen(s), 0);	
}

void 
sgmlClose(PyObject *self)
{
    feed((SGMLParserObject*)self, "", 0, 1);	
}

int 
sgmlParse(PyObject *self, char *s, int len)
{
    ((SGMLParserObject*)self)->bufferlen = 0;
    ((SGMLParserObject*)self)->parsing = 0;
    return feed((SGMLParserObject*)self, s, len, 1);	
}

/* -------------------------------------------------------------------- */
/* the parser does it all in a single loop, keeping the necessary
   state in a few flag variables and the data buffer.  if you have
   a good optimizer, this can be incredibly fast. */

#define TAG 0x100
#define TAG_START 0x101
#define TAG_END 0x102
#define TAG_EMPTY 0x103
#define DIRECTIVE 0x104
#define DOCTYPE 0x105
#define PI 0x106
#define DTD_START 0x107
#define DTD_END 0x108
#define DTD_ENTITY 0x109
#define CDATA 0x200
#define ENTITYREF 0x400
#define CHARREF 0x401
#define COMMENT 0x800

static int
fastfeed(SGMLParserObject* self)
{
    CHAR_T *end; /* tail */
    CHAR_T *p, *q, *s; /* scanning pointers */
    CHAR_T *b, *t, *e; /* token start/end */

    int token;

    s = q = p = (CHAR_T*) self->buffer;
    end = (CHAR_T*) (self->buffer + self->bufferlen);
	
    while (p < end) {
		
        q = p; /* start of token */
		
        if (*p == '<') {
            int has_attr;

            /* <tags> */
            token = TAG_START;
            if (++p >= end)
                goto eol;

            if (*p == '!') {
                /* <! directive */
                if (++p >= end)
                    goto eol;
                token = DIRECTIVE;
                b = t = p;
                if (*p == '-') {
                    /* <!-- comment --> */
                    token = COMMENT;
                    b = p + 2;
                    for (;;) {
                        if (p+3 >= end)
                            goto eol;
                        if (p[1] != '-')
                            p += 2; /* boyer moore, sort of ;-) */
                        else if (p[0] != '-' || p[2] != '>')
                            p++;
                        else
                            break;
                    }
                    e = p;
                    p += 3;
                    goto eot;
                } else if (self->xml) {
                    /* FIXME: recognize <!ATTLIST data> ? */
                    /* FIXME: recognize <!ELEMENT data> ? */
                    /* FIXME: recognize <!ENTITY data> ? */
                    /* FIXME: recognize <!NOTATION data> ? */
                    if (*p == 'D' ) {
                        /* FIXME: make sure this really is a !DOCTYPE tag */
                        /* <!DOCTYPE data> or <!DOCTYPE data [ data ]> */
                        token = DOCTYPE;
                        self->doctype = MAYBE;
                    } else if (*p == '[') {
                        /* FIXME: make sure this really is a ![CDATA[ tag */
                        /* FIXME: recognize <![INCLUDE */
                        /* FIXME: recognize <![IGNORE */
                        /* <![CDATA[data]]> */
                        token = CDATA;
                        b = t = p + 7;
                        for (;;) {
                            if (p+3 >= end)
                                goto eol;
                            if (p[1] != ']')
                                p += 2;
                            else if (p[0] != ']' || p[2] != '>')
                                p++;
                            else
                                break;
                        }
                        e = p;
                        p += 3;
                        goto eot;
                    }
                }
            } else if (*p == '?') {
                token = PI;
                if (++p >= end)
                    goto eol;
            } else if (*p == '/') {
                /* </endtag> */
                token = TAG_END;
                if (++p >= end)
                    goto eol;
            }

            /* process tag name */
            b = p;
            if (!self->xml)
                while (ISALNUM(*p) || *p == '-' || *p == '.' ||
                       *p == ':' || *p == '?') {
                    *p = TOLOWER(*p);
                    if (++p >= end)
                        goto eol;
                }
            else
                while (ISALNUM(*p) || *p == '-' || *p == '.' || *p == '_' ||
                       *p == ':' || *p == '?') {
                    if (++p >= end)
                        goto eol;
                }

            t = p;

            has_attr = 0;

            if (*p == '/' && !self->xml) {
                /* <tag/data/ or <tag/> */
                token = TAG_START;
                e = p;
                if (++p >= end)
                    goto eol;
                if (*p == '>') {
                    /* <tag/> */
                    token = TAG_EMPTY;
                    if (++p >= end)
                        goto eol;
                } else
                    /* <tag/data/ */
                    self->shorttag = SURE;
                    /* we'll generate an end tag when we stumble upon
                       the end slash */

            } else {

                /* skip attributes */
                int quote = 0;
                int last = 0;
                while (*p != '>' || quote) {
                    if (!ISSPACE(*p)) {
                        has_attr = 1;
                        /* FIXME: note: end tags cannot have attributes! */
                    }
                    if (quote) {
                        if (*p == quote)
                            quote = 0;
                    } else {
                        if (*p == '"' || *p == '\'')
                            quote = *p;
                    }
                    if (*p == '[' && !quote && self->doctype) {
                        self->doctype = SURE;
                        token = DTD_START;
                        e = p++;
                        goto eot;
                    }
                    last = *p;
                    if (++p >= end)
                        goto eol;
                }

                e = p++;

                if (last == '/') {
                    /* <tag/> */
                    e--;
                    token = TAG_EMPTY;
                } else if (token == PI && last == '?')
                    e--;

                if (self->doctype == MAYBE)
                    self->doctype = 0; /* there was no dtd */

                if (has_attr)
                    ; /* FIXME: process attributes */

            }

        } else if (*p == '/' && self->shorttag) {

            /* end of shorttag. this generates an empty end tag */
            token = TAG_END;
            self->shorttag = 0;
            b = t = e = p;
            if (++p >= end)
                goto eol;

        } else if (*p == ']' && self->doctype) {

            /* end of dtd. this generates an empty end tag */
            token = DTD_END;
            /* FIXME: who handles the ending > !? */
            b = t = e = p;
            if (++p >= end)
                goto eol;
            self->doctype = 0;

        } else if (*p == '%' && self->doctype) {

            /* doctype entities */
            token = DTD_ENTITY;
            if (++p >= end)
                goto eol;
            b = t = p;
            while (ISALNUM(*p) || *p == '.')
                if (++p >= end)
                    goto eol;
            e = p;
            if (*p == ';')
                p++;

        } else if (*p == '&') {

            /* entities */
            token = ENTITYREF;
            if (++p >= end)
                goto eol;
            if (*p == '#') {
                token = CHARREF;
                if (++p >= end)
                    goto eol;
            }
            b = t = p;
            while (ISALNUM(*p) || *p == '.')
                if (++p >= end)
                    goto eol;
            e = p;
            if (*p == ';')
                p++;

        } else {
			
            /* raw data */
            if (++p >= end) {
                q = p;
                goto eol;
            }
            continue;
        }

	eot: /* end of token */
		
        if (q != s && self->handle_data) {
            /* flush any raw data before this tag */
			if (self->c) {
				if (((cfData *)self->handle_data)(self->obj, s, q-s) == -1)
                    return -1;
			} else {
				PyObject* res;
				res = PyObject_CallFunction(self->handle_data, "s#", s, (int)(q-s));
				if (!res)
					return -1;
				Py_DECREF(res);
			}
        }
		
        /* invoke callbacks */
        if (token & TAG) {
            if (token == TAG_END) {
                if (self->handle_endtag) {
					if (self->c) {
						char t1 = *(b+(t-b));
						*(b+(t-b)) = '\0';
						if (((cfTag *)self->handle_endtag)(self->obj, b) == -1)
                            return -1;
						*(b+(t-b)) = t1;
					} else {
						PyObject* res;
						res = PyObject_CallFunction(self->handle_endtag,"s#", b, (int)(t-b));
						if (!res)
							return -1;
						Py_DECREF(res);
					}
                }
            } else if (token == DIRECTIVE || token == DOCTYPE) {
                if (self->handle_special) {
					if (self->c) {
						char t1 = *(b+(e-b));
						*(b+(e-b)) = '\0';
						if (((cfTag *)self->handle_special)(self->obj, b) == -1)
                            return -1;
						*(b+(e-b)) = t1;
					} else {
						PyObject* res;
						res = PyObject_CallFunction(self->handle_special,"s#", b, 
                                (int)(e-b));
						if (!res)
							return -1;
						Py_DECREF(res);
					}
				}
            } else if (token == PI) {
                if (self->handle_proc) {
					int len = t-b;
					while (ISSPACE(*t))
						t++;
					if (self->c) {
						char t1 = *(b+len);
						char t2 = *(t+(e-t));
						*(b+len) = '\0';
						*(t+(e-t)) = '\0';
						if (((cfProc *)self->handle_proc)(self->obj, b, t) == -1)
                            return -1;
						*(b+len) = t1;
						*(t+(e-t)) = t2;
					} else {
						PyObject* res;
						res = PyObject_CallFunction(self->handle_proc,"s#s#", b, len,
                                t, (int)(e-t));
						if (!res)
							return -1;
						Py_DECREF(res);
					}
				}
            } else if (self->handle_starttag) {
                int len = t-b;
                while (ISSPACE(*t))
                    t++;
 				if (self->c) {					
					char t1 = *(b+len);
					char t2 = *(t+(e-t));
					*(b+len) = '\0';
					*(t+(e-t)) = '\0';
					if (((cfStartTag *)self->handle_starttag)(self->obj, b, t) == -1)
                        return -1;
					*(b+len) = t1;
					*(t+(e-t)) = t2;
				} else {
					PyObject* res;
					PyObject* attr;
					if (self->attrasstring) {
						if (e-t > 0)
							attr = PyString_FromStringAndSize(t, (int)(e-t));
						else {
							Py_INCREF(Py_None);
							attr = Py_None;
						}
					} else {
						attr = attrparse(t, e-t, self->xml);
						if (!attr)
							return -1;
					}
					res = PyObject_CallFunction(self->handle_starttag, "s#O", b, len, attr);
					Py_DECREF(attr);
					if (!res)
						return -1;
					Py_DECREF(res);
					if (token == TAG_EMPTY && self->handle_endtag) {
						res = PyObject_CallFunction(self->handle_endtag,"s#", b, len);
						if (!res)
							return -1;
						Py_DECREF(res);
					}
                }
            }
        } else if (token == ENTITYREF) {
			if (self->handle_entityref) {
 				if (self->c) {					
					char t1 = *(b+(e-b));
					*(b+(e-b)) = '\0';
					if (((cfTag *)self->handle_entityref)(self->obj, b) == -1)
                        return -1;
					*(b+(e-b)) = t1;
				} else {
					PyObject* res;
					res = PyObject_CallFunction(self->handle_entityref,"s#", b, (int)(e-b));
					if (!res)
						return -1;
					Py_DECREF(res);
				}
			} else if (self->handle_data) {
				entityStruct *entity;
				PyObject* res;
				for (entity = xmlEntities; entity->id != NULL; ++entity) {
					if (!strncmp(entity->id, b, e - b)) {
						if (self->c) {					
							if (((cfData *)self->handle_data)(self->obj, &entity->c, sizeof(CHAR_T)) == -1)
                                return -1;
						} else {
							res = PyObject_CallFunction(self->handle_data, "s#",
														&entity->c, sizeof(CHAR_T));
							if (!res)
								return -1;
							Py_DECREF(res);
						}
						break;
					}
				}
			}
        } else if (token == CHARREF && (self->handle_charref ||
                                        self->handle_data)) {
            PyObject* res;
            if (self->handle_charref)
 				if (self->c) {					
					char t1 = *(b+(e-b));
					*(b+(e-b)) = '\0';
					if (((cfTag *)self->handle_charref)(self->obj, b) == -1)
                        return -1;
					*(b+(e-b)) = t1;
				} else {
					res = PyObject_CallFunction(self->handle_charref,"s#", b, (int)(e-b));
					if (!res)
						return -1;
					Py_DECREF(res);
				}
            else if (self->handle_data) {
                /* fallback: handle charref's as data */
                /* FIXME: hexadecimal charrefs? */
                CHAR_T ch;
                CHAR_T *p;
                ch = 0;
                for (p = b; p < e; p++)
                    ch = ch*10 + *p - '0';
 				if (self->c) {					
					if (((cfData *)self->handle_data)(self->obj, &ch, sizeof(CHAR_T)) == -1)
                        return -1;
				} else {
					res = PyObject_CallFunction(self->handle_data,"s#", &ch, sizeof(CHAR_T));
					if (!res)
						return -1;
					Py_DECREF(res);
				}
            }
        } else if (token == CDATA && (self->handle_cdata ||
                                      self->handle_data)) {
            PyObject* res;
            if (self->handle_cdata) {
 				if (self->c) {					
					if (((cfData *)self->handle_cdata)(self->obj, b, e-b) == -1)
                        return -1;
				} else {
                    res = PyObject_CallFunction(self->handle_cdata,"s#", b, (int)(e-b));
					if (!res)
						return -1;
					Py_DECREF(res);
				}
            } else {
                /* fallback: handle cdata as plain data */
 				if (self->c) {					
					if (((cfData *)self->handle_data)(self->obj, b, e-b) == -1)
                        return -1;
				} else {
					res = PyObject_CallFunction(self->handle_data,"s#", b, (int)(e-b));
					if (!res)
						return -1;
					Py_DECREF(res);
				}
            }
        } else if (token == COMMENT && self->handle_comment) {
			if (self->c) {					
				if (((cfData *)self->handle_comment)(self->obj, b, e-b) == -1)
                    return -1;
			} else {
				PyObject* res;
				res = PyObject_CallFunction(self->handle_comment,"s#", b, (int)(e-b));
				if (!res)
					return -1;
				Py_DECREF(res);
			}
        }
		q = p; /* start of token */
		s = p; /* start of span */
	}
  eol: /* end of line */
	if (q != s && self->handle_data) {
		if (self->c) {					
			if (((cfData *)self->handle_data)(self->obj, b, e-b) == -1)
                return -1;
		} else {
			PyObject* res;
			res = PyObject_CallFunction(self->handle_data,"s#", s, (int)(q-s));
			if (!res)
				return -1;
			Py_DECREF(res);
		}
    }

    /* returns the number of bytes consumed in this pass */
    return ((char*) q) - self->buffer;
}

static PyObject*
attrparse(const CHAR_T* p, int len, int xml)
{
    PyObject* attrs;
    PyObject* key = NULL;
    PyObject* value = NULL;
    const CHAR_T* end = p + len;
    const CHAR_T* q;

    if (xml)
        attrs = PyDict_New();
    else
        attrs = PyList_New(0);

    while (p < end) {

        /* skip leading space */
        while (p < end && ISSPACE(*p))
            p++;
        if (p >= end)
            break;

        /* get attribute name (key) */
        q = p;
        while (p < end && *p != '=' && !ISSPACE(*p))
            p++;

        key = PyString_FromStringAndSize(q, (int)(p-q));
        if (key == NULL)
            goto err;

        while (p < end && ISSPACE(*p))
            p++;

        if (p < end && *p != '=') {

            /* attribute value not specified: set value to name */
            value = key;
            Py_INCREF(value);

        } else {

            /* attribute value found */

            if (p < end)
                p++;
            while (p < end && ISSPACE(*p))
                p++;

            q = p;
            if (p < end && (*p == '"' || *p == '\'')) {
                p++;
                while (p < end && *p != *q)
                    p++;
                value = PyString_FromStringAndSize(q+1, (int)(p-q-1));
                if (p < end && *p == *q)
                    p++;
            } else {
                while (p < end && !ISSPACE(*p))
                    p++;
                value = PyString_FromStringAndSize(q, (int)(p-q));
            }

            if (value == NULL)
                goto err;

        }

        if (xml) {

            /* add to dictionary */

            /* PyString_InternInPlace(&key); */
            if (PyDict_SetItem(attrs, key, value) < 0)
                goto err;
            Py_DECREF(key);
            Py_DECREF(value);

        } else {

            /* add to list */

            PyObject* res;
            res = PyTuple_New(2);
            if (!res)
                goto err;
            PyTuple_SET_ITEM(res, 0, key);
            PyTuple_SET_ITEM(res, 1, value);
            if (PyList_Append(attrs, res) < 0) {
                Py_DECREF(res);
                goto err;
            }
            Py_DECREF(res);

        }

        key = NULL;
        value = NULL;
        
    }

    return attrs;

  err:
    Py_XDECREF(key);
    Py_XDECREF(value);
    Py_DECREF(attrs);
    return NULL;
}
