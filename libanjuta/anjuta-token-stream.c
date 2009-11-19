/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-token-stream.c
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "anjuta-token-stream.h"

#include "anjuta-debug.h"

#include <glib-object.h>

#include <stdio.h>
#include <string.h>

/*
 * This object manages two list of tokens:
 *  - One is can be read as a string and is used by the lexer
 *  - The other one is written token by token by the lexer
 */ 

/* Types declarations
 *---------------------------------------------------------------------------*/

struct _AnjutaTokenStream
{
    AnjutaToken *token;

    /* Beginning of current token */
    AnjutaToken *start;
    gsize begin;

    AnjutaToken *end;   /* Last token */

    /* Next data read stream */
    AnjutaToken *next;
    gsize pos;

    /* Place to put new token */
    AnjutaToken *first;
    AnjutaToken *last;

    AnjutaTokenStream *parent;    
};

/* Helpers functions
 *---------------------------------------------------------------------------*/

/* Private functions
 *---------------------------------------------------------------------------*/

/* Public functions
 *---------------------------------------------------------------------------*/

void
anjuta_token_stream_append_token (AnjutaTokenStream *stream, AnjutaToken *token)
{
    if (stream->last == NULL)
    {
        stream->last = anjuta_token_insert_child (stream->first, token);
    }
    else
    {
        while (anjuta_token_parent (stream->last) != stream->first)
        {
            stream->last = anjuta_token_parent (stream->last);
        }
        stream->last = anjuta_token_insert_after (stream->last, token);
    }
}

AnjutaToken* 
anjuta_token_stream_tokenize (AnjutaTokenStream *stream, gint type, gsize length)
{
    AnjutaToken *frag;
    AnjutaToken *end;

    frag = anjuta_token_new_fragment (type, NULL, 0);

    for (end = stream->start; end != NULL;)
    {
        if (anjuta_token_get_type (end) < ANJUTA_TOKEN_PARSED)
        {
            gint toklen = anjuta_token_get_length (end);
            AnjutaToken *copy = anjuta_token_cut (end, stream->begin, length);
    
            if (toklen >= (length + stream->begin))
            {

                if (end == stream->start)
                {
                    /* Get whole token */
                    anjuta_token_free (frag);
                    anjuta_token_set_type (copy, type);
                    frag = copy;
                }
                else
                {
                    /* Get several token */
                    anjuta_token_append_child (frag, copy);
                }

                if (toklen == (length + stream->begin))
                {
                    stream->start = anjuta_token_next (end);
                    stream->begin = 0;
                }
                else
                {
                    stream->start = end;
                    stream->begin += length;
                }
                break;
            }
            else
            {
                anjuta_token_append_child (frag, copy);
                length -= toklen;
                end = anjuta_token_next (end);
                stream->begin = 0;
            }
        }
        else
        {
            end = anjuta_token_next (end);
            stream->begin = 0;
        }
    }
    
    anjuta_token_stream_append_token (stream, frag);

    return frag;
}

gint 
anjuta_token_stream_read (AnjutaTokenStream *stream, gchar *buffer, gsize max_size)
{
    gint result = 0;

    if (stream->next != NULL)
    {
        gsize length = anjuta_token_get_length (stream->next);

        if ((anjuta_token_get_type (stream->next) >= ANJUTA_TOKEN_PARSED) || (stream->pos >= length))
        {
            for (;;)
            {
                /* Last token */
                if (stream->next == stream->end) return 0;

                if (anjuta_token_get_type (stream->next) >= ANJUTA_TOKEN_PARSED)
                {
                    stream->next = anjuta_token_next (stream->next);
                }
                else
                {
                    stream->next = anjuta_token_next (stream->next);
                }

                if ((stream->next == NULL) || (anjuta_token_get_type (stream->next) == ANJUTA_TOKEN_EOV))
                {
                    /* Last token */
                    return 0;
                }
                else if ((anjuta_token_get_length (stream->next) != 0) && (anjuta_token_get_type (stream->next) < ANJUTA_TOKEN_PARSED))
                {
                    /* Find some data */
                    stream->pos = 0;
                    length = anjuta_token_get_length (stream->next);
                    break;  
                }
            }
        }

        if (stream->pos < length)
        {
            const gchar *start = anjuta_token_get_string (stream->next);

            length -= stream->pos;
            
            if (length > max_size) length = max_size;
            memcpy (buffer, start + stream->pos, length);
            stream->pos += length;
            result = length;
        }
    }

    return result;
}

AnjutaToken* 
anjuta_token_stream_get_root (AnjutaTokenStream *stream)
{
	g_return_val_if_fail (stream != NULL, NULL);
	
	return stream->first;
}



/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AnjutaTokenStream *
anjuta_token_stream_push (AnjutaTokenStream *stream, AnjutaToken *token)
{
	AnjutaTokenStream *child;

    child = g_new (AnjutaTokenStream, 1);
    child->token = token;
    child->pos = 0;
    child->begin = 0;
    child->parent = stream;

    child->next = anjuta_token_next (token);
    child->start = child->next;
    child->end = anjuta_token_last (token);
    if (child->end == token) child->end = NULL;

	child->last = NULL;
	child->first = (stream != NULL) ? stream->last : anjuta_token_new_static (ANJUTA_TOKEN_FILE, NULL);
	
	return child;
}

AnjutaTokenStream *
anjuta_token_stream_pop (AnjutaTokenStream *stream)
{
	AnjutaTokenStream *parent;

	g_return_val_if_fail (stream != NULL, NULL);
	
	parent = stream->parent;
    g_free (stream);

	return parent;
}
