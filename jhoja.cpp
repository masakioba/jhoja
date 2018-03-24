/* jhoja Ver 1.01
 Java class file diss assembler for Jasmin assembler code.
SWAG javap2 based.(http://www.swag.uwaterloo.ca/javap2/index.html)
Apache v2 license.
http://www.nabeta.tk/en/
admin@nabeta.tk
*/
#define WIN32
#if defined(WIN32)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(WIN32)
#include <io.h>
#include <string.h>

#define open _open
#define read _read
#define close _close
#define alloca _alloca

typedef unsigned char    u1T;
typedef unsigned short    u2T;
typedef unsigned __int32  u4T;
typedef signed   __int32  s4T;
typedef unsigned __int64  u8T;
#else
#include <wchar.h>
#include <errno.h>
#include <unistd.h>
#include <alloca.h>

#define _O_BINARY 0

typedef unsigned char    u1T;
typedef unsigned short    u2T;
typedef unsigned int    u4T;
typedef signed int      s4T;
typedef unsigned long long  u8T;

#endif

// http://java.sun.com/docs/books/vmspec/2nd-edition/html/ClassFile.doc.html

int Out = 1;
u4T Label[100000];
int Lcnt = 0;

typedef enum {
  CONSTANT_Null               =  0,  // An unused constant_pool entry
  CONSTANT_Utf8               =  1,
  CONSTANT_Integer            =  3,
  CONSTANT_Float              =  4,
  CONSTANT_Long               =  5,
  CONSTANT_Double             =  6,
  CONSTANT_Class              =  7,
  CONSTANT_String             =  8,
  CONSTANT_Fieldref           =  9,
  CONSTANT_Methodref          = 10,
  CONSTANT_InterfaceMethodref = 11,
  CONSTANT_NameAndType        = 12  
} constantE;

typedef struct {
  u2T        attribute_name_index;
  u2T        seen;
     u4T        attribute_length;
     u1T        *infoP;
} attribute_infoT;

typedef struct {
  constantE  tag;
  union {
    struct {
      u2T      index1;
      u2T      index2;
    } ref;
    u1T        byte;
    u2T         *stringP;
    int        ival;
    float      fval;
    long long    lval;
    double      dval;
    u4T        u4val;
    u8T        u8val;
  } u;
} cp_infoT;

typedef struct {
  u2T        access_flags;
  u2T        name_index;
  u2T        descriptor_index;
  u2T        attributes_count;
  attribute_infoT  *attributesP;
} field_infoT;

typedef field_infoT  method_infoT;

typedef struct {
    cp_infoT      *m_constant_poolP;
    u2T           *m_interfacesP;
    field_infoT    *m_fieldsP;
    method_infoT   *m_methodsP;
  attribute_infoT *m_attributesP;

  u4T      m_magic;
  u2T      m_minor_version;
  u2T      m_major_version;
    u2T      m_constant_pool_count;
    u2T      m_access_flags;
    u2T     m_this_class;
    u2T      m_super_class;
    u2T      m_interfaces_count;
    u2T      m_fields_count;
    u2T     m_methods_count;
    u2T      m_attributes_count;
} classFileT;

typedef struct {
  int      start_pc;
  int      line_number;
} lineNumberT;

typedef struct {
  int      start_pc;
  int      end_pc;
  const u2T  *nameP;
  const u2T  *descriptorP;
  u2T      index;
} localVariableT;

static u1T  *g_imageP        = 0;
static int  g_image_maxsize  = -1;
static int  g_image_size     = 0;
static u1T  *g_needP         = 0;
static int  g_need_maxsize   = -1;
static int  g_need_size      = 0;
static const char *g_nameP   = 0;
static int  g_verbose        = 0;
static int  g_demangle       = 0;
static int  g_tables         = 0;

static classFileT  g_cf;
int Guardf = 0; /* add decompile protection code; */

const char *
constantName(int i)
{
  static char  buffer[32];

  static const char *constant_name[] = {
    "? 0 ?",
    "Utf8",
    "? 2 ?",
    "int",
    "float",
    "long",
    "double",
    "class",
    "string",
    "field",
    "method",
    "interface",
    "name&type"
  };
  if (i < 0 || i > 12) {
    sprintf(buffer, "? %d ?", i);
    return(buffer);
  }
  return(constant_name[i]);
}

#define Align(X) ((X) + 3) & ~3

u1T  *
getU2(u1T *startP, u2T *valP)
{
  register  u1T  *P = startP;
  register  u2T  val;

  assert(!(((long) valP) & 1));
  val   = (*P++ << 8);
  val  |= *P++;
  *valP = val;
  return(P);
}
  
u1T  *
getU4(u1T *startP, u4T *valP)
{
  register  u1T  *P = startP;
  register  u4T  val;

  assert(!(((long) valP) & 3));
  val   = (*P++ << 24);
  val  |= (*P++ << 16);
  val  |= (*P++ << 8);
  val  |= *P++;
  *valP = val;
  return (P);
}
  
u1T  *
getU8(u1T *startP, u8T *valP)
{
  register  u1T  *P = startP;

  u8T  val;

//  assert(!(((int) valP) & 7));
  val   = (((u8T) *P) << 56); ++P;
  val  |= (((u8T) *P) << 48); ++P;
  val  |= (((u8T) *P) << 40); ++P;
  val  |= (((u8T) *P) << 32); ++P;
  val  |= (((u8T) *P) << 24); ++P;
  val  |= (((u8T) *P) << 16); ++P;
  val  |= (((u8T) *P) << 8);  ++P;
  val  |= *P++;
  *valP = val;
  return (P);
}
  
int
utf8lth(int  bytes, const u1T *compressedP)
{
  const u1T  *P    = compressedP;
  const u1T  *endP = P + bytes;
  int      ret   = 0;
  u1T      c;

  for (; P < endP; ++P) {
    c = *P;
    if (c & 0x80) {
      if (!(c & 0x20)) {
        // Two   byte encoding
        assert((c & 0xd0) == 0xc0);    // x
        c = *++P;
        assert((c & 0xc0) == 0x80);    // y
      } else {
        // Three byte encoding

        //assert((c & 0xd0) == 0xd0);    // x BUG??
        c = *++P;
        assert((c & 0xc0) == 0x80);    // y
        c = *++P;
        assert((c & 0xc0) == 0x80);    // z
    }  }
    ++ret;
  }
  assert(P == endP);
  return(ret);
}

int ucs2to_utf8(const u2T *ucs2s,char *utf8s)
{
  int ucs2cnt, utf8cnt = 0;

  ucs2cnt = 0;
  utf8cnt = 0;
  while(1){
    if(ucs2s[ucs2cnt] == (u2T)0){
      break;
    }
    if ( (unsigned short)ucs2s[ucs2cnt] <= 0x007f) {
        utf8s[utf8cnt] = ucs2s[ucs2cnt];
      utf8cnt += 1;
    } else if ( (unsigned short)ucs2s[ucs2cnt] <= 0x07ff) {
        utf8s[utf8cnt] = ((ucs2s[ucs2cnt] & 0x07C0) >> 6 ) | 0xc0; // 2002.08.17 fix 
        utf8s[utf8cnt+1] = (ucs2s[ucs2cnt] & 0x003f) | 0x80;
      utf8cnt += 2;
    }
    else
    if(0xd800 <= ucs2s[ucs2cnt] && ucs2s[ucs2cnt] <= 0xdbff && 0xdc00 <= ucs2s[ucs2cnt+1] && ucs2s[ucs2cnt+1] <= 0xdfff){
      //surrogate pair
      unsigned long sg;
      sg = (ucs2s[ucs2cnt] & 0x3ff) << 10;
      sg |= (ucs2s[ucs2cnt+1] & 0x3ff);
      sg += 0x10000;
      utf8s[utf8cnt] = ((sg & 0x1c00000) >> 18) | 0xf0;
      utf8s[utf8cnt+1] = ((sg & 0x3f000) >> 12) | 0x80;
      utf8s[utf8cnt+2] = ((sg & 0xfc0) >> 6) | 0x80;
      utf8s[utf8cnt+3] = (sg & 0x003f) | 0x80;
      ucs2cnt++;
      utf8cnt += 4;
    }
    else{
      utf8s[utf8cnt] = ((ucs2s[ucs2cnt] & 0xf000) >> 12) | 0xe0; // 2002.08.04 fix
      utf8s[utf8cnt+1] = ((ucs2s[ucs2cnt] & 0x0fc0) >> 6) | 0x80;
      utf8s[utf8cnt+2] = (ucs2s[ucs2cnt] & 0x003f) | 0x80;
      utf8cnt += 3;
    }
    ucs2cnt++;
  }
  utf8s[utf8cnt] = '\0';

  return utf8cnt;
}

static void
dumpUnicode(const u2T *stringP)
{
  //u2T  c;
  //const register u2T *P;
  char *cp;
  char bc;
  char utf8s[15000];
  //if(Out)printf("%S", stringP);
  utf8s[0] = 0;
  ucs2to_utf8(stringP,(char *)utf8s);
  for(cp = utf8s; bc = *cp; ++cp){
    if(bc == '"'){
      if(Out)fprintf(stdout,"\\\"");
    }
    else
    if(bc == '\\'){
      if(Out)fprintf(stdout,"\\\\");
    }
    else
    if(bc == (char)0xa){
      // put "\n"
      if(Out)fprintf(stdout,"\\n");
    }
    else
    if(bc == (char)0xd){
      // put "\r"
      if(Out)fprintf(stdout,"\\r");
    }
    else
    if(Out)fputc(bc, stdout);
  }
/*
  for (P = stringP; c = *P; ++P) {
    if(*P >= 128){
      u2T ucs2[10];
      ucs2s[0] = *P;
      ucs2s[1] = 0;
      ucs2to_utf8(ucs2s,(char *)utf8s);
      u2T b;
      unsigned char a;
      b = *P;
      b >>= 8;
      a = b;

      if(Out)fprintf(stdout,utf8s);
    }
    else
    if(c == (u2T)'"'){
      if(Out)fprintf(stdout,"\\\"");
    }
    else
    if(c == (u2T)'\\'){
      if(Out)fprintf(stdout,"\\\\");
    }
    else
    if(c == (u2T)0xa){
      // put "\n"
      if(Out)fprintf(stdout,"\\n");
    }
    else
    if(c == (u2T)0xd){
      // put "\r"
      if(Out)fprintf(stdout,"\\r");
    }
    else
    if(Out)fputc(c, stdout);
  }
*/
}
  
u1T *
getUtf8(int bytes, u1T *compressedP, u2T **stringPP)
{
  u1T  *P       = compressedP;
  u1T  *endP    = P + bytes;
  u2T *startP  = *stringPP;
  u2T  *stringP = startP;
  u1T  c;
  u2T  val;

  for (; P < endP; ++P) {
    c = *P;
    if (!(c & 0x80)) {
      // One byte encoding
      val = c;
    } else {
      if (!(c & 0x20)) {
        // Two byte encoding
        val  = (c & 0x1F) << 6;
      } else {
        // Three byte encoding
        val  = (c & 0xF) << 12;
        c    = *++P;
        val |= (c & 0x3F) << 6;
      }
      c    = *++P;
      val |= c & 0x3F;
    }
    *stringP++ = val;
  }
  *stringP++ = 0;
  if ((stringP - startP) & 1) {
    // Align
    *stringP++ = 0;
  }

  *stringPP  = stringP;
  assert(P == endP);
  return(endP);
}

static void
dump_access(u2T  access)
{
  static const char *accessName[] = {
    /* 0x0001 */  "public ",      // ACC_PUBLIC 0x0001 Visible to everyone  Class, Method, Variable 
    /* 0x0002 */  "private ",      // ACC_PRIVATE 0x0002 Visible only to the defining class  Method, Variable 
    /* 0x0004 */  "protected ",    // ACC_PROTECTED 0x0004 Visible to subclasses  Method, Variable 
    /* 0x0008 */  "static ",      // ACC_STATIC 0x0008 Variable or method is static  Method, Variable
    /* 0x0010 */  "final ",      // ACC_FINAL 0x0010 No further subclassing, overriding, or assignment after initialization Class, Method, Variable 
    /* 0x0020 */  "synchronized ",  // ACC_SYNCHRONIZED 0x0020 Wrap use in monitor lock Method 
    /* 0x0040 */  "volatile ",    // ACC_VOLATILE 0x0040 Can't cache Variable 
    /* 0x0080 */  "transient ",    // ACC_TRANSIENT 0x0080 Not to be written or read by a per  sistent object manager  Variable
    /* 0x0100 */  "native ",      // ACC_NATIVE 0x0100 Implemented in a language other than Java Method 
    /* 0x0200 */  "interface ",    // ACC_INTERFACE 0x0200 Is an interface  Class 
    /* 0x0400 */  "abstract ",    // ACC_ABSTRACT 0x0400 No body provided  Class, Method 
    /* 0x0800 */  "strict ",      // ACC_STRICT 0x0800 Declared strictfp; floating-point mode is FP-strict 
    /* 0x1000 */  "synthetic ",    // ACC_SYNTHETIC
    /* 0x2000 */  "annotation ",    // ACC_ANNOTATION
    /* 0x4000 */  "enum "        // ACC_ENUM
  };
  int  mask, i;

  mask = 1;
  for (i = 0; i < 14; ++i) {
    if (access & mask) {
      if(Out)fputs(accessName[i], stdout);
    }
    mask <<= 1;
  }
  mask = (access & 0x8000);
  if (mask) {
    if(Out)printf("0x%04x ", mask);
  }
}

static int unicodecmp(const u2T *string1P, const u2T *string2P)
{
  register  const u2T  *P1 = string1P;
  register  const u2T  *P2 = string2P;

  for (;;) {
    if (!*P1) {
      if (!*P2) {
        return(0);
      }
      return(1);
    }
    if (!*P1) {
      return(1);
    }
    if (*P1 != *P2) {
      return(1);
    }
    ++P1;
    ++P2;
}  }

static const u2T *
get_Utf8(int index)
{
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;
  u2T      *stringP;

  assert(index > 0 && index < g_cf.m_constant_pool_count);
  assert(cpP->tag == CONSTANT_Utf8);
  stringP = cpP->u.stringP;
  return(stringP);
}

static u2T *
get_Utf8_sono2(int index)
{
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;
  u2T      *stringP;

  assert(index > 0 && index < g_cf.m_constant_pool_count);
  assert(cpP->tag == CONSTANT_Utf8);
  stringP = cpP->u.stringP;
  return(stringP);
}

static void
dump_Utf8(int index)
{
  dumpUnicode(get_Utf8(index));
}

static void
dump_descriptor(const u2T *descriptorP, attribute_infoT *localVariableTableP, int isStatic)
{
  if (g_demangle) {
    const  char *P;
    u2T    c, c1, name_slot, cnt, i, index;
    int    dimension = 0;
    int    parameter = 0;
    int    offset;
    u1T    *P1;
    
//    dumpUnicode(descriptorP);
//    if(Out)fputs("->", stdout);

    for (;;) {
      switch (c = *descriptorP++) {
      //case '\\':
        //if(Out)fprintf(stdout,"\\\\");
        //continue;
      case 0:
        return;
      case '[':
        ++dimension;
        continue;
      case 'L':
        if (parameter > 1) {
          if(Out)fputs(", ", stdout);
        }
        for (; (c1 = *descriptorP) && c1 != ';'; ++descriptorP) {
          //if (c1 == '/') {
            //c1 = '.';
          //}
          if (c1 == '.') {
            c1 = '/';
          }
          if(Out)fputc(c1, stdout);
        }
        goto show_array;
      case '(':
        if(Out)fputc('(', stdout);
        parameter = 1;
        continue;
      case ')':
        if(Out)fputs(") returns ", stdout);
        parameter = 0;
        continue;
      case 'B':
        P = "byte";
        break;
      case 'C':
        P = "char";
        break;
      case 'D':
        P = "double";
        break;
      case 'F':
        P = "float";
        break;
      case 'I':
        P = "int";
        break;
      case 'J':
        P = "long";
        break;
      case 'S':
        P = "short";
        break;
      case 'Z':
        P = "boolean";
        break;
      case 'V':
        P = "void";
        break;
      case ';':
        continue;
      default:
        assert(0);
      }
      if (parameter > 1) {
        if(Out)fputs(", ", stdout);
      }
      if(Out)fputs(P, stdout);
show_array:
      for (; dimension > 0; --dimension) {
        if(Out)fputs("[]", stdout);
      }
      if (parameter) {
        if (localVariableTableP) {
          offset = parameter;
          if (isStatic) {
            --offset;
          }
          P1 = getU2(localVariableTableP->infoP, &cnt);
          for (i = 0; i < cnt; ++i) {
            P1 += sizeof(u2T) /* start pc */ + sizeof(u2T) /* pc length */;
            P1  = getU2(P1, &name_slot);
            P1 += sizeof(u2T) /* descriptor slot */;
            P1  = getU2(P1, &index);
            if (index == offset) {
              if(Out)fputc(' ', stdout);
              dump_Utf8(name_slot);
              break;
          }  }
        }
        ++parameter;
      }
  }  }  

  dumpUnicode(descriptorP);
  return;
}

static void
dump_constant(int index)
{
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;

  assert(index > 0 && index < g_cf.m_constant_pool_count);

  switch (cpP->tag) {
  case CONSTANT_Integer:
    if(Out)printf("%d", cpP->u.ival);
    break;
  case CONSTANT_Float:
    if(Out)printf("%f", cpP->u.fval);
    break;
  case CONSTANT_Long:
    if(Out)printf("%lld", (long long) cpP->u.lval);
    break;
  case CONSTANT_Double:
    if(Out)printf("%lf", cpP->u.dval);
    break;
  case CONSTANT_String:
    if(Out)fputc('"', stdout);
    dump_Utf8(cpP->u.ref.index1);
    if(Out)fputc('"', stdout);
    break;
  case CONSTANT_Class:
    //if(Out)fputs("/* Class */ ", stdout);
    dump_Utf8(cpP->u.ref.index1);
    break;
  default:
    assert(0);
}  }

static void
dumpConstantValue(u4T length, u1T *infoP)
{
  u2T  u2;

  assert(length == 2);
  getU2(infoP, &u2);
  dump_constant(u2);
}

static void
dump_class(int index)
{
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;
  const u2T  *P;
  u2T      c;

  assert(index > 0 && index < g_cf.m_constant_pool_count);

  assert(cpP->tag == CONSTANT_Class);

  P = get_Utf8(cpP->u.ref.index1);

  for (; (c = *P) ; ++P) {
    //if (c == '/') {
      //c = '.';
    //}
    if (c == '.') {
      c = '/';
    }
    if(Out)fputc(c, stdout);
  }
}

static void
dump_nameAndType(int index)
{
  unsigned short *tp;
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;

  assert(index > 0 && index < g_cf.m_constant_pool_count);

  assert(cpP->tag == CONSTANT_NameAndType);
  dump_Utf8(cpP->u.ref.index1);
  tp = get_Utf8_sono2(cpP->u.ref.index2);
  if(*tp != (unsigned short)'('){    
    if(Out)fputc(' ', stdout);
  }
  dump_descriptor(get_Utf8(cpP->u.ref.index2), 0, 0);
}

static void
dump_fieldref(int index)
{
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;

  assert(index > 0 && index < g_cf.m_constant_pool_count);

  assert(cpP->tag == CONSTANT_Fieldref);
  dump_class(cpP->u.ref.index1);
  //if(Out)fputc('.', stdout);
  if(Out)fputc('/', stdout);
  dump_nameAndType(cpP->u.ref.index2);
}

static void
dump_methodref(int index)
{
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;

  assert(index > 0 && index < g_cf.m_constant_pool_count);

  assert(cpP->tag == CONSTANT_Methodref);
  dump_class(cpP->u.ref.index1);
  //if(Out)fputc('.', stdout);
  if(Out)fputc('/', stdout);
  dump_nameAndType(cpP->u.ref.index2);
}

static void
dump_interfaceref(int index)
{
  cp_infoT  *cpP = g_cf.m_constant_poolP + index;

  assert(index > 0 && index < g_cf.m_constant_pool_count);

  assert(cpP->tag == CONSTANT_InterfaceMethodref);
  dump_class(cpP->u.ref.index1);
  //if(Out)fputc('.', stdout);
  if(Out)fputc('/', stdout);
  dump_nameAndType(cpP->u.ref.index2);
}

static void
dump_imports(void)
{
  cp_infoT  *cpP;
  int      i;

  cpP = g_cf.m_constant_poolP;
  for (i = 0; i < g_cf.m_constant_pool_count; ++i) {
    if (cpP->tag == CONSTANT_Class) {
      if(Out)fputs(";imports ", stdout);
      dump_class(i);
      if(Out)fprintf(stdout,";\n");
    }
    ++cpP;
  }
  if(Out)fputc('\n', stdout);
}

static void
dumpSourceFile(u4T length, u1T *infoP)
{
  u2T    u2;

  assert(length == 2);
  getU2(infoP, &u2);
  dump_Utf8(u2);
}

static void
dumpExceptions(u4T length, u1T *infoP)
{
  u2T  number_of_exceptions, slot;
  u1T  *P;
  int  i;

  if(Out)fputs("\n\t.throws ", stdout);

  P = getU2(infoP, &number_of_exceptions);
  for (i = 0; i < number_of_exceptions; ++i) {
    if (i) {
      if(Out)fputs(", ", stdout);
    }
    P = getU2(P, &slot);
    dump_class(slot);
  }
}

static void
dumpLineNumberTable(u4T length, u1T *infoP)
{
  u1T  *P;
  u2T  cnt, u2;
  int  i;

  P = getU2(infoP, &cnt);
  for (i = 0; i < cnt; ++i) {
    P = getU2(P, &u2);
    if(Out)printf("pc=%d ", (int) u2);
    P = getU2(P, &u2);
    if(Out)printf("lineno=%d; ", (int) u2);
  }
  if(Out)fputc('\n', stdout);

  assert ((P - infoP) == length);
}

static void
dumpLocalVariableTable(int length, u1T *infoP)
{
  u2T start_pc, pc_length, name_slot, descriptor_slot, index;
  u1T  *P;
  u2T  cnt;
  int  i;

  P = getU2(infoP, &cnt);
  for (i = 0; i < cnt; ++i) {
    P = getU2(P, &start_pc);
    P = getU2(P, &pc_length);
    P = getU2(P, &name_slot);
    P = getU2(P, &descriptor_slot);
    P = getU2(P, &index);
    if(Out)printf("index=%d ", (int) index);
    if(Out)printf("start_pc=%d ", (int) start_pc);
    if(Out)printf("length=%d ", (int) pc_length);
    if(Out)fputs("nameP=", stdout);
    dumpUnicode(get_Utf8(name_slot));
    if(Out)fputs(" descriptorP=", stdout);
    dump_descriptor(get_Utf8(descriptor_slot), 0, 0);
    if(Out)fputc('\n', stdout);
  }
  assert ((P - infoP) == length);
}

static void
dumpInnerClasses(int length, u1T *infoP)
{
  u1T  *P;
  u2T  cnt, inner_class, outer_class, inner_name, access;
  int  i;

  P = getU2(infoP, &cnt);
  for (i = 0; i < cnt; ++i) {
    P = getU2(P, &inner_class);
    P = getU2(P, &outer_class);
    P = getU2(P, &inner_name);
    P = getU2(P, &access);
    if(Out)fputc('\n', stdout);
    if(Out)fputs(";//", stdout);
    if (outer_class) {
      if(Out)fputs(" outerclass=", stdout);
      dump_class(outer_class);
    }
    
    if (inner_class) {
      if(Out)fputs(" innerclass=", stdout);
      dump_class(inner_class);
    }
    if (inner_name) {
      if(Out)fputs(" innername=", stdout);
      dump_Utf8(inner_name);
    }
    if(Out)fputc(' ', stdout);
    dump_access(access);
  }
  assert ((P - infoP) == length);
  if(Out)fputc('\n', stdout);
}

/*
The EnclosingMethod Attribute
 
The EnclosingMethod attribute is an optional fixed-length attribute
in the attributes table of the ClassFile (?4.2) structure. A class 
must have an EnclosingMethod attribute if and only if it is a local class 
or an anonymous class. A class may have no more than one EnclosingMethod attribute.
 
The EnclosingMethod attribute has the following format:
 
EnclosingMethod_attribute {
    u2 attribute_name_index;
    u4 attribute_length;
    u2 class_index
    u2 method_index;
}
 
The items of the EnclosingMethod_attribute structure are as follows:
 
attribute_name_index
 
The value of the attribute_name_index item must be a valid index into the constant_pool table. 
The constant_pool entry at that index must be a CONSTANT_Utf8_info (?4.5.7) structure 
representing the string "EnclosingMethod".
 
attribute_length
 
The value of the attribute_length item is four.
 
class_index
 
The value of the class_index item must be a valid index into the constant_pool table. 
The constant_pool entry at that index must be a CONSTANT_Class_info (?4.5.1) structure 
representing a the innermost class that encloses the declaration of the current class.
 
method_index
 
If the current class is not immediately enclosed by a method or constructor, then the 
value of the method_index item must be zero. Otherwise, the value of the method_index 
item must be a valid index into the constant_pool table. The constant_pool entry at 
that index must be a CONSTANT_NameAndType_info (?4.5.6) structure representing a the 
name and type of a method in the class referenced by the class_index attribute above. 
It is the responsibility of the Java compiler to ensure that the method identified via 
the method_index is indeed the closest lexically enclosing method of the class that 
contains this EnclosingMethod attribute.

*/

static void
dumpEnclosingMethod(int length, u1T *infoP)
{
    u2T class_index, method_index;
  u1T  *P;
  
  assert(length == 4);
  P = getU2(infoP, &class_index);
  P = getU2(P,     &method_index);
  
  if(Out)fputs(" ;// EnclosingMethod ", stdout);

  dump_class(class_index);
  if (method_index) {
    if(Out)fputs(" ", stdout);
    dump_nameAndType(method_index);
  }
}

static int
dump_local_variable(int pc, int slot, localVariableT *localVariablesP)
{
  localVariableT  *localVariableP;
  const u2T    *nameP;
  int count;

  count = 0;
  if (localVariableP = localVariablesP) {
    for (; nameP = localVariableP->nameP; ++localVariableP) {
      if (localVariableP->index == slot && localVariableP->start_pc <= pc && localVariableP->end_pc >= pc) {
        if(Out)fprintf(stdout,"\t;",count++);
        dumpUnicode(nameP);
        if(Out)fputc(' ' , stdout);
        dump_descriptor(localVariableP->descriptorP, 0, 0);
        if(Out)fputc('\t', stdout);
        return(1);
  }  }  }
  return(0);
}

static void dump_attributes(int attributes_count, attribute_infoT *attributesP);

const static u2T  sourceFile[] =
  {'S','o','u','r','c','e','F','i','l','e',0};
const static u2T  constantValue[] =
  {'C','o','n','s','t','a','n','t','V','a','l','u','e',0};
const static u2T  exceptions[] = 
  {'E','x','c','e','p','t','i','o','n','s',0};
const static u2T  lineNumberTable[] =
  {'L','i','n','e','N','u','m','b','e','r','T','a','b','l','e',0};
const static u2T  localVariableTable[] =
  {'L','o','c','a','l','V','a','r','i','a','b','l','e','T','a','b','l','e',0};
const static u2T  innerClasses[] =
  {'I','n','n','e','r','C','l','a','s','s','e','s',0};
const static u2T  code[] =
  {'C','o','d','e',0};
const static u2T  enclosingMethod[] = 
  {'E','n','c','l','o','s','i','n','g','M','e','t','h','o','d',0};

static void
dump_code(u4T length, u1T *infoP)
{
  static const char *array_type[] = 
  {0,0,0,0,
  "boolean", "char", "float", "double", "byte", "short", "int", "long"};

#define U1X       0x01
#define U2X       0x02
#define U4X       0x04
#define WIDEX     0x10
#define BRANCHX   0x20
#define TSWITCHX  0x40
#define LSWITCHX  0x80
#define INVALIDX  0x100

  typedef struct {
    const char  *nameP;
    u4T      flags;
    s4T      variable;
    const char  *commentP;
  } opcodeT;

  static const opcodeT opcodes[] = {

/*  00 (0x00) */ {"nop",      0, -1, "No-op"},
/*  01 (0x01) */ {"aconst_null",  0, -1, "Stack ... -> ...,null"},
/*  02 (0x02) */ {"iconst_m1",    0, -1, "Stack ... -> ...,-1"},
/*  03 (0x03) */ {"iconst_0",    0, -1, "Stack ... -> ...,[int] 0"},
/*  04 (0x04) */ {"iconst_1",    0, -1, "Stack ... -> ...,[int] 1"},
/*  05 (0x05) */ {"iconst_2",    0, -1, "Stack ... -> ...,[int] 2"},
/*  06 (0x06) */ {"iconst_3",    0, -1, "Stack ... -> ...,[int] 3"},
/*  07 (0x07) */ {"iconst_4",    0, -1, "Stack ... -> ...,[int] 4"},
/*  08 (0x08) */ {"iconst_5",    0, -1, "Stack ... -> ...,[int] 5"},
/*  09 (0x09) */ {"lconst_0",    0, -1, "Stack ... -> ...,[long] 0.H,L"},
/*  10 (0x0a) */ {"lconst_1",    0, -1, "Stack ... -> ...,[long] 1.H,L"},
/*  11 (0x0b) */ {"fconst_0",    0, -1, "Stack ... -> ...,[float] 0"},
/*  12 (0x0c) */ {"fconst_1",    0, -1, "Stack ... -> ...,[float] 1"},
/*  13 (0x0d) */ {"fconst_2",    0, -1, "Stack ... -> ...,[float] 2"},
/*  14 (0x0e) */ {"dconst_0",    0, -1, "Stack ... -> ...,[dbl] 0.H,L"},
/*  15 (0x0f) */ {"dconst_1",    0, -1, "Stack ... -> ...,[dbl] 1.H,L"},
/*  16 (0x10) */ {"bipush",      U1X, -1, "Stack ... -> ..., [sbyte] arg"},
/*  17 (0x11) */ {"sipush",      U2X, -1, "Stack ... -> ..., [sshort] arg"},
/*  18 (0x12) */ {"ldc",          U1X, -1, "Stack ... -> ..., const"},
/*  19 (0x13) */ {"ldc_w",        U2X, -1, "Stack ... -> ..., const"},
/*  20 (0x14) */ {"ldc2_w",       U2X, -1, "Stack ... -> ..., const.H,L"},
/*  21 (0x15) */ {"iload",  WIDEX|U1X, -2, "Stack ... -> ..., [int] var"},
/*  22 (0x16) */ {"lload",  WIDEX|U1X, -2, "Stack ... -> ..., [long] var.H,L"},
/*  23 (0x17) */ {"fload",  WIDEX|U1X, -2, "Stack ... -> ..., [float] var"},
/*  24 (0x18) */ {"dload",  WIDEX|U1X, -2, "Stack ... -> ..., [dbl] var.H,L"},
/*  25 (0x19) */ {"aload",  WIDEX|U1X, -2, "Stack ... -> ..., [ref] var"},
/*  26 (0x1a) */ {"iload_0",        0,  0, "Stack ... -> ..., [int] var"},
/*  27 (0x1b) */ {"iload_1",        0,  1, "Stack ... -> ..., [int] var"},
/*  28 (0x1c) */ {"iload_2",        0,  2, "Stack ... -> ..., [int] var"},
/*  29 (0x1d) */ {"iload_3",        0,  3, "Stack ... -> ..., [int] var"},
/*  30 (0x1e) */ {"lload_0",        0,  0, "Stack ... -> ..., [long] var.H,L"},
/*  31 (0x1f) */ {"lload_1",        0,  1, "Stack ... -> ..., [long] var.H,L"},
/*  32 (0x20) */ {"lload_2",        0,  2, "Stack ... -> ..., [long] var.H,L"},
/*  33 (0x21) */ {"lload_3",        0,  3, "Stack ... -> ..., [long] var.H,L"},
/*  34 (0x22) */ {"fload_0",        0,  0, "Stack ... -> ..., [float] var"},
/*  35 (0x23) */ {"fload_1",        0,  1, "Stack ... -> ..., [float] var"},
/*  36 (0x24) */ {"fload_2",        0,  2, "Stack ... -> ..., [float] var"},
/*  37 (0x25) */ {"fload_3",        0,  3, "Stack ... -> ..., [float] var"},
/*  38 (0x26) */ {"dload_0",        0,  0, "Stack ... -> ..., [dbl] var.H,L"},
/*  39 (0x27) */ {"dload_1",        0,  1, "Stack ... -> ..., [dbl] var.H,L"},
/*  40 (0x28) */ {"dload_2",        0,  2, "Stack ... -> ..., [dbl] var.H,L"},
/*  41 (0x29) */ {"dload_3",        0,  3, "Stack ... -> ..., [dbl] var.H,L"},
/*  42 (0x2a) */ {"aload_0",        0,  0, "Stack ... -> ..., [ref] var"},
/*  43 (0x2b) */ {"aload_1",        0,  1, "Stack ... -> ..., [ref] var"},
/*  44 (0x2c) */ {"aload_2",        0,  2, "Stack ... -> ..., [ref] var"},
/*  45 (0x2d) */ {"aload_3",        0,  3, "Stack ... -> ..., [ref] var"},
/*  46 (0x2e) */ {"iaload",      0, -1, "Stack ..., a, i -> ..., [int] a[i]"},
/*  47 (0x2f) */ {"laload",      0, -1, "Stack ..., a, i -> ..., [long] a[i].H,L"},
/*  48 (0x30) */ {"faload",      0, -1, "Stack ..., a, i -> ..., [float] a[i]"},
/*  49 (0x31) */ {"daload",      0, -1, "Stack ..., a, i -> ..., [dbl] a[i].H,L"},
/*  50 (0x32) */ {"aaload",      0, -1, "Stack ..., a, i -> ..., [ref] a[i]"},
/*  51 (0x33) */ {"baload",      0, -1, "Stack ..., a, i -> ..., [sbyte] a[i]"},
/*  52 (0x34) */ {"caload",      0, -1, "Stack ..., a, i -> ..., [char] a[i]"},
/*  53 (0x35) */ {"saload",      0, -1, "Stack ..., a, i -> ..., [sshort] a[i]"},
/*  54 (0x36) */ {"istore",  WIDEX|U1X, -2, "Stack ...,int -> ... & var = int"},
/*  55 (0x37) */ {"lstore",  WIDEX|U1X, -2, "Stack ...,long.H,L -> ... & var = long"},
/*  56 (0x38) */ {"fstore",  WIDEX|U1X, -2, "Stack ...,float -> ... & var = float"},
/*  57 (0x39) */ {"dstore",  WIDEX|U1X, -2, "Stack ...,dbl.H,L -> ... & var = dbl"},
/*  58 (0x3a) */ {"astore",  WIDEX|U1X, -2, "Stack ...,ref -> ... & var = ref"},
/*  59 (0x3b) */ {"istore_0",      0,  0, "Stack ...,int -> ... & var = int"},
/*  60 (0x3c) */ {"istore_1",      0,  1, "Stack ...,int -> ... & var = int"},
/*  61 (0x3d) */ {"istore_2",      0,  2, "Stack ...,int -> ... & var = int"},
/*  62 (0x3e) */ {"istore_3",      0,  3, "Stack ...,int -> ... & var = int"},
/*  63 (0x3f) */ {"lstore_0",      0,  0, "Stack ...,long.H,L -> ... & var = long"},
/*  64 (0x40) */ {"lstore_1",      0,  1, "Stack ...,long.H,L -> ... & var = long"},
/*  65 (0x41) */ {"lstore_2",      0,  2, "Stack ...,long.H,L -> ... & var = long"},
/*  66 (0x42) */ {"lstore_3",      0,  3, "Stack ...,long.H,L -> ... & var = long"},
/*  67 (0x43) */ {"fstore_0",      0,  0, "Stack ...,float -> ... & var = float"},
/*  68 (0x44) */ {"fstore_1",      0,  1, "Stack ...,float -> ... & var = float"},
/*  69 (0x45) */ {"fstore_2",      0,  2, "Stack ...,float -> ... & var = float"},
/*  70 (0x46) */ {"fstore_3",      0,  3, "Stack ...,float -> ... & var = float"},
/*  71 (0x47) */ {"dstore_0",      0,  0, "Stack ...,dbl.H,L -> ... & var = dbl"},
/*  72 (0x48) */ {"dstore_1",      0,  1, "Stack ...,dbl.H,L -> ... & var = dbl"},
/*  73 (0x49) */ {"dstore_2",      0,  2, "Stack ...,dbl.H,L -> ... & var = dbl"},
/*  74 (0x4a) */ {"dstore_3",      0,  3, "Stack ...,dbl.H,L -> ... & var = dbl"},
/*  75 (0x4b) */ {"astore_0",      0,  0, "Stack ...,ref -> ... & var = ref"},
/*  76 (0x4c) */ {"astore_1",      0,  1, "Stack ...,ref -> ... & var = ref"},
/*  77 (0x4d) */ {"astore_2",      0,  2, "Stack ...,ref -> ... & var = ref"},
/*  78 (0x4e) */ {"astore_3",      0,  3, "Stack ...,ref -> ... & var = ref"},
/*  79 (0x4f) */ {"iastore",    0, -1, "Stack ...,a,i, v -> ... & a[i] = [int] v"},
/*  80 (0x50) */ {"lastore",    0, -1, "Stack ...,a,i, v.H,L -> ... & a[i] = [long] v"},
/*  81 (0x51) */ {"fastore",    0, -1, "Stack ...,a,i, v -> ... & a[i] = [float] v"},
/*  82 (0x52) */ {"dastore",    0, -1, "Stack ...,a,i, v.H,L -> ... & a[i] = [dbl] v"},
/*  83 (0x53) */ {"aastore",    0, -1, "Stack ...,a,i, v -> ... & a[i] = [ref] v"},
/*  84 (0x54) */ {"bastore",    0, -1, "Stack ...,a,i, v -> ... & a[i] = [sbyte] v"},
/*  85 (0x55) */ {"castore",    0, -1, "Stack ...,a,i, v -> ... & a[i] = [char] v"},
/*  86 (0x56) */ {"sastore",    0, -1, "Stack ...,a,i, v -> ... & a[i] = [sshort] v"},
/*  87 (0x57) */ {"pop",      0, -1, "Stack: ...,x -> ..."},
/*  88 (0x58) */ {"pop2",      0, -1, "Stack: ...,x2,x1 -> ..."},
/*  89 (0x59) */ {"dup",      0, -1, "Stack: ...,x -> ...,x, x"},
/*  90 (0x5a) */ {"dup_x1",      0, -1, "Stack ...,x2,x1 -> ...,x1,x2,x1",},
/*  91 (0x5b) */ {"dup_x2",      0, -1, "Stack ...,x3,x2,x1 -> ...,x1,x3,x2,x1"},
/*  92 (0x5c) */ {"dup2",      0, -1, "Stack: ...,x2,x1 -> ...,x2,x1,x2,x1"},
/*  93 (0x5d) */ {"dup2_x1",    0, -1, "Stack: ...,x3,x2,x1 -> ...,x2,x1,x3,x2,x1"},
/*  94 (0x5e) */ {"dup2_x2",    0, -1, "Stack ...,x4,x3,x2,x1 -> ...,x2,x1,x4,x3,x2,x1"},
/*  95 (0x5f) */ {"swap",      0, -1, "Stack ...,x2,x1 -> ...,x1,x2"},
/*  96 (0x60) */ {"iadd",      0, -1, "Stack ...,int2,int1 -> ...,(int2+int1)"},
/*  97 (0x61) */ {"ladd",      0, -1, "Stack ...,long2.H,L,long1.H.L -> ...,(long2+long1).H,L"},
/*  98 (0x62) */ {"fadd",      0, -1, "Stack ...,float2,float1 -> ...,(float2+float1)"},
/*  99 (0x63) */ {"dadd",      0, -1, "Stack ...,dbl2.H,L,dbl1.H.L -> ...,(dbl2+dbl1).H,L"},
/* 100 (0x64) */ {"isub",      0, -1, "Stack ...,int2,int1 -> ...,(int2-int1)"},
/* 101 (0x65) */ {"lsub",      0, -1, "Stack ...,long2.H,L,long2.H,L -> ...,(long2-long1).H,L"},
/* 102 (0x66) */ {"fsub",      0, -1, "Stack ...,float2,float1 -> ...,(float2-float1)"      },
/* 103 (0x67) */ {"dsub",      0, -1, "Stack ...,dbl2.H,L,dbl2.H,L -> ...,(dbl2-dbl1).H,L"},
/* 104 (0x68) */ {"imul",      0, -1, "Stack ...,int2,int1 -> ...,(int2*int1)"},
/* 105 (0x69) */ {"lmul",      0, -1, "Stack ...,long2.H,L,long1.H.L -> ...,(long2*long1).H,L"},
/* 106 (0x6a) */ {"fmul",      0, -1, "Stack ...,float2,float1 -> ...,(float2*float1)"},
/* 107 (0x6b) */ {"dmul",      0, -1, "Stack ...,dbl2.H,L,dbl1.H.L -> ...,(dbl2*dbl1).H,L"},
/* 108 (0x6c) */ {"idiv",      0, -1, "Stack ...,int2,int1 -> ...,(int2/int1)"},
/* 109 (0x6d) */ {"ldiv",      0, -1, "Stack ...,long2.H,L,long1.H.L -> ...,(long2/long1).H,L"},
/* 110 (0x6e) */ {"fdiv",      0, -1, "Stack ...,float2,float1 -> ...,(float2/float1)"},
/* 111 (0x6f) */ {"ddiv",      0, -1, "Stack ...,dbl2.H,L,dbl1.H.L -> ...,(dbl2/dbl1).H,L"},
/* 112 (0x70) */ {"irem",      0, -1, "Stack ...,int2,int1 -> ...,rem(int2/int1)"},
/* 113 (0x71) */ {"lrem",      0, -1, "Stack ...,long2.H,L,long1.H.L -> ...,rem(long2/long1).H,L"},
/* 114 (0x72) */ {"frem",      0, -1, "Stack ...,float2,float1 -> ...,rem(float2/float1)"},
/* 115 (0x73) */ {"drem",      0, -1, "Stack ...,dbl2.H,L,dbl1.H.L -> ...,rem(dbl2/dbl1).H,L"},
/* 116 (0x74) */ {"ineg",      0, -1, "Stack ...,int -> ...,(0-int)"},
/* 117 (0x75) */ {"lneg",      0, -1, "Stack ...,long.H,L -> ...,(0-long).H,L"},
/* 118 (0x76) */ {"fneg",      0, -1, "Stack ...,float -> ...,(0-float)"},
/* 119 (0x77) */ {"dneg",      0, -1, "Stack ...,dbl.H,L -> ...,(0-dbl).H,L"},
/* 120 (0x78) */ {"ishl",      0, -1, "Stack ...,int2,int1 -> ...,(int2<<(int1&0x1F))"},
/* 121 (0x79) */ {"lshl",      0, -1, "Stack ...,long2.H,L,long1/H,L -> ...,(long2<<(long1&0x3F))"},
/* 122 (0x7a) */ {"ishr",      0, -1, "Stack ...,int2,int1 -> ...,(signed int2>>(int1&0x1F))"},
/* 123 (0x7b) */ {"lshr",      0, -1, "Stack ...,long2.H,L,long1.H,L -> ...,(signed long2>>(long1&0x3F)).H,L"},
/* 124 (0x7c) */ {"iushr",      0, -1, "Stack ...,int2,int1 -> ...,(unsigned int2>>(int1&0x1F))"},
/* 125 (0x7d) */ {"lushr",      0, -1, "Stack ...,long2.H,L,long1.H,L -> ...,(unsigned long2>>(long1&0x3F)).H,L"},
/* 126 (0x7e) */ {"iand",      0, -1, "Stack ...,int2,int1 -> ...,(int2&int1)"},
/* 127 (0x7f) */ {"land",      0, -1, "Stack ...,long2.H,L,long1.H,L -> ...,(long2&long1).H,L"},
/* 128 (0x80) */ {"ior",      0, -1, "Stack ...,int2,int1 -> ...,(int2|int1)"},
/* 129 (0x81) */ {"lor",      0, -1, "Stack ...,long2.H,L,long1.H,L -> ...,(long2|long1).H,L"},
/* 130 (0x82) */ {"ixor",      0, -1, "Stack ...,int2,int1 -> ...,(int2^int1)"},
/* 131 (0x83) */ {"lxor",      0, -1, "Stack ...,long2.H,L,long1.H,L -> ...,(long2^long1).H,L"},
/* 132 (0x84) */ {"iinc",   WIDEX|U1X, -2, "var += arg"},
/* 133 (0x85) */ {"i2l",      0, -1, "Stack ...,int -> ...,long.H,L"},
/* 134 (0x86) */ {"i2f",      0, -1, "Stack ...,int -> ...,float"},
/* 135 (0x87) */ {"i2d",      0, -1, "Stack ...,int -> ...,dbl.H,L"},
/* 136 (0x88) */ {"l2i",      0, -1, "Stack ...,long.H,L -> ...,int"},
/* 137 (0x89) */ {"l2f",      0, -1, "Stack ...,long.H,L -> ...,float"},
/* 138 (0x8a) */ {"l2d",      0, -1, "Stack ...,long.H,L -> ...,dbl.H,L"},
/* 139 (0x8b) */ {"f2i",      0, -1, "Stack ...,float -> ...,int"},
/* 140 (0x8c) */ {"f2l",      0, -1, "Stack ...,float -> ...,long.H,L"},
/* 141 (0x8d) */ {"f2d",      0, -1, "Stack ...,float -> ...,dbl.H,L"},
/* 142 (0x8e) */ {"d2i",      0, -1, "Stack ...,dbl.H,L -> ...,int"},
/* 143 (0x8f) */ {"d2l",      0, -1, "Stack ...,dbl.H,L -> ...,long.H,L"},
/* 144 (0x90) */ {"d2f",      0, -1, "Stack ...,dbl.H,L -> ...,float"},
/* 145 (0x91) */ {"i2b",      0, -1, "Stack ...,int -> ...,(signed (int&0xFF))"},
/* 146 (0x92) */ {"i2c",      0, -1, "Stack ...,int -> ...,(unsigned (int&0xFFFF))"},
/* 147 (0x93) */ {"i2s",      0, -1, "Stack ...,int -> ...,(signed (int&0xFFFF))"},
/* 148 (0x94) */ {"lcmp",      0, -1, "Stack ...,long2.H,L,long1.H,L -> ..., (+1/0/-1 : long2 >|=|< long1)"},
/* 149 (0x95) */ {"fcmpl",      0, -1, "Stack ...,float2,float1 -> ..., (+1/0/-1=NAN : float2 >|=|< float1)"},
/* 150 (0x96) */ {"fcmpg",      0, -1, "Stack ...,float2,float1 -> ..., (+1=NAN/0/-1 : float2 >|=|< float1)"},
/* 151 (0x97) */ {"dcmpl",      0, -1, "Stack ...,dbl2,H,L,dbl1.H,L -> ..., (+1/0/-1=NAN : dbl2 >|=|< dbl1)"},
/* 152 (0x98) */ {"dcmpg",      0, -1, "Stack ...,dbl2,H,L,dbl1.H,L -> ..., (+1=NAN/0/-1 : dbl2 >|=|< dbl1)"},
/* 153 (0x99) */ {"ifeq", BRANCHX|U2X, -1, "Stack ...,int -> ... & if int==0"},
/* 154 (0x9a) */ {"ifne", BRANCHX|U2X, -1, "Stack ...,int -> ... & if int!=0"},
/* 155 (0x9b) */ {"iflt", BRANCHX|U2X, -1, "Stack ...,int -> ... & if int<0"},
/* 156 (0x9c) */ {"ifge", BRANCHX|U2X, -1, "Stack ...,int -> ... & if int>=0"},
/* 157 (0x9d) */ {"ifgt", BRANCHX|U2X, -1, "Stack ...,int -> ... & if int>0"},
/* 158 (0x9e) */ {"ifle", BRANCHX|U2X, -1, "Stack ...,int -> ... & if int<=0"},
/* 159 (0x9f) */ {"if_icmpeq",BRANCHX|U2X, -1, "Stack ...,int2,int1 -> ... & if int2==int1"},
/* 160 (0xa0) */ {"if_icmpne",BRANCHX|U2X, -1, "Stack ...,int2,int1 -> ... & if int2!=int1"},
/* 161 (0xa1) */ {"if_icmplt",BRANCHX|U2X, -1, "Stack ...,int2,int1 -> ... & if int2<int1"},
/* 162 (0xa2) */ {"if_icmpge",BRANCHX|U2X, -1, "Stack ...,int2,int1 -> ... & if int2>=int1"},
/* 163 (0xa3) */ {"if_icmpgt",BRANCHX|U2X, -1, "Stack ...,int2,int1 -> ... & if int2>int1"},
/* 164 (0xa4) */ {"if_icmple",BRANCHX|U2X, -1, "Stack ...,int2,int1 -> ... & if int2<=int1"},
/* 165 (0xa5) */ {"if_acmpeq",BRANCHX|U2X, -1, "Stack ...,ref2,ref1 -> ... & if ref2==ref1"},
/* 166 (0xa6) */ {"if_acmpne",BRANCHX|U2X, -1, "Stack ...,ref2,ref1 -> ... & if ref2!=ref1"},
/* 167 (0xa7) */ {"goto ",BRANCHX|U2X, -1, ""},
/* 168 (0xa8) */ {"jsr",  BRANCHX|U2X, -2, "Stack ... -> ..., returnAddress &"},
/* 169 (0xa9) */ {"ret",  WIDEX|U1X, -1, "Go to address in var"},
/* 170 (0xaa) */ {"tableswitch", TSWITCHX, -1, "Stack ..., index -> ... & branch"},
/* 171 (0xab) */ {"lookupswitch",LSWITCHX, -1, "Stack ..., key -> ... & branch"},
/* 172 (0xac) */ {"ireturn",      0, -1, "Stack ...,int -> [empty] & return int to caller"},
/* 173 (0xad) */ {"lreturn",      0, -1, "Stack ...,long.H,L -> [empty] & return long to caller"},
/* 174 (0xae) */ {"freturn",      0, -1, "Stack ...,float -> [empty] & return float to caller"},
/* 175 (0xaf) */ {"dreturn",      0, -1, "Stack ...,dbl.H,L -> [empty] & return dbl to caller"},
/* 176 (0xb0) */ {"areturn",      0, -1, "Stack ...,ref -> [empty] & return ref to caller"},
/* 177 (0xb1) */ {"return",        0, -1, "Stack ... -> [empty] & return to caller"},
/* 178 (0xb2) */ {"getstatic",      U2X, -1, "Stack ... -> ...,val[H,L] where static Field has val"},
/* 179 (0xb3) */ {"putstatic",      U2X, -1, "Stack ..., val[H,L] -> ... & static Field=val"},
/* 180 (0xb4) */ {"getfield",      U2X, -1, "Stack ... -> ...,val[H,L] where Field has val"},
/* 181 (0xb5) */ {"putfield",      U2X, -1, "Stack ..., val[H,L] -> ... & Field=val"},
/* 182 (0xb6) */ {"invokevirtual",    U2X, -1, "Stack ...,ref [,arg1 , arg2 ...] -> ... & Function invoked"},
/* 183 (0xb7) */ {"invokespecial",    U2X, -1, "Stack ...,ref [,arg1 , arg2 ...] -> ... & Function invoked"},
/* 184 (0xb8) */ {"invokestatic",    U2X, -1, "Stack ..., [,arg1 , arg2 ...] -> ... & Function invoked"},
/* 185 (0xb9) */ {"invokeinterface",  U2X, -1, "Stack ...,ref [,arg1 , arg2 ...] -> ... & Interface invoked"},
/* 186 (0xba) */ {"xxxunusedxxx1",      0, -1, 0},
/* 187 (0xbb) */ {"new",        U2X, -1, "Stack ... -> ...,ref"},
/* 188 (0xbc) */ {"newarray",      U1X, -1, "Stack ...,size -> ..., array[size]"},
/* 189 (0xbd) */ {"anewarray",      U2X, -1, "Stack ...,size -> ...,array[size]"},
/* 190 (0xbe) */ {"arraylength",    0, -1, "Stack ...,array -> ...,array.length"},
/* 191 (0xbf) */ {"athrow",        0, -1, "Stack ...,Throwable ref -> [undefined] & throw"},
/* 192 (0xc0) */ {"checkcast",      U2X, -1, "Stack ...,ref -> ...,ref & throw if not castable"},
/* 193 (0xc1) */ {"instanceof",      U2X, -1, "Stack ...,ref -> ...,(1 if instanceof else 0)"},
/* 194 (0xc2) */ {"monitorenter",    0, -1, "Stack ..., ref -> ... & exclusive access to ref"},
/* 195 (0xc3) */ {"monitorexit",    0, -1, "Stack ..., ref -> ... & release lock on ref"},
/* 196 (0xc4) */ {";wide",          0, -1, "Treat next instruction as having wider argument"},
/* 197 (0xc5) */ {"multianewarray",    U2X, -1, "Stack ...,d1,d2,...dn -> array[d1][d2]...[dn] where n=arg2"},
/* 198 (0xc6) */ {"ifnull",   BRANCHX|U2X, -1, "Stack ...,ref -> ... & if ref==null"},
/* 199 (0xc7) */ {"ifnonnull",BRANCHX|U2X, -1, "Stack ...,ref -> ... & if ref!=null"},
/* 200 (0xc8) */ {"goto_w",    BRANCHX|U4X, -1, ""},
/* 201 (0xc9) */ {"jsr_w",    BRANCHX|U4X, -1, "Stack ... -> ..., returnAddress &"},
/* 202 (0xca) */ {"breakpoint",         0, -1, 0},  /* Reserved opcodes start here */  
/* 203 (0xcb) */ {"?203?",       INVALIDX, -1, 0},
/* 204 (0xcc) */ {"?204?",       INVALIDX, -1, 0},
/* 205 (0xcd) */ {"?205?",       INVALIDX, -1, 0},
/* 206 (0xce) */ {"?206?",       INVALIDX, -1, 0},
/* 207 (0xcf) */ {"?207?",       INVALIDX, -1, 0},
/* 208 (0xd0) */ {"?208?",       INVALIDX, -1, 0},
/* 209 (0xd1) */ {"ret_w",            U2X, -2, "Go to return address in variable"},
/* 210 (0xd2) */ {"?210?",       INVALIDX, -1, 0},
/* 211 (0xd3) */ {"?211?",       INVALIDX, -1, 0},
/* 212 (0xd4) */ {"?212?",       INVALIDX, -1, 0},
/* 213 (0xd5) */ {"?213?",       INVALIDX, -1, 0},
/* 214 (0xd6) */ {"?214?",       INVALIDX, -1, 0},
/* 215 (0xd7) */ {"?215?",       INVALIDX, -1, 0},
/* 216 (0xd8) */ {"?216?",       INVALIDX, -1, 0},
/* 217 (0xd9) */ {"?217?",       INVALIDX, -1, 0},
/* 218 (0xda) */ {"?218?",       INVALIDX, -1, 0},
/* 219 (0xdb) */ {"?219?",       INVALIDX, -1, 0},
/* 220 (0xdc) */ {"?220?",       INVALIDX, -1, 0},
/* 221 (0xdd) */ {"?221?",       INVALIDX, -1, 0},
/* 222 (0xde) */ {"?222?",       INVALIDX, -1, 0},
/* 223 (0xdf) */ {"?223?",       INVALIDX, -1, 0},
/* 224 (0xe0) */ {"?224?",       INVALIDX, -1, 0},
/* 225 (0xe1) */ {"?225?",       INVALIDX, -1, 0},
/* 226 (0xe2) */ {"?226?",       INVALIDX, -1, 0},
/* 227 (0xe3) */ {"?227?",       INVALIDX, -1, 0},
/* 228 (0xe4) */ {"?228?",       INVALIDX, -1, 0},
/* 229 (0xe5) */ {"?229?",       INVALIDX, -1, 0},
/* 230 (0xe6) */ {"?230?",       INVALIDX, -1, 0},
/* 231 (0xe7) */ {"?231?",       INVALIDX, -1, 0},
/* 232 (0xe8) */ {"?232?",       INVALIDX, -1, 0},
/* 233 (0xe9) */ {"?233?",       INVALIDX, -1, 0},
/* 234 (0xea) */ {"?234?",       INVALIDX, -1, 0},
/* 235 (0xeb) */ {"?235?",       INVALIDX, -1, 0},
/* 236 (0xec) */ {"?236?",       INVALIDX, -1, 0},
/* 237 (0xed) */ {"?237?",       INVALIDX, -1, 0},
/* 238 (0xee) */ {"?238?",       INVALIDX, -1, 0},
/* 239 (0xef) */ {"?239?",       INVALIDX, -1, 0},
/* 240 (0xf0) */ {"?240?",       INVALIDX, -1, 0},
/* 241 (0xf1) */ {"?241?",       INVALIDX, -1, 0},
/* 242 (0xf2) */ {"?242?",       INVALIDX, -1, 0},
/* 243 (0xf3) */ {"?243?",       INVALIDX, -1, 0},
/* 244 (0xf4) */ {"?244?",       INVALIDX, -1, 0},
/* 245 (0xf5) */ {"?245?",       INVALIDX, -1, 0},
/* 246 (0xf6) */ {"?246?",       INVALIDX, -1, 0},
/* 247 (0xf7) */ {"?247?",       INVALIDX, -1, 0},
/* 248 (0xf8) */ {"?248?",       INVALIDX, -1, 0},
/* 249 (0xf9) */ {"?249?",       INVALIDX, -1, 0},
/* 250 (0xfa) */ {"?250?",       INVALIDX, -1, 0},
/* 251 (0xfb) */ {"?251?",       INVALIDX, -1, 0},
/* 252 (0xfc) */ {"?252?",       INVALIDX, -1, 0},
/* 253 (0xfd) */ {"?253?",       INVALIDX, -1, 0},
/* 254 (0xfe) */ {"impdep1",     INVALIDX, -1, 0},
/* 255 (0xff) */ {"impdep2",     INVALIDX, -1, 0}
  };

  lineNumberT    *lineNumbersP    = 0;
  localVariableT  *localVariablesP = 0;

  u2T      max_stack, max_locals, u2,u2_bak;
  u4T      u4, code_length, branch;
  signed int  s4;
  u1T      u1, *P, *P_bak, *startCodeP, *endCodeP;
  lineNumberT  *lineNumberP;
  u4T      exception_table_length, attributes_count, lineno_changes_at_pc, current_lineno, pc, flags;
  int      code, variable;
  const opcodeT  *opcodeP;
  u4T      wide_index = 0;

  char    buffer[32];

  if(Out)fputc('\n', stdout);

  P = infoP;

  P = getU2(P, &max_stack);
  P = getU2(P, &max_locals);

  if(Out)printf("\t.limit stack %d\n",max_stack);
  if(Out)printf("\t.limit locals %d\n",max_locals);

  current_lineno       = 0xFFFFFFFF;
  lineno_changes_at_pc = 0x7FFFFFFF;
  lineNumberP          = 0;
  
  startCodeP = getU4(P, &code_length);
  endCodeP   = P = startCodeP + code_length;

  P = getU2(P, &u2);
  P_bak = P;
  u2_bak = u2;

  if (exception_table_length = u2) {
    unsigned int start_pc, end_pc, handler_pc, i;

    //if(Out)fputs("\nException table\n", stdout);
    for (i = 0; i < exception_table_length; ++i) {
      P = getU2(P, &u2);
      start_pc = u2;
      P = getU2(P, &u2);
      end_pc = u2;
      P = getU2(P, &u2);
      handler_pc = u2;
      //if(Out)printf("\tstart_pc=%d\tend_pc=%d\tbranch_pc=%d", start_pc, end_pc, handler_pc); 
      P = getU2(P, &u2);
      if (u2) {
        //if(Out)fputc('\t', stdout);
        //dump_class(u2);
      }
      //if(Out)fputc('\n', stdout);
    }
  }
  P = getU2(P, &u2);
  if (attributes_count = u2) {
    attribute_infoT  *attributesP    = (attribute_infoT *) alloca(sizeof(attribute_infoT) * attributes_count);
    attribute_infoT  *attributeP     = attributesP;
    u4T        attribute_length;
    const u2T    *nameP;
    unsigned int  i,varc;

    for (i = 0,varc = 0; i < attributes_count; ++i) {
      P     = getU2(P, &attributeP->attribute_name_index);
      nameP = get_Utf8(attributeP->attribute_name_index);

      P = getU4(P, &attribute_length);
      attributeP->attribute_length = attribute_length;
      attributeP->infoP = P;

      if (!unicodecmp(nameP, lineNumberTable)) {
        lineNumberT *endLineNumberP;

        if (g_tables) {
          dumpLineNumberTable(attribute_length, attributeP->infoP);
        }
        P = getU2(P, &u2);
        lineNumbersP = lineNumberP = (lineNumberT *) alloca(sizeof(lineNumberT) * (u2+1));
        for (endLineNumberP = lineNumberP + u2; lineNumberP < endLineNumberP; ++lineNumberP) {
          P = getU2(P, &u2);
          lineNumberP->start_pc = u2;
          P = getU2(P, &u2);
          lineNumberP->line_number = u2;
        }
        assert ((P - attributeP->infoP) == attribute_length);
        endLineNumberP->line_number = -1;
        endLineNumberP->start_pc    = 0x7FFFFFFF;
        current_lineno = lineNumbersP->line_number;
        lineNumberP    = lineNumbersP+1;
        lineno_changes_at_pc = lineNumberP->start_pc;
        continue;
      }
      if (!unicodecmp(nameP, localVariableTable)) {
        localVariableT  *localVariableP, *endLocalVariableP;

        if (g_tables) {
          dumpLocalVariableTable(attribute_length, attributeP->infoP);
        }
        P = getU2(P, &u2);
        localVariablesP = localVariableP = (localVariableT *) alloca(sizeof(localVariableT) * (u2 + 1));
        for (endLocalVariableP = localVariableP + u2; localVariableP < endLocalVariableP; ++localVariableP) {
          P = getU2(P, &u2);
          localVariableP->start_pc = u2;
          P = getU2(P, &u2);
          localVariableP->end_pc = localVariableP->start_pc + u2;
          P = getU2(P, &u2);
          localVariableP->nameP = get_Utf8(u2);
          P = getU2(P, &u2);
          localVariableP->descriptorP = get_Utf8(u2);
          P = getU2(P, &localVariableP->index);
//
        if(Out)fprintf(stdout,"\t;.var %d is\t",varc++);
        dumpUnicode(localVariableP->nameP);
        if(Out)fputc(' ' , stdout);
        dump_descriptor(localVariableP->descriptorP, 0, 0);
        if(Out)fputc('\n' , stdout);
//
        }
        endLocalVariableP->nameP = 0;
        assert ((P - attributeP->infoP) == attribute_length);
        continue;
      }
      P += attribute_length;
      ++attributeP;
    }
    /*
    no need
    if (attributeP != attributesP) {
      //if(Out)fputs("\nAttributes\n", stdout);
      //dump_attributes(attributeP-attributesP, attributesP);
    }
    */
  }
  assert(P - infoP == length);

  if(Out)fputc('\n', stdout);

  int old_cn;
  old_cn = -99;
  for (P = startCodeP; P < endCodeP; ) {
    sprintf(buffer,"");
    pc = (u4T) (P - startCodeP);
    if (lineNumberP) {
      while (pc >= lineno_changes_at_pc) {
        current_lineno = lineNumberP->line_number;
        ++lineNumberP;
        lineno_changes_at_pc = lineNumberP->start_pc;
      }

      sprintf(buffer,"\t");
      if(old_cn != current_lineno){
        if(Out){
          for(int lc=0;lc < Lcnt;++lc){
            if(Label[lc] == pc){
              sprintf(buffer, "\t;.line %u\nZ%u:\n\t",current_lineno,pc);
              break;
            }
          } 
        }
      }
      else{
        if(Out){
          for(int lc=0;lc < Lcnt;++lc){
            if(Label[lc] == pc){
              sprintf(buffer, "Z%u:\n\t",pc);
              break;
            }
          } 
        }
      }
      old_cn = current_lineno;
      //if(Out)printf("%-12.12s", buffer);
      if(Out)printf("%s", buffer);
    } else {
      sprintf(buffer,"\t");
      if(Out){
        for(int lc=0;lc < Lcnt;++lc){
          if(Label[lc] == pc){
            sprintf(buffer, "Z%u:\n\t",pc);
            //if(Out)fputc('\n', stdout);
            //if(Out)fputc('\t', stdout);
            break;
          }
        } 
      }
      //if(Out)printf("%-12.12s", buffer);
      if(Out)printf("%s", buffer);
    }

    code = *P++;
    opcodeP = opcodes + code;
    //if(Out)printf("%-16.16s", opcodeP->nameP);
    if(wide_index){
      if(strcmp(opcodeP->nameP,"iinc") == 0){
        if(Out)printf("iinc_w");
      }
      else{
        if(Out)printf("iinc");
      }
    }
    else{
      if(Out)printf("%s", opcodeP->nameP);
    }
    flags = opcodeP->flags;

    if (flags & INVALIDX) {
            fprintf(stderr, ": Unknown pcode operation of %d 0x%02x\n", code, code);
      assert(0);
    }
    if (flags & (U1X|U2X|U4X)) {
      if ((flags & U1X)) {
        if (wide_index) {
          P = getU2(P, &u2);
          u4     = u2;
          s4     = ((signed short) u2);
          if (code != 132) {
            wide_index = 0;
          }
        } else {
          u4     = u1 = *P++;
          s4     = ((signed char) u1);
        }
      } else if (flags & U2X) {
        P = getU2(P, &u2);
        u4     = u2;
        s4     = ((signed short) u2);
      } else {
        P = getU4(P, &u4);
        s4     = (signed int) u4;
      }
      branch = pc + s4;
    }  

    if ((variable = opcodeP->variable) != -1) {
      if (variable < 0) {
        variable = u4;
      }
      if (opcodeP->variable < 0) {
        if(Out)printf(" %d ", u4);
      }
      dump_local_variable(P-startCodeP, variable, localVariablesP);
/*
      if (!dump_local_variable(P-startCodeP, variable, localVariablesP) && opcodeP->variable < 0) {
        if(Out)printf(" %d ", u4);
      }
*/
    }
  
    // Pushing Constants
    // http://sunsite.ee/java/vmspec/vmspec-14.html#HEADING14-0
    int notabf;
    notabf = 0;

    switch (code) {
    case 16:
    case 17:
      if(Out)fputc('\t', stdout);
      if(Out)printf("%d", s4);
      break;
    case 18:
    case 19:
    case 20:
      if(Out)fputc('\t', stdout);
      dump_constant(u4);
      break;
    case 132:
      notabf = 1;
      if (wide_index) {
        P = getU2(P, &u2);
        s4 = ((signed short) u2);
        wide_index = 0;
      } else {
        s4 = *((signed char *) P);
        ++P;
      }
      //if(Out)fputc('\t', stdout);
      if(Out)printf("%d", s4);
      break;
    case 167:  // goto
      notabf = 1;
      break;
    case 170:  // tableswitch
    {
      s4T  def, low, high, jump;
      s4T  i;

      while ((P-startCodeP) & 3) {
        assert(!*P);
        ++P;
      }
      P = getU4(P, (u4T *) &def);  // default
      P = getU4(P, (u4T *) &low);
      P = getU4(P, (u4T *) &high);

      if(Out)fputc('\t', stdout);
      for (i = low; i <= high; ++i) {
        P = getU4(P, (u4T *) &jump);
        if(Out)printf("%d:%d ", i, (int) (((s4T) pc)+ jump) );
      }
      if(Out)printf("%d\t", (int) (((s4T) pc)+ def) );
      break;
    }
    case 171:  // LookupSwitch
    {
      if(Out)fputc('\t', stdout);
      s4T  def, pairs, match, jump, i;

      while ((P - startCodeP) & 3) {
        assert(!*P);
        ++P;
      }
      P = getU4(P, (u4T *) &def);    // default
      P = getU4(P, (u4T *) &pairs);  // number of pairs
      for (i = 0; i < pairs; ++i) {
        P = getU4(P, (u4T *) &match);
        P = getU4(P, (u4T *) &jump);
        if(Out)printf("%d->%d ", (int) match, (int) (((s4T) pc)+ jump) );
      }
      if(Out)printf("%d\t", (int) (((s4T) pc)+ def));
      break;
    }
    case 178:
    case 179:
    case 180:
    case 181:
      if(Out)fputc('\t', stdout);
      dump_fieldref(u4);
      //if(Out)fputc('\t', stdout); iran
      break;
    case 182:
    case 183:    // Invokespecial
    case 184:
      if(Out)fputc('\t', stdout);
      dump_methodref(u4);
      //if(Out)fputc('\t', stdout); iran
      break;
    case 185:  // Invokeinterface
    {
      int    nargs;

      if(Out)fputc('\t', stdout);
      dump_interfaceref(u4);
      if(Out)fputc('\t', stdout);
      nargs = *P++;
      if(Out)printf("%d\t", nargs);
      *P++;  // Reserved
      break;
    }

    // Managing arrays
    // http://sunsite.ee/java/vmspec/vmspec-18.html#HEADING18-0

    case 187:
    case 189:
    case 192:
    case 193:
      if(Out)fputc('\t', stdout);
      dump_class(u4);
      break;
    case 188:  // newarray
      if(Out)fputc('\t', stdout);
      if (u4 >= 4 && u4 <= 11) {
        if(Out)printf("%s", array_type[u4]);
      } else {
        if(Out)printf("%d", s4);
      }
      if(Out)fputc('\t', stdout);
      break;
    case 196:  // wide
      wide_index = 1;
      break;
    case 197:  // multianewarray
      if(Out)printf("\t");
      dump_class(u4);
      u4 = *P++;  // Dimension
      if(Out)printf("\t%d", u4);
      break;
    }
    if (opcodeP->flags & BRANCHX) {
      if(Out){
        if(notabf == 0){
          fputc('\t',stdout);
        }
        printf("Z%u", branch);//ifnonnull nado no goto saki
      }
      else{
        Label[Lcnt++] = (u4T)branch;
      }
    }

/*
    if (opcodeP->commentP && g_verbose) {
      if(Out)fputc('\n', stdout);
      if(Out)fputs(";// ", stdout);
      if(Out)fputs(opcodeP->commentP, stdout);
      if (opcodeP->flags & BRANCHX) {
        if (opcodeP->commentP[0]) {
          if(Out)fputc(' ', stdout);
        }
        if(Out)printf("goto %d", branch);
      }
    }
*/
    if(Out)fputc('\n', stdout);
  }

  assert(P == endCodeP);

        P = P_bak;
        u2 = u2_bak;
  if (exception_table_length = u2) {
    unsigned int start_pc, end_pc, handler_pc, i;

    //if(Out)fputs("\nException table\n", stdout);
    for (i = 0; i < exception_table_length; ++i) {
      P = getU2(P, &u2);
      start_pc = u2;
      P = getU2(P, &u2);
      end_pc = u2;
      P = getU2(P, &u2);
      handler_pc = u2;
      P = getU2(P, &u2);
      if (u2) {
        if(Out)printf("\t.catch\t");
        dump_class(u2);
              if(Out){
                printf(" from Z%u to Z%u using Z%u", start_pc, end_pc, handler_pc); 
              }
              else{
                Label[Lcnt++] = (u4T)start_pc;
                Label[Lcnt++] = (u4T)end_pc;
                Label[Lcnt++] = (u4T)handler_pc;
              }
      }
      if(Out)fputc('\n', stdout);
    }
        }
}

static void
dump_attribute(int type, const u2T *nameP, u4T length, u1T *infoP)
{
  if (!length) {
    if(Out)fputc('\n',stdout);
    if(Out)fputs(";//\t", stdout);
    dumpUnicode(nameP);
  } else {
    switch (type) {
    case 0:  //    L"SourceFile"
                        /*
      //if(Out)fputs("\n// ", stdout);
      dumpUnicode(nameP);
      if(Out)fputs(" = ", stdout);
                        */
      if(Out)fputs(".source\t",stdout);
      dumpSourceFile(length, infoP);
      break;
    case 1:  //    L"ConstantValue"
      if(Out)fputs(" = ", stdout);
      dumpConstantValue(length, infoP);
      return;
    case 2:  //    L"Exceptions"
      dumpExceptions(length, infoP);
      break;
    case 3: //    L"LineNumberTable"
            if(Out)fputc('\n',stdout);
      if(Out)fputs(";//\t", stdout);
      dumpUnicode(nameP);
      if(Out)fputs(" = ", stdout);
      dumpLineNumberTable(length, infoP);
      break;
    case 4:  //    L"LocalVariableTable"
            if(Out)fputc('\n',stdout);
      if(Out)fputs(";//\t", stdout);
      dumpUnicode(nameP);
      if(Out)fputs(" = ", stdout);
      dumpLocalVariableTable(length, infoP);
      break;
    case 5: //    L"InnerClasses"
      //dumpInnerClasses(length, infoP);
      break;
    case 6:  //    L"EnclosingMethod"
      dumpEnclosingMethod(length, infoP);
      break;
    case 7:  //    L"Code"
      dump_code(length, infoP);
      break;
    default:
    {
      u1T  *endinfoP;

            if(Out)fputc('\n',stdout);
      if(Out)fputs(";//\t", stdout);
      dumpUnicode(nameP);
      if(Out)fputs(" = ", stdout);
      if(Out)fputs("0x", stdout);
      for (endinfoP = infoP + length; infoP < endinfoP; ++infoP) {
        if(Out)printf("%02x", *infoP);
      }
      //if(Out)fputs("\n", stdout);
      break;
  }
    }
  }

  if(Out)fputc('\n', stdout);

}  

static void
dump_attributes(int attributes_count, attribute_infoT *attributesP)
{
  // Order is the order in which the attributes are printed

  const static u2T  *predefinedP[] = {
    /* 0 */  sourceFile,
    /* 1 */ constantValue,
    /* 2 */ exceptions,
    /* 3 */ lineNumberTable,
    /* 4 */ localVariableTable,
    /* 5 */ innerClasses,
    /* 6 */ enclosingMethod,
    /* 7 */ code,
    0
    };

  if (attributes_count) {

    attribute_infoT  *attributeP     = attributesP;
    attribute_infoT  *endAttributesP = attributeP + attributes_count;
    const u2T    *nameP, *name1P;
    int        type;

    for (; attributeP < endAttributesP; ++attributeP) {
      attributeP->seen = 0;
    }

    // Print the known attributes first in the indicated order

    for (type = 0; name1P = (const u2T *) predefinedP[type]; ++type) {
      for (attributeP = attributesP; attributeP < endAttributesP; ++attributeP) {
        nameP = get_Utf8(attributeP->attribute_name_index);
        if (!unicodecmp(nameP, name1P)) {
          dump_attribute(type, nameP, attributeP->attribute_length, attributeP->infoP);
          attributeP->seen = 1;
          break;
    }  }  }

    for (attributeP = attributesP; attributeP < endAttributesP; ++attributeP) {
      if (!attributeP->seen) {
        nameP    = get_Utf8(attributeP->attribute_name_index);
        dump_attribute(-1, nameP, attributeP->attribute_length, attributeP->infoP);
    }  }
}  }  

static void
dump_interfaces(void)
{
  int  count;

  if (count = g_cf.m_interfaces_count) {
    cp_infoT  *constant_poolP;
    int      seen;
    u2T      *interfaceP, *end_interfaceP, slot;

    constant_poolP = g_cf.m_constant_poolP;
    interfaceP     = g_cf.m_interfacesP;
    for (end_interfaceP = interfaceP + count; interfaceP < end_interfaceP; ++interfaceP) {
      slot = *interfaceP;
      if (slot) {
        if(Out)fputs(".implements ", stdout);
        dump_class(slot);
  if(Out)fputc('\n',stdout);
      }
    }
    if(Out)fputc('\n',stdout);
  }
}

static void
dump_fields(void)
{
  int  count;
  if (count = g_cf.m_fields_count) {
    cp_infoT  *constant_poolP = g_cf.m_constant_poolP;
    field_infoT  *fieldP, *end_fieldP;
  
    fieldP = g_cf.m_fieldsP;
    for (end_fieldP = fieldP + count; fieldP < end_fieldP; ++fieldP) {
      if(Out)fputs(".field\t", stdout);
      dump_access(fieldP->access_flags);
      dump_Utf8(fieldP->name_index);
      if(Out)fputc(' ', stdout);
      dump_descriptor(get_Utf8(fieldP->descriptor_index), 0, 0);
      dump_attributes(fieldP->attributes_count, fieldP->attributesP);
      if(Out)fputc('\n', stdout);
}  }  }

/* add decompile protection code */
void guardc()
{
  int i,n,pf,cd;
  if(Out == 0){
    return;
  }
  srand(time(NULL));

  fputs("\tpop\n",stdout);
  n = rand() % 8 + 2; // 2-9
  pf = 0;
  for(i=0;i<n;++i){
    cd = rand() % 22;
    if(cd == 0){
      fprintf(stdout,"\tldc\t\"%c\"\n",'A' + rand()%27);
    }
    else
    if(cd == 1){
      fputs("\tiand\n",stdout);
    }
    else
    if(cd == 2){
      fputs("\tfmul\n",stdout);
    }
    else
    if(cd == 3){
      fputs("\tdup\n",stdout);
    }
    else
    if(cd == 4){
      fputs("\tdcmpg\n",stdout);
    }
    else
    if(cd == 5){
      fputs("\tddiv\n",stdout);
    }
    else
    if(cd == 6){
      fprintf(stdout,"\tldc\t%d\n",(rand()%8)-4);
    }
    else
    if(cd == 7){
      //fprintf(stdout,"\iadd\n");
    }
    else
    if(cd == 8){
      fprintf(stdout,"\ticonst_0\n");
    }
    else
    if(cd == 9){
      fprintf(stdout,"\taastore\n");
    }
    else
    if(cd == 10){
      fprintf(stdout,"\tldc\t\"%c%c\"\n",'a' + rand()%27,'a'+rand()%27);
    }
    else
    if(cd == 11){
      fprintf(stdout,"\tisub\n");
    }
    else
    if(cd == 12){
      fprintf(stdout,"\tfadd\n");
    }
    else
    if(cd == 13){
      fprintf(stdout,"\tireturn\n");
    }
    else
    if(cd == 14){
      fprintf(stdout,"\treturn\n");
    }
    else
    if(cd == 15){
      fprintf(stdout,"\tf2i\n");
    }
    else
    if(cd == 16){
      fprintf(stdout,"\ticonst_1\n");
    }
    else
    if(cd == 17){
      fprintf(stdout,"\tfsub\n");
    }
    else
    if(cd == 18){
      fprintf(stdout,"\tidiv\n");
    }
    else
    if(cd == 19){
      fprintf(stdout,"\tbipush\t%d\n",rand()%128);
    }
    else
    if(cd == 20){
      fprintf(stdout,"\tsipush\t%d\n",rand()%128);
    }
    else{
      fputs("\tpop\n",stdout);
      pf = 1;
    }
  }
  if(pf == 0){
      fputs("\tpop\n",stdout);
  }
  if((rand()%5)==0){
    int gyo;
    gyo = rand() % 1000;
    printf("G%d:\n",gyo);
    if((rand()%2)==0){
      printf("\tldc\t%d.%d\n",rand() % 3000,rand() % 10);
      printf("\tldc\t%d.%d\n",rand() % 3000,rand() % 10);
      if((rand()%2)==0){
        fputs("\tfadd\n",stdout);
      }
      else{
        fputs("\tfmul\n",stdout);
      }
    }
    else{
      printf("\tldc\t%d\n",rand() % 3000);
      printf("\tldc\t%d\n",rand() % 3000);
      if((rand()%2)==0){
        fputs("\tiadd\n",stdout);
      }
      else{
        fputs("\timul\n",stdout);
      }
    }
    printf("\tgoto\tG%d\n",gyo);
  }
  if(rand()%2){
    fputs("\treturn\n",stdout);
  }
  else{
    fputs("\tireturn\n",stdout);
  }
}

static void
dump_methods(void)
{
  int        count;
  u2T        attributes_cnt, u2, name_index, i;
  attribute_infoT  attribute_info;
  attribute_infoT  *attributesP, *codeP, *localVariableTableP, *endAttributesP;
  const u2T    *nameP;
  u1T        *P;
  u4T        length;

  if (count = g_cf.m_methods_count) {
    cp_infoT    *constant_poolP = g_cf.m_constant_poolP;
    method_infoT  *methodP, *end_methodP;
  
    methodP = g_cf.m_methodsP;
    for (end_methodP = methodP + count; methodP < end_methodP; ++methodP) {
      Lcnt = 0;
      for(int lc=0;lc < 2;++lc){
        Out = lc; 
        if(Out)fputc('\n',stdout);
        if(Out)fputs(".method", stdout);
        if(Out)fputc('\t', stdout);
        dump_access(methodP->access_flags);
        dump_Utf8(methodP->name_index);
        //if(Out)fputc(' ', stdout);
        
        localVariableTableP = 0;
  
        // Find the code attribute 
        attributes_cnt = methodP->attributes_count;
        attributesP    = codeP = methodP->attributesP;
        for (endAttributesP = attributesP + attributes_cnt; ; ++codeP) {
          if (codeP >= endAttributesP) {
            codeP = 0;
            break;
          }
          nameP = get_Utf8(codeP->attribute_name_index);
          if (!unicodecmp(nameP, code)) {
            break;
          }
        }
  
        if (codeP) {
          // Find the localVariableTableP
          P  = codeP->infoP + sizeof(u2T) /* max_stack */ + sizeof(u2T) /* max_locals */;
          P = getU4(P, &length);  /* Code length */
          // Skip the code
          P += length;
  
          P  = getU2(P, &u2);
          // Skip exception table
          P += u2 * sizeof(u2T) * 4;
          P  = getU2(P, &u2);
  
          for (i = 0; i < u2; ++i) {
            P = getU2(P, &name_index);
            nameP = get_Utf8(name_index);
            P = getU4(P, &length);
            if (!unicodecmp(nameP, localVariableTable)) {
              attribute_info.attribute_length = length;
              attribute_info.infoP = P;
              localVariableTableP = &attribute_info;
              break;
            }
            P += length;
          }
        }
        dump_descriptor(get_Utf8(methodP->descriptor_index), localVariableTableP, (methodP->access_flags & 8));
if(Out)fputc('\n', stdout);//need
        dump_attributes(attributes_cnt, attributesP);
        if(Out){
          if(Guardf){
            guardc();
          }
        }
        if(Out)fputs(".end method\n",stdout);
      }
    }
  }
}

static void
dump(void)
{
  cp_infoT  *constant_poolP = g_cf.m_constant_poolP;
  u2T      slot;
  int      attributes_count;

  if(Out)printf(";/* Class file %s version %d.%d */\n", g_nameP, g_cf.m_major_version, g_cf.m_minor_version);

  if (attributes_count = g_cf.m_attributes_count) {
    //if(Out)fputs("\n;// Attributes \n", stdout);
    dump_attributes(attributes_count, g_cf.m_attributesP);
    if(Out)fputc('\n', stdout);
  }

  //dump_imports();

        if(Out)fputs(".class\t",stdout);
  dump_access(g_cf.m_access_flags);
  if (!(g_cf.m_access_flags & 0x0200)) {
    //if(Out)fputs("class ", stdout);
  }
  dump_class(g_cf.m_this_class);
  if(Out)fputc('\n', stdout);
  
  slot = g_cf.m_super_class;
  if (slot) {
    if(Out)fputs(".super\t", stdout);
    dump_class(slot);
    if(Out)fputc('\n', stdout);
  }

  dump_interfaces();
  //if(Out)fputs(" {\n", stdout);

  //if(Out)fputs("\n\t/* Fields */\n", stdout);

  dump_fields();


  dump_methods();

  //if(Out)fputs("}\n", stdout);

  //if(Out)fputs("/* End of Report */\n", stdout);
}

/* Read the class image from disk */

static int
read_image(void)
{
  struct  stat   stats;
  int        fileno;
  int        ret;

  if (stat(g_nameP, &stats)) {
    fprintf(stderr, "Can't stat %s\n", g_nameP);
    goto fail;
  }
  g_image_size = stats.st_size;

  if (g_image_maxsize < g_image_size) {
    if (g_imageP) {
      free(g_imageP);
    }
    g_imageP = (u1T *) malloc(g_image_size + 1);
    if (!g_imageP) {
      fprintf(stderr, "Can't malloc %d bytes\n", g_image_size+1);
      goto fail;
    }
    g_image_maxsize = g_image_size;
  }
  fileno = open(g_nameP, O_RDONLY | _O_BINARY , 0 );
  if (fileno == -1) {
    fprintf(stderr, "Can't open %s\n", g_nameP);
    goto fail;
  }
  ret = read(fileno, g_imageP, g_image_size);
  if (ret != g_image_size) {
    fprintf(stderr, "Can't read %s\n", g_nameP);
    goto fail;
  }

  if (close(fileno)) {
    fprintf(stderr, "Can't close %s\n", g_nameP);
    goto fail;
  }

  g_imageP[g_image_size] = 0;
  return(0);
fail:
  return(-1);
}

/* Phase 1: Load basic things and compute space needed for tables */

static int
compute_needed(void)
{
  int        need;

  u1T        *P;
  int        i, j;
  u4T        attribute_length, unicode_length;
  u2T        length, interfaces_count, fields_count, methods_count;
  u2T        attributes_count, u2;

  need         = 0;

  P = getU4(g_imageP, &g_cf.m_magic);

  if (g_cf.m_magic != 0xCAFEBABE) {
    fprintf(stderr, "%s is not a valid class file - has magic value 0x%08x\n", g_nameP, g_cf.m_magic);
    goto fail;
  }
  P = getU2(P, &g_cf.m_minor_version);
  P = getU2(P, &g_cf.m_major_version);
  P = getU2(P, &g_cf.m_constant_pool_count);

  g_cf.m_constant_poolP = 0;
  if (g_cf.m_constant_pool_count) {
    need += Align(sizeof(g_cf.m_constant_poolP[0]) * g_cf.m_constant_pool_count);

    for (i = 1; i < g_cf.m_constant_pool_count; ++i) {
//      fprintf(stderr, "%d) %s ", i, constantName(*P));
      switch (*P++) {
      case CONSTANT_Utf8:
        getU2(P, &length);
        P    += 2;

        unicode_length = utf8lth(length,P) + 1;
        need += Align(unicode_length * sizeof(u2T));
        P    += length;
        break;
      case CONSTANT_Class:
      case CONSTANT_String:
        getU2(P, &u2);
//          fprintf(stderr, "%u", u2);
        P    += 2;
        break;
      case CONSTANT_Integer:
      case CONSTANT_Float:
      case CONSTANT_NameAndType:
      case CONSTANT_Fieldref:
      case CONSTANT_Methodref:
      case CONSTANT_InterfaceMethodref:
        P    += 4;
        break;
      case CONSTANT_Long:
      case CONSTANT_Double:
        P    += 8;
        // 8 byte values take up two slots in the constant point table
        ++i;
        break;
      default:
        fprintf(stderr, "Unknown constant code %d\n", P[-1]);
        goto fail;
      }
//        fputc('\n', stderr);
  }  }
  P = getU2(P, &g_cf.m_access_flags);
  P = getU2(P, &g_cf.m_this_class);
  P = getU2(P, &g_cf.m_super_class);
  P = getU2(P, &g_cf.m_interfaces_count);

  g_cf.m_interfacesP = 0;
  if (interfaces_count = g_cf.m_interfaces_count) {
    need += Align(sizeof(g_cf.m_interfacesP[0]) * interfaces_count);
    P    += sizeof(u2T) * interfaces_count;
  }

  P = getU2(P, &g_cf.m_fields_count);
  g_cf.m_fieldsP = 0;
  if (fields_count = g_cf.m_fields_count) {
    need += Align(sizeof(g_cf.m_fieldsP[0]) * fields_count);

    for (i = 0; i < fields_count; ++i) {
      P    += 6;
      P     = getU2(P, &attributes_count);
      if (attributes_count) {
        need += Align(sizeof(attribute_infoT) * attributes_count);
        for (j = 0; j < attributes_count; ++j) {
          P += 2;  // attribute_name_index
          P  = getU4(P, &attribute_length);
          P += attribute_length;
  }  }  }  }

  P = getU2(P, &g_cf.m_methods_count);
  g_cf.m_methodsP = 0;
  if (methods_count = g_cf.m_methods_count) {
    need += Align(sizeof(g_cf.m_methodsP[0]) * methods_count);
    for (i = 0; i < methods_count; ++i) {
      P    += 6;
      P     = getU2(P, &attributes_count);
      if (attributes_count) {
        need += Align(sizeof(attribute_infoT) * attributes_count);
        for (j = 0; j < attributes_count; ++j) {
          P += 2;  // attribute_name_index
          P  = getU4(P, &attribute_length);
          P += attribute_length;
  }  }  }  }

  P = getU2(P, &g_cf.m_attributes_count);
  g_cf.m_attributesP = 0;
  if (attributes_count = g_cf.m_attributes_count) {
    need += Align(sizeof(attribute_infoT) * attributes_count);
  }

  assert(P - g_imageP <= g_image_size);

  g_need_size = need;
  return(0);
fail:
  return(-1);
}

/* Phase 2: Load tables */

static int
load_tables(void)
{
  u1T        *P, *P1;
  cp_infoT    *cpP;
  field_infoT    *fieldP;
  method_infoT  *methodP;
  u2T        *interfaceP;
  attribute_infoT  *attributeP;
  int        i, j;
  u4T        attribute_length;
  u2T        length, interfaces_count, fields_count, methods_count;
  u2T        attributes_count;

  if (g_need_size > g_need_maxsize) {
    if (g_needP) {
      free(g_needP);
    }
    g_needP = (u1T *) malloc(g_need_size);
    
    if (!g_needP) {
      fprintf(stderr, "Can't allocate needed space for %s\n", g_nameP);
      goto fail;
    }
    g_need_maxsize = g_need_size;
  }
  P  = g_imageP + 10;
  P1 = g_needP;
  if (g_cf.m_constant_pool_count) {
    g_cf.m_constant_poolP = cpP = (cp_infoT *) P1;
    P1 += Align(sizeof(*cpP) * g_cf.m_constant_pool_count);

    cpP->tag = CONSTANT_Null;
    ++cpP;
    for (i = 1; i < g_cf.m_constant_pool_count; ++i) {
/*
      if (i == 130) {
        fprintf(stderr, "trapped\n");
      }
*/
      cpP->tag = (constantE) *P++;
      switch (cpP->tag) {
      case CONSTANT_Utf8:
        getU2(P, &length);
        P    += 2;
        cpP->u.stringP = (u2T *) P1;

        P     = getUtf8(length, P, (u2T **) &P1);
        break;
      case CONSTANT_Integer:
      case CONSTANT_Float:
        P     = getU4(P, &cpP->u.u4val);
        break;
      case CONSTANT_NameAndType:
        P     = getU2(P, &cpP->u.ref.index1);
        P     = getU2(P, &cpP->u.ref.index2);
        break;
      case CONSTANT_Long:
      case CONSTANT_Double:
        P     = getU8(P, &cpP->u.u8val);
        // 8 byte values take 2 slots in the constant pool table
        ++cpP;
        cpP->tag = CONSTANT_Null;
        ++i;
        break;
      case CONSTANT_Class:
      case CONSTANT_String:
        P     = getU2(P, &cpP->u.ref.index1);
        break;
      case CONSTANT_Fieldref:
      case CONSTANT_Methodref:
      case CONSTANT_InterfaceMethodref:
        P     = getU2(P, &cpP->u.ref.index1);
        P     = getU2(P, &cpP->u.ref.index2);
        break;
      default:
        fprintf(stderr, "Unknown constant code %d\n", P[-1]);
        goto fail;
      }
      ++cpP;
  }  }

  assert(cpP == g_cf.m_constant_poolP + g_cf.m_constant_pool_count);

  P += 8;

  if (interfaces_count = g_cf.m_interfaces_count) {
    interfaceP = (u2T *) P1;
    g_cf.m_interfacesP = interfaceP;
    P1 += Align(sizeof(*interfaceP) * interfaces_count);
    for (i = 0; i < interfaces_count; ++i) {
      P = getU2(P, interfaceP);
      ++interfaceP;
  }  }

  P += 2;
  if (fields_count = g_cf.m_fields_count) {
    fieldP = (field_infoT *) P1;
    g_cf.m_fieldsP = fieldP;
    P1 += Align(sizeof(*fieldP) * fields_count);
    for (i = 0; i < fields_count; ++i) {
      P = getU2(P, &fieldP->access_flags);
      P = getU2(P, &fieldP->name_index);
      P = getU2(P, &fieldP->descriptor_index);
      P = getU2(P, &fieldP->attributes_count);
      if (!(attributes_count = fieldP->attributes_count)) {
        fieldP->attributesP = 0;
      } else {
        attributeP = (attribute_infoT *) P1;
        fieldP->attributesP = attributeP;
        P1 += Align(sizeof(*attributeP) * attributes_count);
        for (j = 0; j < attributes_count; ++j) {
          P = getU2(P, &attributeP->attribute_name_index);
          P = getU4(P, &attribute_length);
          attributeP->attribute_length = attribute_length;
          attributeP->infoP = P;
          P += attribute_length;
          ++attributeP;
      }  }
      ++fieldP;
  }  }

  P += 2;
  if (methods_count = g_cf.m_methods_count) {
    methodP = (method_infoT *) P1;
    g_cf.m_methodsP = methodP;
    P1 += Align(sizeof(*methodP) * methods_count);
    for (i = 0; i < methods_count; ++i) {
      P = getU2(P, &methodP->access_flags);
      P = getU2(P, &methodP->name_index);
      P = getU2(P, &methodP->descriptor_index);
      P = getU2(P, &methodP->attributes_count);
      if (!(attributes_count = methodP->attributes_count)) {
        methodP->attributesP = 0;
      } else {
        attributeP = (attribute_infoT *) P1;
        methodP->attributesP = attributeP;
        P1 += Align(sizeof(*attributeP) * attributes_count);
        for (j = 0; j < attributes_count; ++j) {
          P = getU2(P, &attributeP->attribute_name_index);
          P = getU4(P, &attribute_length);
          attributeP->attribute_length = attribute_length;
          attributeP->infoP = P;
          P += attribute_length;
          ++attributeP;
      }  }
      ++methodP;
  }  }

  P += 2;
  if (attributes_count = g_cf.m_attributes_count) {
    attributeP = (attribute_infoT *) P1;
    g_cf.m_attributesP = attributeP;
    P1 += Align(sizeof(*attributeP) * attributes_count);
    for (j = 0; j < attributes_count; ++j) {
      P = getU2(P, &attributeP->attribute_name_index);
      P = getU4(P, &attribute_length);
      attributeP->attribute_length = attribute_length;
      attributeP->infoP = P;
      P += attribute_length;
      ++attributeP;
  }  }

  assert((P - g_imageP) == g_image_size);
  assert((P1 - g_needP) == g_need_size);
  return(0);
fail:
  return(-1);
}

void copyright(void)
{
  fprintf(stderr, "jhoja ver 1.01  Masaki Oba 2018\n");
  fprintf(stderr, "SWAG javap2 Ver 1.02 based Java disassembler.\n");
  fprintf(stderr, "Convert Java class file to Jasmin Java assembler code.\n");
  fprintf(stderr, "jhoja can insert decompile protection code.(-g option)\n");
}

void help(void)
{
  copyright();
  fprintf(stderr, "\nUsage: jhoja [-g] [-h] xxx.class > xxx.j\n");
  fprintf(stderr, "   -g: Insert decompile protection code.\n");
  fprintf(stderr, "   -h: show help.\n");
  fprintf(stderr, "\nExamples:\n");
  fprintf(stderr, "jhoja -g xxx.class > xxx.j\n");
  fprintf(stderr, "jhoja xxx.class > xxx.j\n");
  fprintf(stderr, "jhoja xxx.class | more\n");
}

int
main(int argc, char **argv)
{
  char  *argP;
  int    exitcode;
  int    i;

  exitcode = 1;

  if(argc == 1){
    help();
    return 0;
  }

  for (i = 1; i < argc; ++i) {
    argP = argv[i];
    if (*argP != '-') {
      break;
    }
    switch (argP[1]) {
    case 'h':
      help();
      return 0;
    case 'g':
      /* add decompile protection code */
      Guardf = 1;
      break;
    case 'c':
      fprintf(stderr, "Version 1.01 Compiled " __DATE__ " at " __TIME__ "\n");
      break;
    case 'v':
      g_verbose = 1;
      break;
    case 'd':
      g_demangle = 1;
      break;
    case 't':
      g_tables = 1;
      break;
    default:
      fprintf(stderr, "Error: Valid option\n");
      help();
      //fprintf(stderr, "Valid options:\n -v/erbose\n -d/emangle -t/ables\n");
      return(1);
  }  }

  for (; i < argc; ++i) {
    g_nameP = argv[i];
    if (read_image()) {
      goto done;
    }

    if (compute_needed()) {
      goto done;
    }

    if (load_tables()) {
      goto done;
    }

    dump();
  }
  exitcode = 0;
done:
  return(exitcode);
}

