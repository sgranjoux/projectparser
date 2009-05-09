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
	amp_ac_scanner_append_token  (yyextra, PM_TOKEN_EOL);
	return PM_TOKEN_EOL;
}

<INITIAL>{WS}+ {
	amp_ac_scanner_append_token  (yyextra, PM_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT);
}

<ARGUMENT>{WSNL}+ {
    //g_message ("space %.*s last %x,%x NEXT %x", yyleng + 4, yytext, amp_ac_scanner_last_token (yyextra), amp_ac_scanner_last_flags (yyextra), ANJUTA_TOKEN_NEXT);
    
    if (amp_ac_scanner_last_flags(yyextra) & (ANJUTA_TOKEN_OPEN | ANJUTA_TOKEN_NEXT))
    {
        amp_ac_scanner_append_token  (yyextra, PM_TOKEN_SPACE | ANJUTA_TOKEN_IRRELEVANT);
    }
    else
    {
		AnjutaToken *token;
        token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_SPACE);   
		yylval->range.first = token;
		yylval->range.last = token;
        return PM_TOKEN_SPACE;
    }
}

<STRING>{WSNL}+ {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_SPACE);
	yylval->range.first = token;
	yylval->range.last = token;
    return PM_TOKEN_SPACE;
}

<INITIAL>{COMMENT} {
    amp_ac_scanner_append_token  (yyextra, PM_TOKEN_COMMENT | ANJUTA_TOKEN_IRRELEVANT);   
    return PM_TOKEN_EOL;
}

<ARGUMENT>{COMMENT} {
    amp_ac_scanner_append_token  (yyextra, PM_TOKEN_COMMENT | ANJUTA_TOKEN_IRRELEVANT);   
}

<INITIAL,ARGUMENT>\[              {
    yy_push_state (STRING, yyscanner);
    amp_ac_scanner_append_token  (yyextra, PM_TOKEN_LEFT_BRACE | ANJUTA_TOKEN_OPEN | ANJUTA_TOKEN_IRRELEVANT);   
}

<STRING>\]  {
    yy_pop_state (yyscanner);
    amp_ac_scanner_append_token  (yyextra, PM_TOKEN_RIGHT_BRACE | ANJUTA_TOKEN_CLOSE | ANJUTA_TOKEN_IRRELEVANT);   
}

<ARGUMENT>, {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_COMMA | ANJUTA_TOKEN_NEXT);   
	yylval->range.first = token;
	yylval->range.last = token;
    return PM_TOKEN_COMMA;
}

<ARGUMENT>\) {
	AnjutaToken *token;
    yy_pop_state (yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_RIGHT_PAREN | ANJUTA_TOKEN_CLOSE);   
	yylval->range.first = token;
	yylval->range.last = token;
    return PM_TOKEN_RIGHT_PAREN;
}

<INITIAL>dnl.*\n {
    amp_ac_scanner_append_token  (yyextra, PM_TOKEN_COMMENT | ANJUTA_TOKEN_IRRELEVANT);   
}

<INITIAL>PKG_CHECK_MODULES\( {
    AnjutaToken *token;
    g_message ("PKG_CHECK_MODULES %.*s", yyleng, yytext);
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_OPEN);
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_PKG_CHECK_MODULES;
}

<INITIAL>AC_OUTPUT\( {
    AnjutaToken *token;
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_OPEN);
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_OBSOLETE_AC_OUTPUT;
}

<INITIAL>AC_OUTPUT {
    AnjutaToken *token;
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_KEYWORD);
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_AC_OUTPUT;
}

<INITIAL>AC_CONFIG_FILES {
    AnjutaToken *token;
    yy_push_state (ARGUMENT, yyscanner);
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_KEYWORD | ANJUTA_TOKEN_OPEN);
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_AC_CONFIG_FILES;
}

<INITIAL>{IDENTIFIER} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_IDENTIFIER);   
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_IDENTIFIER;
}

<INITIAL,ARGUMENT,STRING>{NAME} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_NAME);   
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_NAME;
}

<INITIAL,ARGUMENT,STRING>\${IDENTIFIER} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_MACRO);   
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_MACRO;
}

<INITIAL,ARGUMENT,STRING>{NUMBER} {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, PM_TOKEN_NUMBER);   
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_NUMBER;
}

<INITIAL,ARGUMENT,STRING>=|>|<|(>=)|(<=) {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, ANJUTA_TOKEN_OPERATOR);   
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_OPERATOR;
}

<INITIAL,ARGUMENT,STRING>. {
	AnjutaToken *token;
    token = amp_ac_scanner_append_token  (yyextra, *yytext);   
    yylval->range.first = token;
    yylval->range.last = token;
    return PM_TOKEN_OPERATOR;
}
     
%%
     
struct _AmpAcScanner
{
    const gchar *pos;
    yyscan_t scanner;
    YY_BUFFER_STATE	 buffer;
    AnjutaTokenFile *file;
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

gboolean
amp_ac_scanner_parse (AmpAcScanner *scanner, AnjutaTokenFile *file)
{
    if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);

	scanner->file = file;
	if (scanner->file == NULL) return FALSE;

    scanner->pos = anjuta_token_file_get_content (scanner->file, NULL);
    scanner->buffer = yy_scan_string (scanner->pos, scanner->scanner);
	
	return amp_ac_yyparse (scanner->scanner) == 0;
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

	g_free (scanner);
}
