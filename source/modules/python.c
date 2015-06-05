/* Minimal main program -- everything is loaded from the library */

#ifdef __ILEC400__
#include "Python.h"
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include "as400misc.h"
#else
#include "Python.h"
#endif
#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif

int
main(int argc, char *__ptr128 *__ptr128 argv)
{
#ifdef __ILEC400__
    int result, i, totallen;
    char * tsptr, *p;
    char * arg[64];
    short parm_count = 0;

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");
    /* copy parameters to teraspace and convert to utf */
    /* first count required size */
    /* if from command the third and last parameter says number of parameters */
    if (argc == 3 && argv[2][0] == '\0' && (argv[2][1] | 0x1f) == 0x1f) {
        /* we assume that the maximum number of parameters is 32 */
        /* the parmeters are all 256 of length */
        memcpy(&parm_count, argv[2], 2);
        totallen = 256 * parm_count + strlen(argv[0]) + strlen(argv[1]);
    } else {
        totallen = 0;
        for (i = 0; i < argc; i++)
            totallen += strlen(argv[i]);
    }
    tsptr = (char *)malloc(totallen * 2);
    if (tsptr == NULL)
        abort();
    arg[0] = tsptr;
    /* first parameter is the program name */
    p = arg[0];
    strcpy(p, argv[0]);
    strLenToUtf(p, strlen(p), p);
    if (argc > 1)
    {
        /* second parameter is the script */
        arg[1] = arg[0] + strlen(arg[0]) + 1;
        p = arg[1];
        strcpy(p, argv[1]);  
        strLenToUtf(p, strlen(p), p);
        if (strlen(p) == 0)
            argc = 1;
    }
    /* if from command the third and last parameter says number of parameters */
    if (argc == 3 && argv[2][0] == '\0' && (argv[2][1] | 0x1f) == 0x1f) {
        char *s;
        s = argv[2] + 2;
        for (i = 2; i < parm_count + 2; i++) {
            arg[i] = arg[i-1] + strlen(arg[i-1]) + 1;
            p = arg[i];
            strncpy(p, s, 256);
            strLenToUtf(p, 256, p);
            s += 256;
        }
        argc = parm_count + 2;
    } else {
        for (i = 2; i < argc; i++) {
            arg[i] = arg[i-1] + strlen(arg[i-1]) + 1;
            p = arg[i];
            strcpy(p, argv[i]);
            strToUtf(p, p, strlen(p));
        }
    }
    result = Py_Main(argc, arg);
    free(tsptr);
    /* send program error message */
    if (result == 0)
        exit(EXIT_SUCCESS);
    else
        abort();
}
#else
	/* 754 requires that FP exceptions run in "no stop" mode by default,
	 * and until C vendors implement C99's ways to control FP exceptions,
	 * Python requires non-stop mode.  Alas, some platforms enable FP
	 * exceptions by default.  Here we disable them.
	 */
#ifdef __FreeBSD__
	fp_except_t m;

	m = fpgetmask();
	fpsetmask(m & ~FP_X_OFL);
#endif
	return Py_Main(argc, argv);
}
#endif
