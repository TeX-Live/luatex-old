/* lnewtokenlib.c
   
   Copyright 2006-2012 Taco Hoekwater <taco@luatex.org>

   This file is part of LuaTeX.

   LuaTeX is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   LuaTeX is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU General Public License along
   with LuaTeX; if not, see <http://www.gnu.org/licenses/>. */

#include "ptexlib.h"
#include "lua/luatex-api.h"

typedef struct lua_token {
    int token;
    int origin;
} lua_token;

typedef struct saved_tex_scanner {
    int token;
    int origin;
    int save_cmd, save_chr, save_cs, save_tok;
} saved_tex_scanner;

#define save_tex_scanner(a) do {		  \
	a.save_cmd = cur_cmd;			  \
	a.save_chr = cur_chr;			  \
	a.save_cs  = cur_cs;			  \
	a.save_tok = cur_tok;			  \
    } while (0)

#define unsave_tex_scanner(a) do {		  \
	cur_cmd = a.save_cmd;			  \
	cur_chr = a.save_chr;			  \
	cur_cs = a.save_cs;			  \
	cur_tok = a.save_tok;			  \
    } while (0)


#define TEX_ORIGIN 0 /* not used yet */
#define LUA_ORIGIN 1

static lua_token *check_istoken(lua_State * L, int ud);

#define TOKEN_METATABLE  "luatex_newtoken"

#define DEBUG 0
#define DEBUG_OUT stdout


/* two core helpers */
#define  is_active_string(s) (strlen((char *)s)>3 && *s==0xEF && *(s+1)==0xBF && *(s+2)==0xBF)

static unsigned char *get_cs_text(int cs)
{
    if (cs == null_cs)
        return (unsigned char *) xstrdup("\\csname\\endcsname");
    else if ((cs_text(cs) < 0) || (cs_text(cs) >= str_ptr))
        return (unsigned char *) xstrdup("");
    else
        return (unsigned char *) makecstring(cs_text(cs));
}



/* maybe this qualify as  a macro, not function */

static lua_token *maybe_istoken(lua_State * L, int ud)
{
    lua_token *p = lua_touserdata(L, ud);
    if (p != NULL) {
        if (lua_getmetatable(L, ud)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaS_index(luatex_newtoken));
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (!lua_rawequal(L, -1, -2))
                p = NULL;
            lua_pop(L, 2);
        }
    }
    return p;
}

/* we could make the message a function and just inline the rest (via a macro) */

lua_token *check_istoken(lua_State * L, int ud)
{
    lua_token *p = maybe_istoken(L, ud);
    if (p != NULL)
        return p;
    luatex_fail("There should have been a lua <token> here, not an object with type %s!", luaL_typename(L, ud));
    return NULL;
}

/* token library functions */

static void make_new_token(lua_State * L, int cmd, int chr, int cs)
{
    int tok = 0;
    lua_token *thetok = lua_newuserdata(L, sizeof(lua_token));
    thetok->origin = LUA_ORIGIN;
    fast_get_avail(thetok->token);
    tok = (cs ? cs_token_flag + cs : token_val(cmd, chr));
    set_token_info(thetok->token, tok);
    lua_rawgeti(L, LUA_REGISTRYINDEX, luaS_index(luatex_newtoken));
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_setmetatable(L, -2);
}

static void push_token(lua_State * L, int tok)
{
    lua_token *thetok = lua_newuserdata(L, sizeof(lua_token));
    thetok->origin = LUA_ORIGIN;
    thetok->token = tok;
    lua_rawgeti(L, LUA_REGISTRYINDEX, luaS_index(luatex_newtoken));
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_setmetatable(L, -2);
}


static int run_get_cs_offset(lua_State * L)
{
    lua_pushnumber(L, cs_token_flag);
    return 1;
}

static int run_get_command_id(lua_State * L)
{
    int cs = -1;
    if (lua_isstring(L, -1)) {
        cs = get_command_id(lua_tostring(L, -1));
    }
    lua_pushnumber(L, cs);
    return 1;
}

static int run_get_csname_id(lua_State * L)
{
    const char *s;
    size_t k, cs = 0;
    if (lua_isstring(L, -1)) {
        s = lua_tolstring(L, -1, &k);
        cs = (size_t) id_lookup(s, k, true);
    }
    lua_pushnumber(L, (lua_Number) cs);
    return 1;
}

static int run_get_next(lua_State * L)
{
    saved_tex_scanner texstate;
    save_tex_scanner(texstate);
    get_next(false);
    make_new_token(L, cur_cmd, cur_chr, cur_cs);
    unsave_tex_scanner(texstate);
    return 1;
}

static int run_scan_keyword(lua_State * L)
{
    saved_tex_scanner texstate;
    const char *s = luaL_checkstring(L, -1);
    int v = 0;
    if (s) {
	save_tex_scanner(texstate);
	if (scan_keyword(s)) {
	    v = 1;
	}
	unsave_tex_scanner(texstate);
    }
    lua_pushboolean(L,v);
    return 1;
}

static int run_scan_int(lua_State * L)
{
    saved_tex_scanner texstate;
    int v = 0;
    scan_result val;
    save_tex_scanner(texstate);
    scan_int(&val);
    v = val.value.int_val;
    unsave_tex_scanner(texstate);
    lua_pushnumber(L,(lua_Number)v);
    return 1;
}

static int run_scan_dimen(lua_State * L)
{
    saved_tex_scanner texstate;
    int v = 0, o = 0;
    int inf = false, mu = false;
    scan_result val;
    int t = lua_gettop(L);
    if (t>0)
	inf = lua_toboolean(L,1); // inf values allowed ?
    if (t>1)
	mu = lua_toboolean(L,2); // mu units required ? 
    save_tex_scanner(texstate);
    scan_dimen(&val, mu,inf, false); // arg3 = shortcut
    v = val.value.dimen_val;
    o = val.info.order;
    unsave_tex_scanner(texstate);
    lua_pushnumber(L,(lua_Number)v);
    lua_pushnumber(L,(lua_Number)o);
    return 2;
}

static int run_scan_glue(lua_State * L)
{
    saved_tex_scanner texstate;
    int v = 0;
    int mu = false;
    scan_result val;
    int t = lua_gettop(L);
    if (t>0)
	mu = lua_toboolean(L,1); // mu units required ? 
    save_tex_scanner(texstate);
    scan_glue(&val, (mu ? mu_val_level : glue_val_level));
    v = val.value.glu_val; // which is a glue_spec node
    unsave_tex_scanner(texstate);
    lua_nodelib_push_fast(L,(halfword)v); 
    return 1;
}


static int run_scan_toks(lua_State * L)
{
    saved_tex_scanner texstate;
    int macro_def = false, xpand = false;
    halfword tail, t, saved_defref;
    int i = 1;
    int top = lua_gettop(L);
    if (top>0)
	macro_def = lua_toboolean(L,1); // \\def ?
    if (top>1)
	xpand = lua_toboolean(L,2); // expand ?
    save_tex_scanner(texstate);
    saved_defref = def_ref;
    tail = scan_toks(macro_def, xpand);
    t = def_ref;
    unsave_tex_scanner(texstate);
    def_ref = saved_defref;
    /* This function returns a pointer to the tail of a new token
       list, and it also makes |def_ref| point to the reference count at the
       head of that list. */
    lua_newtable(L);
    while (t != tail) {
	t = token_link(t);
	push_token(L,t);
	lua_rawseti(L,-2,i++);
    }
    return 1;
}

static int run_expand(lua_State * L)
{
    (void) L;
    expand();
    return 0;
}


static int run_lookup(lua_State * L)
{
    const char *s;
    size_t l;
    int cs, cmd, chr;
    if (lua_isstring(L, -1)) {
        s = lua_tolstring(L, -1, &l);
        if (l > 0) {
            cs = id_lookup(s, l, true);
            cmd = eq_type(cs);
            chr = equiv(cs);
            make_new_token(L, cmd, chr, cs);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int run_build(lua_State * L)
{
    int cmd, chr, cs;
    if (lua_isnumber(L, 1)) {
        cs = 0;
        chr = (int) lua_tointeger(L, 1);
        cmd = (int) luaL_optinteger(L, 2, get_cat_code(int_par(cat_code_table_code),chr));
        if (cmd == 0 || cmd == 9 || cmd == 14 || cmd == 15) {
            fprintf(stdout,
                    "\n\nluatex error: not a good token.\nCatcode %i can not be returned, so I replaced it by 12 (other)",
                    (int) cmd);
            error();
            cmd = 12;
        }
        if (cmd == 13) {
            cs = active_to_cs(chr, false, true);
            cmd = eq_type(cs);
            chr = equiv(cs);
        }
        make_new_token(L, cmd, chr, cs);
        return 1;
    } else {
        return run_lookup(L);
    }
}

/* token instance functions */

static int lua_tokenlib_free(lua_State * L)
{
    lua_token *n;
    n = check_istoken(L, 1);
    if (n->origin == LUA_ORIGIN) {
	free_avail(n->token);
    }
    return 1;
}


static int lua_tokenlib_getfield(lua_State * L)
{
    lua_token *n;
    const char *s;
    halfword t ;
    n = check_istoken(L, 1);
    s = lua_tostring(L, 2);
    t = token_info(n->token);
    if (lua_key_eq(s, cmd)) {
	if (t >= cs_token_flag) {
	    lua_pushnumber(L, eq_type((t - cs_token_flag)));
        } else {
	    lua_pushnumber(L, token_cmd(t));
	}
    } else if (lua_key_eq(s, id)) {
       lua_pushnumber(L, n->token);
    } else if (lua_key_eq(s, tok)) {
       lua_pushnumber(L, t);
    } else if (lua_key_eq(s, csname)) {
	unsigned char *s;
        if (t >= cs_token_flag && ((s = get_cs_text(t - cs_token_flag)) != (unsigned char *) NULL)) {
            if (is_active_string(s))
                lua_pushstring(L, (char *) (s + 3));
            else
                lua_pushstring(L, (char *) s);
        } else {
            lua_pushstring(L, "");
        }
    } else if (lua_key_eq(s, active)) {
	unsigned char *s;
        if (t >= cs_token_flag && ((s = get_cs_text(t - cs_token_flag)) != (unsigned char *) NULL)) {
            if (is_active_string(s))
                lua_pushboolean(L,1);
            else
                lua_pushboolean(L,0);
            free(s);
        } else {
            lua_pushboolean(L,0);
        }
    } else if (lua_key_eq(s, expandable)) {
        int cmd = (t >= cs_token_flag ? eq_type(t - cs_token_flag) : token_cmd(t));
        if (cmd > max_command_cmd) {
            lua_pushboolean(L, 1);
        } else {
            lua_pushboolean(L, 0);
        }
    } else if (lua_key_eq(s, protected)) {
        int cmd = (t >= cs_token_flag ? eq_type(t - cs_token_flag) : token_cmd(t));
        int chr = (t >= cs_token_flag ? equiv(t - cs_token_flag) : token_chr(t));
        if (cmd > max_command_cmd && ((cmd >= call_cmd) && (cmd < end_template_cmd)) &&
            token_info(token_link(chr)) == protected_token) {
            lua_pushboolean(L, 1);
        } else {
            lua_pushboolean(L, 0);
        }
    } else if (lua_key_eq(s, mod)) {
	if (t >= cs_token_flag) {
	    lua_pushnumber(L, equiv((t - cs_token_flag)));
        } else {
	    lua_pushnumber(L, token_chr(t));
	}
    } else if (lua_key_eq(s, cmdname)) {
        int cmd = (t >= cs_token_flag ? eq_type(t - cs_token_flag) : token_cmd(t));
        lua_pushstring(L, command_names[cmd].cmd_name);
    }
    return 1;
}

static int lua_tokenlib_equal(lua_State * L)
{
    lua_token *n, *m;
    n = check_istoken(L, 1);
    m = check_istoken(L, 2);
    if (token_info(n->token) == token_info(m->token)) {
	lua_pushboolean(L,1);
        return 1;
    }
    lua_pushboolean(L,0);
    return 1;
}

static int lua_tokenlib_tostring(lua_State * L)
{
    char *msg;
    lua_token *n;
    n = check_istoken(L, 1);
    msg = xmalloc(256);
    snprintf(msg, 255, "<%s token %d: %d>", (n->origin==LUA_ORIGIN?"lua":"tex"), n->token , token_info(n->token));
    lua_pushstring(L, msg);
    free(msg);
    return 1;
}


static const struct luaL_Reg tokenlib[] = {
    {"get_next", run_get_next},
/*     {"expand", run_expand}, */ /* does not work yet! */
    {"scan_keyword", run_scan_keyword},
    {"scan_int", run_scan_int},
    {"scan_dimen", run_scan_dimen},
    {"scan_glue", run_scan_glue},
    {"scan_toks", run_scan_toks},
    {"create", run_build},
    {"csname_id", run_get_csname_id},
    {"command_id", run_get_command_id},
    {"cs_offset", run_get_cs_offset},
    {NULL, NULL}                /* sentinel */
};

static const struct luaL_Reg tokenlib_m[] = {
    {"__index", lua_tokenlib_getfield},
    {"__tostring", lua_tokenlib_tostring},
    {"__eq", lua_tokenlib_equal},
    {"__gc", lua_tokenlib_free},
    {NULL, NULL} /* sentinel */
};


int luaopen_newtoken(lua_State * L)
{
    /* the main metatable of token userdata */
    luaL_newmetatable(L, TOKEN_METATABLE);
    luaL_register(L, NULL, tokenlib_m);
    luaL_register(L, "newtoken", tokenlib);
    return 1;
}