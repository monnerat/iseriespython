#****************************************************************************
# Name:         create.py
# Description:  Create Python modules and programs
# Author:       Per Gummedal
#****************************************************************************
import sys, os, os400, time, traceback
import codecs, itertools
from datetime import date, timedelta

INCLUDE = ['/python27/source/include','/python27/source/python',
          '/python27/source/as400','/python27/source/modules/expat',
          '/python27/source/modules/zlib','/python27/source/bz2',
          '/python27/source/sqlite','/python27/source/ibm_db',
          '/qibm/proddata/ilec','/qibm/proddata/ilec/include',
          '/qibm/include','/qibm/include/sys','/usr/include']

SKIPFILES = {'python':['dynload*','thread*'],
             'zlib':['example.c'],
             'sqlite':['shell.c']
           }

PREFIX = {'zlib':'Z_', 'bz2':'BZ'}

SPGM = [('python'    , ([],['abstract','acceler','asdl','ast','as400misc','bitset',
                       'bltinmodul','boolobject','bufferobje','bytearrayo',
                       'bytesmetho','capsule','cellobject','ceval','classobjec',
                       'cobject','codecs','codeobject','compile','complexobj',
                       'config','datetimemo','descrobjec','dictobject','dtoa','dynloadas4',
                       'enumobject','errors','exceptions','fileobject','firstsets',
                       'floatobjec','formatters','formatteru','frameobjec','frozen',
                       'frozenmain', 'funcobject','future','gcmodule','genobject',
                       'getargs','getbuildin','getcompile',
                       'getcopyrig','getopt','getpath','getplatfor','getversion',
                       'graminit','grammar','grammar1','import','importdl',
                       'intobject','iterobject','listnode',
                       'listobject','longobject','main','marshal','memoryobje',
                       'metagramma','methodobje','modsupport','moduleobje',
                       'myreadline','mysnprintf','mystrtoul','node','object',
                       'obmalloc','operator','parser','parsetok','peephole','pgen',
                       'posixmodul','printgramm','pyarena','pyctype','pyfpe',
                       'pymath','pystate','pystrcmp','pystrtod','pythonast',
                       'pythonrun','rangeobjec','setobject','signalmodu',
                       'sliceobjec','strdup','stringobje','stropmodul',
                       'structmemb','structseq','symtable','sysmodule',
                       'thread','timemodule','tokenizer','traceback','tupleobjec',
                       'typeobject','unicodecty','unicodeobj','weakrefobj',
                       'zcodecsmod','zipimport','zlocalemod','zwarnings',
                       ])),
        ('array'     , (['python'],['arraymodul'])),
        ('audioop'   , (['python'],['audioop'])),
        ('binascii'  , (['python'],['binascii'])),
        ('bz2'       , (['python'],['bz2module','bzip2','bzlib',
                        'bzblocksor','bzcompress','bzcrctable',
                        'bzdecompre','bzhuffman', 'bzrandtabl'])),
        ('cmath'     , (['python'],['cmathmodul','zmath'])),
        ('cpickle'   , (['python'],['cpickle'])),
        ('cstringio' , (['python'],['cstringio'])),
        ('errno'     , (['python'],['errnomodul'])),
        ('fcntl'     , (['python'],['fcntlmodul'])),
        ('file400'   , (['python'],['file400mod'])),
        ('fpectl'    , (['python'],['fpectlmodu'])),
        ('future_bui', (['python'],['futurebuil'])),
        ('grp'       , (['python'],['grpmodule'])),
        ('ibm_db'    , (['python'],['ibmdb'])),
        ('imageop'   , (['python'],['imageop'])),
        ('itertools' , (['python'],['itertoolsm'])),
        ('zmath'     , (['python'],['zmath'])),
        ('math'      , (['python'],['mathmodule','zmath'])),
        ('mmap'      , (['python'],['mmapmodule'])),
        ('osutil'    , (['python'],['osutil'])),
        ('parser'    , (['python'],['parsermodu'])),
        ('pure'      , (['python'],['puremodule'])),
        ('pwd'       , (['python'],['pwdmodule'])),
        ('pyexpat'   , (['python'],['pyexpat','xmlparse','xmlrole','xmltok'])),
        ('select'    , (['python'],['selectmodu'])),
        ('sgml'      , (['python'],['sgmlmodule'])),
        ('thread'    , (['python'],['threadmodu'])),
        ('timing'    , (['python'],['timingmodu'])),
        ('unicodedat', (['python'],['unicodedat'])),
        ('zbisect'   , (['python'],['zbisectmod'])),
        ('zcollectio', (['python'],['zcollectio'])),
        ('zdb2'      , (['python'],['zdb2'])),
        ('zcsv'      , (['python'],['zcsv'])),
        ('zelementtr', (['python','pyexpat'],['zelementtr'])),
        ('zfunctools', (['python'],['zfunctools'])),
        ('zheapq'    , (['python'],['zheapqmodu'])),
        ('zhotshot'  , (['python'],['zhotshot'])),
        ('zjson'     , (['python'],['zjson'])),
        ('zlib'      , (['python'],['zlibmodule','z_adler32','z_compress','z_crc32',
                        'z_deflate','z_gzio','z_infback','z_inffast','z_inflate',
                        'z_inftrees','z_minigzip','z_trees','z_uncompr','z_zutil'])),
        ('zlsprof'   , (['python'],['zlsprof','rotatingtr'])),
        ('zmd5'      , (['python'],['md5','md5module'])),
        ('i5pgm'     , (['python'],['i5pgm'])),
        ('zos400'    , (['python'],['zos400'])),
        ('zrandom'   , (['python'],['zrandommod'])),
        ('zsha'      , (['python'],['shamodule'])),
        ('zsha256'   , (['python'],['sha256modu'])),
        ('zsha512'   , (['python'],['sha512modu'])),
        ('zsocket'   , (['python'],['socketmodu'])),
        ('zsre'      , (['python'],['zsre'])),
        ('zstruct'   , (['python'],['zstruct'])),
        ('zsymtable' , (['python'],['symtablemo'])),
        ('ztestcapi' , (['python'],['ztestcapim'])),
        ('zweakref'  , (['python'],['zweakref'])),
        ('zio'       , (['python'],['ziomodule','bufferedio','bytesio','fileio',
                        'iobase','stringio','textio'])),
        ('zmultiproc', (['python'],['multiproce','semaphore','socketconn'])),
        ('zsqlite3'  , (['python'],['cache','connection','cursor','microproto',
			'module','preparepro','row','statement','util','sqlite3'])),
        ('zaes', (['python'],['aes'])),
        ('zarc2', (['python'],['arc2'])),
        ('zarc4', (['python'],['arc4'])),
        ('zcast', (['python'],['cast'])),
        ('zdes', (['python'],['des'])),
        ('zdes3', (['python'],['des3'])),
        ('zmd2', (['python'],['md2'])),
        ('zmd4', (['python'],['md4'])),
        ('zripemd160', (['python'],['ripemd160'])),
        ('zsha224', (['python'],['sha224'])),
        ('zsha384', (['python'],['sha384'])),
        ('zxor', (['python'],['xor'])),
        ('zblowfish', (['python'],['blowfish'])),
        ('zcounter', (['python'],['zcounter'])),
        ('strxor', (['python'],['strxor'])),
        ]

SPGMDICT = dict((x[0],x[1]) for x in SPGM)

def compile(lib, modname, path, debug, tgtrls = '*CURRENT'):
    os400.sndpgmmsg('Compiling %s %s' % (path, debug))
    if debug:
        cmd = "CRTCMOD MODULE(%s/%s) " % (lib, modname) + \
              "SRCSTMF('%s') OUTPUT(*PRINT) OPTIMIZE(10) " % path + \
              "INLINE(*OFF) DBGVIEW(*ALL) SYSIFCOPT(*IFS64IO) " + \
              "LOCALETYPE(*LOCALEUTF) FLAG(10) TERASPACE(*YES *TSIFC) " + \
              "STGMDL(*TERASPACE) TGTRLS(%s) DTAMDL(*LLP64)" % tgtrls
    else:
        cmd = "CRTCMOD MODULE(%s/%s) " % (lib, modname) + \
              "SRCSTMF('%s') OPTIMIZE(40) " % path + \
              "INLINE(*ON *AUTO) DBGVIEW(*NONE) SYSIFCOPT(*IFS64IO) " + \
              "LOCALETYPE(*LOCALEUTF) FLAG(10) TERASPACE(*YES *TSIFC) " + \
              "STGMDL(*TERASPACE) TGTRLS(%s) DTAMDL(*LLP64)" % tgtrls
    if os.system(cmd):
        os400.sndpgmmsg('*** F A I L E D ***')

def create_srvpgm(srvpgm, lib = 'PYTHON27', tgtrls = '*CURRENT'):
    os400.sndpgmmsg('Creating *srvpgm %s/%s' % (lib, srvpgm))
    srvpgm = srvpgm.lower()
    if srvpgm not in SPGMDICT:
        os400.sndpgmmsg('*** F A I L E D ***   srvpgm  %s not valid' % srvpgm)
    bnds, modules = SPGMDICT[srvpgm]
    cmd = "CRTSRVPGM SRVPGM(%s/%s) MODULE(" % (lib, srvpgm) + \
          ' '.join('%s/%s' % (lib,x) for x in modules) + \
          ") EXPORT(*ALL) TGTRLS(%s) ACTGRP(%s) " % (tgtrls, lib) + \
          "STGMDL(*TERASPACE) ALWLIBUPD(*YES)"
    if bnds:
        cmd += " BNDSRVPGM(" + \
               ' '.join('%s/%s' % (lib,x) for x in bnds) + \
               ")"
    if os.system(cmd):
        os400.sndpgmmsg('*** F A I L E D ***')

def create_pgm(lib = 'PYTHON27', tgtrls = '*CURRENT'):
    os400.sndpgmmsg('Creating *pgm %s/PYTHON' % lib)
    cmd = "CRTPGM PGM(%s/PYTHON) BNDSRVPGM(%s/PYTHON) " % (lib, lib) + \
          "ALWLIBUPD(*YES) USRPRF(*OWNER) TGTRLS(%s) " % tgtrls + \
          "ACTGRP(%s) STGMDL(*TERASPACE)" % lib
    if os.system(cmd):
        os400.sndpgmmsg('*** F A I L E D ***')

def skip(dir, fn):
    files = SKIPFILES.get(dir)
    if files:
        if fn in files:
            return True
        for f in files:
            if f.endswith('*') and fn.startswith(f[-1]):
                return True
    return False

def create_module(dirname, filename = '', debug = False, tgtrls = '*CURRENT', lib = 'PYTHON27'):
    cmd = "chgenvvar envvar(INCLUDE) value('" + \
          ':'.join([dirname] + INCLUDE) + "')"
    os.system(cmd)
    debug = debug in (True, 'True', '1')
    dirname1 = os.path.split(dirname)[-1]
    os400.sndpgmmsg(dirname)
    # List of valid modules
    modules = set(itertools.chain(*(x[1][1] for x in SPGM)))
    modules.add('python')
    for fn in os.listdir(dirname):
        fn = fn.lower()
        if not fn.endswith('.c'):
            continue
        if not fn.startswith(filename):
            continue
        if skip(dirname1, fn):
            os400.sndpgmmsg('Skipped %s' % fn)
            continue
        modname = fn[:-2].upper()
        if modname.startswith('_'):
            modname = 'Z' + modname[1:]
        modname = modname.replace('-','').replace('_','')[:10]
        if dirname1 in PREFIX and not modname.startswith(PREFIX[dirname1]):
            modname = (PREFIX[dirname1] + modname)[:10]
        if modname.lower() not in modules:
            continue
        compile(lib, modname, os.path.join(dirname, fn), debug, tgtrls)

def create_all(dirname, debug = False, tgtrls = '*CURRENT', lib = 'PYTHON27'):
    create_allmod(dirname, debug, tgtrls, lib)
    create_allpgm(tgtrls, lib)

def create_allmod(dirname, debug = False, tgtrls = '*CURRENT', lib = 'PYTHON27'):
    create_module(os.path.join(dirname, 'python'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'parser'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'objects'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'modules'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'modules/expat'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'modules/zlib'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'modules/_io'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'modules/_multiprocessing'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'sqlite'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'modules/_sqlite'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'as400'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'pycrypto'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'bz2'), '', debug, tgtrls, lib)
    create_module(os.path.join(dirname, 'ibm_db'), '', debug, tgtrls, lib)

def create_allpgm(tgtrls = '*CURRENT', lib = 'PYTHON27'):
    # create all srvpgm
    for srvpgm, parms in SPGM:
        create_srvpgm(srvpgm, lib, tgtrls)
    create_pgm(lib, tgtrls)


def main(command, *args):
    if command == 'module':
        create_module(*args)
    elif command == 'pgm':
        create_pgm(*args)
    elif command == 'srvpgm':
        create_srvpgm(*args)
    elif command == 'allmod':
        create_allmod(*args)
    elif command == 'allpgm':
        create_allpgm(*args)
    elif command == 'all':
        create_all(*args)

if __name__ == "__main__":
    main(*sys.argv[1:])
