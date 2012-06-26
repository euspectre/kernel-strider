/*
 * Parser for string which contain holes for parameters substitution.
 * 
 * Format of such holes: $paramName$.
 * "$$" construction means '$' character(not a hole).
 * 
 * Mainly used for program usage description.
 */

#ifndef TEMPLATE_PARSER_H
#define TEMPLATE_PARSER_H

#ifdef __KERNEL__
#include <linux/slab.h> /* krealloc, kfree */
#else
#include <string.h> /* strncmp */
#include <stdlib.h> /*realloc, free */
#endif 

/* Specification of the parameter to substitute. */
struct param_spec
{
	/* Name of the parameter */
	const char* name;
	/* 
	 * Function which print parameter value in snprintf-like form.
	 * 
	 * Function accept buffer 'buf' with size 'buf_size'.
	 * Function should store parameter value in this buffer as null-
	 * terminated string; no more than 'buf_size' characters should be
	 * stored.
	 * Function should return length of the parameter value.
	 */
	int (*print)(char* buf, int buf_size, void* user_data);
};

/* 
 * Object used for parse string with holes and
 * substitute parameters values into them.
 */
struct template_parser
{
	const char* str;
	const char* str_end;
	
	const struct param_spec* param_specs;
	int param_specs_n;
	
	void* user_data;
	
	const char* current_pos;
	char* param_value;
};

/* 
 * Initialize template parser with given string with holes
 * and parameters specifications.
 */
static inline void template_parser_init(struct template_parser* parser,
	const char* str, const char* str_end,
	const struct param_spec* param_specs, int param_specs_n,
	void* user_data)
{
	parser->str = str;
	parser->str_end = str_end;
	
	parser->param_specs = param_specs;
	parser->param_specs_n = param_specs_n;
	
	parser->user_data = user_data;
	
	parser->current_pos = str;
	parser->param_value = NULL;
}

static inline void template_parser_destroy(struct template_parser* parser)
{
#ifdef __KERNEL__
	kfree(parser->param_value);
#else
	free(parser->param_value);
#endif
}

/* 
 * Read next chunk of substituted string with holes.
 * 
 * Return next chunk extracted.
 * 'chunk_size' contain size of the returned chunk.
 * 
 * On EOF return NULL.
 * 
 * On any error return NULL.
 */
static inline const char* template_parser_next_chunk(
	struct template_parser* parser, int* chunk_size)
{
	const char* param_start;
	int param_len;
	int param_index;
	
	if(parser->current_pos == parser->str_end)
	{
		return NULL;
	}
	else if(*parser->current_pos != '$')
	{
		const char* result = parser->current_pos;
		for(++parser->current_pos;
			parser->current_pos != parser->str_end;
			++parser->current_pos)
		{
			if(*parser->current_pos == '$') break;
		}
		*chunk_size = parser->current_pos - result;
		return result;
	}
	else
	{
		++parser->current_pos;
		
		if(parser->current_pos == parser->str_end)
			return NULL; /* Tailed '$'. */
		else if(*parser->current_pos == '$')
		{
			/* "$$" */
			parser->current_pos++;
			*chunk_size = 1;
			return parser->current_pos - 1;
		}
	}
	/* Hole for parameter substitution */
	param_start = parser->current_pos;
	for(++parser->current_pos;
		(parser->current_pos != parser->str_end)
		&& (*parser->current_pos != '$');
		++parser->current_pos);
	if(parser->current_pos == parser->str_end)
		return NULL; /* Unterminated hole */
	/* Correct parameter name */
	param_len = parser->current_pos - param_start;
	parser->current_pos++;
	
	for(param_index = 0;
		param_index < parser->param_specs_n;
		param_index++)
	{
		int param_value_len;
		char* param_value;
		const struct param_spec* spec = &parser->param_specs[param_index];
		if(strncmp(spec->name, param_start, param_len)) continue;
		if(spec->name[param_len] != '\0') continue;
		
		param_value_len = spec->print(NULL, 0, parser->user_data);
#ifdef __KERNEL__
		param_value = (char*)krealloc(parser->param_value, param_value_len + 1, GFP_KERNEL);
#else
		param_value = (char*)realloc(parser->param_value, param_value_len + 1);
#endif
		if(param_value == NULL) return NULL;/* Not enough memory */
		
		parser->param_value = param_value;
		spec->print(parser->param_value, param_value_len + 1, parser->user_data);
		
		*chunk_size = param_value_len;
		return parser->param_value;
	}
	return NULL;/* Unknown parameter */
}

#endif /* TEMPLATE_PARSER_H */