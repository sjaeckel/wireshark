/* voip_calls.c
 * VoIP calls summary addition for ethereal
 *
 * $Id$
 *
 * Copyright 2004, Ericsson, Spain
 * By Francisco Alcoba <francisco.alcoba@ericsson.com>
 *
 * based on h323_calls.c
 * Copyright 2004, Iskratel, Ltd, Kranj
 * By Miha Jemec <m.jemec@iskratel.si>
 * 
 * H323, RTP and Graph Support
 * By Alejandro Vaquero, alejandro.vaquero@verso.com
 * Copyright 2005, Verso Technologies Inc.
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation,	Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "graph_analysis.h"
#include "voip_calls.h"
#include "voip_calls_dlg.h"
#include "rtp_stream.h"

#include "globals.h"

#include <epan/tap.h>
#include <epan/dissectors/packet-sip.h>
#include <epan/dissectors/packet-mtp3.h>
#include <epan/dissectors/packet-isup.h>
#include <epan/dissectors/packet-h225.h>
#include <epan/dissectors/packet-h245.h>
#include <epan/dissectors/packet-q931.h>
#include <epan/dissectors/packet-sdp.h>
#include <epan/dissectors/packet-rtp.h>
#include "rtp_pt.h"

#include "alert_box.h"
#include "simple_dialog.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <string.h>


char *voip_call_state_name[6]={
	"CALL SETUP",
	"IN CALL",
	"CANCELLED",
	"COMPLETED",
	"REJECTED",
	"UNKNOWN"
	};

/* defines whether we can consider the call active */
char *voip_protocol_name[3]={
	"SIP",
	"ISUP",
	"H323"
	};



/****************************************************************************/
/* the one and only global voip_calls_tapinfo_t structure */
static voip_calls_tapinfo_t the_tapinfo_struct =
	{0, NULL, 0, NULL, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0};

/* static voip_calls_tapinfo_t the_tapinfo_struct;
*/

/****************************************************************************/
/* when there is a [re]reading of packet's */
void voip_calls_reset(voip_calls_tapinfo_t *tapinfo)
{
	voip_calls_info_t *strinfo;
	sip_calls_info_t *tmp_sipinfo;
	h323_calls_info_t *tmp_h323info;

	graph_analysis_item_t *graph_item;

	GList* list;
	GList* list2;

	/* free the data items first */
	list = g_list_first(tapinfo->strinfo_list);
	while (list)
	{
		strinfo = list->data;
		g_free(strinfo->from_identity);
		g_free(strinfo->to_identity);
		if (strinfo->protocol == VOIP_SIP){
			tmp_sipinfo = strinfo->prot_info;
			g_free(tmp_sipinfo->call_identifier);
		}
		if (strinfo->protocol == VOIP_H323){
			tmp_h323info = strinfo->prot_info;
			g_free(tmp_h323info->guid);
			/* free the H245 list address */
			list2 = g_list_first(tmp_h323info->h245_list);
			while (list2)
			{
				g_free(list2->data);
				list2 = g_list_next(list2);
			}
			g_list_free(tmp_h323info->h245_list);
			tmp_h323info->h245_list = NULL;
		}
		g_free(strinfo->prot_info);

		g_free(list->data);
		list = g_list_next(list);
	}
	g_list_free(tapinfo->strinfo_list);
	tapinfo->strinfo_list = NULL;
	tapinfo->ncalls = 0;
	tapinfo->npackets = 0;
	tapinfo->start_packets = 0;
    tapinfo->completed_calls = 0;
    tapinfo->rejected_calls = 0;

	/* free the graph data items first */
	list = g_list_first(tapinfo->graph_analysis->list);
	while (list)
	{
		graph_item = list->data;
		g_free(graph_item->frame_label);
		g_free(graph_item->comment);
		g_free(list->data);
		list = g_list_next(list);
	}
	g_list_free(tapinfo->graph_analysis->list);
	tapinfo->graph_analysis->nconv = 0;
	tapinfo->graph_analysis->list = NULL;

	++(tapinfo->launch_count);

	return;
}

/****************************************************************************/
void graph_analysis_data_init(){
	the_tapinfo_struct.graph_analysis = g_malloc(sizeof(graph_analysis_info_t));
	the_tapinfo_struct.graph_analysis->nconv = 0;
	the_tapinfo_struct.graph_analysis->list = NULL;

}

/****************************************************************************/
/* Add the RTP streams into the graph data */
/** 
 * TODO: 
 * - reimplement this function
 * This function does not work with VoIP taps
 * which keep the window up to date.
 * Either make rtp_stream() work without redissection or
 * add an additional tap listener for rtp to this file.
 * The second solution is cleaner and probably easier to implement, 
 * leaving the current rtp analysis feature untouched.
 */
void add_rtp_streams_graph()
{
	rtp_stream_info_t *strinfo;
	GList *strinfo_list;
	guint nfound;
	guint item;
	GList* voip_calls_graph_list;
	graph_analysis_item_t *gai;
	graph_analysis_item_t *new_gai;
	guint16 conv_num;
	guint32 duration;

	/* Scan for rtpstream */
	rtpstream_scan();

	/* assigne the RTP streams to calls */
	nfound = 0;
	strinfo_list = g_list_first(rtpstream_get_info()->strinfo_list);
	while (strinfo_list)
	{
		strinfo = (rtp_stream_info_t*)(strinfo_list->data);

		/* look in the voip calls graph list if there is a match for this RTP stream */
		voip_calls_graph_list = g_list_first(the_tapinfo_struct.graph_analysis->list);
		item = 0;
		while (voip_calls_graph_list)
		{			
			gai = voip_calls_graph_list->data;
			conv_num = gai->conv_num;
			if (strinfo->setup_frame_number == gai->frame_num){
				while(voip_calls_graph_list){
					gai = voip_calls_graph_list->data;
					/* add the RTP item to the graph */
					if (strinfo->first_frame_num<gai->frame_num){
						new_gai = g_malloc(sizeof(graph_analysis_item_t));
						new_gai->frame_num = strinfo->first_frame_num;
						new_gai->time = (double)strinfo->start_rel_sec + (double)strinfo->start_rel_usec/1000000;
						g_memmove(&new_gai->ip_src, strinfo->src_addr.data, 4);
						g_memmove(&new_gai->ip_dst, strinfo->dest_addr.data, 4);
						new_gai->port_src = strinfo->src_port;
						new_gai->port_dst = strinfo->dest_port;
						duration = (strinfo->stop_rel_sec*1000000 + strinfo->stop_rel_usec) - (strinfo->start_rel_sec*1000000 + strinfo->start_rel_usec);
						new_gai->frame_label = g_strdup_printf("RTP (%s)", val_to_str(strinfo->pt, rtp_payload_type_short_vals, "%u"));
						new_gai->comment = g_strdup_printf("RTP Num packets:%d  Duration:%d.%03ds ssrc:%d", strinfo->npackets, duration/1000000,(duration%1000000)/1000, strinfo->ssrc);
						new_gai->conv_num = conv_num;
						new_gai->display=FALSE;
						new_gai->line_style = 2;  /* the arrow line will be 2 pixels width */
						the_tapinfo_struct.graph_analysis->list = g_list_insert(the_tapinfo_struct.graph_analysis->list, new_gai, item);
						
						break;
					}
					voip_calls_graph_list = g_list_next(voip_calls_graph_list);
					item++;
				}
				break;
			}
			voip_calls_graph_list = g_list_next(voip_calls_graph_list);
			item++;
		}
		strinfo_list = g_list_next(strinfo_list);
	}
}

/****************************************************************************/
/* Add a new item into the graph */
int add_to_graph(voip_calls_tapinfo_t *tapinfo _U_, packet_info *pinfo, gchar *frame_label, gchar *comment, guint16 call_num)
{
	graph_analysis_item_t *gai;

	gai = g_malloc(sizeof(graph_analysis_item_t));
	gai->frame_num = pinfo->fd->num;
	gai->time= (double)pinfo->fd->rel_secs + (double) pinfo->fd->rel_usecs/1000000;
	g_memmove(&gai->ip_src, pinfo->src.data, 4);
	g_memmove(&gai->ip_dst, pinfo->dst.data, 4);
	gai->port_src=pinfo->srcport;
	gai->port_dst=pinfo->destport;
	if (frame_label != NULL)
		gai->frame_label = g_strdup(frame_label);
	else
		gai->frame_label = g_strdup("");

	if (comment != NULL)
		gai->comment = g_strdup(comment);
	else
		gai->comment = g_strdup("");
	gai->conv_num=call_num;
	gai->line_style=1;
	gai->display=FALSE;

	tapinfo->graph_analysis->list = g_list_append(tapinfo->graph_analysis->list, gai);

	return 1;

}

/****************************************************************************/
/* Append str to frame_label and comment in a graph item */
/* return 0 if the frame_num is not in the graph list */
int append_to_frame_graph(voip_calls_tapinfo_t *tapinfo _U_, guint32 frame_num, const gchar *new_frame_label, const gchar *new_comment)
{
	graph_analysis_item_t *gai;
	GList* list;
	gchar *tmp_str = NULL;
	gchar *tmp_str2 = NULL;

	list = g_list_first(tapinfo->graph_analysis->list);
	while (list)
	{
		gai = list->data;
		if (gai->frame_num == frame_num){
			tmp_str = gai->frame_label;
			tmp_str2 = gai->comment;

			if (new_frame_label != NULL){
				gai->frame_label = g_strdup_printf("%s %s", gai->frame_label, new_frame_label);
				g_free(tmp_str);
			}


			if (new_comment != NULL){
				gai->comment = g_strdup_printf("%s %s", gai->comment, new_comment);
				g_free(tmp_str2);
			}
			break;
		}
		list = g_list_next (list);
	}
	if (tmp_str == NULL) return 0;		/* it is not in the list */
	return 1;

}

/****************************************************************************/
/* ***************************TAP for SIP **********************************/
/****************************************************************************/

/****************************************************************************/
/* whenever a SIP packet is seen by the tap listener */
static int 
SIPcalls_packet( void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *SIPinfo)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	/* we just take note of the ISUP data here; when we receive the MTP3 part everything will
	   be compared with existing calls */

	voip_calls_info_t *tmp_listinfo;
	voip_calls_info_t *strinfo = NULL;
	sip_calls_info_t *tmp_sipinfo;
	GList* list;
	guint32 tmp_src, tmp_dst;
	gchar *frame_label = NULL;
	gchar *comment = NULL;

	const sip_info_value_t *pi = SIPinfo;

	/* do not consider packets without call_id */
	if (pi->tap_call_id ==NULL){
		return 0;
	}

	/* check wether we already have a call with these parameters in the list */
	list = g_list_first(tapinfo->strinfo_list);
	while (list)
	{
		tmp_listinfo=list->data;
		if (tmp_listinfo->protocol == VOIP_SIP){
			tmp_sipinfo = tmp_listinfo->prot_info;
			if (strcmp(tmp_sipinfo->call_identifier,pi->tap_call_id)==0){
				strinfo = (voip_calls_info_t*)(list->data);
				break;
			}
		}
		list = g_list_next (list);
	}

	/* not in the list? then create a new entry if the message is INVITE -i.e. if this session is a call*/
	if ((strinfo==NULL) &&(pi->request_method!=NULL)){
		if (strcmp(pi->request_method,"INVITE")==0){
			strinfo = g_malloc(sizeof(voip_calls_info_t));
			strinfo->call_active_state = VOIP_ACTIVE;
			strinfo->call_state = VOIP_CALL_SETUP;
			strinfo->from_identity=g_strdup(pi->tap_from_addr);
			strinfo->to_identity=g_strdup(pi->tap_to_addr);
			g_memmove(&(strinfo->initial_speaker), pinfo->src.data, 4);
			strinfo->first_frame_num=pinfo->fd->num;
			strinfo->selected=FALSE;
			strinfo->start_sec=pinfo->fd->rel_secs;
			strinfo->start_usec=pinfo->fd->rel_usecs;
			strinfo->protocol=VOIP_SIP;
			strinfo->prot_info=g_malloc(sizeof(sip_calls_info_t));
			tmp_sipinfo=strinfo->prot_info;
			tmp_sipinfo->call_identifier=strdup(pi->tap_call_id);
			tmp_sipinfo->sip_state=SIP_INVITE_SENT;
			tmp_sipinfo->invite_cseq=pi->tap_cseq_number;
			strinfo->npackets = 0;
			strinfo->call_num = tapinfo->ncalls++;
			tapinfo->strinfo_list = g_list_append(tapinfo->strinfo_list, strinfo);
		}
	}

	g_free(pi->tap_from_addr);
	g_free(pi->tap_to_addr);
	g_free(pi->tap_call_id);

	if (strinfo!=NULL){

		/* let's analyze the call state */

		g_memmove(&(tmp_src), pinfo->src.data, 4);
		g_memmove(&(tmp_dst), pinfo->dst.data, 4);
		
		if (pi->request_method == NULL){
			frame_label = g_strdup_printf("%d %s", pi->response_code, pi->reason_phrase );
			comment = g_strdup_printf("SIP Status");

			if ((pi->tap_cseq_number == tmp_sipinfo->invite_cseq)&&(tmp_dst==strinfo->initial_speaker)){
				if ((pi->response_code > 199) && (pi->response_code<300) && (tmp_sipinfo->sip_state == SIP_INVITE_SENT)){
					tmp_sipinfo->sip_state = SIP_200_REC;
				}
				else if ((pi->response_code>299)&&(tmp_sipinfo->sip_state == SIP_INVITE_SENT)){
					strinfo->call_state = VOIP_REJECTED;
					tapinfo->rejected_calls++;
				}
			}

		}
		else{
			frame_label = g_strdup(pi->request_method);

			if ((strcmp(pi->request_method,"INVITE")==0)&&(tmp_src == strinfo->initial_speaker)){
				tmp_sipinfo->invite_cseq = pi->tap_cseq_number;
				comment = g_strdup_printf("SIP From: %s To:%s", strinfo->from_identity, strinfo->to_identity);
			}
			else if ((strcmp(pi->request_method,"ACK")==0)&&(pi->tap_cseq_number == tmp_sipinfo->invite_cseq)
				&&(tmp_src == strinfo->initial_speaker)&&(tmp_sipinfo->sip_state==SIP_200_REC)){
				strinfo->call_state = VOIP_IN_CALL;
				comment = g_strdup_printf("SIP Request");
			}
			else if (strcmp(pi->request_method,"BYE")==0){
				strinfo->call_state = VOIP_COMPLETED;
				tapinfo->completed_calls++;
				comment = g_strdup_printf("SIP Request");
			}
			else if ((strcmp(pi->request_method,"CANCEL")==0)&&(pi->tap_cseq_number == tmp_sipinfo->invite_cseq)
				&&(tmp_src == strinfo->initial_speaker)&&(strinfo->call_state==VOIP_CALL_SETUP)){
				strinfo->call_state = VOIP_CANCELLED;
				tmp_sipinfo->sip_state = SIP_CANCEL_SENT;
				comment = g_strdup_printf("SIP Request");
			} else {
				comment = g_strdup_printf("SIP Request");
			}
		}

		strinfo->stop_sec=pinfo->fd->rel_secs;
		strinfo->stop_usec=pinfo->fd->rel_usecs;
		strinfo->last_frame_num=pinfo->fd->num;
		++(strinfo->npackets);
		/* increment the packets counter of all calls */
		++(tapinfo->npackets);

		/* add to the graph */
		add_to_graph(tapinfo, pinfo, frame_label, comment, strinfo->call_num);  
		g_free(comment);
		g_free(frame_label);
	}
	return 1;  /* refresh output */
}


/****************************************************************************/
const voip_calls_tapinfo_t* voip_calls_get_info(void)
{
	return &the_tapinfo_struct;
}


/****************************************************************************/
/* TAP INTERFACE */
/****************************************************************************/
static gboolean have_SIP_tap_listener=FALSE;
/****************************************************************************/
void
sip_calls_init_tap(void)
{
	GString *error_string;

	if(have_SIP_tap_listener==FALSE)
	{
		/* don't register tap listener, if we have it already */
		error_string = register_tap_listener("sip", &(the_tapinfo_struct.sip_dummy), NULL,
			voip_calls_dlg_reset, 
			SIPcalls_packet, 
			voip_calls_dlg_draw
			);
		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_SIP_tap_listener=TRUE;
	}
}



/* XXX just copied from gtk/rpc_stat.c */
void protect_thread_critical_region(void);
void unprotect_thread_critical_region(void);

/****************************************************************************/
void
remove_tap_listener_sip_calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.sip_dummy));
	unprotect_thread_critical_region();

	have_SIP_tap_listener=FALSE;
}

/****************************************************************************/
/* ***************************TAP for ISUP **********************************/
/****************************************************************************/

static gchar		isup_called_number[255], isup_calling_number[255];
static guint16		isup_cic;
static guint8		isup_message_type;

/****************************************************************************/
/* whenever a isup_ packet is seen by the tap listener */
static int 
isup_calls_packet(void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *isup_info _U_)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	const isup_tap_rec_t *pi = isup_info;

	if (pi->calling_number!=NULL){
		strcpy(isup_calling_number, pi->calling_number);
	}
	if (pi->called_number!=NULL){
		strcpy(isup_called_number, pi->called_number);
	}
	isup_message_type = pi->message_type;
	isup_cic = pinfo->circuit_id;

	return 0;
}

/****************************************************************************/

static gboolean have_isup_tap_listener=FALSE;

void
isup_calls_init_tap(void)
{
	GString *error_string;


	if(have_isup_tap_listener==FALSE)
	{
		error_string = register_tap_listener("isup", &(the_tapinfo_struct.isup_dummy),
			NULL,
			voip_calls_dlg_reset, 
			isup_calls_packet, 
			voip_calls_dlg_draw
			);
	
		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_isup_tap_listener=TRUE;
	}
}

/****************************************************************************/

void
remove_tap_listener_isup_calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.isup_dummy));
	unprotect_thread_critical_region();

	have_isup_tap_listener=FALSE;
}


/****************************************************************************/
/* ***************************TAP for MTP3 **********************************/
/****************************************************************************/


/****************************************************************************/
/* whenever a mtp3_ packet is seen by the tap listener */
static int 
mtp3_calls_packet(void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *mtp3_info _U_)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	voip_calls_info_t *tmp_listinfo;
	voip_calls_info_t *strinfo = NULL;
	isup_calls_info_t *tmp_isupinfo;
	gboolean found = FALSE;
	gboolean forward;
	gboolean right_pair = TRUE;
	GList* list;

	const mtp3_tap_rec_t *pi = mtp3_info;

	/* check wether we already have a call with these parameters in the list */
	list = g_list_first(tapinfo->strinfo_list);
	while (list)
	{
		tmp_listinfo=list->data;
		if ((tmp_listinfo->protocol == VOIP_ISUP)&&(tmp_listinfo->call_active_state==VOIP_ACTIVE)){
			tmp_isupinfo = tmp_listinfo->prot_info;
			if ((tmp_isupinfo->cic == isup_cic)&&(tmp_isupinfo->ni == pi->addr_opc.ni)){
				if ((tmp_isupinfo->opc == pi->addr_opc.pc)&&(tmp_isupinfo->dpc == pi->addr_dpc.pc)){
					 forward = TRUE;
				 }
				 else if ((tmp_isupinfo->dpc == pi->addr_opc.pc)&&(tmp_isupinfo->opc == pi->addr_dpc.pc)){
					 forward = FALSE;
				 }
				 else{
					 right_pair = FALSE;
				 }
				 if (right_pair){
					/* if there is an IAM for a call that is not in setup state, that means the previous call in the same
					   cic is no longer active */
					if (tmp_listinfo->call_state == VOIP_CALL_SETUP){
						found = TRUE;
					}
					else if (isup_message_type != 1){
						found = TRUE;
					}
					else{
						tmp_listinfo->call_active_state=VOIP_INACTIVE;
					}
				}
				if (found){
					strinfo = (voip_calls_info_t*)(list->data);
					break;
				}
			}
		}
		list = g_list_next (list);
	}

	/* not in the list? then create a new entry if the message is IAM
	   -i.e. if this session is a call*/


	if ((strinfo==NULL) &&(isup_message_type==1)){

		strinfo = g_malloc(sizeof(voip_calls_info_t));
		strinfo->call_active_state = VOIP_ACTIVE;
		strinfo->call_state = VOIP_UNKNOWN;
		g_memmove(&(strinfo->initial_speaker), pinfo->src.data, 4);
		strinfo->selected=FALSE;
		strinfo->first_frame_num=pinfo->fd->num;
		strinfo->start_sec=pinfo->fd->rel_secs;
		strinfo->start_usec=pinfo->fd->rel_usecs;
		strinfo->protocol=VOIP_ISUP;
		strinfo->from_identity=g_strdup(isup_calling_number);
		strinfo->to_identity=g_strdup(isup_called_number);
		strinfo->prot_info=g_malloc(sizeof(isup_calls_info_t));
		tmp_isupinfo=strinfo->prot_info;
		tmp_isupinfo->opc = pi->addr_opc.pc;
		tmp_isupinfo->dpc = pi->addr_dpc.pc;
		tmp_isupinfo->ni = pi->addr_opc.ni;
		tmp_isupinfo->cic = pinfo->circuit_id;
		strinfo->npackets = 0;
		strinfo->call_num = tapinfo->ncalls++;
		tapinfo->strinfo_list = g_list_append(tapinfo->strinfo_list, strinfo);
	}

	if (strinfo!=NULL){
		strinfo->stop_sec=pinfo->fd->rel_secs;
		strinfo->stop_usec=pinfo->fd->rel_usecs;
		strinfo->last_frame_num=pinfo->fd->num;
		++(strinfo->npackets);

		/* Let's analyze the call state */

		switch(isup_message_type){
			case 1: /* IAM */
				strinfo->call_state=VOIP_CALL_SETUP;
				break;
			case 7: /* CONNECT */
			case 9: /* ANSWER */
				strinfo->call_state=VOIP_IN_CALL;
				break;
			case 12: /* RELEASE */
				if (strinfo->call_state==VOIP_CALL_SETUP){
					if (forward){
						strinfo->call_state=VOIP_CANCELLED;
					}
					else{
						strinfo->call_state=VOIP_REJECTED;
						tapinfo->rejected_calls++;
					}
				}
				else if (strinfo->call_state == VOIP_IN_CALL){
					strinfo->call_state = VOIP_COMPLETED;
					tapinfo->completed_calls++;
				}
				break;
		}

		/* increment the packets counter of all calls */
		++(tapinfo->npackets);
	}


	return 1;
}

/****************************************************************************/

static gboolean have_mtp3_tap_listener=FALSE;

void
mtp3_calls_init_tap(void)
{
	GString *error_string;


	if(have_mtp3_tap_listener==FALSE)
	{
		error_string = register_tap_listener("mtp3", &(the_tapinfo_struct.mtp3_dummy),
			NULL,
			voip_calls_dlg_reset, 
			mtp3_calls_packet, 
			voip_calls_dlg_draw
			);

		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_mtp3_tap_listener=TRUE;
	}
}

/****************************************************************************/

void
remove_tap_listener_mtp3_calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.mtp3_dummy));
	unprotect_thread_critical_region();

	have_mtp3_tap_listener=FALSE;
}

/****************************************************************************/
/* ***************************TAP for Q931 **********************************/
/****************************************************************************/

static gchar *q931_calling_number;
static gchar *q931_called_number;
static guint8 q931_cause_value;
static gint32 q931_crv;
static guint8 q931_message_type;
static guint32 q931_frame_num;

/****************************************************************************/
/* whenever a q931_ packet is seen by the tap listener */
static int 
q931_calls_packet(void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *q931_info _U_)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	const q931_packet_info *pi = q931_info;

	/* free previously allocated q931_calling/ed_number */
	g_free(q931_calling_number);
	g_free(q931_called_number);
	
	if (pi->calling_number!=NULL)
		q931_calling_number = g_strdup(pi->calling_number);
	else
		q931_calling_number = g_strdup("");

	if (pi->called_number!=NULL)
		q931_called_number = g_strdup(pi->called_number);
	else
		q931_called_number = g_strdup("");
	q931_cause_value = pi->cause_value;
	q931_frame_num = pinfo->fd->num;
	q931_crv = pi->crv;

	return 0;
}

/****************************************************************************/
static gboolean have_q931_tap_listener=FALSE;

void
q931_calls_init_tap(void)
{
	GString *error_string;


	if(have_q931_tap_listener==FALSE)
	{
		error_string = register_tap_listener("q931", &(the_tapinfo_struct.q931_dummy),
			NULL,
			voip_calls_dlg_reset,
			q931_calls_packet,
			voip_calls_dlg_draw
			);
			
		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_q931_tap_listener=TRUE;
	}
}

/****************************************************************************/

void
remove_tap_listener_q931_calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.q931_dummy));
	unprotect_thread_critical_region();

	have_q931_tap_listener=FALSE;
}

/****************************************************************************/
/****************************TAP for H323 ***********************************/
/****************************************************************************/

#define GUID_LEN	16
static const guint8 guid_allzero[GUID_LEN] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/* defines specific H323 data */

void add_h245_Address(h323_calls_info_t *h323info, guint32 h245_address, guint16 h245_port)
{
	h245_address_t *h245_add = NULL;

	h245_add = g_malloc(sizeof(h245_address_t));
		h245_add->h245_address = h245_address;
		h245_add->h245_port = h245_port;

	h323info->h245_list = g_list_append(h323info->h245_list, h245_add);				
}

/****************************************************************************/
/* whenever a H225 packet is seen by the tap listener */
static int 
H225calls_packet(void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *H225info)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	voip_calls_info_t *tmp_listinfo;
	voip_calls_info_t *strinfo = NULL;
	h323_calls_info_t *tmp_h323info;
	gchar *frame_label;
	gchar *comment;
	GList* list;
	guint32 tmp_src, tmp_dst;

	const h225_packet_info *pi = H225info;

	/* if not guid and RAS return because did not belong to a call */
	if ((memcmp(pi->guid, guid_allzero, GUID_LEN) == 0) && (pi->msg_type == H225_RAS))
		return 0;
	

	if ( (memcmp(pi->guid, guid_allzero, GUID_LEN) == 0) && (q931_frame_num == pinfo->fd->num) ){
		/* check wether we already have a call with this Q931 CRV */
		list = g_list_first(tapinfo->strinfo_list);
		while (list)
		{
			tmp_listinfo=list->data;
			if (tmp_listinfo->protocol == VOIP_H323){
				tmp_h323info = tmp_listinfo->prot_info;
				if ( ((tmp_h323info->q931_crv == q931_crv) || (tmp_h323info->q931_crv2 == q931_crv)) && (q931_crv!=-1)){
					strinfo = (voip_calls_info_t*)(list->data);
					break;
				}
			}
			list = g_list_next (list);
		}
		if (strinfo==NULL) 	return 0;		
	} else {
		/* check wether we already have a call with this guid in the list */
		list = g_list_first(tapinfo->strinfo_list);
		while (list)
		{
			tmp_listinfo=list->data;
			if (tmp_listinfo->protocol == VOIP_H323){
				tmp_h323info = tmp_listinfo->prot_info;
				if (memcmp(tmp_h323info->guid, pi->guid,GUID_LEN)==0){ 
					strinfo = (voip_calls_info_t*)(list->data);
					break;
				}
			}
			list = g_list_next (list);
		}
	}

	/* not in the list? then create a new entry */
	if ((strinfo==NULL)){
		strinfo = g_malloc(sizeof(voip_calls_info_t));
		strinfo->call_active_state = VOIP_ACTIVE;
		strinfo->call_state = VOIP_UNKNOWN;
		strinfo->from_identity=g_strdup("");
		strinfo->to_identity=g_strdup("");

		g_memmove(&(strinfo->initial_speaker), pinfo->src.data,4);
		strinfo->selected=FALSE;
		strinfo->first_frame_num=pinfo->fd->num;
		strinfo->start_sec=pinfo->fd->rel_secs;
		strinfo->start_usec=pinfo->fd->rel_usecs;
		strinfo->protocol=VOIP_H323;
		strinfo->prot_info=g_malloc(sizeof(h323_calls_info_t));
		tmp_h323info = strinfo->prot_info;
		tmp_h323info->guid = (guint8 *) g_memdup(pi->guid,GUID_LEN);
		tmp_h323info->h225SetupAddr = 0;
		tmp_h323info->h245_list = NULL;
		tmp_h323info->is_faststart_Setup = FALSE;
		tmp_h323info->is_faststart_Proc = FALSE;
		tmp_h323info->is_h245Tunneling = FALSE;
		tmp_h323info->is_h245 = FALSE;
		tmp_h323info->q931_crv = -1;
		tmp_h323info->q931_crv2 = -1;
		strinfo->call_num = tapinfo->ncalls++;
		strinfo->npackets = 0;

		tapinfo->strinfo_list = g_list_append(tapinfo->strinfo_list, strinfo);				
	}

	if (strinfo!=NULL){

		/* let's analyze the call state */

		g_memmove(&(tmp_src), pinfo->src.data, 4);
		g_memmove(&(tmp_dst), pinfo->dst.data, 4);

		strinfo->stop_sec=pinfo->fd->rel_secs;
		strinfo->stop_usec=pinfo->fd->rel_usecs;
		strinfo->last_frame_num=pinfo->fd->num;
		++(strinfo->npackets);
		/* increment the packets counter of all calls */
		++(tapinfo->npackets);

		/* change the status */
		if (pi->msg_type == H225_CS){
			if (tmp_h323info->q931_crv == -1) {
				tmp_h323info->q931_crv = q931_crv;
			} else if (tmp_h323info->q931_crv != q931_crv) {
				tmp_h323info->q931_crv2 = q931_crv;
			}

			if (pi->is_h245 == TRUE){
				add_h245_Address(tmp_h323info, pi->h245_address, pi->h245_port);
			}

			if (pi->cs_type != H225_RELEASE_COMPLET) tmp_h323info->is_h245Tunneling = pi->is_h245Tunneling;

			frame_label = g_strdup(pi->frame_label);

			switch(pi->cs_type){
			case H225_SETUP:
				tmp_h323info->is_faststart_Setup = pi->is_faststart;

				/* set te calling and called number from the Q931 packet */
				if (q931_frame_num == pinfo->fd->num){
					if (q931_calling_number != NULL){
						g_free(strinfo->from_identity);
						strinfo->from_identity=g_strdup(q931_calling_number);
					}
					if (q931_called_number != NULL){
						g_free(strinfo->to_identity);
						strinfo->to_identity=g_strdup(q931_called_number);
					}
				}
				if (tmp_h323info->h225SetupAddr == 0) g_memmove(&(tmp_h323info->h225SetupAddr), pinfo->src.data,4);
				strinfo->call_state=VOIP_CALL_SETUP;
				comment = g_strdup_printf("H225 From: %s To:%s  TunnH245:%s FS:%s", strinfo->from_identity, strinfo->to_identity, (tmp_h323info->is_h245Tunneling==TRUE?"on":"off"), 
											(pi->is_faststart==TRUE?"on":"off"));
				break;
			case H225_CONNECT:
				strinfo->call_state=VOIP_IN_CALL;
				if (pi->is_faststart == TRUE) tmp_h323info->is_faststart_Proc = TRUE;
				comment = g_strdup_printf("H225 TunnH245:%s FS:%s", (tmp_h323info->is_h245Tunneling==TRUE?"on":"off"), 
											(pi->is_faststart==TRUE?"on":"off"));
				break;
			case H225_RELEASE_COMPLET:
				g_memmove(&(tmp_src), pinfo->src.data, 4);
				if (strinfo->call_state==VOIP_CALL_SETUP){
					if (tmp_h323info->h225SetupAddr == tmp_src){  /* forward direction */
						strinfo->call_state=VOIP_CANCELLED;
					}
					else{												/* reverse */
						strinfo->call_state=VOIP_REJECTED;
						tapinfo->rejected_calls++;
					}
				}
				else if (strinfo->call_state == VOIP_IN_CALL){
					strinfo->call_state = VOIP_COMPLETED;
					tapinfo->completed_calls++;
				}
				/* get the Q931 Release cause code */
				if (q931_frame_num == pinfo->fd->num){
					if (q931_cause_value != 0xFF){		
						comment = g_strdup_printf("H225 Q931 Rel Cause (%i):%s", q931_cause_value, val_to_str(q931_cause_value, q931_cause_code_vals, "<unknown>"));
					} else {			/* Cause not set */
						comment = g_strdup("H225 No Q931 Rel Cause");
					}
				}
				break;
			case H225_PROGRESS:
			case H225_ALERTING:
			case H225_CALL_PROCEDING:
				if (pi->is_faststart == TRUE) tmp_h323info->is_faststart_Proc = TRUE;
				comment = g_strdup_printf("H225 TunnH245:%s FS:%s", (tmp_h323info->is_h245Tunneling==TRUE?"on":"off"), 
											(pi->is_faststart==TRUE?"on":"off"));
				break;
			default:
				comment = g_strdup_printf("H225 TunnH245:%s FS:%s", (tmp_h323info->is_h245Tunneling==TRUE?"on":"off"), 
											(pi->is_faststart==TRUE?"on":"off"));

			}
		} else if (pi->msg_type == H225_RAS){
			frame_label = g_strdup_printf("%s", val_to_str(pi->msg_tag, RasMessage_vals, "<unknown>"));
			comment = g_strdup("H225 RAS");
		} else {
			frame_label = g_strdup("H225: Unknown");
			comment = g_strdup("");
		}

		/* add to graph analysis */
		if (!append_to_frame_graph(tapinfo, pinfo->fd->num, pi->frame_label, comment))		/* if the frame number exists in graph, append to it*/
			add_to_graph(tapinfo, pinfo, frame_label, comment, strinfo->call_num);  /* if not exist, add to the graph */

		g_free(frame_label);
		g_free(comment);
	}

	return 1;  /* refresh output */
}


/****************************************************************************/
/* TAP INTERFACE */
/****************************************************************************/
static gboolean have_H225_tap_listener=FALSE;
/****************************************************************************/
void
h225_calls_init_tap(void)
{
	GString *error_string;

	if(have_H225_tap_listener==FALSE)
	{
		/* don't register tap listener, if we have it already */
		error_string = register_tap_listener("h225", &(the_tapinfo_struct.h225_dummy), NULL,
			voip_calls_dlg_reset, 
			H225calls_packet, 
			voip_calls_dlg_draw
			);
			
		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_H225_tap_listener=TRUE;
	}
}



/* XXX just copied from gtk/rpc_stat.c */
void protect_thread_critical_region(void);
void unprotect_thread_critical_region(void);

/****************************************************************************/
void
remove_tap_listener_h225_calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.h225_dummy));
	unprotect_thread_critical_region();

	have_H225_tap_listener=FALSE;
}


/****************************************************************************/
/* whenever a H245dg packet is seen by the tap listener (when H245 tunneling is ON) */
static int 
H245dgcalls_packet(void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *H245info)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	voip_calls_info_t *tmp_listinfo;
	voip_calls_info_t *strinfo = NULL;
	h323_calls_info_t *tmp_h323info;
	gchar *frame_label;
	gchar *comment;
	GList* list;
	GList* list2;
	guint32 tmp_src, tmp_dst;
	h245_address_t *h245_add = NULL;

	const h245_packet_info *pi = H245info;

	/* if H245 tunneling is on, append to graph */
	if (append_to_frame_graph(tapinfo, pinfo->fd->num, pi->frame_label, pi->comment)) return 1;


	/* it is not Tunneling or it is not in the list. So check if there is no tunneling
	   and there is a call with this H245 address */
	list = g_list_first(tapinfo->strinfo_list);
	while (list)
	{
		tmp_listinfo=list->data;
		if (tmp_listinfo->protocol == VOIP_H323){
			tmp_h323info = tmp_listinfo->prot_info;

			g_memmove(&(tmp_src), pinfo->src.data, 4);
			g_memmove(&(tmp_dst), pinfo->dst.data, 4);
			list2 = g_list_first(tmp_h323info->h245_list);
			while (list2)
			{
				h245_add=list2->data;
				if ( ((h245_add->h245_address == tmp_src) && (h245_add->h245_port == pinfo->srcport))
					|| ((h245_add->h245_address == tmp_dst) && (h245_add->h245_port == pinfo->destport)) ){
					strinfo = (voip_calls_info_t*)(list->data);

					++(strinfo->npackets);
					/* increment the packets counter of all calls */
					++(tapinfo->npackets);

					break;
				}			
			list2 = g_list_next(list2);
			}
			if (strinfo!=NULL) break;
		}
		list = g_list_next(list);
	}

	if (strinfo!=NULL){
		frame_label = g_strdup(pi->frame_label);
		comment = g_strdup(pi->comment);
		add_to_graph(tapinfo, pinfo, frame_label, comment, strinfo->call_num);
		g_free(frame_label);
		g_free(comment);
	}

	return 1;  /* refresh output */
}


/****************************************************************************/
/* TAP INTERFACE */
/****************************************************************************/
static gboolean have_H245dg_tap_listener=FALSE;
/****************************************************************************/
void
h245dg_calls_init_tap(void)
{
	GString *error_string;

	if(have_H245dg_tap_listener==FALSE)
	{
		/* don't register tap listener, if we have it already */
		error_string = register_tap_listener("h245dg", &(the_tapinfo_struct.h245dg_dummy), NULL,
			voip_calls_dlg_reset,
			H245dgcalls_packet, 
			voip_calls_dlg_draw
			);
		
		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_H245dg_tap_listener=TRUE;
	}
}


/* XXX just copied from gtk/rpc_stat.c */
void protect_thread_critical_region(void);
void unprotect_thread_critical_region(void);

/****************************************************************************/
void
remove_tap_listener_h245dg_calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.h245dg_dummy));
	unprotect_thread_critical_region();

	have_H245dg_tap_listener=FALSE;
}


/****************************************************************************/
/****************************TAP for SDP PROTOCOL ***************************/
/****************************************************************************/
/* whenever a SDP packet is seen by the tap listener */
static int 
SDPcalls_packet(void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *SDPinfo)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	const sdp_packet_info *pi = SDPinfo;
	char summary_str[50];
	
	/* Append to graph the SDP summary if the packet exists */
	g_snprintf(summary_str, 50, "SDP (%s)", pi->summary_str);
	append_to_frame_graph(tapinfo, pinfo->fd->num, summary_str, NULL);

	return 1;  /* refresh output */
}


/****************************************************************************/
/* TAP INTERFACE */
/****************************************************************************/
static gboolean have_sdp_tap_listener=FALSE;
/****************************************************************************/
void
sdp_calls_init_tap(void)
{
	GString *error_string;

	if(have_sdp_tap_listener==FALSE)
	{
		/* don't register tap listener, if we have it already */
		error_string = register_tap_listener("sdp", &(the_tapinfo_struct.sdp_dummy), NULL,
			voip_calls_dlg_reset, 
			SDPcalls_packet, 
			voip_calls_dlg_draw
			);
			
		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_sdp_tap_listener=TRUE;
	}
}


/* XXX just copied from gtk/rpc_stat.c */
void protect_thread_critical_region(void);
void unprotect_thread_critical_region(void);

/****************************************************************************/
void
remove_tap_listener_sdp_calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.sdp_dummy));
	unprotect_thread_critical_region();

	have_sdp_tap_listener=FALSE;
}





/****************************************************************************/
/* ***************************TAP for OTHER PROTOCOL **********************************/
/****************************************************************************/

/****************************************************************************/
/* whenever a prot_ packet is seen by the tap listener */
/*
static int 
prot_calls_packet(void *ptr _U_, packet_info *pinfo, epan_dissect_t *edt _U_, const void *prot_info _U_)
{
	voip_calls_tapinfo_t *tapinfo = &the_tapinfo_struct;
	if (strinfo!=NULL){
		strinfo->stop_sec=pinfo->fd->rel_secs;
		strinfo->stop_usec=pinfo->fd->rel_usecs;
		strinfo->last_frame_num=pinfo->fd->num;
		++(strinfo->npackets);
		++(tapinfo->npackets);
	}

	return 1;
}
*/
/****************************************************************************/
/*
static gboolean have_prot__tap_listener=FALSE;

void
prot_calls_init_tap(void)
{
	GString *error_string;

	if(have_prot__tap_listener==FALSE)
	{
		error_string = register_tap_listener("prot_", &(the_tapinfo_struct.prot__dummy),
			NULL,
			voip_calls_dlg_reset,
			prot__calls_packet, 
			voip_calls_dlg_draw
			);

		if (error_string != NULL) {
			simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK,
				      error_string->str);
			g_string_free(error_string, TRUE);
			exit(1);
		}
		have_prot__tap_listener=TRUE;
	}
}
*/
/****************************************************************************/
/*
void
remove_tap_listener_prot__calls(void)
{
	protect_thread_critical_region();
	remove_tap_listener(&(the_tapinfo_struct.prot__dummy));
	unprotect_thread_critical_region();

	have_prot__tap_listener=FALSE;
}
*/

