/*
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2001 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* Perl-Compatible Regular Expression (PCRE) internal field type.
 * Used with the "matches" dfilter operator, allowing efficient
 * compilation and studying of a PCRE pattern in dfilters.
 *
 * PCRE is provided with libpcre (http://www.pcre.org/).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ftypes-int.h>
#include <epan/emem.h>

#ifdef HAVE_LIBPCRE

#include <string.h>

#include <pcre.h>

/* Create a pcre_tuple_t object based on the given string pattern */ 
static pcre_tuple_t *
pcre_tuple_new(const char *value)
{
	pcre_tuple_t *tuple;
	const char *pcre_error_text;
	int pcre_error_offset;
	void *tmpp;

	tuple = ep_malloc(sizeof(pcre_tuple_t));
	tuple->string = ep_strdup(value); /* The RE as string */
	tuple->ex = NULL;
	/* Compile the RE and convert it into ep allocated memory */
	tmpp = pcre_compile(
			value,				/* pattern */
			0,					/* PCRE options */
			&pcre_error_text,	/* PCRE constant error string */
			&pcre_error_offset,	/* Start offset of error in pattern */
			NULL				/* Default char tables (C locale) */
			);
	tuple->re = ep_alloc(sizeof(pcre));
	memcpy(tuple->re, tmpp, sizeof(pcre));
	g_free(tmpp);

	if (pcre_error_text) {
		tuple->error = ep_strdup_printf("In regular expression \"%s\":\n"
				"%s (character position %d)",
				value, pcre_error_text, pcre_error_offset);
		return tuple;
	} else {
		tuple->error = NULL;
	}
	/* Study the RE and convert it into ep allocated memory */
	tmpp = pcre_study(tuple->re, 0, &pcre_error_text);
	tuple->ex = ep_alloc(sizeof(pcre_extra));
	memcpy(tuple->ex, tmpp, sizeof(pcre_extra));
	g_free(tmpp);

	if (pcre_error_text) {
		if (tuple->error) {
			tuple->error = ep_strdup_printf("In regular expression \"%s\":\n"
					"%s. %s",
					value, tuple->error, pcre_error_text);
		} else {
			tuple->error = ep_strdup_printf("In regular expression \"%s\":\n"
					"%s",
					value, pcre_error_text);
		}
	}
	return tuple;
}

static void
pcre_fvalue_new(fvalue_t *fv)
{
	fv->value.re = NULL;
}

/* Generate a FT_PCRE from a parsed string pattern.
 * Uses the specified logfunc() to report errors. */
static gboolean
val_from_string(fvalue_t *fv, char *pattern, LogFunc logfunc)
{
	fv->value.re = pcre_tuple_new(pattern);
	if (fv->value.re->error) {
		logfunc(fv->value.re->error);
		return FALSE;
	}
	return TRUE;
}

/* Generate a FT_PCRE from an unparsed string pattern.
 * Uses the specified logfunc() to report errors. */
static gboolean
val_from_unparsed(fvalue_t *fv, char *pattern, gboolean allow_partial_value _U_, LogFunc logfunc)
{
	g_assert(! allow_partial_value);

	fv->value.re = pcre_tuple_new(pattern);
	if (fv->value.re->error) {
		logfunc(fv->value.re->error);
		return FALSE;
	}
	return TRUE;
}

static int
pcre_repr_len(fvalue_t *fv, ftrepr_t rtype)
{
	g_assert(rtype == FTREPR_DFILTER);
	return strlen(fv->value.re->string);
}

static void
pcre_to_repr(fvalue_t *fv, ftrepr_t rtype, char *buf)
{
	g_assert(rtype == FTREPR_DFILTER);
	strcpy(buf, fv->value.re->string);
}

/* BEHOLD - value contains the string representation of the regular expression,
 * and we want to store the compiled PCRE RE object into the value. */
static void
pcre_fvalue_set(fvalue_t *fv, gpointer value, gboolean already_copied)
{
	g_assert(value != NULL);
	g_assert(! already_copied);
	fv->value.re = pcre_tuple_new(value);
}

static gpointer
pcre_fvalue_get(fvalue_t *fv)
{
	return fv->value.re;
}

void
ftype_register_pcre(void)
{
	static ftype_t pcre_type = {
		FT_PCRE,		/* ftype */
		"FT_PCRE",		/* name */
		"Compiled Perl-Compatible Regular Expression object", /* pretty_name */
		0,			/* wire_size */
		pcre_fvalue_new,	/* new_value */
		NULL,			/* free_value */
		val_from_unparsed,	/* val_from_unparsed */
		val_from_string,	/* val_from_string */
		pcre_to_repr,		/* val_to_string_repr */
		pcre_repr_len,		/* len_string_repr */

		pcre_fvalue_set,	/* set_value */
		NULL,			/* set_value_integer */
		NULL,			/* set_value_integer64 */
		NULL,			/* set_value_floating */

		pcre_fvalue_get,	/* get_value */
		NULL,			/* get_value_integer */
		NULL,			/* get_value_integer64 */
		NULL,			/* get_value_floating */

		NULL,			/* cmp_eq */
		NULL,			/* cmp_ne */
		NULL,			/* cmp_gt */
		NULL,			/* cmp_ge */
		NULL,			/* cmp_lt */
		NULL,			/* cmp_le */
		NULL,			/* cmp_bitwise_and */
		NULL,			/* cmp_contains */
		NULL,			/* cmp_matches */

		NULL,			/* len */
		NULL,			/* slice */
	};
	ftype_register(FT_PCRE, &pcre_type);
}

#else /* HAVE_LIBPCRE */

void
ftype_register_pcre(void)
{
	static ftype_t pcre_type = {
		FT_PCRE,		/* ftype */
		"FT_PCRE",			/* name */
		"Compiled Perl-Compatible Regular Expression object", /* pretty_name */
		0,				/* wire_size */
		NULL,				/* new_value */
		NULL,				/* free_value */
		NULL,				/* val_from_unparsed */
		NULL,				/* val_from_string */
		NULL,				/* val_to_string_repr */
		NULL,				/* len_string_repr */

		NULL,				/* set_value */
		NULL,				/* set_value_integer */
		NULL,				/* set_value_integer64 */
		NULL,				/* set_value_floating */

		NULL,				/* get_value */
		NULL,				/* get_value_integer */
		NULL,				/* get_value_integer64 */
		NULL,				/* get_value_floating */ 

		NULL,				/* cmp_eq */
		NULL,				/* cmp_ne */
		NULL,				/* cmp_gt */
		NULL,				/* cmp_ge */
		NULL,				/* cmp_lt */
		NULL,				/* cmp_le */
		NULL,				/* cmp_bitwise_and */
		NULL,				/* cmp_contains */
		NULL,				/* cmp_matches */

		NULL,				/* len */
		NULL,				/* slice */
	};
	ftype_register(FT_PCRE, &pcre_type);
}

#endif /* HAVE_LIBPCRE */
