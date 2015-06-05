/* 
 * dynload_as400.c
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
 *--------------------------------------------------------------------*/

#include "Python.h"
#include "importdl.h"
#include "as400misc.h"
#include <qusec.h>


const struct filedescr _PyImport_DynLoadFiletab[] = {
	{0, 0}
};


dl_funcptr _PyImport_GetDynLoadFunc(const char *fqname, const char *shortname,
				    const char *pathname, FILE *fp)
{
	char funcname[258];
	int actmark;
	_OPENPTR func;

	sprintf(funcname, "init%.200s", shortname);
	actmark = getSrvpgm((char*)shortname, Py_GetProgramLib());
	if (actmark > 0) {
		func = getFunction(actmark, funcname);
		return func;
	}
	return NULL;
}
