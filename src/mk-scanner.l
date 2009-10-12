/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * mk-scanner.l
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
#include "mk-scanner.h"
#include "mk-parser.h"

#include "libanjuta/anjuta-debug.h"


/* Eliminate warning */
#define YY_NO_UNPUT 1 

#define YY_INPUT(buf, result, max_size) result = mkp_scanner_input (yyextra, buf, max_size)
/*#define YY_INPUT(buf,result,the_max_size) \
    result = 0;*/

#define YY_USER_ACTION mkp_update_location(yylloc, yytext, yyleng);
    
#define YY_EXTRA_TYPE  MkpScanner*

//#define YY_USER_INIT {yy_flex_debug = 1;}
 
static AnjutaToken* mkp_scanner_append_token (MkpScanner *scanner, gint token);
static gint mkp_scanner_last_token (MkpScanner *scanner);
static gint mkp_scanner_last_flags (MkpScanner *scanner);
static void mkp_scanner_update_line_width (MkpScanner *scanner, YYLTYPE *loc);
static void mkp_update_location (YYLTYPE *loc, const gchar *text, gint length);
static int mkp_parse (yyscan_t scanner);
static gint mkp_scanner_input (MkpScanner *scanner, gchar *buffer, gsize max_size);
static gint mkp_scanner_parse_end (MkpScanner *scanner);


void mkp_project_update_variable (MkpProject *project, AnjutaToken *variable);
void mkp_project_add_rule (MkpProject *project, AnjutaToken *rule);
gboolean mkp_ac_yyparse (MkpScanner *scanner);

#define RETURN(tok) *yylval = mkp_scanner_append_token (yyextra, tok); \
                    return tok

%}

%option reentrant stack noyywrap yylineno

%option prefix="mkp_mk_yy"

/* Necessary because autotools wrapper always looks for a file named "lex.yy.c",
 * not "lex.mkp_yy.c" 
%option outfile="lex.yy.c"*/

%option bison-bridge bison-locations

%option never-interactive

%option batch

%option debug

NAME          [^ \t\n\r:#=$"'`&@\\]*

%%

\n                          { RETURN (EOL); }


([ ]|\\\n)([ \t]|\\\n)*     { RETURN (SPACE); }

#                           { RETURN (HASH); }

\t                          { RETURN (TAB); }

\$\([^ \t\n\r:#=$)]+\)      { RETURN (VARIABLE); }

\$\{[^ \t\n\r:#=$}]+\}      { RETURN (VARIABLE); }

\$[^ \t\n\r\(\{]            { RETURN (VARIABLE); }

,                           { RETURN (COMMA); }

:                           { RETURN (COLON); }

::                          { RETURN (DOUBLE_COLON); }

;                           { RETURN (SEMI_COLON); }

\|                          { RETURN (ORDER); }

\=                          { RETURN (EQUAL); }

:=                          { RETURN (IMMEDIATE_EQUAL); }

\?=                         { RETURN (CONDITIONAL_EQUAL); }

\+=                         { RETURN (APPEND); }

\\[ ]                       { RETURN (CHARACTER); }

\\:                         { RETURN (CHARACTER); }

\\=                         { RETURN (CHARACTER); }

\\#                         { RETURN (CHARACTER); }

.PHONY                      { RETURN (_PHONY); }

.SUFFIXES                   { RETURN (_SUFFIXES); }

.DEFAULT                    { RETURN (_DEFAULT); }

.PRECIOUS                   { RETURN (_PRECIOUS); }

.INTERMEDIATE               { RETURN (_INTERMEDIATE); }
    
.SECONDARY                  { RETURN (_SECONDARY); }

.SECONDEXPANSION            { RETURN (_SECONDEXPANSION); }

.DELETE_ON_ERROR            { RETURN (_DELETE_ON_ERROR); }

.IGNORE                     { RETURN (_IGNORE); }

.LOW_RESOLUTION_TIME        { RETURN (_LOW_RESOLUTION_TIME); }

.SILENT                     { RETURN (_SILENT); }

.EXPORT_ALL_VARIABLES       { RETURN (_EXPORT_ALL_VARIABLES); }

.NOTPARALLEL                { RETURN (_NOTPARALLEL); }

{NAME}                      { RETURN (NAME);}

.                           { RETURN (CHARACTER); }

<<EOF>>                     { if (mkp_scanner_parse_end (yyextra) == YY_NULL) return YY_NULL; }


%%

typedef struct _MkpBuffer MkpBuffer;

struct _MkpBuffer
{
    AnjutaToken *token;

    /* Beginning of current token */
    AnjutaToken *start;
    gsize begin;

    /* Position in buffer */
    AnjutaToken *next;
    gsize pos;

    MkpBuffer *parent;    
};
     
struct _MkpScanner
{
    yyscan_t scanner;

    AnjutaTokenFile *file;
    gchar *filename;

    MkpBuffer *buffer;

    MkpProject *project;

	guint line_width;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static AnjutaToken*
mkp_scanner_append_token (MkpScanner *scanner, gint token)
{
    AnjutaToken *frag;

    if (scanner->file != NULL)
    {
        const gchar *start = anjuta_token_file_get_content (scanner->file, NULL);
            
        frag = anjuta_token_new_fragment (token, start + scanner->buffer->begin, yyget_leng (scanner->scanner));
        anjuta_token_file_append (scanner->file, frag);
        scanner->buffer->begin += yyget_leng (scanner->scanner);
    }
    else if (scanner->buffer->token != NULL)
    {
        gsize length = yyget_leng (scanner->scanner);
        AnjutaToken *end;

        for (end = scanner->buffer->start; end != NULL; end = anjuta_token_next_sibling (end))
        {
            if (anjuta_token_get_type (end) < ANJUTA_TOKEN_PARSED)
            {
                gint toklen = anjuta_token_get_length (end);
    
                if (toklen >= length)
                {
                    if (toklen > length)
                    {
                        AnjutaToken *start = end;

                        end = anjuta_token_split (end, length);
                        if (start == scanner->buffer->start)
                        {
                            scanner->buffer->start = end;
                        }
                    }

                    if (end == scanner->buffer->start)
                    {
                        /* Get whole token */
                        frag = end;
                        anjuta_token_set_type (frag, token);
                        scanner->buffer->start = anjuta_token_next_sibling (end);
                    }
                    else
                    {
                        frag = anjuta_token_new_fragment (token, NULL, 0);
                        anjuta_token_insert_before (scanner->buffer->start, frag);
                        scanner->buffer->start = anjuta_token_next_sibling (end);
                        anjuta_token_group (frag, end);
                    }
                    break;
                }
                else
                {
                    length -= toklen;
                }
            }
        }
    }
    
    return frag;
}

static void
mkp_scanner_update_line_width (MkpScanner *scanner, YYLTYPE *loc)
{
	anjuta_token_file_update_line_width (scanner->file, loc->last_column);
}

void
mkp_scanner_update_variable (MkpScanner *scanner, AnjutaToken *variable)
{
    mkp_project_update_variable (scanner->project, variable);
}

void
mkp_scanner_parse_variable (MkpScanner *scanner, AnjutaToken *variable)
{
    AnjutaToken *token;

    token = mkp_project_get_variable_token (scanner->project, variable);

    if (token != NULL)
    {
        token = anjuta_token_next_child (token);
        if (token != NULL)
        {
            anjuta_token_set_type (variable, ANJUTA_TOKEN_VARIABLE);
            token = anjuta_token_copy (token);
            anjuta_token_insert_after (variable, token);

            mkp_scanner_parse_token (scanner, token, NULL);
        }
    }
}

void
mkp_scanner_add_rule (MkpScanner *scanner, AnjutaToken *rule)
{
    mkp_project_add_rule (scanner->project, rule);
}

static void
mkp_update_location (YYLTYPE *loc, const gchar *text, gint length)
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

static gint
mkp_scanner_input (MkpScanner *scanner, gchar *buffer, gsize max_size)
{
    gint result = YY_NULL;

    if (scanner->file != NULL)
    {
        gsize length = anjuta_token_file_get_length (scanner->file, NULL);
        if (scanner->buffer->pos < length)
        {
            const gchar *start = anjuta_token_file_get_content (scanner->file, NULL);

            length -= scanner->buffer->pos;

            if (length > max_size) length = max_size;
            memcpy (buffer, start + scanner->buffer->pos, length);
            scanner->buffer->pos += length;
            result = length;
        }
    }
    else if (scanner->buffer->token != NULL)
    {
        if (scanner->buffer->next)
        {
            gsize length = anjuta_token_get_length (scanner->buffer->next);

            if ((anjuta_token_get_type (scanner->buffer->next) >= ANJUTA_TOKEN_PARSED) || (scanner->buffer->pos >= length))
            {
                for (;;)
                {
                    scanner->buffer->next = anjuta_token_next_sibling (scanner->buffer->next);
                    if (scanner->buffer->next == NULL)
                    {
                        /* Last token */
                        return 0;
                     }
                    else if ((anjuta_token_get_length (scanner->buffer->next) != 0) && (anjuta_token_get_type (scanner->buffer->next) < ANJUTA_TOKEN_PARSED))
                    {
                        /* Find some data */
                        scanner->buffer->pos = 0;
                        length = anjuta_token_get_length (scanner->buffer->next);
                        break;  
                    }
                }
            }

            if (scanner->buffer->pos < length)
            {
                const gchar *start = anjuta_token_get_string (scanner->buffer->next);

                length -= scanner->buffer->pos;
            
                if (length > max_size) length = max_size;
                memcpy (buffer, start + scanner->buffer->pos, length);
                scanner->buffer->pos += length;
                result = length;
            }
        }
    }

    return result;
}

static gint
mkp_scanner_parse_end (MkpScanner *scanner)
{
    MkpBuffer *buffer;

    yypop_buffer_state(scanner->scanner);
    buffer = scanner->buffer;
    scanner->buffer = buffer->parent;
    g_free (buffer);

    if (scanner->buffer == NULL)
    {
        yyterminate();
    }
    else
    {
        return 1;
    }
}

/* Public functions
 *---------------------------------------------------------------------------*/

int
mkp_yylex (YYSTYPE * yylval_param,YYLTYPE * yylloc_param ,MkpScanner *scanner)
{
    return mkp_mk_yylex (yylval_param, yylloc_param, scanner->scanner);
}


gboolean
mkp_scanner_parse (MkpScanner *scanner, AnjutaTokenFile *file, GError **error)
{
    gboolean ok;

    g_return_val_if_fail (file != NULL, FALSE);

    scanner->buffer = g_new (MkpBuffer, 1);

	scanner->file = file;
    scanner->buffer->pos = 0;
    scanner->buffer->begin = 0;
    scanner->buffer->token = NULL;
	
	ok = mkp_yyparse (scanner) == 0;

    g_free (scanner->buffer);
    scanner->buffer = NULL;

    return ok;
}

gboolean
mkp_scanner_parse_token (MkpScanner *scanner, AnjutaToken *token, GError **error)
{
    AnjutaToken *child;
    gboolean ok;
    MkpBuffer *buffer;

    buffer = g_new (MkpBuffer, 1);
    buffer->token = token;
    buffer->pos = 0;
    buffer->begin = 0;
    buffer->parent = NULL;
    scanner->file = NULL;

    /* Add a level for this operation if it is not already done */
    if (anjuta_token_get_length (token) != 0)
    {
        AnjutaToken *copy = anjuta_token_copy (token);
        
        anjuta_token_insert_child (token, copy);
        anjuta_token_clear (token);
    }

    /* Move all know data in a list */
    for (child = anjuta_token_next_child (token); child != NULL;)
    {
        child = anjuta_token_ungroup (child);
        if (anjuta_token_get_length (child) == 0)
        {
            child = anjuta_token_delete (child);
        }
        else
        {
            child = anjuta_token_next_sibling (child);
        }
    }

    buffer->next = anjuta_token_next_child (token);
    buffer->start = buffer->next;

    if (scanner->buffer != NULL)
    {
        /* Parse an included file or a expanded variable */
        buffer->parent = scanner->buffer;
        scanner->buffer = buffer;

        yypush_buffer_state(yy_create_buffer(NULL, YY_BUF_SIZE, scanner->scanner), scanner->scanner);

        return TRUE;    
    }
    else
    {
        scanner->buffer = buffer;

	    return mkp_yyparse (scanner) == 0;
    }
}

guint
mkp_scanner_get_line_width (MkpScanner *scanner)
{
	return scanner->line_width;
}

const gchar*
mkp_scanner_get_filename (MkpScanner *scanner)
{
	g_free (scanner->filename);
    scanner->filename = NULL;
    if (scanner->file) scanner->filename = g_file_get_path (anjuta_token_file_get_file (scanner->file));
	
	return scanner->filename;
}


/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

MkpScanner *
mkp_scanner_new (MkpProject *project)
{
	MkpScanner *scanner;

	scanner = g_new0 (MkpScanner, 1);

    yylex_init(&scanner->scanner);
    yyset_extra (scanner, scanner->scanner);

    scanner->project = project;

	return scanner;
};

void
mkp_scanner_free (MkpScanner *scanner)
{
	g_return_if_fail (scanner != NULL);

    //if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);
    yylex_destroy(scanner->scanner);

	g_free (scanner->filename);
	scanner->filename = NULL;

	g_free (scanner);
}
