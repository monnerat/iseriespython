#ifndef SGMLMODULE_H
#define SGMLMODULE_H
#ifdef __cplusplus
extern "C" {
#endif

/* parser type definition */
typedef struct {
    PyObject_HEAD

    /* mode flags */
    int xml; /* 0=sgml/html 1=xml */
    int c; /* 0=python, 1=c */
    /* state attributes */
    int feed;
    int shorttag; /* 0=normal 2=parsing shorttag */
    int doctype; /* 0=normal 1=dtd pending 2=parsing dtd */
    int attrasstring; /* 0=normal 1=attribute as string */
    /* buffer (holds incomplete tags) */
    char* buffer;
    int bufferlen; /* current amount of data */
    int buffertotal; /* actually allocated */
    int parsing; /* document parsing in progress */

    /* user object*/
	void *obj;
    /* callbacks */
    void * handle_startdoc;
    void * handle_enddoc;
    void * handle_starttag;
    void * handle_endtag;
    void * handle_proc;
    void * handle_special;
    void * handle_charref;
    void * handle_entityref;
    void * handle_data;
    void * handle_cdata;
    void * handle_comment;

} SGMLParserObject;

/* sgmlmodule functions */

extern DL_IMPORT(PyTypeObject) SGMLParser_Type;

#define SGMLParser_Check(op) ((op)->ob_type == &SGMLParser_Type)

extern DL_IMPORT(PyObject *) sgmlSGMLParser(void *o);
extern DL_IMPORT(PyObject *) sgmlXMLParser(void *o);
extern DL_IMPORT(int) sgmlFeed(PyObject *self, char *s, int len);
extern DL_IMPORT(void) sgmlClose(PyObject *self);
extern DL_IMPORT(int) sgmlParse(PyObject *self, char *s, int len);

#ifdef __cplusplus
}
#endif
#endif /* !SGMLMODULE_H */
