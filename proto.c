/* proto.c
 * Routines for protocol tree
 *
 * $Id: proto.c,v 1.60 2000/04/04 17:07:07 gram Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@zing.org>
 * Copyright 1998 Gerald Combs
 *
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifndef _STDIO_H
#include <stdio.h>
#endif

#include <stdarg.h>

#ifndef _STRING_H
#include <string.h>
#endif

#ifdef NEED_SNPRINTF_H
# include "snprintf.h"
#endif

#ifndef __G_LIB_H__
#include <glib.h>
#endif

#ifndef __PROTO_H__
#include "proto.h"
#endif

#ifndef __PACKET_H__
#include "packet.h"
#endif

#ifndef __RESOLV_H__
#include "resolv.h"
#endif

#ifndef __REGISTER_H__
#include "register.h"
#endif

#include "packet-ipv6.h"

#define cVALS(x) (const value_string*)(x)

static gboolean
proto_tree_free_node(GNode *node, gpointer data);

static void fill_label_boolean(field_info *fi, gchar *label_str);
static void fill_label_uint(field_info *fi, gchar *label_str);
static void fill_label_enumerated_uint(field_info *fi, gchar *label_str);
static void fill_label_enumerated_bitfield(field_info *fi, gchar *label_str);
static void fill_label_numeric_bitfield(field_info *fi, gchar *label_str);
static void fill_label_int(field_info *fi, gchar *label_str);
static void fill_label_enumerated_int(field_info *fi, gchar *label_str);

static int hfinfo_bitwidth(header_field_info *hfinfo);
static char* hfinfo_uint_vals_format(header_field_info *hfinfo);
static char* hfinfo_uint_format(header_field_info *hfinfo);
static char* hfinfo_int_vals_format(header_field_info *hfinfo);
static char* hfinfo_int_format(header_field_info *hfinfo);

static gboolean check_for_protocol_or_field_id(GNode *node, gpointer data);
static gboolean check_for_field_within_protocol(GNode *node, gpointer data);

static field_info*
proto_tree_add_field_info(int hfindex, gint start, gint length, int visible);

static proto_item*
proto_tree_add_node(proto_tree *tree, field_info *fi);

static proto_item *
proto_tree_add_pi(proto_tree *tree, int hfindex, gint start, gint length,
		const char *format, gboolean visible, field_info **pfi, va_list ap);

static void
proto_tree_set_value(field_info *fi, gint length, va_list ap);
static void
proto_tree_set_bytes(field_info *fi, const guint8* start_ptr, gint length);
static void
proto_tree_set_time(field_info *fi, struct timeval *value_ptr);
static void
proto_tree_set_string(field_info *fi, const char* value);
static void
proto_tree_set_ether(field_info *fi, guint8* value);
static void
proto_tree_set_ipxnet(field_info *fi, guint32 value);
static void
proto_tree_set_ipv4(field_info *fi, guint32 value);
static void
proto_tree_set_ipv6(field_info *fi, guint8* value_ptr);
static void
proto_tree_set_boolean(field_info *fi, guint32 value);
static void
proto_tree_set_double(field_info *fi, double value);
static void
proto_tree_set_uint(field_info *fi, guint32 value);

static int proto_register_field_init(header_field_info *hfinfo, int parent);

/* special-case header field used within proto.c */
int hf_text_only = 1;

/* Contains information about protocols and header fields. Used when
 * dissectors register their data */
GMemChunk *gmc_hfinfo = NULL;

/* Contains information about a field when a dissector calls
 * proto_tree_add_item.  */
GMemChunk *gmc_field_info = NULL;

/* String space for protocol and field items for the GUI */
GMemChunk *gmc_item_labels = NULL;

/* List which stores protocols and fields that have been registered */
GPtrArray *gpa_hfinfo = NULL;

/* Points to the first element of an array of Booleans, indexed by
   a subtree item type; that array element is TRUE if subtrees of
   an item of that type are to be expanded. */
gboolean	*tree_is_expanded;

/* Number of elements in that array. */
int		num_tree_types;

/* Is the parsing being done for a visible proto_tree or an invisible one?
 * By setting this correctly, the proto_tree creation is sped up by not
 * having to call vsnprintf and copy strings around.
 */
gboolean proto_tree_is_visible = TRUE;

/* initialize data structures and register protocols and fields */
void
proto_init(void)
{
	static hf_register_info hf[] = {
		{ &hf_text_only,
		{ "Text",	"text", FT_TEXT_ONLY, BASE_NONE, NULL, 0x0,
			"" }},
	};

	if (gmc_hfinfo)
		g_mem_chunk_destroy(gmc_hfinfo);
	if (gmc_field_info)
		g_mem_chunk_destroy(gmc_field_info);
	if (gmc_item_labels)
		g_mem_chunk_destroy(gmc_item_labels);
	if (gpa_hfinfo)
		g_ptr_array_free(gpa_hfinfo, FALSE);
	if (tree_is_expanded != NULL)
		g_free(tree_is_expanded);

	gmc_hfinfo = g_mem_chunk_new("gmc_hfinfo",
		sizeof(struct header_field_info), 50 * sizeof(struct 
		header_field_info), G_ALLOC_ONLY);
	gmc_field_info = g_mem_chunk_new("gmc_field_info",
		sizeof(struct field_info), 200 * sizeof(struct field_info),
		G_ALLOC_AND_FREE);
	gmc_item_labels = g_mem_chunk_new("gmc_item_labels",
		ITEM_LABEL_LENGTH, 20 * ITEM_LABEL_LENGTH,
		G_ALLOC_AND_FREE);
	gpa_hfinfo = g_ptr_array_new();

	/* Allocate "tree_is_expanded", with one element for ETT_NONE,
	   and initialize that element to FALSE. */
	tree_is_expanded = g_malloc(sizeof (gint));
	tree_is_expanded[0] = FALSE;
	num_tree_types = 1;

	/* Have each dissector register its protocols and fields, and
	   do whatever one-time initialization it needs to do. */
	register_all_protocols();

	/* Now have the ones that register a "handoff", i.e. that
	   specify that another dissector for a protocol under which
	   this dissector's protocol lives call it. */
	register_all_protocol_handoffs();

	/* Register one special-case FT_TEXT_ONLY field for use when
		converting ethereal to new-style proto_tree. These fields
		are merely strings on the GUI tree; they are not filterable */
	proto_register_field_array(-1, hf, array_length(hf));

	/* We've assigned all the subtree type values; allocate the array
	   for them, and zero it out. */
	tree_is_expanded = g_malloc(num_tree_types*sizeof (gint *));
	memset(tree_is_expanded, '\0', num_tree_types*sizeof (gint *));
}

void
proto_cleanup(void)
{
	if (gmc_hfinfo)
		g_mem_chunk_destroy(gmc_hfinfo);
	if (gmc_field_info)
		g_mem_chunk_destroy(gmc_field_info);
	if (gmc_item_labels)
		g_mem_chunk_destroy(gmc_item_labels);
	if (gpa_hfinfo)
		g_ptr_array_free(gpa_hfinfo, FALSE);
}

/* frees the resources that the dissection a proto_tree uses */
void
proto_tree_free(proto_tree *tree)
{
	g_node_traverse((GNode*)tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
		proto_tree_free_node, NULL);
	g_node_destroy((GNode*)tree);
}

static gboolean
proto_tree_free_node(GNode *node, gpointer data)
{
	field_info *fi = (field_info*) (node->data);

	if (fi != NULL) {
		if (fi->representation)
			g_mem_chunk_free(gmc_item_labels, fi->representation);
		if (fi->hfinfo->type == FT_STRING)
			g_free(fi->value.string);
		else if (fi->hfinfo->type == FT_BYTES) 
			g_free(fi->value.bytes);
		g_mem_chunk_free(gmc_field_info, fi);
	}
	return FALSE; /* FALSE = do not end traversal of GNode tree */
}	

/* Finds a record in the hf_info_records array by id. */
struct header_field_info*
proto_registrar_get_nth(int hfindex)
{
	g_assert(hfindex >= 0 && hfindex < gpa_hfinfo->len);
	return g_ptr_array_index(gpa_hfinfo, hfindex);
}

/* Finds a dissector table by field name. */
dissector_table_t
find_dissector_table(const char *name)
{
	header_field_info	*hfinfo;
	int			i, len;

	len = gpa_hfinfo->len;
	for (i = 0; i < len ; i++) {
		hfinfo = proto_registrar_get_nth(i);
		if (strcmp(name, hfinfo->abbrev) == 0)
			return hfinfo->sub_dissectors;
	}
	return NULL;
}

/* Add a node with no text */
proto_item *
proto_tree_add_notext(proto_tree *tree, gint start, gint length)
{
	field_info	*fi;
	proto_item	*pi;

	if (!tree)
		return(NULL);

	fi = proto_tree_add_field_info(hf_text_only, start, length,
	    proto_tree_is_visible);
	pi = proto_tree_add_node(tree, fi);
	fi->representation = NULL;

	return pi;
}

/* Add a text-only node to the proto_tree */
proto_item *
proto_tree_add_text(proto_tree *tree, gint start, gint length,
	const char *format, ...)
{
	field_info	*fi;
	proto_item	*pi;
	va_list		ap;

	if (!tree)
		return(NULL);

	fi = proto_tree_add_field_info(hf_text_only, start, length,
	    proto_tree_is_visible);
	pi = proto_tree_add_node(tree, fi);

	/* Only add text if we know that proto_tree is to be shown */
	if (proto_tree_is_visible) {
		va_start(ap, format);
		fi->representation = g_mem_chunk_alloc(gmc_item_labels);
		vsnprintf(fi->representation, ITEM_LABEL_LENGTH, format, ap);
		va_end(ap);
	}
	else {
		fi->representation = NULL;
	}

	return pi;
}

/* Add an item to a proto_tree, using the text label registered to that item. */
proto_item *
proto_tree_add_item(proto_tree *tree, int hfindex, gint start, gint length, ...)
{
	proto_item	*pi;
	va_list		ap;
	field_info	*new_fi;

	if (!tree)
		return(NULL);

	va_start(ap, length);
	/* ap won't be used since format is NULL */
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			NULL, TRUE, &new_fi, ap);
	proto_tree_set_value(new_fi, length, ap);
	va_end(ap);

	return pi;
}

/* Add an item to a proto_tree, but make it invisible in GUI. The field is
 * still searchable, though. */
proto_item *
proto_tree_add_item_hidden(proto_tree *tree, int hfindex, gint start, gint length, ...)
{
	proto_item	*pi;
	va_list		ap;
	field_info	*new_fi;

	if (!tree)
		return(NULL);

	va_start(ap, length);
	/* ap won't be used since format is NULL */
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			NULL, FALSE, &new_fi, ap);
	proto_tree_set_value(new_fi, length, ap);
	va_end(ap);

	return pi;
}

static void
proto_tree_set_value(field_info *fi, gint length, va_list ap)
{
	header_field_info	*hfinfo;

	hfinfo = fi->hfinfo;

	/* from the stdarg man page on Solaris 2.6:
	NOTES
	     It is up to the calling routine to specify  in  some  manner
	     how  many arguments there are, since it is not always possi-
	     ble to determine the number  of  arguments  from  the  stack
	     frame.   For example, execl is passed a zero pointer to sig-
	     nal the end of the list.  printf can tell how many arguments
	     there  are  by  the format.  It is non-portable to specify a
	     second argument of char, short, or float to va_arg,  because
	     arguments  seen  by the called function are not char, short,
	     or float.  C converts char and short arguments  to  int  and
	     converts  float arguments to double before passing them to a
	     function.
	*/
	switch(hfinfo->type) {
		case FT_NONE:
			break;

		case FT_BYTES:
			proto_tree_set_bytes(fi, va_arg(ap, guint8*), length);
			break;

		case FT_BOOLEAN:
			proto_tree_set_boolean(fi, va_arg(ap, unsigned int));
			break;

		case FT_UINT8:
		case FT_UINT16:
		case FT_UINT24:
		case FT_UINT32:
		case FT_INT8:
		case FT_INT16:
		case FT_INT24:
		case FT_INT32:
			proto_tree_set_uint(fi, va_arg(ap, unsigned int));
			break;

		case FT_IPv4:
			proto_tree_set_ipv4(fi, va_arg(ap, unsigned int));
			break;

		case FT_IPXNET:
			proto_tree_set_ipxnet(fi, va_arg(ap, unsigned int));
			break;

		case FT_IPv6:
			proto_tree_set_ipv6(fi, va_arg(ap, guint8*));
			break;

		case FT_DOUBLE:
			proto_tree_set_double(fi, va_arg(ap, double));
			break;

		case FT_ETHER:
			proto_tree_set_ether(fi, va_arg(ap, guint8*));
			break;

		case FT_ABSOLUTE_TIME:
		case FT_RELATIVE_TIME:
			proto_tree_set_time(fi, va_arg(ap, struct timeval*));
			break;

		case FT_STRING:
			/* This g_strdup'ed memory is freed in proto_tree_free_node() */
			proto_tree_set_string(fi, va_arg(ap, char*));
			break;

		case FT_TEXT_ONLY:
			; /* nothing */
			break;

		default:
			g_error("hfinfo->type %d not handled\n", hfinfo->type);
			g_assert_not_reached();
			break;
	}
}


/* Add a FT_NONE to a proto_tree */
proto_item *
proto_tree_add_protocol_format(proto_tree *tree, int hfindex, gint start, gint length,
		const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_NONE);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, NULL, ap);
	/* no value to set for FT_NONE */
	va_end(ap);

	return pi;
}

/* Add a FT_BYTES to a proto_tree */
proto_item *
proto_tree_add_bytes_format(proto_tree *tree, int hfindex, gint start, gint length,
		const guint8 *start_ptr, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_BYTES);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_bytes(new_fi, start_ptr, length);
	va_end(ap);

	return pi;
}

/* Set the FT_BYTES value */
static void
proto_tree_set_bytes(field_info *fi, const guint8* start_ptr, gint length)
{
	g_assert(start_ptr != NULL);
	g_assert(length > 0);
	/* This g_malloc'ed memory is freed in
	   proto_tree_free_node() */
	fi->value.bytes = g_malloc(length);
	memcpy(fi->value.bytes, start_ptr, length);
}

/* Add a FT_*TIME to a proto_tree */
proto_item *
proto_tree_add_time_format(proto_tree *tree, int hfindex, gint start, gint length,
		struct timeval *value_ptr, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_ABSOLUTE_TIME ||
				hfinfo->type == FT_RELATIVE_TIME);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_time(new_fi, value_ptr);
	va_end(ap);

	return pi;
}

/* Set the FT_*TIME value */
static void
proto_tree_set_time(field_info *fi, struct timeval *value_ptr)
{
	memcpy(&fi->value.time, value_ptr, sizeof(struct timeval));
}

/* Add a FT_IPXNET to a proto_tree */
proto_item *
proto_tree_add_ipxnet_format(proto_tree *tree, int hfindex, gint start, gint length,
		guint32 value, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_IPXNET);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_ipxnet(new_fi, value);
	va_end(ap);

	return pi;
}

/* Set the FT_IPXNET value */
static void
proto_tree_set_ipxnet(field_info *fi, guint32 value)
{
	fi->value.numeric = value;
}

/* Add a FT_IPv4 to a proto_tree */
proto_item *
proto_tree_add_ipv4_format(proto_tree *tree, int hfindex, gint start, gint length,
		guint32 value, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_IPv4);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_ipv4(new_fi, value);
	va_end(ap);

	return pi;
}

/* Set the FT_IPv4 value */
static void
proto_tree_set_ipv4(field_info *fi, guint32 value)
{
	ipv4_addr_set_net_order_addr(&(fi->value.ipv4), value);
	ipv4_addr_set_netmask_bits(&(fi->value.ipv4), 32);
}

/* Add a FT_IPv6 to a proto_tree */
proto_item *
proto_tree_add_ipv6_format(proto_tree *tree, int hfindex, gint start, gint length,
		guint8* value_ptr, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_IPv6);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_ipv6(new_fi, value_ptr);
	va_end(ap);

	return pi;
}

/* Set the FT_IPv4 value */
static void
proto_tree_set_ipv6(field_info *fi, guint8* value_ptr)
{
	memcpy(fi->value.ipv6, value_ptr, 16);
}

/* Add a FT_STRING to a proto_tree */
proto_item *
proto_tree_add_string_format(proto_tree *tree, int hfindex, gint start, gint length,
		const char* value, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_STRING);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_string(new_fi, value);
	va_end(ap);

	return pi;
}

/* Set the FT_STRING value */
static void
proto_tree_set_string(field_info *fi, const char* value)
{
	/* This g_strdup'ed memory is freed in proto_tree_free_node() */
	fi->value.string = g_strdup(value);
}

/* Add a FT_ETHER to a proto_tree */
proto_item *
proto_tree_add_ether_format(proto_tree *tree, int hfindex, gint start, gint length,
		guint8* value, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_ETHER);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_ether(new_fi, value);
	va_end(ap);

	return pi;
}

/* Set the FT_ETHER value */
static void
proto_tree_set_ether(field_info *fi, guint8* value)
{
	memcpy(fi->value.ether, value, 6);
}

/* Add a FT_BOOLEAN to a proto_tree */
proto_item *
proto_tree_add_boolean_format(proto_tree *tree, int hfindex, gint start, gint length,
		guint32 value, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_BOOLEAN);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_boolean(new_fi, value);
	va_end(ap);

	return pi;
}

/* Set the FT_BOOLEAN value */
static void
proto_tree_set_boolean(field_info *fi, guint32  value)
{
	fi->value.numeric = value;
}

/* Add a FT_DOUBLE to a proto_tree */
proto_item *
proto_tree_add_double_format(proto_tree *tree, int hfindex, gint start, gint length,
		double value, const char *format, ...)
{
	proto_item		*pi;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(hfinfo->type == FT_DOUBLE);

	va_start(ap, format);
	pi = proto_tree_add_pi(tree, hfindex, start, length,
			format, TRUE, &new_fi, ap);
	proto_tree_set_double(new_fi, value);
	va_end(ap);

	return pi;
}

/* Set the FT_DOUBLE value */
static void
proto_tree_set_double(field_info *fi, double value)
{
	fi->value.floating = value;
}

/* Add any FT_UINT* to a proto_tree */
proto_item *
proto_tree_add_uint_format(proto_tree *tree, int hfindex, gint start, gint length,
		guint32 value, const char *format, ...)
{
	proto_item		*pi = NULL;
	va_list			ap;
	field_info		*new_fi;
	header_field_info	*hfinfo;

	if (!tree)
		return (NULL);

	hfinfo = proto_registrar_get_nth(hfindex);
	switch(hfinfo->type) {
		case FT_UINT8:
		case FT_UINT16:
		case FT_UINT24:
		case FT_UINT32:
			va_start(ap, format);
			pi = proto_tree_add_pi(tree, hfindex, start, length,
					format, TRUE, &new_fi, ap);
			proto_tree_set_uint(new_fi, value);
			va_end(ap);
			break;

		default:
			g_assert_not_reached();
	}

	return pi;
}

/* Set the FT_UINT* value */
static void
proto_tree_set_uint(field_info *fi, guint32 value)
{
	header_field_info *hfinfo;

	hfinfo = fi->hfinfo;
	fi->value.numeric = value;
	if (hfinfo->bitmask) {
		/* Mask out irrelevant portions */
		fi->value.numeric &= hfinfo->bitmask;

		/* Shift bits */
		if (hfinfo->bitshift > 0) {
			fi->value.numeric >>= hfinfo->bitshift;
		}
	}
}



/* Create a new field_info struct, and initialize it */
static field_info *
proto_tree_add_field_info(int hfindex, gint start, gint length, int visible)
{
	field_info *fi;

	fi = g_mem_chunk_alloc(gmc_field_info);

	g_assert(hfindex >= 0 && hfindex < gpa_hfinfo->len);
	fi->hfinfo = proto_registrar_get_nth(hfindex);
	g_assert(fi->hfinfo != NULL);
	fi->start = start;
	fi->length = length;
	fi->tree_type = ETT_NONE;
	fi->visible = visible;

	return fi;
}

/* Add a field_info struct to the proto_tree, encapsulating it in a GNode (proto_item) */
static proto_item *
proto_tree_add_node(proto_tree *tree, field_info *fi)
{
	proto_item *pi;

	pi = (proto_item*) g_node_new(fi);
	g_node_append((GNode*)tree, (GNode*)pi);

	return pi;
}


/* Generic way to allocate field_info, add to proto_tree, and set representation.
 * Sets *pfi to address of newly-allocated field_info struct, if pfi is non-NULL. */
static proto_item *
proto_tree_add_pi(proto_tree *tree, int hfindex, gint start, gint length,
		const char *format, gboolean visible, field_info **pfi, va_list ap)
{
	proto_item	*pi;
	field_info	*fi;

	if (!tree)
		return(NULL);

	/* either visibility flag can nullify the other */
	visible = proto_tree_is_visible && visible;

	fi = proto_tree_add_field_info(hfindex, start, length, visible);

	pi = proto_tree_add_node(tree, fi);

	/* are there any formatting arguments? */
	if (visible && format) {
		fi->representation = g_mem_chunk_alloc(gmc_item_labels);
		vsnprintf(fi->representation, ITEM_LABEL_LENGTH, format, ap);
	}
	else {
		fi->representation = NULL;
	}

	if (pfi) {
		*pfi = fi;
	}

	return pi;
}


void
proto_item_set_text(proto_item *pi, const char *format, ...)
{
	field_info *fi = (field_info*) (((GNode*)pi)->data);
	va_list	ap;

	if (fi->representation)
		g_mem_chunk_free(gmc_item_labels, fi->representation);

	fi->representation = g_mem_chunk_alloc(gmc_item_labels);
	va_start(ap, format);
	vsnprintf(fi->representation, ITEM_LABEL_LENGTH,
				format, ap);
	va_end(ap);
}

void
proto_item_set_len(proto_item *pi, gint length)
{
	field_info *fi = (field_info*) (((GNode*)pi)->data);
	fi->length = length;
}

proto_tree*
proto_tree_create_root(void)
{
	return (proto_tree*) g_node_new(NULL);
}

proto_tree*
proto_item_add_subtree(proto_item *pi,  gint idx) {
	field_info *fi = (field_info*) (((GNode*)pi)->data);
	g_assert(idx >= 0 && idx < num_tree_types);
	fi->tree_type = idx;
	return (proto_tree*) pi;
}


int
proto_register_protocol(char *name, char *abbrev)
{
	struct header_field_info *hfinfo;

	/* Here we do allocate a new header_field_info struct */
	hfinfo = g_mem_chunk_alloc(gmc_hfinfo);
	hfinfo->name = name;
	hfinfo->abbrev = abbrev;
	hfinfo->type = FT_NONE;
	hfinfo->strings = NULL;
	hfinfo->bitmask = 0;
	hfinfo->bitshift = 0;
	hfinfo->blurb = "";
	hfinfo->parent = -1; /* this field differentiates protos and fields */

	hfinfo->sub_dissectors = NULL;	/* clear sub-dissector table pointer */

	return proto_register_field_init(hfinfo, hfinfo->parent);
}

/* for use with static arrays only, since we don't allocate our own copies
of the header_field_info struct contained withing the hf_register_info struct */
void
proto_register_field_array(int parent, hf_register_info *hf, int num_records)
{
	int			field_id, i;
	hf_register_info	*ptr = hf;

	for (i = 0; i < num_records; i++, ptr++) {
		field_id = proto_register_field_init(&ptr->hfinfo, parent);
		*ptr->p_id = field_id;
	}
}

static int
proto_register_field_init(header_field_info *hfinfo, int parent)
{
	/* These types of fields are allowed to have value_strings or true_false_strings */
	g_assert((hfinfo->strings == NULL) || (
			(hfinfo->type == FT_UINT8) ||
			(hfinfo->type == FT_UINT16) ||
			(hfinfo->type == FT_UINT24) ||
			(hfinfo->type == FT_UINT32) ||
			(hfinfo->type == FT_INT8) ||
			(hfinfo->type == FT_INT16) ||
			(hfinfo->type == FT_INT24) ||
			(hfinfo->type == FT_INT32) ||
			(hfinfo->type == FT_BOOLEAN) ));

	/* if this is a bitfield, compure bitshift */
	if (hfinfo->bitmask) {
		while ((hfinfo->bitmask & (1 << hfinfo->bitshift)) == 0)
			hfinfo->bitshift++;
	}

	hfinfo->parent = parent;

	/* if we always add and never delete, then id == len - 1 is correct */
	g_ptr_array_add(gpa_hfinfo, hfinfo);
	hfinfo->id = gpa_hfinfo->len - 1;
	return hfinfo->id;
}

void
proto_register_subtree_array(gint **indices, int num_indices)
{
	int	i;
	gint	**ptr = indices;

	/*
	 * Add "num_indices" elements to "tree_is_expanded".
	 */
	tree_is_expanded = g_realloc(tree_is_expanded,
	    (num_tree_types + num_indices)*sizeof (gint));

	/*
	 * Assign "num_indices" subtree numbers starting at "num_tree_types",
	 * returning the indices through the pointers in the array whose
	 * first element is pointed to by "indices", set to FALSE the
	 * elements to which those subtree numbers refer, and update
	 * "num_tree_types" appropriately.
	 */
	for (i = 0; i < num_indices; i++, ptr++, num_tree_types++) {
		tree_is_expanded[num_tree_types] = FALSE;
		**ptr = num_tree_types;
	}
}

void
proto_item_fill_label(field_info *fi, gchar *label_str)
{
	struct header_field_info	*hfinfo = fi->hfinfo;
	guint32				n_addr; /* network-order IPv4 address */

	switch(hfinfo->type) {
		case FT_NONE:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s", hfinfo->name);
			break;

		case FT_BOOLEAN:
			fill_label_boolean(fi, label_str);
			break;

		case FT_BYTES:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %s", hfinfo->name, 
				 bytes_to_str(fi->value.bytes, fi->length));
			break;

		/* Four types of integers to take care of:
		 * 	Bitfield, with val_string
		 * 	Bitfield, w/o val_string
		 * 	Non-bitfield, with val_string
		 * 	Non-bitfield, w/o val_string
		 */
		case FT_UINT8:
		case FT_UINT16:
		case FT_UINT24:
		case FT_UINT32:
			if (hfinfo->bitmask) {
				if (hfinfo->strings) {
					fill_label_enumerated_bitfield(fi, label_str);
				}
				else {
					fill_label_numeric_bitfield(fi, label_str);
				}
			}
			else {
				if (hfinfo->strings) {
					fill_label_enumerated_uint(fi, label_str);
				}
				else {
					fill_label_uint(fi, label_str);
				}
			}
			break;

		case FT_INT8:
		case FT_INT16:
		case FT_INT24:
		case FT_INT32:
			g_assert(!hfinfo->bitmask);
			if (hfinfo->strings) {
				fill_label_enumerated_int(fi, label_str);
			}
			else {
				fill_label_int(fi, label_str);
			}
			break;

		case FT_DOUBLE:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %g", fi->hfinfo->name,
				fi->value.floating);
			break;

		case FT_ABSOLUTE_TIME:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %s", fi->hfinfo->name,
				abs_time_to_str(&fi->value.time));
			break;

		case FT_RELATIVE_TIME:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %s seconds", fi->hfinfo->name,
				rel_time_to_str(&fi->value.time));
			break;

		case FT_IPXNET:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: 0x%08X (%s)", fi->hfinfo->name,
				fi->value.numeric, get_ipxnet_name(fi->value.numeric));
			break;

		case FT_ETHER:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %s (%s)", fi->hfinfo->name,
				ether_to_str(fi->value.ether),
				get_ether_name(fi->value.ether));
			break;

		case FT_IPv4:
			n_addr = ipv4_get_net_order_addr(&fi->value.ipv4);
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %s (%s)", fi->hfinfo->name,
				get_hostname(n_addr),
				ip_to_str((guint8*)&n_addr));
			break;

		case FT_IPv6:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %s (%s)", fi->hfinfo->name,
				get_hostname6((struct e_in6_addr *)fi->value.ipv6),
				ip6_to_str((struct e_in6_addr*)fi->value.ipv6));
			break;
	
		case FT_STRING:
			snprintf(label_str, ITEM_LABEL_LENGTH,
				"%s: %s", fi->hfinfo->name, fi->value.string);
			break;

		default:
			g_error("hfinfo->type %d not handled\n", fi->hfinfo->type);
			break;
	}
}

static void
fill_label_boolean(field_info *fi, gchar *label_str)
{
	char *p = label_str;
	int bitfield_byte_length = 0, bitwidth;
	guint32 unshifted_value;

	struct header_field_info	*hfinfo = fi->hfinfo;
	struct true_false_string	default_tf = { "True", "False" };
	struct true_false_string	*tfstring = &default_tf;

	if (hfinfo->strings) {
		tfstring = (struct true_false_string*) hfinfo->strings;
	}

	if (hfinfo->bitmask) {
		/* Figure out the bit width */
		bitwidth = hfinfo_bitwidth(hfinfo);

		/* Un-shift bits */
		unshifted_value = fi->value.numeric;
		if (hfinfo->bitshift > 0) {
			unshifted_value <<= hfinfo->bitshift;
		}

		/* Create the bitfield first */
		p = decode_bitfield_value(label_str, unshifted_value, hfinfo->bitmask, bitwidth);
		bitfield_byte_length = p - label_str;
	}

	/* Fill in the textual info */
	snprintf(p, ITEM_LABEL_LENGTH - bitfield_byte_length,
		"%s: %s",  hfinfo->name,
		fi->value.numeric ? tfstring->true_string : tfstring->false_string);
}


/* Fills data for bitfield ints with val_strings */
static void
fill_label_enumerated_bitfield(field_info *fi, gchar *label_str)
{
	char *format = NULL, *p;
	int bitfield_byte_length, bitwidth;
	guint32 unshifted_value;

	struct header_field_info	*hfinfo = fi->hfinfo;

	/* Figure out the bit width */
	bitwidth = hfinfo_bitwidth(hfinfo);

	/* Pick the proper format string */
	format = hfinfo_uint_vals_format(hfinfo);

	/* Un-shift bits */
	unshifted_value = fi->value.numeric;
	if (hfinfo->bitshift > 0) {
		unshifted_value <<= hfinfo->bitshift;
	}

	/* Create the bitfield first */
	p = decode_bitfield_value(label_str, unshifted_value, hfinfo->bitmask, bitwidth);
	bitfield_byte_length = p - label_str;

	/* Fill in the textual info using stored (shifted) value */
	snprintf(p, ITEM_LABEL_LENGTH - bitfield_byte_length,
			format,  hfinfo->name,
			val_to_str(fi->value.numeric, cVALS(hfinfo->strings), "Unknown"),
			fi->value.numeric);
}

static void
fill_label_numeric_bitfield(field_info *fi, gchar *label_str)
{
	char *format = NULL, *p;
	int bitfield_byte_length, bitwidth;
	guint32 unshifted_value;

	struct header_field_info	*hfinfo = fi->hfinfo;

	/* Figure out the bit width */
	bitwidth = hfinfo_bitwidth(hfinfo);

	/* Pick the proper format string */
	format = hfinfo_uint_format(hfinfo);

	/* Un-shift bits */
	unshifted_value = fi->value.numeric;
	if (hfinfo->bitshift > 0) {
		unshifted_value <<= hfinfo->bitshift;
	}

	/* Create the bitfield using */
	p = decode_bitfield_value(label_str, unshifted_value, hfinfo->bitmask, bitwidth);
	bitfield_byte_length = p - label_str;

	/* Fill in the textual info using stored (shifted) value */
	snprintf(p, ITEM_LABEL_LENGTH - bitfield_byte_length,
			format,  hfinfo->name, fi->value.numeric);
}

static void
fill_label_enumerated_uint(field_info *fi, gchar *label_str)
{
	char *format = NULL;
	struct header_field_info	*hfinfo = fi->hfinfo;

	/* Pick the proper format string */
	format = hfinfo_uint_vals_format(hfinfo);

	/* Fill in the textual info */
	snprintf(label_str, ITEM_LABEL_LENGTH,
			format,  hfinfo->name,
			val_to_str(fi->value.numeric, cVALS(hfinfo->strings), "Unknown"),
			fi->value.numeric);
}

static void
fill_label_uint(field_info *fi, gchar *label_str)
{
	char *format = NULL;
	struct header_field_info	*hfinfo = fi->hfinfo;

	/* Pick the proper format string */
	format = hfinfo_uint_format(hfinfo);

	/* Fill in the textual info */
	snprintf(label_str, ITEM_LABEL_LENGTH,
			format,  hfinfo->name, fi->value.numeric);
}

static void
fill_label_enumerated_int(field_info *fi, gchar *label_str)
{
	char *format = NULL;
	struct header_field_info	*hfinfo = fi->hfinfo;

	/* Pick the proper format string */
	format = hfinfo_int_vals_format(hfinfo);

	/* Fill in the textual info */
	snprintf(label_str, ITEM_LABEL_LENGTH,
			format,  hfinfo->name,
			val_to_str(fi->value.numeric, cVALS(hfinfo->strings), "Unknown"),
			fi->value.numeric);
}

static void
fill_label_int(field_info *fi, gchar *label_str)
{
	char *format = NULL;
	struct header_field_info	*hfinfo = fi->hfinfo;

	/* Pick the proper format string */
	format = hfinfo_int_format(hfinfo);

	/* Fill in the textual info */
	snprintf(label_str, ITEM_LABEL_LENGTH,
			format,  hfinfo->name, fi->value.numeric);
}

static int
hfinfo_bitwidth(header_field_info *hfinfo)
{
	int bitwidth = 0;

	if (!hfinfo->bitmask) {
		return 0;
	}

	switch(hfinfo->type) {
		case FT_UINT8:
		case FT_INT8:
			bitwidth = 8;
			break;
		case FT_UINT16:
		case FT_INT16:
			bitwidth = 16;
			break;
		case FT_UINT24:
		case FT_INT24:
			bitwidth = 24;
			break;
		case FT_UINT32:
		case FT_INT32:
			bitwidth = 32;
			break;
		case FT_BOOLEAN:
			bitwidth = hfinfo->display; /* hacky? :) */
			break;
		default:
			g_assert_not_reached();
			;
	}
	return bitwidth;
}

static char*
hfinfo_uint_vals_format(header_field_info *hfinfo)
{
	char *format = NULL;

	switch(hfinfo->display) {
		case BASE_DEC:
		case BASE_NONE:
		case BASE_OCT: /* I'm lazy */
		case BASE_BIN: /* I'm lazy */
			format = "%s: %s (%u)";
			break;
		case BASE_HEX:
			switch(hfinfo->type) {
				case FT_UINT8:
					format = "%s: %s (0x%02x)";
					break;
				case FT_UINT16:
					format = "%s: %s (0x%04x)";
					break;
				case FT_UINT24:
					format = "%s: %s (0x%06x)";
					break;
				case FT_UINT32:
					format = "%s: %s (0x%08x)";
					break;
				default:
					g_assert_not_reached();
					;
			}
			break;
		default:
			g_assert_not_reached();
			;
	}
	return format;
}

static char*
hfinfo_uint_format(header_field_info *hfinfo)
{
	char *format = NULL;

	/* Pick the proper format string */
	switch(hfinfo->display) {
		case BASE_DEC:
		case BASE_NONE:
		case BASE_OCT: /* I'm lazy */
		case BASE_BIN: /* I'm lazy */
			format = "%s: %u";
			break;
		case BASE_HEX:
			switch(hfinfo->type) {
				case FT_UINT8:
					format = "%s: 0x%02x";
					break;
				case FT_UINT16:
					format = "%s: 0x%04x";
					break;
				case FT_UINT24:
					format = "%s: 0x%06x";
					break;
				case FT_UINT32:
					format = "%s: 0x%08x";
					break;
				default:
					g_assert_not_reached();
					;
			}
			break;
		default:
			g_assert_not_reached();
			;
	}
	return format;
}

static char*
hfinfo_int_vals_format(header_field_info *hfinfo)
{
	char *format = NULL;

	switch(hfinfo->display) {
		case BASE_DEC:
		case BASE_NONE:
		case BASE_OCT: /* I'm lazy */
		case BASE_BIN: /* I'm lazy */
			format = "%s: %s (%d)";
			break;
		case BASE_HEX:
			switch(hfinfo->type) {
				case FT_INT8:
					format = "%s: %s (0x%02x)";
					break;
				case FT_INT16:
					format = "%s: %s (0x%04x)";
					break;
				case FT_INT24:
					format = "%s: %s (0x%06x)";
					break;
				case FT_INT32:
					format = "%s: %s (0x%08x)";
					break;
				default:
					g_assert_not_reached();
					;
			}
			break;
		default:
			g_assert_not_reached();
			;
	}
	return format;
}

static char*
hfinfo_int_format(header_field_info *hfinfo)
{
	char *format = NULL;

	/* Pick the proper format string */
	switch(hfinfo->display) {
		case BASE_DEC:
		case BASE_NONE:
		case BASE_OCT: /* I'm lazy */
		case BASE_BIN: /* I'm lazy */
			format = "%s: %d";
			break;
		case BASE_HEX:
			switch(hfinfo->type) {
				case FT_INT8:
					format = "%s: 0x%02x";
					break;
				case FT_INT16:
					format = "%s: 0x%04x";
					break;
				case FT_INT24:
					format = "%s: 0x%06x";
					break;
				case FT_INT32:
					format = "%s: 0x%08x";
					break;
				default:
					g_assert_not_reached();
					;
			}
			break;
		default:
			g_assert_not_reached();
			;
	}
	return format;
}



int
proto_registrar_n(void)
{
	return gpa_hfinfo->len;
}

char*
proto_registrar_get_name(int n)
{
    struct header_field_info *hfinfo;
    hfinfo = proto_registrar_get_nth(n);
    if (hfinfo)
        return hfinfo->name;
    else        return NULL;
}

char*
proto_registrar_get_abbrev(int n)
{
	struct header_field_info *hfinfo;

	hfinfo = proto_registrar_get_nth(n);
	if (hfinfo)
		return hfinfo->abbrev;
	else
		return NULL;
}

int
proto_registrar_get_ftype(int n)
{
	struct header_field_info *hfinfo;

	hfinfo = proto_registrar_get_nth(n);
	if (hfinfo)
		return hfinfo->type;
	else
		return -1;
}

int
proto_registrar_get_parent(int n)
{
	struct header_field_info *hfinfo;

	hfinfo = proto_registrar_get_nth(n);
	if (hfinfo)
		return hfinfo->parent;
	else
		return -2;
}

gboolean
proto_registrar_is_protocol(int n)
{
	struct header_field_info *hfinfo;

	hfinfo = proto_registrar_get_nth(n);
	if (hfinfo)
		return (hfinfo->parent == -1 ? TRUE : FALSE);
	else
		return FALSE;
}

/* Returns length of field in packet (not necessarily the length
 * in our internal representation, as in the case of IPv4).
 * 0 means undeterminable at time of registration
 * -1 means the field is not registered. */
gint
proto_registrar_get_length(int n)
{
	struct header_field_info *hfinfo;

	hfinfo = proto_registrar_get_nth(n);
	if (!hfinfo)
		return -1;

	switch (hfinfo->type) {
		case FT_TEXT_ONLY: /* not filterable */
		case NUM_FIELD_TYPES: /* satisfy picky compilers */
			return -1;

		case FT_NONE:
		case FT_BYTES:
		case FT_BOOLEAN:
		case FT_STRING:
		case FT_DOUBLE:
		case FT_ABSOLUTE_TIME:
		case FT_RELATIVE_TIME:
			return 0;

		case FT_UINT8:
		case FT_INT8:
			return 1;

		case FT_UINT16:
		case FT_INT16:
			return 2;

		case FT_UINT24:
		case FT_INT24:
			return 3;

		case FT_UINT32:
		case FT_INT32:
		case FT_IPXNET:
		case FT_IPv4:
			return 4;

		case FT_ETHER:
			return 6;

		case FT_IPv6:
			return 16;
	}
	g_assert_not_reached();
	return -1;
}

/* Looks for a protocol or a field in a proto_tree. Returns TRUE if
 * it exists anywhere, or FALSE if it exists nowhere. */
gboolean
proto_check_for_protocol_or_field(proto_tree* tree, int id)
{
	proto_tree_search_info	sinfo;

	sinfo.target = id;
	sinfo.result.node = NULL;
	sinfo.parent = -1;
	sinfo.traverse_func = NULL;

	/* do a quicker check if target is a protocol */
	if (proto_registrar_is_protocol(id) == TRUE) {
		proto_find_protocol_multi(tree, id, &check_for_protocol_or_field_id, &sinfo);
	}
	else {
		/* find the field's parent protocol */
		sinfo.parent = proto_registrar_get_parent(id);

		/* Go through each protocol subtree, checking if the protocols
		 * is the parent protocol of the field that we're looking for.
		 * We may have protocols that occur more than once (e.g., IP in IP),
		 * so we do indeed have to check all protocol subtrees, looking
		 * for the parent protocol. That's why proto_find_protocol()
		 * is not used --- it assumes a protocol occurs only once. */
		g_node_traverse((GNode*)tree, G_IN_ORDER, G_TRAVERSE_ALL, 2,
						check_for_field_within_protocol, &sinfo);
	}

	if (sinfo.result.node)
		return TRUE;
	else
		return FALSE;
}

static gboolean
check_for_protocol_or_field_id(GNode *node, gpointer data)
{
	field_info		*fi = (field_info*) (node->data);
	proto_tree_search_info	*sinfo = (proto_tree_search_info*) data;

	if (fi) { /* !fi == the top most container node which holds nothing */
		if (fi->hfinfo->id == sinfo->target) {
			sinfo->result.node = node;
			return TRUE; /* halt traversal */
		}
	}
	return FALSE; /* keep traversing */
}

static gboolean
check_for_field_within_protocol(GNode *node, gpointer data)
{
	field_info		*fi = (field_info*) (node->data);
	proto_tree_search_info	*sinfo = (proto_tree_search_info*) data;

	if (fi) { /* !fi == the top most container node which holds nothing */
		if (fi->hfinfo->id == sinfo->parent) {
			g_node_traverse(node, G_IN_ORDER, G_TRAVERSE_ALL, -1,
					check_for_protocol_or_field_id, sinfo);
			if (sinfo->result.node)
				return TRUE; /* halt traversal */
		}
	}
	return FALSE; /* keep traversing */
}

/* Looks for a protocol at the top layer of the tree. The protocol can occur
 * more than once, for those encapsulated protocols. For each protocol subtree
 * that is found, the callback function is called.
 */
void
proto_find_protocol_multi(proto_tree* tree, int target, GNodeTraverseFunc callback,
			proto_tree_search_info *sinfo)
{
	g_assert(callback != NULL);
	g_node_traverse((GNode*)tree, G_IN_ORDER, G_TRAVERSE_ALL, 2, callback, (gpointer*)sinfo);
}

/* Simple wrappter to traverse all nodes, calling the sinfo traverse function with sinfo as an arg */
gboolean
proto_get_field_values(proto_tree* subtree, proto_tree_search_info *sinfo)
{
	/* Don't try to check value of top-level NULL GNode */
	if (!((GNode*)subtree)->data) {
		return FALSE; /* don't halt */
	}
	g_node_traverse((GNode*)subtree, G_IN_ORDER, G_TRAVERSE_ALL, -1, sinfo->traverse_func, (gpointer*)sinfo);
	return FALSE; /* don't halt */
}

/* Dumps the contents of the registration database to stdout. An indepedent program can take
 * this output and format it into nice tables or HTML or whatever.
 *
 * There is one record per line. Each record is either a protocol or a header
 * field, differentiated by the first field. The fields are tab-delimited.
 *
 * Protocols
 * ---------
 * Field 1 = 'P'
 * Field 2 = protocol name
 * Field 3 = protocol abbreviation
 *
 * Header Fields
 * -------------
 * Field 1 = 'F'
 * Field 2 = field name
 * Field 3 = field abbreviation
 * Field 4 = type ( textual representation of the the ftenum type )
 * Field 5 = parent protocol abbreviation
 */
void
proto_registrar_dump(void)
{
	header_field_info	*hfinfo, *parent_hfinfo;
	int			i, len;
	const char 		*enum_name;

	len = gpa_hfinfo->len;
	for (i = 0; i < len ; i++) {
		hfinfo = proto_registrar_get_nth(i);

		/* format for protocols */
		if (proto_registrar_is_protocol(i)) {
			printf("P\t%s\t%s\n", hfinfo->name, hfinfo->abbrev);
		}
		/* format for header fields */
		else {
			parent_hfinfo = proto_registrar_get_nth(hfinfo->parent);
			g_assert(parent_hfinfo);

			switch(hfinfo->type) {
			case FT_NONE:
				enum_name = "FT_NONE";
				break;
			case FT_BOOLEAN:
				enum_name = "FT_BOOLEAN";
				break;
			case FT_UINT8:
				enum_name = "FT_UINT8";
				break;
			case FT_UINT16:
				enum_name = "FT_UINT16";
				break;
			case FT_UINT24:
				enum_name = "FT_UINT24";
				break;
			case FT_UINT32:
				enum_name = "FT_UINT32";
				break;
			case FT_INT8:
				enum_name = "FT_INT8";
				break;
			case FT_INT16:
				enum_name = "FT_INT16";
				break;
			case FT_INT24:
				enum_name = "FT_INT24";
				break;
			case FT_INT32:
				enum_name = "FT_INT32";
				break;
			case FT_DOUBLE:
				enum_name = "FT_DOUBLE";
				break;
			case FT_ABSOLUTE_TIME:
				enum_name = "FT_ABSOLUTE_TIME";
				break;
			case FT_RELATIVE_TIME:
				enum_name = "FT_RELATIVE_TIME";
				break;
			case FT_STRING:
				enum_name = "FT_STRING";
				break;
			case FT_ETHER:
				enum_name = "FT_ETHER";
				break;
			case FT_BYTES:
				enum_name = "FT_BYTES";
				break;
			case FT_IPv4:
				enum_name = "FT_IPv4";
				break;
			case FT_IPv6:
				enum_name = "FT_IPv6";
				break;
			case FT_IPXNET:
				enum_name = "FT_IPXNET";
				break;
			case FT_TEXT_ONLY:
				enum_name = "FT_TEXT_ONLY";
				break;
			default:
				enum_name = "UNKNOWN";
				break;
			}
			printf("F\t%s\t%s\t%s\t%s\n", hfinfo->name, hfinfo->abbrev,
				enum_name,parent_hfinfo->abbrev);
		}
	}
}
