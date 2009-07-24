/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ac-parser.y
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
#include "ac-scanner.h"

#define YYDEBUG 1

#include "libanjuta/anjuta-debug.h"

//static void amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s);

//amp_ac_yydebug = 1;

%}

%union {
	AnjutaToken *token;
	AnjutaTokenRange range;
}

%token  EOL '\n'

%token  <token> SPACE ' '

%token  HASH               '#'
%token  LEFT_PAREN     '('
%token  <token> RIGHT_PAREN    ')'
%token  LEFT_CURLY		'{'
%token  RIGHT_CURLY    '}'
%token  LEFT_BRACE     '['
%token  RIGHT_BRACE    ']'
%token  EQUAL             '='
%token  <token> COMMA             ','
%token  LOWER              '<'
%token  GREATER           '>'
%token  LOWER_OR_EQUAL
%token  GREATER_OR_EQUAL	
    
%token  COMMENT
%token  <token> NAME
%token  KEYWORD
%token  TOPNAME
%token  <token> IDENTIFIER
%token  <token> NUMBER
%token  <token> MACRO
%token  <token> OPERATOR
%token	<token> AC_MACRO_WITH_ARG
%token	<token> AC_MACRO_WITHOUT_ARG

%token	<token> PKG_CHECK_MODULES
%token	<token> OBSOLETE_AC_OUTPUT
%token	<token> AC_OUTPUT
%token	<token> AC_CONFIG_FILES
%token	<token> AC_SUBST

%type <token> pkg_check_modules obsolete_ac_output ac_config_files
%type <token> ac_macro_with_arg ac_macro_without_arg
%type <token> name strip_name
%type <token> optional_space_list space_list strip_space_list
%type <token> list_empty_optional list_arg_optional list_optional_optional
%type <token> optional_list optional_arg_list
%type <token> spaces
%type <token> optional_space
%type <token> separator
%type <token> arg_string arg

%defines

%pure_parser

%parse-param {void* scanner}
%lex-param   {void* scanner}

%name-prefix="amp_ac_yy"

%locations

%start file

%debug


%{
static void amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s);

%}


%%

file:
	statement
	| file EOL statement
	;
   
statement:
	line_or_empty
	| ac_macro_with_arg
	| ac_macro_without_arg
	| pkg_check_modules 
	| obsolete_ac_output
	| ac_output
	| ac_config_files
	;

line_or_empty:
	/* empty */
	| line
	;

line:
	token
	| line token
	;

pkg_check_modules:
	PKG_CHECK_MODULES  name  COMMA  space_list   list_empty_optional  {
		anjuta_token_set_flags ($1, ANJUTA_TOKEN_SIGNIFICANT);
		anjuta_token_merge ($1, $5);
	}
	| PKG_CHECK_MODULES  name  COMMA  space_list  list_arg_optional
	;

ac_macro_without_arg:
	AC_MACRO_WITHOUT_ARG
	;

ac_macro_with_arg:
	AC_MACRO_WITH_ARG optional_list {
		anjuta_token_merge ($1, $2);
	}
	;

obsolete_ac_output:
	OBSOLETE_AC_OUTPUT  optional_space_list  list_optional_optional {
		anjuta_token_set_flags ($1, ANJUTA_TOKEN_SIGNIFICANT); 
		anjuta_token_merge ($1, $3);
	}
	;

ac_output:
	AC_OUTPUT
	;
	
ac_config_files:
	AC_CONFIG_FILES  space_list  list_optional_optional {
		anjuta_token_set_flags ($1, ANJUTA_TOKEN_SIGNIFICANT); 
		anjuta_token_merge ($1, $3);
	}
	;
	
list_empty_optional:
	RIGHT_PAREN
	| separator RIGHT_PAREN {
		$$ = $2;
	}
	| separator separator arg_string_or_empty  RIGHT_PAREN {
		$$ = $4;
	}
	;

list_arg_optional:
	separator arg_string  RIGHT_PAREN {
		$$ = $3;
	}
	| separator  arg_string  separator  arg_string_or_empty  RIGHT_PAREN {
		$$ = $5;
	}
	;

list_optional_optional:
	RIGHT_PAREN
	| separator  arg_string_or_empty  RIGHT_PAREN {
		$$ =$3;
	}
	| separator  arg_string_or_empty  separator  arg_string_or_empty RIGHT_PAREN {
		$$ = $5;
	}
	;

optional_list:
	RIGHT_PAREN {
		anjuta_token_set_type ($$, ANJUTA_TOKEN_SEPARATOR);
	}
	| spaces RIGHT_PAREN {
		anjuta_token_merge ($1, $2);
		anjuta_token_set_type ($$, ANJUTA_TOKEN_SEPARATOR);
	}
	| optional_arg_list RIGHT_PAREN {
		$$ = $2;
		anjuta_token_set_type ($$, ANJUTA_TOKEN_SEPARATOR);
	}
	;
	
optional_arg_list:
	arg_string {
		anjuta_token_insert_before ($1,
			anjuta_token_new_static (ANJUTA_TOKEN_SPACE, NULL));
	}
	| spaces arg_string
	| optional_arg_list separator arg_string_or_empty
	;

arg_string_or_empty:
	/* empty */
	| arg_string
	;

arg_string:
	arg
	| arg_string arg {
		anjuta_token_merge ($1, $2);
	}
	| arg_string SPACE {
		anjuta_token_merge ($1, $2);
	}
	;

name:
	optional_space strip_name optional_space {
		if ($1)
		{
			anjuta_token_merge ($1, $3 != NULL ? $3 : $2);
		}
		else if ($3)
		{
			$$ = anjuta_token_merge ($2, $3);
		}
		else
		{
			$$ = $2;
		}
	}
	;

strip_name: 
	NAME
	| MACRO
	| OPERATOR
	| strip_name MACRO {
		$$ = anjuta_token_merge ($1, $2);
	}
	| strip_name OPERATOR {
		$$ = anjuta_token_merge ($1, $2);
	}
	| strip_name NAME {
		$$ = anjuta_token_merge ($1, $2);
	}
	;

optional_space_list:
	optional_space
	| space_list
	;

space_list:
	optional_space strip_space_list optional_space {
		if ($1) anjuta_token_merge_previous ($2, $1);
		if ($3) anjuta_token_merge ($2, $3);
		$$ = $2;
	}
	;

strip_space_list:
	strip_name {
		$$ = anjuta_token_merge (
			anjuta_token_insert_before ($1,
					anjuta_token_new_static (ANJUTA_TOKEN_LIST, NULL)),
			$1);
	}
	| strip_space_list spaces strip_name {
		anjuta_token_merge ($1, $3);
	}
	;

optional_space:
	/* empty */ {
		$$ = NULL;
	}
	| spaces {
		anjuta_token_set_flags ($1, ANJUTA_TOKEN_IRRELEVANT);
	}
	;

separator:
	COMMA {
		anjuta_token_set_type ($$, ANJUTA_TOKEN_SEPARATOR);
	}
	| COMMA spaces {
		anjuta_token_merge ($1, $2);
		anjuta_token_set_type ($$, ANJUTA_TOKEN_SEPARATOR);
	}
	;

spaces:
	SPACE
	| spaces SPACE {
		anjuta_token_merge ($1, $2);
	}
	;

arg:
	IDENTIFIER
	| OPERATOR
	| NUMBER
	| NAME
	| MACRO            
	;

token:
	SPACE
	| IDENTIFIER
	| OPERATOR
	| NUMBER
	| NAME
	| MACRO
	| COMMA
	| RIGHT_PAREN
	;            

%%
    
static void
amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s)
{
	g_message ("scanner %p", scanner);
    g_message ("%s (%d:%d-%d:%d) %s\n", amp_ac_scanner_get_filename ((AmpAcScanner *)scanner), loc->first_line, loc->first_column, loc->last_line, loc->last_column, s);
}

/* Public functions
 *---------------------------------------------------------------------------*/

