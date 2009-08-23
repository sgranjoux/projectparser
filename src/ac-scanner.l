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

#define YY_INPUT(buf, result, max_size) result = amp_ac_scanner_input (yyextra, buf, max_size)

#define YY_USER_ACTION amp_update_location(yylloc, yytext, yyleng);

#define YY_EXTRA_TYPE  AmpAcScanner*

#define YY_DECL static int ac_yylex (YYSTYPE * yylval_param,YYLTYPE * yylloc_param ,yyscan_t yyscanner)

//#define YY_USER_INIT {yy_flex_debug = 1;}
 
static AnjutaToken* amp_ac_scanner_append_token (AmpAcScanner *scanner, gint token);
static gint amp_ac_scanner_last_token (AmpAcScanner *scanner);
static gint amp_ac_scanner_last_flags (AmpAcScanner *scanner);
static void amp_update_location (YYLTYPE *loc, const gchar *text, gint length);
static gint amp_ac_scanner_input (AmpAcScanner *scanner, gchar *buffer, gsize max_size);

#define RETURN(tok) *yylval = amp_ac_scanner_append_token (yyextra, tok); \
                    return tok
                    

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

WS          [ \t\r\v]+
NL          \n
COMMENT     #
OPENQUOTE   \[
CLOSEQUOTE  \]
OPENPARG   \(
CLOSEPARG  \)
COMMA       ,
EQUAL       =
LOWER       <
GREATER     >
NAME        [A-Za-z_][A-Za-z0-9_]*
VARIABLE    $[A-Za-z_0-9]+
OTHER       [^ \t\r\v\n#\[\]\\(),=><$_A-Za-z_]+

%x SPACE_LIST2

%%

{WS}                    { RETURN (SPACE); }

\\\n                    { RETURN (SPACE); }

{NL}                    { RETURN (EOL); }

{COMMENT}               { RETURN (HASH); }

{OPENQUOTE}             { RETURN (LEFT_BRACE); }

{CLOSEQUOTE}            { RETURN (RIGHT_BRACE); }

{OPENPARG}              { RETURN (LEFT_PAREN); }

{CLOSEPARG}             { RETURN (RIGHT_PAREN); }

{COMMA}                 { RETURN (COMMA); }

{EQUAL}                 { RETURN (EQUAL); }

{LOWER}                 { RETURN (LOWER); }

{GREATER}                 { RETURN (GREATER); }

dnl                     { RETURN (DNL); }

PKG_CHECK_MODULES\(     { RETURN (PKG_CHECK_MODULES); }

AC_OUTPUT\(             { RETURN (OBSOLETE_AC_OUTPUT); }
 
AC_OUTPUT               { RETURN (AC_OUTPUT); }

AC_INIT\(               { RETURN (AC_INIT); }
 
AC_CONFIG_FILES\(       { RETURN (AC_CONFIG_FILES); }

{NAME}                  { RETURN (NAME); }

{VARIABLE}              { RETURN (VARIABLE); }

{OTHER}|\$|\\           { RETURN (WORD); }



<SPACE_LIST2>{WS}                    { RETURN (SPACE); }

<SPACE_LIST2>\\\n                    { RETURN (SPACE); }

<SPACE_LIST2>{NL}                    { RETURN (EOL); }

<SPACE_LIST2>=|<|>|<=|>=             { RETURN (OPERATOR); }

<SPACE_LIST2>.+                      { RETURN (WORD); }

%%

struct _AmpAcScanner
{
    yyscan_t scanner;

    AnjutaTokenFile *file;
    gchar *filename;

    AnjutaToken *token;
    gint first;

    AnjutaToken *start;
    gsize pos;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static AnjutaToken*
amp_ac_scanner_append_token (AmpAcScanner *scanner, gint token)
{
    AnjutaToken *frag;

    if (scanner->file != NULL)
    {
        const gchar *start = anjuta_token_file_get_content (scanner->file, NULL);
            
        frag = anjuta_token_new_fragment (token, start + scanner->pos, yyget_leng (scanner->scanner));
        anjuta_token_file_append (scanner->file, frag);
        scanner->pos += yyget_leng (scanner->scanner);
    }
    
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

static int
amp_anjuta_to_bison_token (gint token)
{
    if (token < ANJUTA_TOKEN_FIRST)
    {
        return token;
    }
    else
    {
        switch (token)
        {
        case AC_TOKEN_SPACE_LIST:
            return SPACE_LIST;
        default:
            return JUNK;
        }
    }
}

static gint
amp_ac_scanner_input (AmpAcScanner *scanner, gchar *buffer, gsize max_size)
{
    gint result = YY_NULL;

    if (scanner->file != NULL)
    {
        gsize length = anjuta_token_file_get_length (scanner->file, NULL);
        if (scanner->pos < length)
        {
            const gchar *start = anjuta_token_file_get_content (scanner->file, NULL);

            length -= scanner->pos;
            if (length > max_size) length = max_size;
            memcpy (buffer, start + scanner->pos, length);
            result = length;
        }
    }
    else if (scanner->token != NULL)
    {
                
    }

    return result;
}

/* Public functions
 *---------------------------------------------------------------------------*/

int
amp_ac_yylex (YYSTYPE * yylval_param,YYLTYPE * yylloc_param ,AmpAcScanner *scanner)
{
    gint token = 0;

    if (scanner->first != 0)
    {
        *yylval_param = NULL;
        
        token = scanner->first;
        scanner->first = 0;
    }
    else
    {
        token = ac_yylex (yylval_param, yylloc_param, scanner->scanner);
    }

    return token;
} 


        
    
    if (scanner->file != NULL)
    {
        /* Parse a file, need to use the flex scanner */
        return 
    }
    else if (scanner->token != NULL)
    {
        gint token = -1;
        /* Parse something already split in token */
        if (scanner->first != 0)
        {
            /* First token */
            *yylval_param = NULL;
            scanner->start = anjuta_token_next_child (scanner->token);
            token = scanner->first;
            scanner->first = 0;
        }
        else if (scanner->start != NULL)
        {
            *yylval_param = scanner->start;
            token = anjuta_token_get_type (scanner->start);
            scanner->start = anjuta_token_next_sibling (scanner->start);
        }

        return amp_anjuta_to_bison_token (token);
    }
}

gboolean
amp_ac_scanner_parse (AmpAcScanner *scanner, AnjutaTokenFile *file, GError **error)
{
    //if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);

	scanner->file = file;
	if (scanner->file == NULL) return FALSE;

    scanner->pos = 0;
	//if (scanner->pos == NULL) return FALSE;

    //scanner->buffer = yy_scan_string (scanner->pos, scanner->scanner);
	
	return amp_ac_yyparse (scanner) == 0;
}

gboolean
amp_ac_scanner_parse_token (AmpAcScanner *scanner, AnjutaToken *token, gint start, GError **error)
{
    //if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);

    scanner->file = NULL;

    scanner->token = token;
    scanner->start = NULL;
    scanner->first = amp_anjuta_to_bison_token (start);
    g_message ("start %d scanner->start %d SPACE_LIST %d AC_TOKEN_SPACE_LIST %d ANJUTA_TOKEN_FIRST %d", start, scanner->start, SPACE_LIST, AC_TOKEN_SPACE_LIST, ANJUTA_TOKEN_FIRST);

	return amp_ac_yyparse (scanner) == 0;
}

const gchar*
amp_ac_scanner_get_filename (AmpAcScanner *scanner)
{
	g_free (scanner->filename);
    scanner->filename = NULL;
    if (scanner->file) scanner->filename = g_file_get_path (anjuta_token_file_get_file (scanner->file));
	
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

    //if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);
    yylex_destroy(scanner->scanner);

	g_free (scanner->filename);
	scanner->filename = NULL;
	
	g_free (scanner);
}
