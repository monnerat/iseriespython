#****************************************************************************
# Name:         create.py
# Description:  Create Python modules and programs
# Author:       Per Gummedal
#****************************************************************************
import sys, os, os400, time, traceback
import codecs, itertools
import shutil
import re
from datetime import date, timedelta

DSTLIB = 'PYTHON27'
DEBUG = False
ALWAYS = True

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

HDRS = [('include', []),
        ('as400', ['as400misc.h']),
       ]

INCLUDE_RE = re.compile('^(#\s*include\s+")([^"/]*)(\.h".*)$')

def mangledb(name):
    if name.startswith('_'):
        name = 'Z' + name[1:]
    name = name.replace('-', '').replace('_', '')[:10]
    return name

def needlib(lib):
    if not os.path.isdir('/QSYS.LIB/%s.LIB' % lib):
        os400.sndpgmmsg('Creating library %s' % lib)
        cmd = "CRTLIB LIB(%s) TYPE(*PROD) TEXT('Python language')" % lib
        if os.system(cmd):
            os400.sndpgmmsg('*** F A I L E D ***')

def should_create(target, dependencies = []):
    if not os.path.exists(target):
        return True
    timestamp = os.path.getmtime(target)
    return filter(lambda x: os.path.getmtime(x) > timestamp, dependencies) 

def install_header(lib, dstpath, srcpath, file, always):
    needlib(lib)
    if not os.path.isdir('/QSYS.LIB/%s.LIB/H.FILE' % lib):
        os400.sndpgmmsg('Creating DB source file %s/H' % lib)
        cmd = "CRTSRCPF FILE(%s/H) RCDLEN(240) TEXT('Python C header files')" %\
	      (lib)
        if os.system(cmd):
            os400.sndpgmmsg('*** F A I L E D ***')
    if not os.path.isdir('%s/include' % dstpath):
        os.makedirs('%s/include' % dstpath)
    os400.sndpgmmsg('Installing header file %s/%s' % (srcpath, file))
    if always or \
      should_create('%s/include/' % dstpath, ['%s/%s' % (srcpath, file)]):
        shutil.copy2('%s/%s' % (srcpath, file), '%s/include/' % dstpath)
    mbrname = mangledb(os.path.splitext(file)[0]).upper()
    mbrpath = '/QSYS.LIB/%s.LIB/H.FILE/%s.MBR' % (lib, mbrname)
    if always or \
      should_create(mbrpath, ['%s/%s' % (srcpath, file)]):
        ofile = open(mbrpath, 'w')
        with open('%s/%s' % (srcpath, file), 'r') as ifile:
            for line in ifile:
                m = INCLUDE_RE.match(line)
                if (m):
                    line = m.group(1) + mangledb(m.group(2)) + m.group(3) + '\n'
                ofile.write(line)
        ofile.close()

def install_allhdrs(dirname, targetdir = None, lib = DSTLIB, always = ALWAYS):
    if not targetdir:
        targetdir = dirname
    for subdir, files in HDRS:
        if files == []:
            for fn in os.listdir(dirname + '/' + subdir):
                if fn.endswith('.h'):
                    files.append(fn)
        for fn in files:
            install_header(lib, targetdir, dirname + '/' + subdir, fn, always)

def compile(lib, modname, path, debug, tgtrls = '*CURRENT', always = ALWAYS):
    needlib(lib)
    if always or should_create('/QSYS.LIB/%s.LIB/%s.MODULE' % (lib, modname), \
      [path]):
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

def create_srvpgm(srvpgm, lib = DSTLIB, tgtrls = '*CURRENT', always = ALWAYS):
    needlib(lib)
    srvpgm = srvpgm.lower()
    if srvpgm not in SPGMDICT:
        os400.sndpgmmsg('*** F A I L E D ***   srvpgm %s not valid' % \
                        srvpgm)
    bnds, modules = SPGMDICT[srvpgm]
    if always or \
      should_create('/QSYS.LIB/%s.LIB/%s.SRVPGM' % (lib, srvpgm), \
                    ('/QSYS.LIB/%s.LIB/%s.%s' % (lib, x, y) for (x, y) in \
                     itertools.chain(([x, 'SRVPGM'] for x in bnds), \
                                     ([x, 'MODULE'] for x in modules)))):
        os400.sndpgmmsg('Creating *srvpgm %s/%s' % (lib, srvpgm.upper()))
        cmd = "CRTSRVPGM SRVPGM(%s/%s) MODULE(" % (lib, srvpgm) + \
              ' '.join('%s/%s' % (lib, x) for x in modules) + \
              ") EXPORT(*ALL) TGTRLS(%s) ACTGRP(%s) " % (tgtrls, lib) + \
              "STGMDL(*TERASPACE) ALWLIBUPD(*YES)"
        if bnds:
            cmd += " BNDSRVPGM(" + \
                   ' '.join('%s/%s' % (lib, x) for x in bnds) + \
                   ")"
        if os.system(cmd):
            os400.sndpgmmsg('*** F A I L E D ***')

def create_pgm(lib = DSTLIB, tgtrls = '*CURRENT', always = ALWAYS):
    needlib(lib)
    if always or should_create('/QSYS.LIB/%s.LIB/PYTHON.PGM' % lib, \
      ['/QSYS.LIB/%s.LIB/PYTHON.MODULE' % lib, \
       '/QSYS.LIB/%s.LIB/PYTHON.SRVPGM' % lib]):
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

def create_module(dirname, filename = '', debug = DEBUG, tgtrls = '*CURRENT', lib = DSTLIB, always = ALWAYS):
    cmd = "ADDENVVAR ENVVAR(INCLUDE) REPLACE(*YES) VALUE('" + \
          ':'.join(INCLUDE) + "')"
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
        modname = mangledb(fn[:-2].upper())
        if dirname1 in PREFIX and not modname.startswith(PREFIX[dirname1]):
            modname = (PREFIX[dirname1] + modname)[:10]
        if modname.lower() not in modules:
            continue
        compile(lib, modname, os.path.join(dirname, fn), debug, tgtrls, always)

def create_all(dirname, debug = DEBUG, tgtrls = '*CURRENT', lib = DSTLIB, always = ALWAYS):
    create_allmod(dirname, debug, tgtrls, lib, always)
    create_allpgm(tgtrls, lib, always)

def create_allmod(dirname, debug = DEBUG, tgtrls = '*CURRENT', lib = DSTLIB, always = ALWAYS):
    create_module(os.path.join(dirname, 'python'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'parser'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'objects'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'modules'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'modules/expat'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'modules/zlib'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'modules/_io'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'modules/_multiprocessing'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'sqlite'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'modules/_sqlite'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'as400'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'pycrypto'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'bz2'), '', debug, tgtrls, lib, always)
    create_module(os.path.join(dirname, 'ibm_db'), '', debug, tgtrls, lib, always)

def create_allpgm(tgtrls = '*CURRENT', lib = DSTLIB, always = ALWAYS):
    # create all srvpgm
    for srvpgm, parms in SPGM:
        create_srvpgm(srvpgm, lib, tgtrls, always)
    create_pgm(lib, tgtrls, always)


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
    elif command == 'headers':
        install_allhdrs(*args)

if __name__ == "__main__":
    main(*sys.argv[1:])
