/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ac-scanner.l
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

#include "ac-scanner.h"
#include "ac-parser.h"

#include "libanjuta/anjuta-debug.h"

/* Eliminate warning */
#define YY_NO_UNPUT 1 

#define YY_INPUT(buf,result,the_max_size) \
    result = 0;

#define YY_USER_ACTION amp_update_location(yylloc, yytext, yyleng);
    
#define YY_EXTRA_TYPE  AmpAcScanner*

//#define YY_USER_INIT {yy_flex_debug = 1;}
 
static AnjutaToken* amp_ac_scanner_append_token (AmpAcScanner *scanner, gint token);
static gint amp_ac_scanner_last_token (AmpAcScanner *scanner);
static gint amp_ac_scanner_last_flags (AmpAcScanner *scanner);
static void amp_update_location (YYLTYPE *loc, const gchar *text, gint length);

%}

%option reentrant stack noyywrap yylineno

%option prefix="amp_ac_yy"

/* Necessary because autotools wrapper always looks for a file named "lex.yy.c",
 * not "lex.amp_ac_yy.c"
%option outfile="lex.yy.c"*/			

%option bison-bridge bison-locations

%option never-interactive

%option batch

%option debug

WS              [ \t]
EOL             \n
WSNL          [ \t\n]
DIGIT           [0-9]
LETTER         [A-Za-z]
COMMENT     #.*\n
IDENTIFIER  [A-Za-z_][A-Za-z0-9_]*
NAME          [A-Za-z_.0-9+][A-Za-z_.0-9+-]*
NUMBER      [0-9][0-9.]*

%x ARGUMENT     
%x STRING
    
%%

<INITIAL>{EOL} {
	amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_EOL);
	return EOL;
}

<INITIAL>{WS}+ {
	amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE);
}

<ARGUMENT>{WSNL}+ {
    //g_message ("space %.*s last %x,%x NEXT %x", yyleng + 4, yytext, amp_ac_scanner_last_token (yyextra), amp_ac_scanner_last_flags (yyextra), ANJUTA_TOKEN_NEXT);
    
/*    if (amp_ac_scanner_last_flags(yyextra) & (ANJUTA_TOKEN_OPEN | ANJUTA_TOKEN_NEXT))
    {
        amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE;
    }
    else*/
    {
		AnjutaToken *token;
        token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE);   
		yylval->token = token;
        return SPACE;
    }
}

<STRING>{WSNL}+ {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE);
	yylval->token = token;
    return SPACE;
}

<INITIAL>{COMMENT} {
    amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_COMMENT);   
    return EOL;
}

<ARGUMENT>{COMMENT} {
    amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_COMMENT);   
}

<INITIAL,ARGUMENT,STRING>\[              {
	AnjutaToken *token;
    yy_push_state (STRING, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE); 
	yylval->token = token;
	return SPACE;
}

<STRING>\]  {
	AnjutaToken *token;
    yy_pop_state (yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_SPACE);   
	yylval->token = token;
	return SPACE;
}

<ARGUMENT>, {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_COMMA | ANJUTA_TOKEN_NEXT);   
	yylval->token = token;
    return COMMA;
}

<ARGUMENT>\) {
	AnjutaToken *token;
    yy_pop_state (yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
	yylval->token = token;
    return RIGHT_PAREN;
}

<INITIAL>dnl.*\n {
    amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_COMMENT);   
}

<INITIAL>PKG_CHECK_MODULES\( {
    AnjutaToken *token;
    DEBUG_PRINT ("PKG_CHECK_MODULES %.*s", yyleng, yytext);
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, AC_TOKEN_PKG_CHECK_MODULES);
    yylval->token = token;
    return PKG_CHECK_MODULES;
}

<INITIAL>AC_OUTPUT\( {
    AnjutaToken *token;
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, AC_TOKEN_OBSOLETE_AC_OUTPUT);
    yylval->token = token;
    return OBSOLETE_AC_OUTPUT;
}

<INITIAL>AC_OUTPUT {
    AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, AC_TOKEN_AC_OUTPUT);
    yylval->token = token;
    return AC_OUTPUT;
}

<INITIAL>AC_INIT\( {
    AnjutaToken *token;
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, AC_TOKEN_AC_INIT);
    yylval->token = token;
    return AC_MACRO_WITH_ARG;
}

<INITIAL>AC_CONFIG_FILES\( {
    AnjutaToken *token;
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, AC_TOKEN_AC_CONFIG_FILES);
    yylval->token = token;
    return AC_CONFIG_FILES;
}

<INITIAL>{IDENTIFIER} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_NAME);   
    yylval->token = token;
    return IDENTIFIER;
}

<INITIAL,ARGUMENT,STRING>{NAME} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_NAME);   
    yylval->token = token;
    return NAME;
}

<INITIAL,ARGUMENT,STRING>\${IDENTIFIER} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_MACRO);   
    yylval->token = token;
    return MACRO;
}

<INITIAL,ARGUMENT,STRING>{NUMBER} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_NUMBER);   
    yylval->token = token;
    return NUMBER;
}

<INITIAL,ARGUMENT,STRING>=|>|<|(>=)|(<=) {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    yylval->token = token;
    return OPERATOR;
}

<INITIAL,ARGUMENT,STRING>. {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, *yytext);   
    yylval->token = token;
    return OPERATOR;
}
     
%%
     
struct _AmpAcScanner
{
    const gchar *pos;
    yyscan_t scanner;
    YY_BUFFER_STATE	 buffer;
    AnjutaTokenFile *file;
	gchar *filename;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static AnjutaToken*
amp_ac_scanner_append_token (AmpAcScanner *scanner, gint token)
{
    AnjutaToken *frag;
            
    frag = anjuta_token_new_fragment (token, scanner->pos, yyget_leng (scanner->scanner));
    anjuta_token_file_append (scanner->file, frag);
    scanner->pos += yyget_leng (scanner->scanner);
    
    return frag;
}

static gint
amp_ac_scanner_last_token (AmpAcScanner *scanner)
{
    return anjuta_token_get_type (anjuta_token_file_last (scanner->file));
}

static gint
amp_ac_scanner_last_flags (AmpAcScanner *scanner)
{
    return anjuta_token_get_flags (anjuta_token_file_last (scanner->file));
}
    
static void
amp_update_location (YYLTYPE *loc, const gchar *text, gint length)
{
	const gchar *ptr;
	const gchar *end = text + length;

    loc->first_line = loc->last_line;
    loc->first_column = loc->last_column + 1;

	for (ptr = text; ptr != end; ptr++)
	{
		if (*ptr == '\n')
		{
			loc->last_column = 0;
			loc->last_line++;
			length -= (ptr + 1 - text);
		}
	}

	loc->last_column += length;
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
amp_ac_scanner_parse (AmpAcScanner *scanner, AnjutaTokenFile *file, GError **error)
{
    if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);

	scanner->file = file;
	if (scanner->file == NULL) return FALSE;

    scanner->pos = anjuta_token_file_get_content (scanner->file, error);
	if (scanner->pos == NULL) return FALSE;

    scanner->buffer = yy_scan_string (scanner->pos, scanner->scanner);
	
	return amp_ac_yyparse (scanner->scanner) == 0;
}

const gchar*
amp_ac_scanner_get_filename (void *yyscanner)
{
	AmpAcScanner *scanner = yyget_extra (yyscanner);
	
	g_free (scanner->filename);
	scanner->filename = g_file_get_path (anjuta_token_file_get_file (scanner->file));
	
	return scanner->filename;
}


/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AmpAcScanner *
amp_ac_scanner_new (void)
{
	AmpAcScanner *scanner;

	scanner = g_new0 (AmpAcScanner, 1);

    yylex_init(&scanner->scanner);

    yyset_extra (scanner, scanner->scanner);

	return scanner;
};

void
amp_ac_scanner_free (AmpAcScanner *scanner)
{
	g_return_if_fail (scanner != NULL);

    if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);
    yylex_destroy(scanner->scanner);

	g_free (scanner->filename);
	scanner->filename = NULL;
	
	g_free (scanner);
}
