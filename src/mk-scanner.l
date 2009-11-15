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

#include "mk-scanner.h"
#include "mk-parser.h"
#include "mk-rule.h"

#include "libanjuta/anjuta-debug.h"

#include <stdlib.h>
#include <string.h>

#define YY_INPUT(buf, result, max_size) result = mkp_scanner_input (yyextra, buf, max_size)

#define YY_EXTRA_TYPE  MkpScanner*

//#define YY_USER_INIT {yy_flex_debug = 1;}

static AnjutaToken* mkp_scanner_append_lex_token (MkpScanner *scanner, gint token);
static gint mkp_scanner_input (MkpScanner *scanner, gchar *buffer, gsize max_size);
static gint mkp_scanner_parse_end (MkpScanner *scanner);

#define RETURN(tok) yylval->token = mkp_scanner_append_lex_token (yyextra, tok); \
                    return tok

%}

%option reentrant noyywrap yylineno

/* Remove some warnings */
%option nounput noinput

%option prefix="mkp_mk_yy"

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

    AnjutaToken *end;   /* Last token */

    /* Next data read buffer */
    AnjutaToken *next;
    gsize pos;

    /* Place to put new token */
    AnjutaToken *first;
    AnjutaToken *last;

    MkpBuffer *parent;    
};
     
struct _MkpScanner
{
    yyscan_t scanner;

    MkpBuffer *buffer;

    MkpProject *project;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static void
mkp_scanner_append_token (MkpScanner *scanner, AnjutaToken *token)
{
    if (scanner->buffer->last == NULL)
    {
        scanner->buffer->last = anjuta_token_insert_child (scanner->buffer->first, token);
    }
    else
    {
        while (anjuta_token_parent (scanner->buffer->last) != scanner->buffer->first)
        {
            scanner->buffer->last = anjuta_token_parent (scanner->buffer->last);
        }
        scanner->buffer->last = anjuta_token_insert_after (scanner->buffer->last, token);
    }
}
static AnjutaToken*
mkp_scanner_append_lex_token (MkpScanner *scanner, gint token)
{
    AnjutaToken *frag;
    gsize length = yyget_leng (scanner->scanner);
    AnjutaToken *end;

    frag = anjuta_token_new_fragment (token, NULL, 0);

    for (end = scanner->buffer->start; end != NULL;)
    {
        if (anjuta_token_get_type (end) < ANJUTA_TOKEN_PARSED)
        {
            gint toklen = anjuta_token_get_length (end);
            AnjutaToken *copy = anjuta_token_cut (end, scanner->buffer->begin, length);
    
            if (toklen >= (length + scanner->buffer->begin))
            {

                if (end == scanner->buffer->start)
                {
                    /* Get whole token */
                    anjuta_token_free (frag);
                    anjuta_token_set_type (copy, token);
                    frag = copy;
                }
                else
                {
                    /* Get several token */
                    anjuta_token_append_child (frag, copy);
                }

                if (toklen == (length + scanner->buffer->begin))
                {
                    scanner->buffer->start = anjuta_token_next (end);
                    scanner->buffer->begin = 0;
                }
                else
                {
                    scanner->buffer->start = end;
                    scanner->buffer->begin += length;
                }
                break;
            }
            else
            {
                anjuta_token_append_child (frag, copy);
                length -= toklen;
                end = anjuta_token_next (end);
                scanner->buffer->begin = 0;
            }
        }
        else
        {
            end = anjuta_token_next (end);
            scanner->buffer->begin = 0;
        }
    }
    
    mkp_scanner_append_token (scanner, frag);

    return frag;
}

static gint
mkp_scanner_input (MkpScanner *scanner, gchar *buffer, gsize max_size)
{
    gint result = YY_NULL;

    if (scanner->buffer->next != NULL)
    {
        gsize length = anjuta_token_get_length (scanner->buffer->next);

        if ((anjuta_token_get_type (scanner->buffer->next) >= ANJUTA_TOKEN_PARSED) || (scanner->buffer->pos >= length))
        {
            for (;;)
            {
                /* Last token */
                if (scanner->buffer->next == scanner->buffer->end) return 0;

                if (anjuta_token_get_type (scanner->buffer->next) >= ANJUTA_TOKEN_PARSED)
                {
                    scanner->buffer->next = anjuta_token_next (scanner->buffer->next);
                }
                else
                {
                    scanner->buffer->next = anjuta_token_next (scanner->buffer->next);
                }

                if ((scanner->buffer->next == NULL) || (anjuta_token_get_type (scanner->buffer->next) == ANJUTA_TOKEN_EOV))
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

/* Parser functions
 *---------------------------------------------------------------------------*/

void
mkp_yyerror (YYLTYPE *loc, MkpScanner *scanner, char const *s)
{
    AnjutaTokenFileLocation location;

    //g_message ("error at %d  error at * %d string \"%s\"", anjuta_token_get_type((AnjutaToken *)loc), anjuta_token_get_type(*loc), anjuta_token_get_string ((AnjutaToken *loc));
    if (mkp_project_get_token_location (scanner->project, &location, *loc))
    {
        g_message ("%s:%d.%d %s\n", location.filename, location.line, location.column, s);
        g_free (location.filename);
    }
    else
    {
        g_message ("%s \n", s);
    }
}

void
mkp_scanner_update_variable (MkpScanner *scanner, AnjutaToken *variable)
{
    mkp_project_update_variable (scanner->project, variable);
}

void
mkp_scanner_add_rule (MkpScanner *scanner, AnjutaToken *rule)
{
    mkp_project_add_rule (scanner->project, rule);
}

void
mkp_scanner_parse_variable (MkpScanner *scanner, AnjutaToken *variable)
{
    AnjutaToken *group;
    AnjutaToken *content;

    anjuta_token_set_type (variable, ANJUTA_TOKEN_VARIABLE);
    content = anjuta_token_new_static (ANJUTA_TOKEN_CONTENT, NULL);
    mkp_scanner_append_token (scanner, content);

    group = mkp_project_get_variable_token (scanner->project, variable);
    fprintf(stdout, "get variable %s is %p\n", anjuta_token_evaluate (variable), group);
    if (group != NULL)
    {
        //AnjutaToken *token;

        //anjuta_token_dump (group);
        //token = anjuta_token_group_into_token (group);
        //anjuta_token_set_type (token, ANJUTA_TOKEN_CONTENT);
        //anjuta_token_dump (token);
        fprintf (stdout,"variable %s\n", anjuta_token_get_string (variable));
        anjuta_token_dump (group);
        mkp_scanner_parse_token (scanner, group, NULL);
        //anjuta_token_free (token);
    }
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
mkp_scanner_parse_token (MkpScanner *scanner, AnjutaToken *token, GError **error)
{
    MkpBuffer *buffer;

    buffer = g_new (MkpBuffer, 1);
    buffer->token = token;
    buffer->pos = 0;
    buffer->begin = 0;
    buffer->parent = NULL;

    buffer->next = anjuta_token_next (token);
    buffer->start = buffer->next;
    buffer->end = anjuta_token_last (token);
    if (buffer->end == token) buffer->end = NULL;

    if (scanner->buffer != NULL)
    {
        /* Parse an included file or a expanded variable */
        buffer->parent = scanner->buffer;
        scanner->buffer = buffer;

        //anjuta_token_dump (token);

        buffer->first = buffer->parent->last;
        buffer->last = NULL;

        yypush_buffer_state(yy_create_buffer(NULL, YY_BUF_SIZE, scanner->scanner), scanner->scanner);

        return TRUE;    
    }
    else
    {
        mkp_yypstate *ps;
        gint status;
        YYSTYPE yylval_param;
        YYLTYPE yylloc_param;
        AnjutaToken* yytok;

        buffer->last = NULL;
        buffer->first = anjuta_token_new_static (ANJUTA_TOKEN_FILE, NULL);

        yytok = buffer->first;

        scanner->buffer = buffer;

        ps = mkp_yypstate_new ();
        do
        {
            gint yychar = mkp_mk_yylex (&yylval_param, &yylloc_param, scanner->scanner);
        
            yylloc_param = yylval_param.token;
            status = mkp_yypush_parse (ps, yychar, &yylval_param, &yylloc_param, scanner);
        } while (status == YYPUSH_MORE);
        mkp_yypstate_delete (ps);

        return TRUE;
    }
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

    yylex_destroy(scanner->scanner);

	g_free (scanner);
}
