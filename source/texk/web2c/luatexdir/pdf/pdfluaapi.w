% pdfluaapi.w
% 
% Copyright 2010 Taco Hoekwater <taco@@luatex.org>

% This file is part of LuaTeX.

% LuaTeX is free software; you can redistribute it and/or modify it under
% the terms of the GNU General Public License as published by the Free
% Software Foundation; either version 2 of the License, or (at your
% option) any later version.

% LuaTeX is distributed in the hope that it will be useful, but WITHOUT
% ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
% FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
% License for more details.

% You should have received a copy of the GNU General Public License along
% with LuaTeX; if not, see <http://www.gnu.org/licenses/>.

@ @c
static const char _svn_version[] =
    "$Id$"
    "$URL$";

#include "ptexlib.h"

@ @c
int new_pdflua(void)
{
    int i, err;
    Byte *uncompr;
    zlib_struct *zp = (zlib_struct *) pdflua_zlib_struct_ptr;
    uLong uncomprLen = zp->uncomprLen;
    if ((uncompr = xtalloc(zp->uncomprLen, Byte)) == NULL)
        pdftex_fail("new_pdflua(): xtalloc()");
    err = uncompress(uncompr, &uncomprLen, zp->compr, zp->comprLen);
    if (err != Z_OK)
        pdftex_fail("new_pdflua(): uncompress()");
    assert(uncomprLen == zp->uncomprLen);
    if (luaL_loadbuffer(Luas, (const char *) uncompr, uncomprLen, "pdflua")
        || lua_pcall(Luas, 0, 1, 0))
        pdftex_fail("new_pdflua(): lua_pcall()");
    luaL_checktype(Luas, -1, LUA_TTABLE);       /* t */
    i = luaL_ref(Luas, LUA_GLOBALSINDEX);       /* - */
    xfree(uncompr);
    return i;
}

@ @c
void pdflua_begin_page(PDF pdf)
{
    int err;
    lua_rawgeti(Luas, LUA_GLOBALSINDEX, pdf->pdflua_ref);       /* t */
    lua_pushstring(Luas, "beginpage");  /* s t */
    lua_gettable(Luas, -2);     /* f */
    lua_newtable(Luas);         /* t f */
    lua_pushnumber(Luas, total_pages + 1);      /* i t f */
    lua_setfield(Luas, -2, "pagenum");  /* t f */
    lua_pushnumber(Luas, pdf->last_page);       /*  i t f */
    lua_setfield(Luas, -2, "page_objnum");      /* t f */
    lua_pushnumber(Luas, pdf->last_stream);     /* i t f */
    lua_setfield(Luas, -2, "stream_objnum");    /* t f */
    lua_pushnumber(Luas, pdf->page_resources->last_resources);  /* i t f */
    lua_setfield(Luas, -2, "resources_objnum"); /* t f */
    err = lua_pcall(Luas, 1, 0, 0);     /* - */
    if (err != 0)
        pdftex_fail("pdflua.lua: beginpage()");
}

@ @c
void pdflua_end_page(PDF pdf, int annots, int beads)
{
    int err;
    lua_rawgeti(Luas, LUA_GLOBALSINDEX, pdf->pdflua_ref);       /* t */
    lua_pushstring(Luas, "endpage");    /* s t */
    lua_gettable(Luas, -2);     /* f */
    lua_newtable(Luas);         /* t f */
    lua_pushnumber(Luas, total_pages);  /* i t f */
    lua_setfield(Luas, -2, "pagenum");  /* t f */
    lua_pushnumber(Luas, cur_page_size.h);      /* i t f */
    lua_setfield(Luas, -2, "hsize");    /* t f */
    lua_pushnumber(Luas, cur_page_size.v);      /* i t f */
    lua_setfield(Luas, -2, "vsize");    /* t f */
    lua_pushnumber(Luas, annots);       /* i t f */
    lua_setfield(Luas, -2, "annots");   /* t f */
    lua_pushnumber(Luas, beads);        /* i t f */
    lua_setfield(Luas, -2, "beads");    /* t f */
    lua_pushnumber(Luas, pdf->img_page_group_val);      /* i t f */
    lua_setfield(Luas, -2, "imggroup"); /* t f */
    err = lua_pcall(Luas, 1, 0, 0);     /* - */
    if (err != 0)
        pdftex_fail("pdflua.lua: endpage()");
}

@ @c
void pdflua_output_pages_tree(PDF pdf)
{
    int err;
    lua_rawgeti(Luas, LUA_GLOBALSINDEX, pdf->pdflua_ref);       /* t */
    lua_pushstring(Luas, "outputpagestree");    /* s t */
    lua_gettable(Luas, -2);     /* f */
    err = lua_pcall(Luas, 0, 0, 0);     /* - */
    if (err != 0)
        pdftex_fail("pdflua.lua: outputpagestree()");
}