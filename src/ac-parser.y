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

//static void amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s);

//amp_ac_yydebug = 1;

%}

%union {AnjutaTokenRange range;}

%token  PM_TOKEN_EOL '\n'

%token  <range> PM_TOKEN_SPACE ' '

%token  PM_TOKEN_HASH               '#'
%token  PM_TOKEN_LEFT_PAREN     '('
%token  PM_TOKEN_RIGHT_PAREN    ')'
%token  PM_TOKEN_LEFT_CURLY		'{'
%token  PM_TOKEN_RIGHT_CURLY    '}'
%token  PM_TOKEN_LEFT_BRACE     '['
%token  PM_TOKEN_RIGHT_BRACE    ']'
%token  PM_TOKEN_EQUAL             '='
%token  <range> PM_TOKEN_COMMA             ','
%token  PM_TOKEN_LOWER              '<'
%token  PM_TOKEN_GREATER           '>'
%token  PM_TOKEN_LOWER_OR_EQUAL
%token  PM_TOKEN_GREATER_OR_EQUAL	
    
%token  PM_TOKEN_COMMENT
%token  <range> PM_TOKEN_NAME
%token  PM_TOKEN_STRING
%token  PM_TOKEN_KEYWORD
%token  PM_TOKEN_TOPNAME
%token  PM_TOKEN_IDENTIFIER
%token  PM_TOKEN_NUMBER
%token  <range> PM_TOKEN_MACRO
%token  <range> PM_TOKEN_OPERATOR

%token	<range> PM_TOKEN_PKG_CHECK_MODULES
%token	<range> PM_TOKEN_OBSOLETE_AC_OUTPUT
%token	<range> PM_TOKEN_AC_OUTPUT
%token	<range> PM_TOKEN_AC_CONFIG_FILES
%token	<range> PM_TOKEN_AC_SUBST
%token	<range> PM_TOKEN_AC_INIT

%type <range> pkg_check_modules	obsolete_ac_output ac_config_files space_list_strip space_list optional_space name name_strip

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
		| file PM_TOKEN_EOL statement
		;
                
statement:
		line_or_empty
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
		PM_TOKEN_PKG_CHECK_MODULES name_strip PM_TOKEN_COMMA space_list_strip list_empty_optional  {anjuta_token_set_flags ($1.first, ANJUTA_TOKEN_SIGNIFICANT); anjuta_token_set_flags ($2.first, ANJUTA_TOKEN_OPEN); anjuta_token_set_flags ($2.last, ANJUTA_TOKEN_CLOSE); anjuta_token_set_flags ($3.first, ANJUTA_TOKEN_IRRELEVANT);}
		| PM_TOKEN_PKG_CHECK_MODULES name_strip PM_TOKEN_COMMA space_list_strip list_arg_optional
		;

obsolete_ac_output:
		PM_TOKEN_OBSOLETE_AC_OUTPUT space_list_strip list_optional_optional {anjuta_token_set_flags ($1.first, ANJUTA_TOKEN_SIGNIFICANT); g_message ("obsolete AC_OUTPUT");}
		;

ac_output:
		PM_TOKEN_AC_OUTPUT
		;
								
ac_config_files:
		PM_TOKEN_AC_CONFIG_FILES space_list_strip list_optional_optional {anjuta_token_set_flags ($1.first, ANJUTA_TOKEN_SIGNIFICANT);}
		;
								
list_empty_optional:
		PM_TOKEN_RIGHT_PAREN
		| PM_TOKEN_COMMA optional_space PM_TOKEN_RIGHT_PAREN
		| PM_TOKEN_COMMA optional_space PM_TOKEN_COMMA arg_string_or_empty PM_TOKEN_RIGHT_PAREN
		;

list_arg_optional:
		PM_TOKEN_COMMA arg_string PM_TOKEN_RIGHT_PAREN
		| PM_TOKEN_COMMA arg_string PM_TOKEN_COMMA arg_string_or_empty PM_TOKEN_RIGHT_PAREN
		;

list_optional_optional:
		PM_TOKEN_RIGHT_PAREN
		| PM_TOKEN_COMMA arg_string_or_empty PM_TOKEN_RIGHT_PAREN
		| PM_TOKEN_COMMA arg_string_or_empty PM_TOKEN_COMMA arg_string_or_empty PM_TOKEN_RIGHT_PAREN
		;

arg_string_or_empty:
		/* empty */
		| PM_TOKEN_SPACE
		| arg_string
		;

arg_string:
		arg
		| PM_TOKEN_SPACE arg
		| arg_string arg
		| arg_string PM_TOKEN_SPACE
		;

								
name_strip:
		optional_space name optional_space
		;            

name: 
		PM_TOKEN_NAME
		| PM_TOKEN_MACRO
		| PM_TOKEN_OPERATOR
		| name PM_TOKEN_MACRO	 {$$.last = $2.last;}
		| name PM_TOKEN_OPERATOR 	{$$.last = $2.last;}
		| name PM_TOKEN_NAME 			{$$.last = $2.last;}
		;
								
								
space_list_strip:
		optional_space space_list optional_space {anjuta_token_set_flags ($2.first, ANJUTA_TOKEN_OPEN); anjuta_token_set_flags ($2.last, ANJUTA_TOKEN_CLOSE | ANJUTA_TOKEN_NEXT);}
		;
            
space_list:
		name
		| space_list PM_TOKEN_SPACE name {$$.first = $1.first; $$.last = $3.last; anjuta_token_set_flags ($2.first, ANJUTA_TOKEN_NEXT | ANJUTA_TOKEN_IRRELEVANT);}
		;
            
optional_space:
		/* empty */
		| PM_TOKEN_SPACE { anjuta_token_set_flags ($1.first, ANJUTA_TOKEN_IRRELEVANT);}
		;

arg:
		PM_TOKEN_IDENTIFIER
		| PM_TOKEN_OPERATOR
		| PM_TOKEN_NUMBER
		| PM_TOKEN_NAME
		| PM_TOKEN_MACRO            
		;
            
token:
		PM_TOKEN_SPACE
		| PM_TOKEN_IDENTIFIER
		| PM_TOKEN_OPERATOR
		| PM_TOKEN_NUMBER
		| PM_TOKEN_NAME
		| PM_TOKEN_MACRO
		| PM_TOKEN_COMMA
		| PM_TOKEN_RIGHT_PAREN
		;            
                                
%%
     
static void
amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s)
{
    g_message ("(%d:%d-%d:%d) %s\n", loc->first_line, loc->first_column, loc->last_line, loc->last_column, s);
}

/* Public functions
 *---------------------------------------------------------------------------*/

