/*
** $Id: $
** Built-in functions
** See Copyright Notice in lua.h
*/


#include <stdio.h>

#include "lapi.h"
#include "lauxlib.h"
#include "lbuiltin.h"
#include "lglobal.h"
#include "lmem.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"


static void nextvar (void)
{
  int i = luaG_nextvar(lua_isnil(lua_getparam(1)) ? 0 :
                             luaG_findsymbolbyname(luaL_check_string(1))+1);
  if (i >= 0) {
    lua_pushstring(luaG_global[i].varname->str);
    luaA_pushobject(&s_object(i));
  }
}


static void next (void)
{
  lua_Object o = lua_getparam(1);
  lua_Object r = lua_getparam(2);
  Node *n;
  luaL_arg_check(lua_istable(o), 1, "table expected");
  luaL_arg_check(r != LUA_NOOBJECT, 2, "value expected");
  n = luaH_next(luaA_Address(o), luaA_Address(r));
  if (n) {
    luaA_pushobject(&n->ref);
    luaA_pushobject(&n->val);
  }
}


static void internaldostring (void)
{
  lua_Object err = lua_getparam(2);
  if (err != LUA_NOOBJECT) {  /* set new error method */
    lua_pushobject(err);
    err = lua_seterrormethod();
  }
  if (lua_dostring(luaL_check_string(1)) == 0)
    if (luaA_passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
  if (err != LUA_NOOBJECT) {  /* restore old error method */
    lua_pushobject(err);
    lua_seterrormethod();
  }
}


static void internaldofile (void)
{
  char *fname = luaL_opt_string(1, NULL);
  if (lua_dofile(fname) == 0)
    if (luaA_passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
}


static char *to_string (lua_Object obj)
{
  char *buff = luaM_buffer(30);
  TObject *o = luaA_Address(obj);
  switch (ttype(o)) {
    case LUA_T_NUMBER:  case LUA_T_STRING:
      return lua_getstring(obj);
    case LUA_T_ARRAY: {
      sprintf(buff, "table: %p", o->value.a);
      return buff;
    }
    case LUA_T_FUNCTION: {
      sprintf(buff, "function: %p", o->value.cl);
      return buff;
    }
    case LUA_T_CFUNCTION: {
      sprintf(buff, "cfunction: %p", o->value.f);
      return buff;
    }
    case LUA_T_USERDATA: {
      sprintf(buff, "userdata: %p", o->value.ts->u.v);
      return buff;
    }
    case LUA_T_NIL:
      return "nil";
    default: return "<unknown object>";
  }
}

static void bi_tostring (void)
{
  lua_pushstring(to_string(lua_getparam(1)));
}


static void luaI_print (void)
{
  int i = 1;
  lua_Object obj;
  while ((obj = lua_getparam(i++)) != LUA_NOOBJECT)
    printf("%s\n", to_string(obj));
}


static void luaI_type (void)
{
  lua_Object o = lua_getparam(1);
  luaL_arg_check(o != LUA_NOOBJECT, 1, "no argument");
  lua_pushstring(luaO_typenames[-ttype(luaA_Address(o))]);
  lua_pushnumber(lua_tag(o));
}


static void lua_obj2number (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isnumber(o))
    lua_pushnumber(lua_getnumber(o));
}


static void luaI_error (void)
{
  char *s = lua_getstring(lua_getparam(1));
  if (s == NULL) s = "(no message)";
  lua_error(s);
}


static void luaI_assert (void)
{
  lua_Object p = lua_getparam(1);
  if (p == LUA_NOOBJECT || lua_isnil(p))
    lua_error("assertion failed!");
}


static void setglobal (void)
{
  lua_Object value = lua_getparam(2);
  luaL_arg_check(value != LUA_NOOBJECT, 2, NULL);
  lua_pushobject(value);
  lua_setglobal(luaL_check_string(1));
  lua_pushobject(value);  /* return given value */
}

static void rawsetglobal (void)
{
  lua_Object value = lua_getparam(2);
  luaL_arg_check(value != LUA_NOOBJECT, 2, NULL);
  lua_pushobject(value);
  lua_rawsetglobal(luaL_check_string(1));
  lua_pushobject(value);  /* return given value */
}

static void getglobal (void)
{
  lua_pushobject(lua_getglobal(luaL_check_string(1)));
}

static void rawgetglobal (void)
{
  lua_pushobject(lua_rawgetglobal(luaL_check_string(1)));
}

static void luatag (void)
{
  lua_pushnumber(lua_tag(lua_getparam(1)));
}


static int getnarg (lua_Object table)
{
  lua_Object temp;
  /* temp = table.n */
  lua_pushobject(table); lua_pushstring("n"); temp = lua_gettable();
  return (lua_isnumber(temp) ? lua_getnumber(temp) : MAX_WORD);
}

static void luaI_call (void)
{
  lua_Object f = lua_getparam(1);
  lua_Object arg = lua_getparam(2);
  int withtable = (strcmp(luaL_opt_string(3, ""), "pack") == 0);
  int narg, i;
  luaL_arg_check(lua_isfunction(f), 1, "function expected");
  luaL_arg_check(lua_istable(arg), 2, "table expected");
  narg = getnarg(arg);
  /* push arg[1...n] */
  for (i=0; i<narg; i++) {
    lua_Object temp;
    /* temp = arg[i+1] */
    lua_pushobject(arg); lua_pushnumber(i+1); temp = lua_gettable();
    if (narg == MAX_WORD && lua_isnil(temp))
      break;
    lua_pushobject(temp);
  }
  if (lua_callfunction(f))
    lua_error(NULL);
  else if (withtable)
    luaA_packresults();
  else
    luaA_passresults();
}


static void settag (void)
{
  lua_Object o = lua_getparam(1);
  luaL_arg_check(lua_istable(o), 1, "table expected");
  lua_pushobject(o);
  lua_settag(luaL_check_number(2));
}


static void newtag (void)
{
  lua_pushnumber(lua_newtag());
}


static void rawgettable (void)
{
  lua_Object t = lua_getparam(1);
  lua_Object i = lua_getparam(2);
  luaL_arg_check(t != LUA_NOOBJECT, 1, NULL);
  luaL_arg_check(i != LUA_NOOBJECT, 2, NULL);
  lua_pushobject(t);
  lua_pushobject(i);
  lua_pushobject(lua_rawgettable());
}


static void rawsettable (void)
{
  lua_Object t = lua_getparam(1);
  lua_Object i = lua_getparam(2);
  lua_Object v = lua_getparam(3);
  luaL_arg_check(t != LUA_NOOBJECT && i != LUA_NOOBJECT && v != LUA_NOOBJECT,
                 0, NULL);
  lua_pushobject(t);
  lua_pushobject(i);
  lua_pushobject(v);
  lua_rawsettable();
}


static void settagmethod (void)
{
  lua_Object nf = lua_getparam(3);
  luaL_arg_check(nf != LUA_NOOBJECT, 3, "value expected");
  lua_pushobject(nf);
  lua_pushobject(lua_settagmethod((int)luaL_check_number(1),
                                  luaL_check_string(2)));
}


static void gettagmethod (void)
{
  lua_pushobject(lua_gettagmethod((int)luaL_check_number(1),
                                  luaL_check_string(2)));
}


static void seterrormethod (void)
{
  lua_Object nf = lua_getparam(1);
  luaL_arg_check(nf != LUA_NOOBJECT, 1, "value expected");
  lua_pushobject(nf);
  lua_pushobject(lua_seterrormethod());
}


static void luaI_collectgarbage (void)
{
  lua_pushnumber(lua_collectgarbage(luaL_opt_number(1, 0)));
}


/*
** =======================================================
** some DEBUG functions
** =======================================================
*/
#ifdef DEBUG

static void testC (void)
{
#define getnum(s)	((*s++) - '0')
#define getname(s)	(nome[0] = *s++, nome)

  static int locks[10];
  lua_Object reg[10];
  char nome[2];
  char *s = luaL_check_string(1);
  nome[1] = 0;
  while (1) {
    switch (*s++) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        lua_pushnumber(*(s-1) - '0');
        break;

      case 'c': reg[getnum(s)] = lua_createtable(); break;
      case 'P': reg[getnum(s)] = lua_pop(); break;
      case 'g': { int n=getnum(s); reg[n]=lua_getglobal(getname(s)); break; }
      case 'G': { int n = getnum(s);
                  reg[n] = lua_rawgetglobal(getname(s));
                  break;
                }
      case 'l': locks[getnum(s)] = lua_ref(1); break;
      case 'L': locks[getnum(s)] = lua_ref(0); break;
      case 'r': { int n=getnum(s); reg[n]=lua_getref(locks[getnum(s)]); break; }
      case 'u': lua_unref(locks[getnum(s)]); break;
      case 'p': { int n = getnum(s); reg[n] = lua_getparam(getnum(s)); break; }
      case '=': lua_setglobal(getname(s)); break;
      case 's': lua_pushstring(getname(s)); break;
      case 'o': lua_pushobject(reg[getnum(s)]); break;
      case 'f': lua_call(getname(s)); break;
      case 'i': reg[getnum(s)] = lua_gettable(); break;
      case 'I': reg[getnum(s)] = lua_rawgettable(); break;
      case 't': lua_settable(); break;
      case 'T': lua_rawsettable(); break;
      default: luaL_verror("unknown command in `testC': %c", *(s-1));
    }
  if (*s == 0) return;
  if (*s++ != ' ') lua_error("missing ` ' between commands in `testC'");
  }
}

#endif


/*
** Internal functions
*/
static struct luaL_reg int_funcs[] = {
#if LUA_COMPAT2_5
  {"setfallback", luaT_setfallback},
#endif
#ifdef DEBUG
  {"testC", testC},
  {"totalmem", luaM_query},
#endif
  {"assert", luaI_assert},
  {"call", luaI_call},
  {"collectgarbage", luaI_collectgarbage},
  {"dofile", internaldofile},
  {"dostring", internaldostring},
  {"error", luaI_error},
  {"getglobal", getglobal},
  {"newtag", newtag},
  {"next", next},
  {"nextvar", nextvar},
  {"print", luaI_print},
  {"rawgetglobal", rawgetglobal},
  {"rawgettable", rawgettable},
  {"rawsetglobal", rawsetglobal},
  {"rawsettable", rawsettable},
  {"seterrormethod", seterrormethod},
  {"setglobal", setglobal},
  {"settagmethod", settagmethod},
  {"gettagmethod", gettagmethod},
  {"settag", settag},
  {"tonumber", lua_obj2number},
  {"tostring", bi_tostring},
  {"tag", luatag},
  {"type", luaI_type}
};


#define INTFUNCSIZE (sizeof(int_funcs)/sizeof(int_funcs[0]))


void luaB_predefine (void)
{
  int i;
  Word n;
  /* pre-register mem error messages, to avoid loop when error arises */
  luaS_newfixedstring(tableEM);
  luaS_newfixedstring(memEM);
  for (i=0; i<INTFUNCSIZE; i++) {
    n = luaG_findsymbolbyname(int_funcs[i].name);
    s_ttype(n) = LUA_T_CFUNCTION;
    fvalue(&s_object(n)) = int_funcs[i].func;
  }
  n = luaG_findsymbolbyname("_VERSION");
  s_ttype(n) = LUA_T_STRING;
  tsvalue(&s_object(n)) = luaS_new(LUA_VERSION);
}


