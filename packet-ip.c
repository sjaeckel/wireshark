/* packet-ip.c
 * Routines for IP and miscellaneous IP protocol packet disassembly
 *
 * $Id: packet-ip.c,v 1.8 1998/10/16 01:18:31 gerald Exp $
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

#include <gtk/gtk.h>
#include <pcap.h>

#include <stdio.h>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#include "ethereal.h"
#include "packet.h"
#include "etypes.h"
#include "resolv.h"

extern packet_info pi;

static void
dissect_ipopt_security(GtkWidget *opt_tree, const char *name,
    const u_char *opd, int offset, guint optlen)
{
  GtkWidget *field_tree = NULL, *tf;
  guint      val;
  gchar     *secl_str;
  static value_string secl_vals[] = {
    {IPSEC_UNCLASSIFIED, "Unclassified"},
    {IPSEC_CONFIDENTIAL, "Confidential"},
    {IPSEC_EFTO,         "EFTO"        },
    {IPSEC_MMMM,         "MMMM"        },
    {IPSEC_RESTRICTED,   "Restricted"  },
    {IPSEC_SECRET,       "Secret"      },
    {IPSEC_TOPSECRET,    "Top secret"  },
    {IPSEC_RESERVED1,    "Reserved"    },
    {IPSEC_RESERVED2,    "Reserved"    },
    {IPSEC_RESERVED3,    "Reserved"    },
    {IPSEC_RESERVED4,    "Reserved"    },
    {IPSEC_RESERVED5,    "Reserved"    },
    {IPSEC_RESERVED6,    "Reserved"    },
    {IPSEC_RESERVED7,    "Reserved"    },
    {IPSEC_RESERVED8,    "Reserved"    },
    {0,                  NULL          } };

  tf = add_item_to_tree(opt_tree, offset,      optlen, "%s:", name);
  field_tree = gtk_tree_new();
  add_subtree(tf, field_tree, ETT_IP_OPTION_SEC);
  offset += 2;

  val = pntohs(opd);
  if ((secl_str = match_strval(val, secl_vals)))
    add_item_to_tree(field_tree, offset,       2,
              "Security: %s", secl_str);
  else
    add_item_to_tree(field_tree, offset,       2,
              "Security: Unknown (0x%x)", val);
  offset += 2;
  opd += 2;

  val = pntohs(opd);
  add_item_to_tree(field_tree, offset,         2,
              "Compartments: %d", val);
  offset += 2;
  opd += 2;

  add_item_to_tree(field_tree, offset,         2,
              "Handling restrictions: %c%c", opd[0], opd[1]);
  offset += 2;
  opd += 2;

  add_item_to_tree(field_tree, offset,         3,
              "Transmission control code: %c%c%c", opd[0], opd[1], opd[2]);
}

static void
dissect_ipopt_route(GtkWidget *opt_tree, const char *name,
    const u_char *opd, int offset, guint optlen)
{
  GtkWidget *field_tree = NULL, *tf;
  int ptr;
  int optoffset = 0;
  struct in_addr addr;

  tf = add_item_to_tree(opt_tree, offset,      optlen, "%s (%d bytes)", name,
              optlen);
  field_tree = gtk_tree_new();
  add_subtree(tf, field_tree, ETT_IP_OPTION_ROUTE);

  optoffset += 2;	/* skip past type and length */
  optlen -= 2;		/* subtract size of type and length */

  ptr = *opd;
  add_item_to_tree(field_tree, offset + optoffset, 1,
              "Pointer: %d%s", ptr,
              ((ptr < 4) ? " (points before first address)" :
               ((ptr & 3) ? " (points to middle of address)" : "")));
  optoffset++;
  opd++;
  optlen--;
  ptr--;	/* ptr is 1-origin */

  while (optlen > 0) {
    if (optlen < 4) {
      add_item_to_tree(field_tree, offset,      optlen,
        "(suboption would go past end of option)");
      break;
    }

    /* Avoids alignment problems on many architectures. */
    memcpy((char *)&addr, (char *)opd, sizeof(addr));

    add_item_to_tree(field_tree, offset + optoffset, 4,
              "%s%s",
              ((addr.s_addr == 0) ? "-" : (char *)get_hostname(addr.s_addr)),
              ((optoffset == ptr) ? " <- (current)" : ""));
    optoffset += 4;
    opd += 4;
    optlen -= 4;
  }
}

static void
dissect_ipopt_sid(GtkWidget *opt_tree, const char *name, const u_char *opd,
    int offset, guint optlen)
{
  add_item_to_tree(opt_tree, offset,      optlen,
    "%s: %d", name, pntohs(opd));
  return;
}

static void
dissect_ipopt_timestamp(GtkWidget *opt_tree, const char *name, const u_char *opd,
    int offset, guint optlen)
{
  GtkWidget *field_tree = NULL, *tf;
  int        ptr;
  int        optoffset = 0;
  int        flg;
  gchar     *flg_str;
  static value_string flag_vals[] = {
    {IPOPT_TS_TSONLY,    "Time stamps only"                      },
    {IPOPT_TS_TSANDADDR, "Time stamp and address"                },
    {IPOPT_TS_PRESPEC,   "Time stamps for prespecified addresses"},
    {0,                  NULL                                    } };

  struct in_addr addr;
  guint ts;

  tf = add_item_to_tree(opt_tree, offset,      optlen, "%s:", name);
  field_tree = gtk_tree_new();
  add_subtree(tf, field_tree, ETT_IP_OPTION_TIMESTAMP);

  optoffset += 2;	/* skip past type and length */
  optlen -= 2;		/* subtract size of type and length */

  ptr = *opd;
  add_item_to_tree(field_tree, offset + optoffset, 1,
              "Pointer: %d%s", ptr,
              ((ptr < 5) ? " (points before first address)" :
               (((ptr - 1) & 3) ? " (points to middle of address)" : "")));
  optoffset++;
  opd++;
  optlen--;
  ptr--;	/* ptr is 1-origin */

  flg = *opd;
  add_item_to_tree(field_tree, offset + optoffset,   1,
        "Overflow: %d", flg >> 4);
  flg &= 0xF;
  if ((flg_str = match_strval(flg, flag_vals)))
    add_item_to_tree(field_tree, offset + optoffset, 1,
        "Flag: %s", flg_str);
  else
    add_item_to_tree(field_tree, offset + optoffset, 1,
        "Flag: Unknown (0x%x)", flg);
  optoffset++;
  opd++;
  optlen--;

  while (optlen > 0) {
    if (flg == IPOPT_TS_TSANDADDR) {
      if (optlen < 4) {
        add_item_to_tree(field_tree, offset + optoffset, optlen,
          "(suboption would go past end of option)");
        break;
      }
      /* XXX - check whether it goes past end of packet */
      ts = pntohl(opd);
      opd += 4;
      optlen -= 4;
      if (optlen < 4) {
        add_item_to_tree(field_tree, offset + optoffset, optlen,
          "(suboption would go past end of option)");
        break;
      }
      /* XXX - check whether it goes past end of packet */
      memcpy((char *)&addr, (char *)opd, sizeof(addr));
      opd += 4;
      optlen -= 4;
      add_item_to_tree(field_tree, offset,      8,
          "Address = %s, time stamp = %u",
          ((addr.s_addr == 0) ? "-" :  (char *)get_hostname(addr.s_addr)),
          ts);
      optoffset += 8;
    } else {
      if (optlen < 4) {
        add_item_to_tree(field_tree, offset + optoffset, optlen,
          "(suboption would go past end of option)");
        break;
      }
      /* XXX - check whether it goes past end of packet */
      ts = pntohl(opd);
      opd += 4;
      optlen -= 4;
      add_item_to_tree(field_tree, offset + optoffset, 4,
          "Time stamp = %u", ts);
      optoffset += 4;
    }
  }
}

static ip_tcp_opt ipopts[] = {
  {
    IPOPT_END,
    "EOL",
    NO_LENGTH,
    0,
    NULL,
  },
  {
    IPOPT_NOOP,
    "NOP",
    NO_LENGTH,
    0,
    NULL,
  },
  {
    IPOPT_SEC,
    "Security",
    FIXED_LENGTH,
    IPOLEN_SEC,
    dissect_ipopt_security
  },
  {
    IPOPT_SSRR,
    "Strict source route",
    VARIABLE_LENGTH,
    IPOLEN_SSRR_MIN,
    dissect_ipopt_route
  },
  {
    IPOPT_LSRR,
    "Loose source route",
    VARIABLE_LENGTH,
    IPOLEN_LSRR_MIN,
    dissect_ipopt_route
  },
  {
    IPOPT_RR,
    "Record route",
    VARIABLE_LENGTH,
    IPOLEN_RR_MIN,
    dissect_ipopt_route
  },
  {
    IPOPT_SID,
    "Stream identifier",
    FIXED_LENGTH,
    IPOLEN_SID,
    dissect_ipopt_sid
  },
  {
    IPOPT_TIMESTAMP,
    "Time stamp",
    VARIABLE_LENGTH,
    IPOLEN_TIMESTAMP_MIN,
    dissect_ipopt_timestamp
  }
};

#define N_IP_OPTS	(sizeof ipopts / sizeof ipopts[0])

/* Dissect the IP or TCP options in a packet. */
void
dissect_ip_tcp_options(GtkWidget *opt_tree, const u_char *opd, int offset,
    guint length, ip_tcp_opt *opttab, int nopts, int eol)
{
  u_char      opt;
  ip_tcp_opt *optp;
  guint       len;

  while (length > 0) {
    opt = *opd++;
    for (optp = &opttab[0]; optp < &opttab[nopts]; optp++) {
      if (optp->optcode == opt)
        break;
    }
    if (optp == &opttab[nopts]) {
      add_item_to_tree(opt_tree, offset,      1, "Unknown");
      /* We don't know how long this option is, so we don't know how much
         of it to skip, so we just bail. */
      return;
    }
    --length;      /* account for type byte */
    if (optp->len_type != NO_LENGTH) {
      /* Option has a length. Is it in the packet? */
      if (length == 0) {
        /* Bogus - packet must at least include option code byte and
           length byte! */
        add_item_to_tree(opt_tree, offset,      1,
              "%s (length byte past end of header)", optp->name);
        return;
      }
      len = *opd++;  /* total including type, len */
      --length;    /* account for length byte */
      if (len < 2) {
        /* Bogus - option length is too short to include option code and
           option length. */
        add_item_to_tree(opt_tree, offset,      2,
              "%s (with too-short option length = %u bytes)", optp->name, 2);
        return;
      } else if (len - 2 > length) {
        /* Bogus - option goes past the end of the header. */
        add_item_to_tree(opt_tree, offset,      length,
              "%s (option goes past end of header)", optp->name);
        return;
      } else if (optp->len_type == FIXED_LENGTH && len != optp->optlen) {
        /* Bogus - option length isn't what it's supposed to be for this
           option. */
        add_item_to_tree(opt_tree, offset,      len,
              "%s (with option length = %u bytes; should be %u)", optp->name,
              len, optp->optlen);
        return;
      } else if (optp->len_type == VARIABLE_LENGTH && len < optp->optlen) {
        /* Bogus - option length is less than what it's supposed to be for
           this option. */
        add_item_to_tree(opt_tree, offset,      len,
              "%s (with option length = %u bytes; should be >= %u)", optp->name,
              len, optp->optlen);
        return;
      } else {
        if (optp->dissect != NULL) {
          /* Option has a dissector. */
          (*optp->dissect)(opt_tree, optp->name, opd, offset, len);
        } else {
          /* Option has no data, hence no dissector. */
          add_item_to_tree(opt_tree, offset,      len, "%s", optp->name);
        }
        len -= 2;	/* subtract size of type and length */
        offset += 2 + len;
      }
      opd += len;
      length -= len;
    } else {
      add_item_to_tree(opt_tree, offset,      1, "%s", optp->name);
      offset += 1;
    }
    if (opt == eol)
      break;
  }
}

void
dissect_ip(const u_char *pd, int offset, frame_data *fd, GtkTree *tree) {
  e_ip       iph;
  GtkWidget *ip_tree, *ti, *field_tree, *tf;
  gchar      tos_str[32];
  guint      hlen, optlen;
  gchar     *proto_str;
  static value_string proto_vals[] = { {IP_PROTO_ICMP, "ICMP"},
                                       {IP_PROTO_IGMP, "IGMP"},
                                       {IP_PROTO_TCP,  "TCP" },
                                       {IP_PROTO_UDP,  "UDP" },
                                       {IP_PROTO_OSPF, "OSPF"},
                                       {0,             NULL  } };


  /* To do: check for runts, errs, etc. */
  /* Avoids alignment problems on many architectures. */
  memcpy(&iph, &pd[offset], sizeof(e_ip));
  iph.ip_len = ntohs(iph.ip_len);
  iph.ip_id  = ntohs(iph.ip_id);
  iph.ip_off = ntohs(iph.ip_off);
  iph.ip_sum = ntohs(iph.ip_sum);

  hlen = iph.ip_hl * 4;	/* IP header length, in bytes */
  
  if (fd->win_info[COL_NUM]) {
    switch (iph.ip_p) {
      case IP_PROTO_ICMP:
      case IP_PROTO_IGMP:
      case IP_PROTO_TCP:
      case IP_PROTO_UDP:
      case IP_PROTO_OSPF:
        /* Names are set in the associated dissect_* routines */
        break;
      default:
        strcpy(fd->win_info[COL_PROTOCOL], "IP");
        sprintf(fd->win_info[COL_INFO], "Unknown IP protocol (%02x)", iph.ip_p);
    }

    strcpy(fd->win_info[COL_SOURCE], get_hostname(iph.ip_src));
    strcpy(fd->win_info[COL_DESTINATION], get_hostname(iph.ip_dst));
  }
  
  iph.ip_tos = IPTOS_TOS(iph.ip_tos);
  switch (iph.ip_tos) {
    case IPTOS_NONE:
      strcpy(tos_str, "None");
      break;
    case IPTOS_LOWDELAY:
      strcpy(tos_str, "Minimize delay");
      break;
    case IPTOS_THROUGHPUT:
      strcpy(tos_str, "Maximize throughput");
      break;
    case IPTOS_RELIABILITY:
      strcpy(tos_str, "Maximize reliability");
      break;
    case IPTOS_LOWCOST:
      strcpy(tos_str, "Minimize cost");
      break;
    default:
      strcpy(tos_str, "Unknown.  Malformed?");
      break;
  }
  
  if (tree) {
    ti = add_item_to_tree(GTK_WIDGET(tree), offset, hlen, "Internet Protocol");
    ip_tree = gtk_tree_new();
    add_subtree(ti, ip_tree, ETT_IP);
    add_item_to_tree(ip_tree, offset,      1, "Version: %d", iph.ip_v);
    add_item_to_tree(ip_tree, offset,      1, "Header length: %d bytes", hlen); 
    add_item_to_tree(ip_tree, offset +  1, 1, "Type of service: 0x%02x (%s)",
      iph.ip_tos, tos_str);
    add_item_to_tree(ip_tree, offset +  2, 2, "Total length: %d", iph.ip_len);
    add_item_to_tree(ip_tree, offset +  4, 2, "Identification: 0x%04x",
      iph.ip_id);
    /* To do: add flags */
    add_item_to_tree(ip_tree, offset +  6, 2, "Fragment offset: %d",
      iph.ip_off & IP_OFFSET);
    add_item_to_tree(ip_tree, offset +  8, 1, "Time to live: %d",
      iph.ip_ttl);
    if ((proto_str = match_strval(iph.ip_p, proto_vals)))
      add_item_to_tree(ip_tree, offset +  9, 1, "Protocol: %s", proto_str);
    else
      add_item_to_tree(ip_tree, offset +  9, 1, "Protocol: Unknown (%x)",
        iph.ip_p);
    add_item_to_tree(ip_tree, offset + 10, 2, "Header checksum: 0x%04x",
      iph.ip_sum);
    add_item_to_tree(ip_tree, offset + 12, 4, "Source address: %s",
		     get_hostname(iph.ip_src));
    add_item_to_tree(ip_tree, offset + 16, 4, "Destination address: %s",
		     get_hostname(iph.ip_dst));

    /* Decode IP options, if any. */
    if (hlen > sizeof (e_ip)) {
      /* There's more than just the fixed-length header.  Decode the
         options. */
      optlen = hlen - sizeof (e_ip);	/* length of options, in bytes */
      tf = add_item_to_tree(ip_tree, offset +  20, optlen,
        "Options: (%d bytes)", optlen);
      field_tree = gtk_tree_new();
      add_subtree(tf, field_tree, ETT_IP_OPTIONS);
      dissect_ip_tcp_options(field_tree, &pd[offset + 20], offset + 20, optlen,
         ipopts, N_IP_OPTS, IPOPT_END);
    }
  }

  pi.srcip = ip_to_str( (guint8 *) &iph.ip_src);
  pi.destip = ip_to_str( (guint8 *) &iph.ip_dst);
  pi.ipproto = iph.ip_p;
  pi.iplen = iph.ip_len;
  pi.iphdrlen = iph.ip_hl;
  pi.ip_src = iph.ip_src;

  offset += hlen;
  switch (iph.ip_p) {
    case IP_PROTO_ICMP:
      dissect_icmp(pd, offset, fd, tree);
     break;
    case IP_PROTO_IGMP:
      dissect_igmp(pd, offset, fd, tree);
     break;
    case IP_PROTO_TCP:
      dissect_tcp(pd, offset, fd, tree);
     break;
   case IP_PROTO_UDP:
      dissect_udp(pd, offset, fd, tree);
      break;
    case IP_PROTO_OSPF:
      dissect_ospf(pd, offset, fd, tree);
     break;
  }
}


const gchar *unreach_str[] = {"Network unreachable",
                              "Host unreachable",
                              "Protocol unreachable",
                              "Port unreachable",
                              "Fragmentation needed",
                              "Source route failed",
                              "Administratively prohibited",
                              "Network unreachable for TOS",
                              "Host unreachable for TOS",
                              "Communication administratively filtered",
                              "Host precedence violation",
                              "Precedence cutoff in effect"};

const gchar *redir_str[] = {"Redirect for network",
                            "Redirect for host",
                            "Redirect for TOS and network",
                            "Redirect for TOS and host"};

const gchar *ttl_str[] = {"TTL equals 0 during transit",
                          "TTL equals 0 during reassembly"};

const gchar *par_str[] = {"IP header bad", "Required option missing"};

void
dissect_icmp(const u_char *pd, int offset, frame_data *fd, GtkTree *tree) {
  e_icmp     ih;
  GtkWidget *icmp_tree, *ti;
  guint16    cksum;
  gchar      type_str[64], code_str[64] = "";

  /* Avoids alignment problems on many architectures. */
  memcpy(&ih, &pd[offset], sizeof(e_icmp));
  /* To do: check for runts, errs, etc. */
  cksum = ntohs(ih.icmp_cksum);
  
  switch (ih.icmp_type) {
    case ICMP_ECHOREPLY:
      strcpy(type_str, "Echo (ping) reply");
      break;
    case ICMP_UNREACH:
      strcpy(type_str, "Destination unreachable");
      if (ih.icmp_code < 12) {
        sprintf(code_str, "(%s)", unreach_str[ih.icmp_code]);
      } else {
        strcpy(code_str, "(Unknown - error?)");
      }
      break;
    case ICMP_SOURCEQUENCH:
      strcpy(type_str, "Source quench (flow control)");
      break;
    case ICMP_REDIRECT:
      strcpy(type_str, "Redirect");
      if (ih.icmp_code < 4) {
        sprintf(code_str, "(%s)", redir_str[ih.icmp_code]);
      } else {
        strcpy(code_str, "(Unknown - error?)");
      }
      break;
    case ICMP_ECHO:
      strcpy(type_str, "Echo (ping) request");
      break;
    case ICMP_TIMXCEED:
      strcpy(type_str, "Time-to-live exceeded");
      if (ih.icmp_code < 2) {
        sprintf(code_str, "(%s)", ttl_str[ih.icmp_code]);
      } else {
        strcpy(code_str, "(Unknown - error?)");
      }
      break;
    case ICMP_PARAMPROB:
      strcpy(type_str, "Parameter problem");
      if (ih.icmp_code < 2) {
        sprintf(code_str, "(%s)", par_str[ih.icmp_code]);
      } else {
        strcpy(code_str, "(Unknown - error?)");
      }
      break;
    case ICMP_TSTAMP:
      strcpy(type_str, "Timestamp request");
      break;
    case ICMP_TSTAMPREPLY:
      strcpy(type_str, "Timestamp reply");
      break;
    case ICMP_MASKREQ:
      strcpy(type_str, "Address mask request");
      break;
    case ICMP_MASKREPLY:
      strcpy(type_str, "Address mask reply");
      break;
    default:
      strcpy(type_str, "Unknown ICMP (obsolete or malformed?)");
  }

  if (fd->win_info[COL_NUM]) {    
    strcpy(fd->win_info[COL_PROTOCOL], "ICMP");
    strcpy(fd->win_info[COL_INFO], type_str);
  }
  
  if (tree) {
    ti = add_item_to_tree(GTK_WIDGET(tree), offset, 4,
      "Internet Control Message Protocol");
    icmp_tree = gtk_tree_new();
    add_subtree(ti, icmp_tree, ETT_ICMP);
    add_item_to_tree(icmp_tree, offset,      1, "Type: %d (%s)",
      ih.icmp_type, type_str);
    add_item_to_tree(icmp_tree, offset +  1, 1, "Code: %d %s",
      ih.icmp_code, code_str);
    add_item_to_tree(icmp_tree, offset +  2, 2, "Checksum: 0x%04x",
      ih.icmp_cksum);
  }
}

void
dissect_igmp(const u_char *pd, int offset, frame_data *fd, GtkTree *tree) {
  e_igmp     ih;
  GtkWidget *igmp_tree, *ti;
  guint16    cksum;
  gchar      type_str[64] = "";

  /* Avoids alignment problems on many architectures. */
  memcpy(&ih, &pd[offset], sizeof(e_igmp));
  /* To do: check for runts, errs, etc. */
  cksum = ntohs(ih.igmp_cksum);
  
  switch (ih.igmp_t) {
    case IGMP_M_QRY:
      strcpy(type_str, "Router query");
      break;
    case IGMP_V1_M_RPT:
      strcpy(type_str, "Host response (v1)");
      break;
    case IGMP_V2_LV_GRP:
      strcpy(type_str, "Leave group (v2)");
      break;
    case IGMP_DVMRP:
      strcpy(type_str, "DVMRP");
      break;
    case IGMP_PIM:
      strcpy(type_str, "PIM");
      break;
    case IGMP_V2_M_RPT:
      strcpy(type_str, "Host reponse (v2)");
      break;
    case IGMP_MTRC_RESP:
      strcpy(type_str, "Traceroute response");
      break;
    case IGMP_MTRC:
      strcpy(type_str, "Traceroute message");
      break;
    default:
      strcpy(type_str, "Unknown IGMP");
  }

  if (fd->win_info[COL_NUM]) {    
    strcpy(fd->win_info[COL_PROTOCOL], "IGMP");
  }
  
  if (tree) {
    ti = add_item_to_tree(GTK_WIDGET(tree), offset, 4,
      "Internet Group Management Protocol");
    igmp_tree = gtk_tree_new();
    add_subtree(ti, igmp_tree, ETT_IGMP);
    add_item_to_tree(igmp_tree, offset,     1, "Version: %d",
      ih.igmp_v);
    add_item_to_tree(igmp_tree, offset    , 1, "Type: %d (%s)",
      ih.igmp_t, type_str);
    add_item_to_tree(igmp_tree, offset + 1, 1, "Unused: 0x%02x",
      ih.igmp_unused);
    add_item_to_tree(igmp_tree, offset + 2, 2, "Checksum: 0x%04x",
      ih.igmp_cksum);
    add_item_to_tree(igmp_tree, offset + 4, 4, "Group address: %s",
      ip_to_str((guint8 *) &ih.igmp_gaddr));
  }
}
