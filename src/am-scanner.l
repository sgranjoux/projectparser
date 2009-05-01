/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * am-scanner.l
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 * 
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
%{

#include <stdlib.h>
#include <string.h>
#include "am-scanner.h"
#include "am-parser.h"


/* Eliminate warning */
#define YY_NO_UNPUT 1 

#define YY_INPUT(buf,result,the_max_size) \
    result = 0;

#define YY_USER_ACTION amp_update_location(yylloc, yytext, yyleng);
    
#define YY_EXTRA_TYPE  AmpAmScanner*

//#define YY_USER_INIT {yy_flex_debug = 1;}
 
static AnjutaToken* amp_am_scanner_append_token (AmpAmScanner *scanner, gint token);
static gint amp_am_scanner_last_token (AmpAmScanner *scanner);
static gint amp_am_scanner_last_flags (AmpAmScanner *scanner);
static void amp_update_location (YYLTYPE *loc, const gchar *text, gint length);

%}

%option reentrant stack noyywrap yylineno

%option prefix="amp_am_yy"

/* Necessary because autotools wrapper always looks for a file named "lex.yy.c",
 * not "lex.amp_am_yy.c" 
%option outfile="lex.yy.c"*/

%option bison-bridge bison-locations

%option never-interactive

%option batch

%option debug

NAME          [^ \t\n\r:#=$"'`&@\\]*

%%

<INITIAL>\n {
	yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_EOL);
	return EOL;
}

<INITIAL>([ ]|\\\n)([ \t]|\\\n)* {
	yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE);
	return SPACE;
}

<INITIAL>([ \t])*#.*\n {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_COMMENT | ANJUTA_TOKEN_IRRELEVANT);   
    return EOL;
}

<INITIAL>\t {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE);   
    return TAB;
}

<INITIAL>@{NAME}@ {
	yylval->token = amp_am_scanner_append_token (yyextra, ANJUTA_TOKEN_MACRO);
	return MACRO;
}

<INITIAL>\$\([^ \t\n\r:#=$)]+\) {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_VARIABLE);   
    return VARIABLE;
}

<INITIAL>\$\{[^ \t\n\r:#=$}]+\} {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_VARIABLE);   
    return VARIABLE;
}

<INITIAL>\$[^ \t\n\r\(\{] {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_VARIABLE);   
    return VARIABLE;
}

<INITIAL>: {
    yylval->token = amp_am_scanner_append_token  (yyextra, ':');   
    return COLON;
}

<INITIAL>:: {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return DOUBLE_COLON;
}

<INITIAL>; {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return SEMI_COLON;
}

<INITIAL>\| {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return ORDER;
}

<INITIAL>\= {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return EQUAL;
}

<INITIAL>:= {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return IMMEDIATE_EQUAL;
}

<INITIAL>\?= {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return CONDITIONAL_EQUAL;
}

<INITIAL>\+= {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return APPEND;
}

<INITIAL>\\[ ] {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return CHARACTER;
}

<INITIAL>\\: {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return CHARACTER;
}

<INITIAL>\\= {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return CHARACTER;
}

<INITIAL>\\# {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    return CHARACTER;
}

<INITIAL>SUBDIRS {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN_SUBDIRS);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_DATA {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__DATA);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_HEADERS {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__HEADERS);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_LIBRARIES {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__LIBRARIES);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_LISP {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__LISP);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_LTLIBRARIES {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__LTLIBRARIES);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_MANS {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__MANS);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_PROGRAMS {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__PROGRAMS);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_PYTHON {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__PYTHON);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_JAVA {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__JAVA);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_SCRIPTS {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__SCRIPTS);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_SOURCES {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__SOURCES);   
    return AM_VARIABLE;
}

<INITIAL>{NAME}_TEXINFOS {
    yylval->token = amp_am_scanner_append_token  (yyextra, AM_TOKEN__TEXINFOS);   
    return AM_VARIABLE;
}

<INITIAL>{NAME} {
    yylval->token = amp_am_scanner_append_token  (yyextra, ANJUTA_TOKEN_NAME);   
    return NAME;
}

%%
     
struct _AmpAmScanner
{
    AnjutaToken *last;
    const gchar *pos;
    yyscan_t scanner;
    YY_BUFFER_STATE	 buffer;
    AnjutaToken *first;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static AnjutaToken*
amp_am_scanner_append_token (AmpAmScanner *scanner, gint token)
{
    AnjutaToken *frag;
            
    frag = anjuta_token_new_fragment (token, scanner->pos, yyget_leng (scanner->scanner));
    anjuta_token_insert_after (scanner->last, frag);
    scanner->last = frag;
    scanner->pos += yyget_leng (scanner->scanner);
    
    return frag;
}

static gint
amp_am_scanner_last_token (AmpAmScanner *scanner)
{
    return anjuta_token_get_type (scanner->last);
}

static gint
amp_am_scanner_last_flags (AmpAmScanner *scanner)
{
    return anjuta_token_get_flags (scanner->last);
}
    
static void
amp_update_location (YYLTYPE *loc, const gchar *text, gint length)
{
    loc->first_line = loc->last_line;
    loc->first_column = loc->last_column;
    while (length)
    {
        const gchar *eol;
        
        for (eol = text; (eol != text + length) && (*eol != '\n'); eol++);
        if (eol == text + length)
        {
            loc->last_column += length;
            return;
        }
        else
        {
            loc->last_column = 0;
            loc->last_line ++;
            text = eol + 1;
            length -= (eol - text);
        }
    }
}

/* Public functions
 *---------------------------------------------------------------------------*/

AnjutaToken *
amp_am_scanner_parse (AmpAmScanner *scanner, GFile *file)
{
	AnjutaToken *config = NULL;

    if (amp_am_scanner_read (scanner, file))
    {
        if (amp_am_yyparse (scanner->scanner) == 0)
        {
            config = amp_am_scanner_get_parse_tree (scanner);
        }
    }

    return config;
}

gboolean
amp_am_scanner_read (AmpAmScanner *scanner, GFile *file)
{
    if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);

	scanner->first = anjuta_token_new_file (file, NULL);
	if (scanner->first == NULL) return FALSE;

    scanner->last = scanner->first;
    scanner->pos = anjuta_token_file_get_content (scanner->first);
    scanner->buffer = yy_scan_string (scanner->pos, scanner->scanner);
	
    return TRUE;
}

AnjutaToken*
amp_am_scanner_get_parse_tree (AmpAmScanner *scanner)
{
    return scanner->first;
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AmpAmScanner *
amp_am_scanner_new (void)
{
	AmpAmScanner *scanner;

	scanner = g_new0 (AmpAmScanner, 1);

    yylex_init(&scanner->scanner);
    yyset_extra (scanner, scanner->scanner);

	return scanner;
};

void
amp_am_scanner_free (AmpAmScanner *scanner)
{
	g_return_if_fail (scanner != NULL);

    if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);
    yylex_destroy(scanner->scanner);

	g_free (scanner);
}
