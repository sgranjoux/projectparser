/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * am-parser.y
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

#include "am-scanner.h"
#include "am-parser.h"

#include <stdlib.h>

#define YYDEBUG 1

/* Token location is found directly from token value, there is no need to
 * maintain a separate location variable */
#define YYLLOC_DEFAULT(Current, Rhs, N)	((Current) = YYRHSLOC(Rhs, (N) ? 1 : 0))
%}


%union {
	AnjutaToken *token;
	AnjutaTokenGroup* group;
}

%token	<token> EOL	'\n'
%token	<token> SPACE
%token	<token> TAB '\t'
%token	<token> MACRO
%token	<token> VARIABLE
%token	<token> COLON ':'
%token	<token> DOUBLE_COLON "::"
%token	<token> ORDER '|'
%token	<token> SEMI_COLON ';'
%token	<token> EQUAL '='
%token	<token> IMMEDIATE_EQUAL ":="
%token	<token> CONDITIONAL_EQUAL "?="
%token	<token> APPEND "+="
%token	<token> CHARACTER
%token	<token> NAME
%token	<token> AM_VARIABLE

%token  <token> SUBDIRS
%token  <token> DIST_SUBDIRS
%token  <token> _DATA
%token  <token> _HEADERS
%token  <token> _LIBRARIES
%token  <token> _LISP
%token  <token> _LTLIBRARIES
%token  <token> _MANS
%token  <token> _PROGRAMS
%token  <token> _PYTHON
%token  <token> _JAVA
%token  <token> _SCRIPTS
%token  <token> _SOURCES
%token  <token> _TEXINFOS

%type   <token> space_token automake_token value_token target_token equal_token rule_token head_token
%type   <token> space optional_space

%type   <group> space_list_value value_list strip_value_list value


%defines

%define api.pure
%define api.push_pull "push"

%parse-param {AmpAmScanner* scanner}
%lex-param   {AmpAmScanner* scanner}

%name-prefix="amp_am_yy"

%locations

%start file

%debug

%{

static gint
amp_am_automake_variable (AnjutaToken *token)
{
    switch (anjuta_token_get_type (token))
    {
    case SUBDIRS: return AM_TOKEN_SUBDIRS;
    case DIST_SUBDIRS: return AM_TOKEN_DIST_SUBDIRS;
    case _DATA: return AM_TOKEN__DATA;
    case _HEADERS: return AM_TOKEN__HEADERS;
    case _LIBRARIES: return AM_TOKEN__LIBRARIES;
    case _LISP: return AM_TOKEN__LISP;
    case _LTLIBRARIES: return AM_TOKEN__LTLIBRARIES;
    case _MANS: return AM_TOKEN__MANS;
    case _PROGRAMS: return AM_TOKEN__PROGRAMS;
    case _PYTHON: return AM_TOKEN__PYTHON;
    case _JAVA: return AM_TOKEN__JAVA;
    case _SCRIPTS: return AM_TOKEN__SCRIPTS;
    case _SOURCES: return AM_TOKEN__SOURCES;
    case _TEXINFOS: return AM_TOKEN__TEXINFOS;
    default: return ANJUTA_TOKEN_NAME;
    }
}

%}


%%

file:
	optional_space statement
	| file EOL optional_space statement
	;
        
statement:
	/* empty */
	| line
	| am_variable
	;

line:
	name_token
	| line token
	;
		
am_variable:
	automake_token space_list_value {
		amp_am_scanner_set_am_variable (scanner, amp_am_automake_variable ($1), $1, $2);
	}
	| automake_token optional_space equal_token optional_space
	;
				
space_list_value: optional_space  equal_token   value_list  {
		$$ = $3;
	}
	;

value_list:
	optional_space strip_value_list optional_space {
		$$ = $2;
	}
		
strip_value_list:
	value {
		$$ = anjuta_token_group_new (ANJUTA_TOKEN_LIST, NULL);
		anjuta_token_group_append ($$, $1);
	}
	| strip_value_list  space  value {
		anjuta_token_group_append ($$, $3);
	}
	;

optional_space:
	/* empty */ {
		$$ = NULL;
	}
	| space
	;

value:
	value_token {
		$$ = anjuta_token_group_new (ANJUTA_TOKEN_ARGUMENT, NULL);
		anjuta_token_group_append_token ($$, $1);
	}
	| value value_token {
		anjuta_token_group_append_token ($1, $2);
	}
	;

space:
	space_token {anjuta_token_set_type ($1, ANJUTA_TOKEN_SPACE);}
	| space space_token	{
		anjuta_token_merge ($1, $2);
	}
	;

		
token:
	space_token
	| value_token
	;            
	
value_token:
	equal_token
	| rule_token
	| target_token
	;

target_token:
	head_token
	| automake_token
	;

space_token:
	SPACE
	| TAB
	;

equal_token:
	EQUAL
	| IMMEDIATE_EQUAL
	| CONDITIONAL_EQUAL
	| APPEND
	;

rule_token:
	COLON
	| DOUBLE_COLON
	;
		
head_token:
	MACRO
	| VARIABLE
	| NAME
	| CHARACTER
	| ORDER
	| SEMI_COLON
	;

name_token:
	MACRO
	| VARIABLE
	| NAME
	| CHARACTER
	;
		
automake_token:
    SUBDIRS
    | DIST_SUBDIRS
    | _DATA
    | _HEADERS
    | _LIBRARIES
    | _LISP
    | _LTLIBRARIES
    | _MANS
    | _PROGRAMS
    | _PYTHON
    | _JAVA
    | _SCRIPTS
    | _SOURCES
    | _TEXINFOS
    ;
    
		
%%
