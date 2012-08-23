/* packet-llrp.c
 * Routines for Low Level Reader Protocol dissection
 * Copyright 2012, Evan Huus <eapache@gmail.com>
 * Copyright 2012, Martin Kupec <martin.kupec@kupson.cz>
 *
 * http://www.gs1.org/gsmp/kc/epcglobal/llrp
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include <epan/packet.h>
#include <epan/expert.h>
#include <epan/dissectors/packet-tcp.h>

#define LLRP_PORT 5084

/* Initialize the protocol and registered fields */
static int proto_llrp                 = -1;
static int hf_llrp_version            = -1;
static int hf_llrp_type               = -1;
static int hf_llrp_length             = -1;
static int hf_llrp_id                 = -1;
static int hf_llrp_cur_ver            = -1;
static int hf_llrp_sup_ver            = -1;
static int hf_llrp_req_cap            = -1;
static int hf_llrp_req_conf           = -1;
static int hf_llrp_rospec             = -1;
static int hf_llrp_antenna_id         = -1;
static int hf_llrp_gpi_port           = -1;
static int hf_llrp_gpo_port           = -1;
static int hf_llrp_rest_fact          = -1;
static int hf_llrp_accessspec         = -1;
static int hf_llrp_vendor             = -1;
static int hf_llrp_impinj_msg_type    = -1;
static int hf_llrp_tlv_type           = -1;
static int hf_llrp_tv_type            = -1;
static int hf_llrp_tlv_len            = -1;
static int hf_llrp_param              = -1;

/* Initialize the subtree pointers */
static gint ett_llrp = -1;
static gint ett_llrp_param = -1;

/* Message Types */
#define LLRP_TYPE_GET_READER_CAPABILITES            1
#define LLRP_TYPE_GET_READER_CONFIG                 2
#define LLRP_TYPE_SET_READER_CONFIG                 3
#define LLRP_TYPE_CLOSE_CONNECTION_RESPONSE         4
#define LLRP_TYPE_GET_READER_CAPABILITES_RESPONSE  11
#define LLRP_TYPE_GET_READER_CONFIG_RESPONSE       12
#define LLRP_TYPE_SET_READER_CONFIG_RESPONSE       13
#define LLRP_TYPE_CLOSE_CONNECTION                 14
#define LLRP_TYPE_ADD_ROSPEC                       20
#define LLRP_TYPE_DELETE_ROSPEC                    21
#define LLRP_TYPE_START_ROSPEC                     22
#define LLRP_TYPE_STOP_ROSPEC                      23
#define LLRP_TYPE_ENABLE_ROSPEC                    24
#define LLRP_TYPE_DISABLE_ROSPEC                   25
#define LLRP_TYPE_GET_ROSPECS                      26
#define LLRP_TYPE_ADD_ROSPEC_RESPONSE              30
#define LLRP_TYPE_DELETE_ROSPEC_RESPONSE           31
#define LLRP_TYPE_START_ROSPEC_RESPONSE            32
#define LLRP_TYPE_STOP_ROSPEC_RESPONSE             33
#define LLRP_TYPE_ENABLE_ROSPEC_RESPONSE           34
#define LLRP_TYPE_DISABLE_ROSPEC_RESPONSE          35
#define LLRP_TYPE_GET_ROSPECS_RESPONSE             36
#define LLRP_TYPE_ADD_ACCESSSPEC                   40
#define LLRP_TYPE_DELETE_ACCESSSPEC                41
#define LLRP_TYPE_ENABLE_ACCESSSPEC                42
#define LLRP_TYPE_DISABLE_ACCESSSPEC               43
#define LLRP_TYPE_GET_ACCESSSPECS                  44
#define LLRP_TYPE_CLIENT_REQUEST_OP                45
#define LLRP_TYPE_GET_SUPPORTED_VERSION            46
#define LLRP_TYPE_SET_PROTOCOL_VERSION             47
#define LLRP_TYPE_ADD_ACCESSSPEC_RESPONSE          50
#define LLRP_TYPE_DELETE_ACCESSSPEC_RESPONSE       51
#define LLRP_TYPE_ENABLE_ACCESSSPEC_RESPONSE       52
#define LLRP_TYPE_DISABLE_ACCESSSPEC_RESPONSE      53
#define LLRP_TYPE_GET_ACCESSSPECS_RESPONSE         54
#define LLRP_TYPE_CLIENT_RESQUEST_OP_RESPONSE      55
#define LLRP_TYPE_GET_SUPPORTED_VERSION_RESPONSE   56
#define LLRP_TYPE_SET_PROTOCOL_VERSION_RESPONSE    57
#define LLRP_TYPE_GET_REPORT                       60
#define LLRP_TYPE_RO_ACCESS_REPORT                 61
#define LLRP_TYPE_KEEPALIVE                        62
#define LLRP_TYPE_READER_EVENT_NOTIFICATION        63
#define LLRP_TYPE_ENABLE_EVENTS_AND_REPORTS        64
#define LLRP_TYPE_KEEPALIVE_ACK                    72
#define LLRP_TYPE_ERROR_MESSAGE                   100
#define LLRP_TYPE_CUSTOM_MESSAGE                 1023

static const value_string message_types[] = {
    { LLRP_TYPE_GET_READER_CAPABILITES,          "Get Reader Capabilites"          },
    { LLRP_TYPE_GET_READER_CONFIG,               "Get Reader Config"               },
    { LLRP_TYPE_SET_READER_CONFIG,               "Set Reader Config"               },
    { LLRP_TYPE_CLOSE_CONNECTION_RESPONSE,       "Close Connection Response"       },
    { LLRP_TYPE_GET_READER_CAPABILITES_RESPONSE, "Get Reader Capabilites Response" },
    { LLRP_TYPE_GET_READER_CONFIG_RESPONSE,      "Get Reader Config Response"      },
    { LLRP_TYPE_SET_READER_CONFIG_RESPONSE,      "Set Reader Config Response"      },
    { LLRP_TYPE_CLOSE_CONNECTION,                "Close Connection"                },
    { LLRP_TYPE_ADD_ROSPEC,                      "Add ROSpec"                      },
    { LLRP_TYPE_DELETE_ROSPEC,                   "Delete ROSpec"                   },
    { LLRP_TYPE_START_ROSPEC,                    "Start ROSpec"                    },
    { LLRP_TYPE_STOP_ROSPEC,                     "Stop ROSpec"                     },
    { LLRP_TYPE_ENABLE_ROSPEC,                   "Enable ROSpec"                   },
    { LLRP_TYPE_DISABLE_ROSPEC,                  "Disable ROSpec"                  },
    { LLRP_TYPE_GET_ROSPECS,                     "Get ROSpecs"                     },
    { LLRP_TYPE_ADD_ROSPEC_RESPONSE,             "Add ROSpec Response"             },
    { LLRP_TYPE_DELETE_ROSPEC_RESPONSE,          "Delete ROSpec Response"          },
    { LLRP_TYPE_START_ROSPEC_RESPONSE,           "Start ROSpec Response"           },
    { LLRP_TYPE_STOP_ROSPEC_RESPONSE,            "Stop ROSpec Response"            },
    { LLRP_TYPE_ENABLE_ROSPEC_RESPONSE,          "Enable ROSpec Response"          },
    { LLRP_TYPE_DISABLE_ROSPEC_RESPONSE,         "Disable ROSpec Response"         },
    { LLRP_TYPE_GET_ROSPECS_RESPONSE,            "Get ROSpecs Response"            },
    { LLRP_TYPE_ADD_ACCESSSPEC,                  "Add AccessSpec"                  },
    { LLRP_TYPE_DELETE_ACCESSSPEC,               "Delete AccessSpec"               },
    { LLRP_TYPE_ENABLE_ACCESSSPEC,               "Enable AccessSpec"               },
    { LLRP_TYPE_DISABLE_ACCESSSPEC,              "Disable AccessSpec"              },
    { LLRP_TYPE_GET_ACCESSSPECS,                 "Get AccessSpecs"                 },
    { LLRP_TYPE_CLIENT_REQUEST_OP,               "Client Request OP"               },
    { LLRP_TYPE_GET_SUPPORTED_VERSION,           "Get Supported Version"           },
    { LLRP_TYPE_SET_PROTOCOL_VERSION,            "Set Protocol Version"            },
    { LLRP_TYPE_ADD_ACCESSSPEC_RESPONSE,         "Add AccessSpec Response"         },
    { LLRP_TYPE_DELETE_ACCESSSPEC_RESPONSE,      "Delete AccessSpec Response"      },
    { LLRP_TYPE_ENABLE_ACCESSSPEC_RESPONSE,      "Enable AccessSpec Response"      },
    { LLRP_TYPE_DISABLE_ACCESSSPEC_RESPONSE,     "Disable AccessSpec Response"     },
    { LLRP_TYPE_GET_ACCESSSPECS_RESPONSE,        "Get AccessSpecs Response"        },
    { LLRP_TYPE_CLIENT_RESQUEST_OP_RESPONSE,     "Client Resquest OP Response"     },
    { LLRP_TYPE_GET_SUPPORTED_VERSION_RESPONSE,  "Get Supported Version Response"  },
    { LLRP_TYPE_SET_PROTOCOL_VERSION_RESPONSE,   "Set Protocol Version Response"   },
    { LLRP_TYPE_GET_REPORT,                      "Get Report"                      },
    { LLRP_TYPE_RO_ACCESS_REPORT,                "RO Access Report"                },
    { LLRP_TYPE_KEEPALIVE,                       "Keepalive"                       },
    { LLRP_TYPE_READER_EVENT_NOTIFICATION,       "Reader Event Notification"       },
    { LLRP_TYPE_ENABLE_EVENTS_AND_REPORTS,       "Enable Events And Reports"       },
    { LLRP_TYPE_KEEPALIVE_ACK,                   "Keepalive Ack"                   },
    { LLRP_TYPE_ERROR_MESSAGE,                   "Error Message"                   },
    { LLRP_TYPE_CUSTOM_MESSAGE,                  "Custom Message"                  },
    { 0,                                          NULL                             }
};
static value_string_ext message_types_ext = VALUE_STRING_EXT_INIT(message_types);

/* Versions */
#define LLRP_VERS_1_0_1 0x01
#define LLRP_VERS_1_1   0x02

static const value_string llrp_versions[] = {
    { LLRP_VERS_1_0_1, "1.0.1" },
    { LLRP_VERS_1_1,   "1.1"   },
    { 0,                NULL   }
};

/* Capabilities */
#define LLRP_CAP_ALL            0
#define LLRP_CAP_GENERAL_DEVICE 1
#define LLRP_CAP_LLRP           2
#define LLRP_CAP_REGULATORY     3
#define LLRP_CAP_AIR_PROTOCOL   4

static const value_string capabilities_request[] = {
    { LLRP_CAP_ALL,            "All"                            },
    { LLRP_CAP_GENERAL_DEVICE, "General Device Capabilities"    },
    { LLRP_CAP_LLRP,           "LLRP Capabilites"               },
    { LLRP_CAP_REGULATORY,     "Regulatory Capabilities"        },
    { LLRP_CAP_AIR_PROTOCOL,   "Air Protocol LLRP Capabilities" },
    { 0,                        NULL                            }
};

/* Configurations */
#define LLRP_CONF_ALL                             0
#define LLRP_CONF_IDENTIFICATION                  1
#define LLRP_CONF_ANTENNA_PROPERTIES              2
#define LLRP_CONF_ANTENNA_CONFIGURATION           3
#define LLRP_CONF_RO_REPORT_SPEC                  4
#define LLRP_CONF_READER_EVENT_NOTIFICATION_SPEC  5
#define LLRP_CONF_ACCESS_REPORT_SPEC              6
#define LLRP_CONF_LLRP_CONFIGURATION_STATE        7
#define LLRP_CONF_KEEPALIVE_SPEC                  8
#define LLRP_CONF_GPI_PORT_CURRENT_STATE          9
#define LLRP_CONF_GPO_WRITE_DATA                 10
#define LLRP_CONF_EVENTS_AND_REPORTS             11

static const value_string config_request[] = {
    { LLRP_CONF_ALL,                            "All"                            },
    { LLRP_CONF_IDENTIFICATION,                 "Identification"                 },
    { LLRP_CONF_ANTENNA_PROPERTIES,             "Antenna Properties"             },
    { LLRP_CONF_ANTENNA_CONFIGURATION,          "Antenna Configuration"          },
    { LLRP_CONF_RO_REPORT_SPEC,                 "RO Report Spec"                 },
    { LLRP_CONF_READER_EVENT_NOTIFICATION_SPEC, "Reader Event Notification Spec" },
    { LLRP_CONF_ACCESS_REPORT_SPEC,             "Access Report Spec"             },
    { LLRP_CONF_LLRP_CONFIGURATION_STATE,       "LLRP Configuration State"       },
    { LLRP_CONF_KEEPALIVE_SPEC,                 "Keepalive Spec"                 },
    { LLRP_CONF_GPI_PORT_CURRENT_STATE,         "GPI Port Current State"         },
    { LLRP_CONF_GPO_WRITE_DATA,                 "GPO Write Data"                 },
    { LLRP_CONF_EVENTS_AND_REPORTS,             "Events and Reports"             },
    { 0,                                         NULL                            }
};
static value_string_ext config_request_ext = VALUE_STRING_EXT_INIT(config_request);

/* TLV Parameter Types */
#define LLRP_TLV_UTC_TIMESTAMP           128
#define LLRP_TLV_UPTIME                  129
#define LLRP_TLV_GENERAL_DEVICE_CAP      137
#define LLRP_TLV_RECEIVE_SENSE_ENTRY     139
#define LLRP_TLV_ANTENNA_AIR_PROTO       140
#define LLRP_TLV_GPIO_CAPABILITIES       141
#define LLRP_TLV_LLRP_CAPABILITIES       142
#define LLRP_TLV_REGU_CAPABILITIES       143
#define LLRP_TLV_UHF_CAPABILITIES        144
#define LLRP_TLV_XMIT_POWER_LEVEL_ENTRY  145
#define LLRP_TLV_FREQ_INFORMATION        146
#define LLRP_TLV_FREQ_HOP_TABLE          147
#define LLRP_TLV_FIXED_FREQ_TABLE        148
#define LLRP_TLV_ANTENNA_RCV_SENSE_RANGE 149
#define LLRP_TLV_RO_SPEC                 177
#define LLRP_TLV_RO_BOUND_SPEC           178
#define LLRP_TLV_RO_SPEC_START_TRIGGER   179
#define LLRP_TLV_PER_TRIGGER_VAL         180
#define LLRP_TLV_GPI_TRIGGER_VAL         181
#define LLRP_TLV_RO_SPEC_STOP_TRIGGER    182
#define LLRP_TLV_AI_SPEC                 183
#define LLRP_TLV_AI_SPEC_STOP            184
#define LLRP_TLV_TAG_OBSERV_TRIGGER      185
#define LLRP_TLV_INVENTORY_PARAM_SPEC    186
#define LLRP_TLV_RF_SURVEY_SPEC          187
#define LLRP_TLV_RF_SURVEY_SPEC_STOP_TR  188
#define LLRP_TLV_ACCESS_SPEC             207
#define LLRP_TLV_ACCESS_SPEC_STOP_TRIG   208
#define LLRP_TLV_ACCESS_COMMAND          209
#define LLRP_TLV_CLIENT_REQ_OP_SPEC      210
#define LLRP_TLV_CLIENT_REQ_RESPONSE     211
#define LLRP_TLV_LLRP_CONF_STATE_VAL     217
#define LLRP_TLV_IDENT                   218
#define LLRP_TLV_GPO_WRITE_DATA          219
#define LLRP_TLV_KEEPALIVE_SPEC          220
#define LLRP_TLV_ANTENNA_PROPS           221
#define LLRP_TLV_ANTENNA_CONF            222
#define LLRP_TLV_RF_RECEIVER             223
#define LLRP_TLV_RF_TRANSMITTER          224
#define LLRP_TLV_GPI_PORT_CURRENT_STATE  225
#define LLRP_TLV_EVENTS_AND_REPORTS      226
#define LLRP_TLV_RO_REPORT_SPEC          237
#define LLRP_TLV_TAG_REPORT_CONTENT_SEL  238
#define LLRP_TLV_ACCESS_REPORT_SPEC      239
#define LLRP_TLV_TAG_REPORT_DATA         240
#define LLRP_TLV_EPC_DATA                241
#define LLRP_TLV_RF_SURVEY_REPORT_DATA   242
#define LLRP_TLV_FREQ_RSSI_LEVEL_ENTRY   243
#define LLRP_TLV_READER_EVENT_NOTI_SPEC  244
#define LLRP_TLV_EVENT_NOTIF_STATE       245
#define LLRP_TLV_READER_EVENT_NOTI_DATA  246
#define LLRP_TLV_HOPPING_EVENT           247
#define LLRP_TLV_GPI_EVENT               248
#define LLRP_TLV_RO_SPEC_EVENT           249
#define LLRP_TLV_REPORT_BUF_LEVEL_WARN   250
#define LLRP_TLV_REPORT_BUF_OVERFLOW_ERR 251
#define LLRP_TLV_READER_EXCEPTION_EVENT  252
#define LLRP_TLV_RF_SURVEY_EVENT         253
#define LLRP_TLV_AI_SPEC_EVENT           254
#define LLRP_TLV_ANTENNA_EVENT           255
#define LLRP_TLV_CONN_ATTEMPT_EVENT      256
#define LLRP_TLV_CONN_CLOSE_EVENT        257
#define LLRP_TLV_LLRP_STATUS             287
#define LLRP_TLV_FIELD_ERROR             288
#define LLRP_TLV_PARAM_ERROR             289
#define LLRP_TLV_C1G2_LLRP_CAP           327
#define LLRP_TLV_C1G2_UHF_RF_MD_TBL      328
#define LLRP_TLV_C1G2_UHF_RF_MD_TBL_ENT  329
#define LLRP_TLV_C1G2_INVENTORY_COMMAND  330
#define LLRP_TLV_C1G2_FILTER             331
#define LLRP_TLV_C1G2_TAG_INV_MASK       332
#define LLRP_TLV_C1G2_TAG_INV_AWARE_FLTR 333
#define LLRP_TLV_C1G2_TAG_INV_UNAWR_FLTR 334
#define LLRP_TLV_C1G2_RF_CONTROL         335
#define LLRP_TLV_C1G2_SINGULATION_CTRL   336
#define LLRP_TLV_C1G2_TAG_INV_AWARE_SING 337
#define LLRP_TLV_C1G2_TAG_SPEC           338
#define LLRP_TLV_C1G2_TARGET_TAG         339
#define LLRP_TLV_C1G2_READ               341
#define LLRP_TLV_C1G2_WRITE              342
#define LLRP_TLV_C1G2_KILL               343
#define LLRP_TLV_C1G2_LOCK               344
#define LLRP_TLV_C1G2_LOCK_PAYLOAD       345
#define LLRP_TLV_C1G2_BLK_ERASE          346
#define LLRP_TLV_C1G2_BLK_WRITE          347
#define LLRP_TLV_C1G2_EPC_MEMORY_SLCTOR  348
#define LLRP_TLV_C1G2_READ_OP_SPEC_RES   349
#define LLRP_TLV_C1G2_WRT_OP_SPEC_RES    350
#define LLRP_TLV_C1G2_KILL_OP_SPEC_RES   351
#define LLRP_TLV_C1G2_LOCK_OP_SPEC_RES   352
#define LLRP_TLV_C1G2_BLK_ERS_OP_SPC_RES 353
#define LLRP_TLV_C1G2_BLK_WRT_OP_SPC_RES 354
#define LLRP_TLV_LOOP_SPEC               355
#define LLRP_TLV_SPEC_LOOP_EVENT         356
#define LLRP_TLV_C1G2_RECOMMISSION       357
#define LLRP_TLV_C1G2_BLK_PERMALOCK      358
#define LLRP_TLV_C1G2_GET_BLK_PERMALOCK  359
#define LLRP_TLV_C1G2_RECOM_OP_SPEC_RES  360
#define LLRP_TLV_C1G2_BLK_PRL_OP_SPC_RES 361
#define LLRP_TLV_C1G2_BLK_PRL_STAT_RES   362
#define LLRP_TLV_MAX_RECEIVE_SENSE       363
#define LLRP_TLV_RF_SURVEY_FREQ_CAP      365
#define LLRP_TLV_CUSTOM_PARAMETER       1023

static const value_string tlv_type[] = {
    { LLRP_TLV_UTC_TIMESTAMP,           "UTC Timestamp"                                  },
    { LLRP_TLV_UPTIME,                  "Uptime"                                         },
    { LLRP_TLV_GENERAL_DEVICE_CAP,      "General Device Capabilities"                    },
    { LLRP_TLV_RECEIVE_SENSE_ENTRY,     "Receive Sensitivity Entry"                      },
    { LLRP_TLV_ANTENNA_AIR_PROTO,       "Antenna Air Protocol"                           },
    { LLRP_TLV_GPIO_CAPABILITIES,       "GPIO Capabilities"                              },
    { LLRP_TLV_LLRP_CAPABILITIES,       "LLRP Capabilities"                              },
    { LLRP_TLV_REGU_CAPABILITIES,       "REGU Capabilities"                              },
    { LLRP_TLV_UHF_CAPABILITIES,        "UHF Capabilities"                               },
    { LLRP_TLV_XMIT_POWER_LEVEL_ENTRY,  "Transmit Power Level Entry"                     },
    { LLRP_TLV_FREQ_INFORMATION,        "Frequency Information"                          },
    { LLRP_TLV_FREQ_HOP_TABLE,          "Frequenct Hop Table"                            },
    { LLRP_TLV_FIXED_FREQ_TABLE,        "Fixed Frequency Table"                          },
    { LLRP_TLV_ANTENNA_RCV_SENSE_RANGE, "Antenna RCV Sensitivity Range"                  },
    { LLRP_TLV_RO_SPEC,                 "RO Spec"                                        },
    { LLRP_TLV_RO_BOUND_SPEC,           "RO Bound Spec"                                  },
    { LLRP_TLV_RO_SPEC_START_TRIGGER,   "RO Spec Start Trigger"                          },
    { LLRP_TLV_PER_TRIGGER_VAL,         "PER Trigger Value"                              },
    { LLRP_TLV_GPI_TRIGGER_VAL,         "GPI Trigger Value"                              },
    { LLRP_TLV_RO_SPEC_STOP_TRIGGER,    "RO Spec Stop Trigger"                           },
    { LLRP_TLV_AI_SPEC,                 "AI Spec"                                        },
    { LLRP_TLV_AI_SPEC_STOP,            "AI Spec Stop"                                   },
    { LLRP_TLV_TAG_OBSERV_TRIGGER,      "Tag Observation Trigger"                        },
    { LLRP_TLV_INVENTORY_PARAM_SPEC,    "Inventory Parameter Spec ID"                    },
    { LLRP_TLV_RF_SURVEY_SPEC,          "RF Survey Spec"                                 },
    { LLRP_TLV_RF_SURVEY_SPEC_STOP_TR,  "RF Survey Spec Stop Trigger"                    },
    { LLRP_TLV_ACCESS_SPEC,             "Access Spec"                                    },
    { LLRP_TLV_ACCESS_SPEC_STOP_TRIG,   "Access Spec Stop Trigger"                       },
    { LLRP_TLV_ACCESS_COMMAND,          "Access Command"                                 },
    { LLRP_TLV_CLIENT_REQ_OP_SPEC,      "Client Request Op Spec"                         },
    { LLRP_TLV_CLIENT_REQ_RESPONSE,     "Client Request Response"                        },
    { LLRP_TLV_LLRP_CONF_STATE_VAL,     "LLRP Configuration State Value"                 },
    { LLRP_TLV_IDENT,                   "Identification"                                 },
    { LLRP_TLV_GPO_WRITE_DATA,          "GPO Write Data"                                 },
    { LLRP_TLV_KEEPALIVE_SPEC,          "Keepalive Spec"                                 },
    { LLRP_TLV_ANTENNA_PROPS,           "Antenna Properties"                             },
    { LLRP_TLV_ANTENNA_CONF,            "Antenna Configuration"                          },
    { LLRP_TLV_RF_RECEIVER,             "RF Receiver"                                    },
    { LLRP_TLV_RF_TRANSMITTER,          "RF Transmitter"                                 },
    { LLRP_TLV_GPI_PORT_CURRENT_STATE,  "GPI Port Current State"                         },
    { LLRP_TLV_EVENTS_AND_REPORTS,      "Events And Reports"                             },
    { LLRP_TLV_RO_REPORT_SPEC,          "RO Report Spec"                                 },
    { LLRP_TLV_TAG_REPORT_CONTENT_SEL,  "Tag Report Content Selector"                    },
    { LLRP_TLV_ACCESS_REPORT_SPEC,      "Access Report Spec"                             },
    { LLRP_TLV_TAG_REPORT_DATA,         "Tag Report Data"                                },
    { LLRP_TLV_EPC_DATA,                "EPC Data"                                       },
    { LLRP_TLV_RF_SURVEY_REPORT_DATA,   "RF Survey Report Data"                          },
    { LLRP_TLV_FREQ_RSSI_LEVEL_ENTRY,   "Frequency RSSI Level Entry"                     },
    { LLRP_TLV_READER_EVENT_NOTI_SPEC,  "Reader Event Notification Spec"                 },
    { LLRP_TLV_EVENT_NOTIF_STATE,       "Event Notification State"                       },
    { LLRP_TLV_READER_EVENT_NOTI_DATA,  "Reader Event Notification Data"                 },
    { LLRP_TLV_HOPPING_EVENT,           "Hopping Event"                                  },
    { LLRP_TLV_GPI_EVENT,               "GPI Event"                                      },
    { LLRP_TLV_RO_SPEC_EVENT,           "RO Spec Event"                                  },
    { LLRP_TLV_REPORT_BUF_LEVEL_WARN,   "Report Buffer Level Warning Event"              },
    { LLRP_TLV_REPORT_BUF_OVERFLOW_ERR, "Report Buffer Overflow Error Event"             },
    { LLRP_TLV_READER_EXCEPTION_EVENT,  "Reader Exception Event"                         },
    { LLRP_TLV_RF_SURVEY_EVENT,         "RF Survey Event"                                },
    { LLRP_TLV_AI_SPEC_EVENT,           "AI Spec Event"                                  },
    { LLRP_TLV_ANTENNA_EVENT,           "ANTENNA Event"                                  },
    { LLRP_TLV_CONN_ATTEMPT_EVENT,      "CONN Attempt Event"                             },
    { LLRP_TLV_CONN_CLOSE_EVENT,        "CONN Close Event"                               },
    { LLRP_TLV_LLRP_STATUS,             "LLRP Status"                                    },
    { LLRP_TLV_FIELD_ERROR,             "Field Error"                                    },
    { LLRP_TLV_PARAM_ERROR,             "Param Error"                                    },
    { LLRP_TLV_C1G2_LLRP_CAP,           "C1G2 LLRP Capabilities"                         },
    { LLRP_TLV_C1G2_UHF_RF_MD_TBL,      "C1G2 UHF RF Mode Table"                         },
    { LLRP_TLV_C1G2_UHF_RF_MD_TBL_ENT,  "C1G2 UHF RF Mode Table Entry"                   },
    { LLRP_TLV_C1G2_INVENTORY_COMMAND,  "C1G2 Inventory Command"                         },
    { LLRP_TLV_C1G2_FILTER,             "C1G2 Filter"                                    },
    { LLRP_TLV_C1G2_TAG_INV_MASK,       "C1G2 Tag Inventory Mask"                        },
    { LLRP_TLV_C1G2_TAG_INV_AWARE_FLTR, "C1G2 Tag Inventory State-Aware Filtre Action"   },
    { LLRP_TLV_C1G2_TAG_INV_UNAWR_FLTR, "C1G2 Tag Inventory State-Unaware Filter Action" },
    { LLRP_TLV_C1G2_RF_CONTROL,         "C1G2 RF Control"                                },
    { LLRP_TLV_C1G2_SINGULATION_CTRL,   "C1G2 Singulation Control"                       },
    { LLRP_TLV_C1G2_TAG_INV_AWARE_SING, "C1G2 Tag Inventory State-Aware Singulation"     },
    { LLRP_TLV_C1G2_TAG_SPEC,           "C1G2 Tag Spec"                                  },
    { LLRP_TLV_C1G2_TARGET_TAG,         "C1G2 Target Tag"                                },
    { LLRP_TLV_C1G2_READ,               "C1G2 Read"                                      },
    { LLRP_TLV_C1G2_WRITE,              "C1G2 Write"                                     },
    { LLRP_TLV_C1G2_KILL,               "C1G2 Kill"                                      },
    { LLRP_TLV_C1G2_LOCK,               "C1G2 Lock"                                      },
    { LLRP_TLV_C1G2_LOCK_PAYLOAD,       "C1G2 Lock Payload"                              },
    { LLRP_TLV_C1G2_BLK_ERASE,          "C1G2 Block Erase"                               },
    { LLRP_TLV_C1G2_BLK_WRITE,          "C1G2 Block Write"                               },
    { LLRP_TLV_C1G2_EPC_MEMORY_SLCTOR,  "C1G2 EPC Memory Selector"                       },
    { LLRP_TLV_C1G2_READ_OP_SPEC_RES,   "C1G2 Read Op Spec Result"                       },
    { LLRP_TLV_C1G2_WRT_OP_SPEC_RES,    "C1G2 Write Op Spec Result"                      },
    { LLRP_TLV_C1G2_KILL_OP_SPEC_RES,   "C1G2 Kill Op Spec Result"                       },
    { LLRP_TLV_C1G2_LOCK_OP_SPEC_RES,   "C1G2 Lock Op Spec Result"                       },
    { LLRP_TLV_C1G2_BLK_ERS_OP_SPC_RES, "C1G2 Block Erase Op Spec Result"                },
    { LLRP_TLV_C1G2_BLK_WRT_OP_SPC_RES, "C1G2 Block Write Op Spec Result"                },
    { LLRP_TLV_LOOP_SPEC,               "Loop Spec"                                      },
    { LLRP_TLV_SPEC_LOOP_EVENT,         "Spec loop event"                                },
    { LLRP_TLV_C1G2_RECOMMISSION,       "C1G2 Recommission"                              },
    { LLRP_TLV_C1G2_BLK_PERMALOCK,      "C1G2 Block Permalock"                           },
    { LLRP_TLV_C1G2_GET_BLK_PERMALOCK,  "C1G2 Get Block Permalock Status"                },
    { LLRP_TLV_C1G2_RECOM_OP_SPEC_RES,  "C1G2 Recommission Op Spec Result"               },
    { LLRP_TLV_C1G2_BLK_PRL_OP_SPC_RES, "C1G2 Block Permalock Op Spec Result"            },
    { LLRP_TLV_C1G2_BLK_PRL_STAT_RES,   "C1G2 Block Permalock Status Op Spec Result"     },
    { LLRP_TLV_MAX_RECEIVE_SENSE,       "Maximum Receive Sensitivity"                    },
    { LLRP_TLV_RF_SURVEY_FREQ_CAP,      "RF Survey Frequency Capabilities"               },
    { LLRP_TLV_CUSTOM_PARAMETER,        "Custom parameter"                               },
    { 0,                                 NULL                                            }
};
static value_string_ext tlv_type_ext = VALUE_STRING_EXT_INIT(tlv_type);

/* TV Parameter Types */
#define LLRP_TV_ANTENNA_ID               1
#define LLRP_TV_FIRST_SEEN_TIME_UTC      2
#define LLRP_TV_FIRST_SEEN_TIME_UPTIME   3
#define LLRP_TV_LAST_SEEN_TIME_UTC       4
#define LLRP_TV_LAST_SEEN_TIME_UPTIME    5
#define LLRP_TV_PEAK_RSSI                6
#define LLRP_TV_CHANNEL_INDEX            7
#define LLRP_TV_TAG_SEEN_COUNT           8
#define LLRP_TV_RO_SPEC_ID               9
#define LLRP_TV_INVENTORY_PARAM_SPEC_ID 10
#define LLRP_TV_C1G2_CRC                11
#define LLRP_TV_C1G2_PC                 12
#define LLRP_TV_EPC96                   13
#define LLRP_TV_SPEC_INDEX              14
#define LLRP_TV_CLIENT_REQ_OP_SPEC_RES  15
#define LLRP_TV_ACCESS_SPEC_ID          16
#define LLRP_TV_OP_SPEC_ID              17
#define LLRP_TV_C1G2_SINGULATION_DET    18
#define LLRP_TV_C1G2_XPC_W1             19
#define LLRP_TV_C1G2_XPC_W2             20

/* Since TV's don't have a length field,
 * use these values instead */
#define LLRP_TV_LEN_ANTENNA_ID               2
#define LLRP_TV_LEN_FIRST_SEEN_TIME_UTC      8
#define LLRP_TV_LEN_FIRST_SEEN_TIME_UPTIME   8
#define LLRP_TV_LEN_LAST_SEEN_TIME_UTC       8
#define LLRP_TV_LEN_LAST_SEEN_TIME_UPTIME    8
#define LLRP_TV_LEN_PEAK_RSSI                1
#define LLRP_TV_LEN_CHANNEL_INDEX            2
#define LLRP_TV_LEN_TAG_SEEN_COUNT           2
#define LLRP_TV_LEN_RO_SPEC_ID               4
#define LLRP_TV_LEN_INVENTORY_PARAM_SPEC_ID  2
#define LLRP_TV_LEN_C1G2_CRC                 2
#define LLRP_TV_LEN_C1G2_PC                  2
#define LLRP_TV_LEN_EPC96                   12
#define LLRP_TV_LEN_SPEC_INDEX               2
#define LLRP_TV_LEN_CLIENT_REQ_OP_SPEC_RES   2
#define LLRP_TV_LEN_ACCESS_SPEC_ID           4
#define LLRP_TV_LEN_OP_SPEC_ID               2
#define LLRP_TV_LEN_C1G2_SINGULATION_DET     4
#define LLRP_TV_LEN_C1G2_XPC_W1              2
#define LLRP_TV_LEN_C1G2_XPC_W2              2

static const value_string tv_type[] = {
    { LLRP_TV_ANTENNA_ID,              "Antenna ID"                    },
    { LLRP_TV_FIRST_SEEN_TIME_UTC,     "First Seen Timestamp UTC"      },
    { LLRP_TV_FIRST_SEEN_TIME_UPTIME,  "First Seen Timestamp Uptime"   },
    { LLRP_TV_LAST_SEEN_TIME_UTC,      "Last Seen Timestamp UTC"       },
    { LLRP_TV_LAST_SEEN_TIME_UPTIME,   "Last Seen Timestamp Uptime"    },
    { LLRP_TV_PEAK_RSSI,               "Peak RSSI"                     },
    { LLRP_TV_CHANNEL_INDEX,           "Channel Index"                 },
    { LLRP_TV_TAG_SEEN_COUNT,          "Tag Seen Count"                },
    { LLRP_TV_RO_SPEC_ID,              "RO Spec ID"                    },
    { LLRP_TV_INVENTORY_PARAM_SPEC_ID, "Inventory Parameter Spec ID"   },
    { LLRP_TV_C1G2_CRC,                "C1G2 CRC"                      },
    { LLRP_TV_C1G2_PC,                 "C1G2 PC"                       },
    { LLRP_TV_EPC96,                   "EPC-96"                        },
    { LLRP_TV_SPEC_INDEX,              "Spec Index"                    },
    { LLRP_TV_CLIENT_REQ_OP_SPEC_RES,  "Client Request Op Spec Result" },
    { LLRP_TV_ACCESS_SPEC_ID,          "Access Spec ID"                },
    { LLRP_TV_OP_SPEC_ID,              "Op Spec ID"                    },
    { LLRP_TV_C1G2_SINGULATION_DET,    "C1G2 Singulation Details"      },
    { LLRP_TV_C1G2_XPC_W1,             "C1G2 XPC W1"                   },
    { LLRP_TV_C1G2_XPC_W2,             "C1G2 XPC W2"                   },
    { 0,                                NULL                           }
};
static value_string_ext tv_type_ext = VALUE_STRING_EXT_INIT(tv_type);

/* Protocol IDs */
#define LLRP_PROT_ID_UNSPECIFIED    0
#define LLRP_PROT_ID_EPC_C1G2       1

static const range_string protocol_id[] = {
    { LLRP_PROT_ID_UNSPECIFIED, LLRP_PROT_ID_UNSPECIFIED, "Unspecified protocol"          },
    { LLRP_PROT_ID_EPC_C1G2, LLRP_PROT_ID_EPC_C1G2,       "EPCGlobal Class 1 Gen 2"       },
    { LLRP_PROT_ID_EPC_C1G2 + 1, 255,                     "Reserved for furure use"       },
    { 0, 0,                                                NULL                           }
};

/* Communication standards */
#define LLRP_COMM_STANDARD_UNSPECIFIED              0
#define LLRP_COMM_STANDARD_US_FCC_PART_15           1
#define LLRP_COMM_STANDARD_ETSI_302_208             2
#define LLRP_COMM_STANDARD_ETSI_300_220             3
#define LLRP_COMM_STANDARD_AUSTRALIA_LIPD_1W        4
#define LLRP_COMM_STANDARD_AUSTRALIA_LIPD_4W        5
#define LLRP_COMM_STANDARD_JAPAN_ARIB_STD_T89       6
#define LLRP_COMM_STANDARD_HONG_KONG_OFTA_1049      7
#define LLRP_COMM_STANDARD_TAIWAN_DGT_LP0002        8
#define LLRP_COMM_STANDARD_KOREA_MIC_ARTICLE_5_2    9

static const value_string comm_standard[] = {
    { LLRP_COMM_STANDARD_UNSPECIFIED,           "Unspecified"           },
    { LLRP_COMM_STANDARD_US_FCC_PART_15,        "US FCC Part 15"        },
    { LLRP_COMM_STANDARD_ETSI_302_208,          "ETSI 302 208"          },
    { LLRP_COMM_STANDARD_ETSI_300_220,          "ETSI 300 220"          },
    { LLRP_COMM_STANDARD_AUSTRALIA_LIPD_1W,     "Australia LIPD 1W"     },
    { LLRP_COMM_STANDARD_AUSTRALIA_LIPD_4W,     "Australia LIPD 4W"     },
    { LLRP_COMM_STANDARD_JAPAN_ARIB_STD_T89,    "Japan_ARIB STD T89"    },
    { LLRP_COMM_STANDARD_HONG_KONG_OFTA_1049,   "Hong_Kong OFTA 1049"   },
    { LLRP_COMM_STANDARD_TAIWAN_DGT_LP0002,     "Taiwan DGT LP0002"     },
    { LLRP_COMM_STANDARD_KOREA_MIC_ARTICLE_5_2, "Korea MIC Article 5 2" },
    { 0,                                        NULL                    }
};
static value_string_ext comm_standard_ext = VALUE_STRING_EXT_INIT(comm_standard);

/* ID type */
#define LLRP_ID_TYPE_MAC    0
#define LLRP_ID_TYPE_EPC    1

static const value_string id_type[] = {
    { LLRP_ID_TYPE_MAC,           "MAC"        },
    { LLRP_ID_TYPE_EPC,           "EPC"        },
    { 0,                          NULL         }
};

/* KeepAlive type */
#define LLRP_KEEPALIVE_TYPE_NULL        0
#define LLRP_KEEPALIVE_TYPE_PERIODIC    1

static const value_string keepalive_type[] = {
    { LLRP_KEEPALIVE_TYPE_NULL,           "Null"            },
    { LLRP_KEEPALIVE_TYPE_PERIODIC,       "Periodic"        },
    { 0,                                  NULL              }
};

/* Notification Event type */
#define LLRP_NOTIFICATION_EVENT_TYPE_UPON_HOPPING_TO_NEXT_CHANNEL     0
#define LLRP_NOTIFICATION_EVENT_TYPE_GPI_EVENT                        1
#define LLRP_NOTIFICATION_EVENT_TYPE_ROSPEC_EVENT                     2
#define LLRP_NOTIFICATION_EVENT_TYPE_REPORT_BUFFER_FILL_WARNING       3
#define LLRP_NOTIFICATION_EVENT_TYPE_READER_EXCEPTION_EVENT           4
#define LLRP_NOTIFICATION_EVENT_TYPE_RFSURVEY_EVENT                   5
#define LLRP_NOTIFICATION_EVENT_TYPE_AISPEC_EVENT                     6
#define LLRP_NOTIFICATION_EVENT_TYPE_AISPEC_EVENT_WITH_DETAILS        7
#define LLRP_NOTIFICATION_EVENT_TYPE_ANTENNA_EVENT                    8
#define LLRP_NOTIFICATION_EVENT_TYPE_SPEC_LOOP_EVENT                  9

static const value_string event_type[] = {
    { LLRP_NOTIFICATION_EVENT_TYPE_UPON_HOPPING_TO_NEXT_CHANNEL,    "Upon hopping to next channel"  },
    { LLRP_NOTIFICATION_EVENT_TYPE_GPI_EVENT,                       "GPI event"                     },
    { LLRP_NOTIFICATION_EVENT_TYPE_ROSPEC_EVENT,                    "ROSpec event"                  },
    { LLRP_NOTIFICATION_EVENT_TYPE_REPORT_BUFFER_FILL_WARNING,      "Report buffer fill warning"    },
    { LLRP_NOTIFICATION_EVENT_TYPE_READER_EXCEPTION_EVENT,          "Reader exception event"        },
    { LLRP_NOTIFICATION_EVENT_TYPE_RFSURVEY_EVENT,                  "RFSurvey event"                },
    { LLRP_NOTIFICATION_EVENT_TYPE_AISPEC_EVENT,                    "AISpec event"                  },
    { LLRP_NOTIFICATION_EVENT_TYPE_AISPEC_EVENT_WITH_DETAILS,       "AISpec event with details"     },
    { LLRP_NOTIFICATION_EVENT_TYPE_ANTENNA_EVENT,                   "Antenna event"                 },
    { LLRP_NOTIFICATION_EVENT_TYPE_SPEC_LOOP_EVENT,                 "SpecLoop event"                },
    { 0,                                                            NULL                            }
};
static value_string_ext event_type_ext = VALUE_STRING_EXT_INIT(event_type);

/* ROSpec event type */
#define LLRP_ROSPEC_EVENT_TYPE_START_OF_ROSPEC          0
#define LLRP_ROSPEC_EVENT_TYPE_END_OF_ROSPEC            1
#define LLRP_ROSPEC_EVENT_TYPE_PREEMPTION_OF_ROSPEC     2

static const value_string roevent_type[] = {
    { LLRP_ROSPEC_EVENT_TYPE_START_OF_ROSPEC,         "Start of ROSpec"      },
    { LLRP_ROSPEC_EVENT_TYPE_END_OF_ROSPEC,           "End of ROSpec"        },
    { LLRP_ROSPEC_EVENT_TYPE_PREEMPTION_OF_ROSPEC,    "Preemption of ROSpec" },
    { 0,                                              NULL                   }
};

/* ROSpec event type */
#define LLRP_RF_SURVEY_EVENT_TYPE_START_OF_SURVEY     0
#define LLRP_RF_SURVEY_EVENT_TYPE_END_OF_SURVEY       1

static const value_string rfevent_type[] = {
    { LLRP_RF_SURVEY_EVENT_TYPE_START_OF_SURVEY,      "Start of survey"      },
    { LLRP_RF_SURVEY_EVENT_TYPE_END_OF_SURVEY,        "End of survey"        },
    { 0,                                              NULL                   }
};

/* AISpec event type */
#define LLRP_AISPEC_EVENT_TYPE_END_OF_AISPEC    0

static const value_string aievent_type[] = {
    { LLRP_AISPEC_EVENT_TYPE_END_OF_AISPEC,          "End of AISpec"        },
    { 0,                                              NULL                  }
};

/* Antenna event type */
#define LLRP_ANTENNA_EVENT_DISCONNECTED      0
#define LLRP_ANTENNA_EVENT_CONNECTED         1

static const value_string antenna_event_type[] = {
    { LLRP_ANTENNA_EVENT_DISCONNECTED,               "Antenna disconnected"  },
    { LLRP_ANTENNA_EVENT_CONNECTED,                  "Antenna connected"     },
    { 0,                                              NULL                   }
};

/* Connection status */
#define LLRP_CONNECTION_SUCCESS                                     0
#define LLRP_CONNECTION_FAILED_READER_INITIATE_ALREADY_EXISTS       1
#define LLRP_CONNECTION_FAILED_CLIENT_INITIATE_ALREADY_EXISTS       2
#define LLRP_CONNECTION_FAILED_OTHER_REASON_THAN_ALREADY_EXISTS     3
#define LLRP_CONNECTION_ANOTHER_CONNECTION_ATTEMPTED                4

static const value_string connection_status[] = {
    { LLRP_CONNECTION_SUCCESS,                                    "Success"                                              },
    { LLRP_CONNECTION_FAILED_READER_INITIATE_ALREADY_EXISTS,      "Failed a reader initiated connection already exists"  },
    { LLRP_CONNECTION_FAILED_CLIENT_INITIATE_ALREADY_EXISTS,      "Failed a client initiated connection already exists"  },
    { LLRP_CONNECTION_FAILED_OTHER_REASON_THAN_ALREADY_EXISTS,    "Failed reason other than a connection already exists" },
    { LLRP_CONNECTION_ANOTHER_CONNECTION_ATTEMPTED,               "Another connection attempted"                         },
    { 0,                                                          NULL                                                   }
};

/* Status code */
#define LLRP_STATUS_CODE_M_SUCCESS                0
#define LLRP_STATUS_CODE_M_PARAMETERERROR       100
#define LLRP_STATUS_CODE_M_FIELDERROR           101
#define LLRP_STATUS_CODE_M_UNEXPECTEDPARAMETER  102
#define LLRP_STATUS_CODE_M_MISSINGPARAMETER     103
#define LLRP_STATUS_CODE_M_DUPLICATEPARAMETER   104
#define LLRP_STATUS_CODE_M_OVERFLOWPARAMETER    105
#define LLRP_STATUS_CODE_M_OVERFLOWFIELD        106
#define LLRP_STATUS_CODE_M_UNKNOWNPARAMETER     107
#define LLRP_STATUS_CODE_M_UNKNOWNFIELD         108
#define LLRP_STATUS_CODE_M_UNSUPPORTEDMESSAGE   109
#define LLRP_STATUS_CODE_M_UNSUPPORTEDVERSION   110
#define LLRP_STATUS_CODE_M_UNSUPPORTEDPARAMETER 111
#define LLRP_STATUS_CODE_P_PARAMETERERROR       200
#define LLRP_STATUS_CODE_P_FIELDERROR           201
#define LLRP_STATUS_CODE_P_UNEXPECTEDPARAMETER  202
#define LLRP_STATUS_CODE_P_MISSINGPARAMETER     203
#define LLRP_STATUS_CODE_P_DUPLICATEPARAMETER   204
#define LLRP_STATUS_CODE_P_OVERFLOWPARAMETER    205
#define LLRP_STATUS_CODE_P_OVERFLOWFIELD        206
#define LLRP_STATUS_CODE_P_UNKNOWNPARAMETER     207
#define LLRP_STATUS_CODE_P_UNKNOWNFIELD         208
#define LLRP_STATUS_CODE_P_UNSUPPORTEDPARAMETER 209
#define LLRP_STATUS_CODE_A_INVALID              300
#define LLRP_STATUS_CODE_A_OUTOFRANGE           301
#define LLRP_STATUS_CODE_R_DEVICEERROR          401

static const value_string status_code[] = {
    { LLRP_STATUS_CODE_M_SUCCESS,             "M_Success"               },
    { LLRP_STATUS_CODE_M_PARAMETERERROR,      "M_ParameterError"        },
    { LLRP_STATUS_CODE_M_FIELDERROR,          "M_FieldError"            },
    { LLRP_STATUS_CODE_M_UNEXPECTEDPARAMETER, "M_UnexpectedParameter"   },
    { LLRP_STATUS_CODE_M_MISSINGPARAMETER,    "M_MissingParameter"      },
    { LLRP_STATUS_CODE_M_DUPLICATEPARAMETER,  "M_DuplicateParameter"    },
    { LLRP_STATUS_CODE_M_OVERFLOWPARAMETER,   "M_OverflowParameter"     },
    { LLRP_STATUS_CODE_M_OVERFLOWFIELD,       "M_OverflowField"         },
    { LLRP_STATUS_CODE_M_UNKNOWNPARAMETER,    "M_UnknownParameter"      },
    { LLRP_STATUS_CODE_M_UNKNOWNFIELD,        "M_UnknownField"          },
    { LLRP_STATUS_CODE_M_UNSUPPORTEDMESSAGE,  "M_UnsupportedMessage"    },
    { LLRP_STATUS_CODE_M_UNSUPPORTEDVERSION,  "M_UnsupportedVersion"    },
    { LLRP_STATUS_CODE_M_UNSUPPORTEDPARAMETER,"M_UnsupportedParameter"  },
    { LLRP_STATUS_CODE_P_PARAMETERERROR,      "P_ParameterError"        },
    { LLRP_STATUS_CODE_P_FIELDERROR,          "P_FieldError"            },
    { LLRP_STATUS_CODE_P_UNEXPECTEDPARAMETER, "P_UnexpectedParameter"   },
    { LLRP_STATUS_CODE_P_MISSINGPARAMETER,    "P_MissingParameter"      },
    { LLRP_STATUS_CODE_P_DUPLICATEPARAMETER,  "P_DuplicateParameter"    },
    { LLRP_STATUS_CODE_P_OVERFLOWPARAMETER,   "P_OverflowParameter"     },
    { LLRP_STATUS_CODE_P_OVERFLOWFIELD,       "P_OverflowField"         },
    { LLRP_STATUS_CODE_P_UNKNOWNPARAMETER,    "P_UnknownParameter"      },
    { LLRP_STATUS_CODE_P_UNKNOWNFIELD,        "P_UnknownField"          },
    { LLRP_STATUS_CODE_P_UNSUPPORTEDPARAMETER,"P_UnsupportedParameter"  },
    { LLRP_STATUS_CODE_A_INVALID,             "A_Invalid"               },
    { LLRP_STATUS_CODE_A_OUTOFRANGE,          "A_OutOfRange"            },
    { LLRP_STATUS_CODE_R_DEVICEERROR,         "R_DeviceError"           },
    { 0,                                      NULL                      }
};
static value_string_ext status_code_ext = VALUE_STRING_EXT_INIT(status_code);

/* C1G2 tag inventory state aware singulation action */
const true_false_string tfs_state_a_b = { "State B", "State A" };
const true_false_string tfs_sl =        { "~SL",     "SL"      };
const true_false_string tfs_all_no =    { "All",     "No"      };

/* Vendors */
#define LLRP_VENDOR_IMPINJ 25882

static const value_string llrp_vendors[] = {
    { LLRP_VENDOR_IMPINJ,  "Impinj" },
    { 0,                   NULL     }
};

/* Vendor subtypes */

/* Impinj custom message types */
#define LLRP_IMPINJ_TYPE_ENABLE_EXTENSIONS            21
#define LLRP_IMPINJ_TYPE_ENABLE_EXTENSIONS_RESPONSE   22
#define LLRP_IMPINJ_TYPE_SAVE_SETTINGS                23
#define LLRP_IMPINJ_TYPE_SAVE_SETTINGS_RESPONSE       24

static const value_string impinj_msg_subtype[] = {
    { LLRP_IMPINJ_TYPE_ENABLE_EXTENSIONS,          "Enable extensions"          },
    { LLRP_IMPINJ_TYPE_ENABLE_EXTENSIONS_RESPONSE, "Enable extensions response" },
    { LLRP_IMPINJ_TYPE_SAVE_SETTINGS,              "Save settings"              },
    { LLRP_IMPINJ_TYPE_SAVE_SETTINGS_RESPONSE,     "Save setting response"      },
    { 0,                                           NULL                         }
};
static value_string_ext impinj_msg_subtype_ext = VALUE_STRING_EXT_INIT(impinj_msg_subtype);

/* Impinj custom parameter types */
#define LLRP_IMPINJ_PARAM_REQUESTED_DATA                           21
#define LLRP_IMPINJ_PARAM_SUBREGULATORY_REGION                     22
#define LLRP_IMPINJ_PARAM_INVENTORY_SEARCH_MODE                    23
#define LLRP_IMPINJ_PARAM_TAG_DIRECTION_REPORTING                  24
#define LLRP_IMPINJ_PARAM_TAG_DIRECTION                            25
#define LLRP_IMPINJ_PARAM_FIXED_FREQUENCY_LIST                     26
#define LLRP_IMPINJ_PARAM_REDUCED_POWER_FREQUENCY_LIST             27
#define LLRP_IMPINJ_PARAM_LOW_DUTY_CYCLE                           28
#define LLRP_IMPINJ_PARAM_DETAILED_VERSION                         29
#define LLRP_IMPINJ_PARAM_FREQUENCY_CAPABILITIES                   30
#define LLRP_IMPINJ_PARAM_TAG_INFORMATION                          31
#define LLRP_IMPINJ_PARAM_FORKLIFT_CONFIGURATION                   32
#define LLRP_IMPINJ_PARAM_FORKLIFT_HEIGHT_THRESHOLD                33
#define LLRP_IMPINJ_PARAM_FORKLIFT_ZEROMOTION_TIME_THRESHOLD       34
#define LLRP_IMPINJ_PARAM_FORKLIFT_COMPANION_BOARD_INFO            35
#define LLRP_IMPINJ_PARAM_GPI_DEBOUNCE_CONFIGURATION               36
#define LLRP_IMPINJ_PARAM_READER_TEMPERATURE                       37
#define LLRP_IMPINJ_PARAM_LINK_MONITOR_CONFIGURATION               38
#define LLRP_IMPINJ_PARAM_REPORT_BUFFER_CONFIGURATION              39
#define LLRP_IMPINJ_PARAM_ACCESS_SPEC_CONFIGURATION                40
#define LLRP_IMPINJ_PARAM_BLOCK_WRITE_WORD_COUNT                   41
#define LLRP_IMPINJ_PARAM_BLOCK_PERMALOCK                          42
#define LLRP_IMPINJ_PARAM_BLOCK_PERMALOCK_OPSPEC_RESULT            43
#define LLRP_IMPINJ_PARAM_GET_BLOCK_PERMALOCK_STATUS               44
#define LLRP_IMPINJ_PARAM_GET_BLOCK_PERMALOCK_STATUS_OPSPEC_RESULT 45
#define LLRP_IMPINJ_PARAM_SET_QT_CONFIG                            46
#define LLRP_IMPINJ_PARAM_SET_QT_CONFIG_OPSPEC_RESULT              47
#define LLRP_IMPINJ_PARAM_GET_QT_CONFIG                            48
#define LLRP_IMPINJ_PARAM_GET_QT_CONFIG_OPSPEC_RESULT              49
#define LLRP_IMPINJ_PARAM_TAG_REPORT_CONTENT_SELECTOR              50
#define LLRP_IMPINJ_PARAM_ENABLE_SERIALIZED_TID                    51
#define LLRP_IMPINJ_PARAM_ENABLE_RF_PHASE_ANGLE                    52
#define LLRP_IMPINJ_PARAM_ENABLE_PEAK_RSSI                         53
#define LLRP_IMPINJ_PARAM_ENABLE_GPS_COORDINATES                   54
#define LLRP_IMPINJ_PARAM_SERIALIZED_TID                           55
#define LLRP_IMPINJ_PARAM_RF_PHASE_ANGLE                           56
#define LLRP_IMPINJ_PARAM_PEAK_RSSI                                57
#define LLRP_IMPINJ_PARAM_GPS_COORDINATES                          58
#define LLRP_IMPINJ_PARAM_LOOP_SPEC                                59
#define LLRP_IMPINJ_PARAM_GPS_NMEA_SENTENCES                       60
#define LLRP_IMPINJ_PARAM_GGA_SENTENCE                             61
#define LLRP_IMPINJ_PARAM_RMC_SENTENCE                             62
#define LLRP_IMPINJ_PARAM_OPSPEC_RETRY_COUNT                       63
#define LLRP_IMPINJ_PARAM_ADVANCE_GPO_CONFIG                       64
#define LLRP_IMPINJ_PARAM_ENABLE_OPTIM_READ                        65
#define LLRP_IMPINJ_PARAM_ACCESS_SPEC_ORDERING                     66
#define LLRP_IMPINJ_PARAM_ENABLE_RF_DOPPLER_FREQ                   67

static const value_string impinj_param_type[] = {
    { LLRP_IMPINJ_PARAM_REQUESTED_DATA,                          "Requested Data"                           },
    { LLRP_IMPINJ_PARAM_SUBREGULATORY_REGION,                    "Sub regulatory region"                    },
    { LLRP_IMPINJ_PARAM_INVENTORY_SEARCH_MODE,                   "Inventory search mode"                    },
    { LLRP_IMPINJ_PARAM_TAG_DIRECTION_REPORTING,                 "Tag direction reporting"                  },
    { LLRP_IMPINJ_PARAM_TAG_DIRECTION,                           "Tag direction"                            },
    { LLRP_IMPINJ_PARAM_FIXED_FREQUENCY_LIST,                    "Fixed frequency list"                     },
    { LLRP_IMPINJ_PARAM_REDUCED_POWER_FREQUENCY_LIST,            "Reduced power frequency list"             },
    { LLRP_IMPINJ_PARAM_LOW_DUTY_CYCLE,                          "Low duty cycle"                           },
    { LLRP_IMPINJ_PARAM_DETAILED_VERSION,                        "Detailed version"                         },
    { LLRP_IMPINJ_PARAM_FREQUENCY_CAPABILITIES,                  "Frequency capabilities"                   },
    { LLRP_IMPINJ_PARAM_TAG_INFORMATION,                         "Tag information"                          },
    { LLRP_IMPINJ_PARAM_FORKLIFT_CONFIGURATION,                  "Forklift configuration"                   },
    { LLRP_IMPINJ_PARAM_FORKLIFT_HEIGHT_THRESHOLD,               "Forklift height threshold"                },
    { LLRP_IMPINJ_PARAM_FORKLIFT_ZEROMOTION_TIME_THRESHOLD,      "Forklift zero motion time treshold"       },
    { LLRP_IMPINJ_PARAM_FORKLIFT_COMPANION_BOARD_INFO,           "Forklift companion board info"            },
    { LLRP_IMPINJ_PARAM_GPI_DEBOUNCE_CONFIGURATION,              "Gpi debounce configuration"               },
    { LLRP_IMPINJ_PARAM_READER_TEMPERATURE,                      "Reader temperature"                       },
    { LLRP_IMPINJ_PARAM_LINK_MONITOR_CONFIGURATION,              "Link monitor configuration"               },
    { LLRP_IMPINJ_PARAM_REPORT_BUFFER_CONFIGURATION,             "Report buffer configuration"              },
    { LLRP_IMPINJ_PARAM_ACCESS_SPEC_CONFIGURATION,               "Access spec configuration"                },
    { LLRP_IMPINJ_PARAM_BLOCK_WRITE_WORD_COUNT,                  "Block write word count"                   },
    { LLRP_IMPINJ_PARAM_BLOCK_PERMALOCK,                         "Block permalock"                          },
    { LLRP_IMPINJ_PARAM_BLOCK_PERMALOCK_OPSPEC_RESULT,           "Block permalock OpSpec result"            },
    { LLRP_IMPINJ_PARAM_GET_BLOCK_PERMALOCK_STATUS,              "Get block permalock status"               },
    { LLRP_IMPINJ_PARAM_GET_BLOCK_PERMALOCK_STATUS_OPSPEC_RESULT,"Get block permalock status OpSpec result" },
    { LLRP_IMPINJ_PARAM_SET_QT_CONFIG,                           "Set QT config"                            },
    { LLRP_IMPINJ_PARAM_SET_QT_CONFIG_OPSPEC_RESULT,             "Set QT config OpSpec result"              },
    { LLRP_IMPINJ_PARAM_GET_QT_CONFIG,                           "Get QT config"                            },
    { LLRP_IMPINJ_PARAM_GET_QT_CONFIG_OPSPEC_RESULT,             "Get QT config OpSpec result"              },
    { LLRP_IMPINJ_PARAM_TAG_REPORT_CONTENT_SELECTOR,             "Tag report content selector"              },
    { LLRP_IMPINJ_PARAM_ENABLE_SERIALIZED_TID,                   "Enable serialized TID"                    },
    { LLRP_IMPINJ_PARAM_ENABLE_RF_PHASE_ANGLE,                   "Enable RF phase angle"                    },
    { LLRP_IMPINJ_PARAM_ENABLE_PEAK_RSSI,                        "Enable peak RSSI"                         },
    { LLRP_IMPINJ_PARAM_ENABLE_GPS_COORDINATES,                  "Enable GPS coordinates"                   },
    { LLRP_IMPINJ_PARAM_SERIALIZED_TID,                          "Serialized TID"                           },
    { LLRP_IMPINJ_PARAM_RF_PHASE_ANGLE,                          "RF phase angle"                           },
    { LLRP_IMPINJ_PARAM_PEAK_RSSI,                               "Peak RSSI"                                },
    { LLRP_IMPINJ_PARAM_GPS_COORDINATES,                         "GPS coordinates"                          },
    { LLRP_IMPINJ_PARAM_LOOP_SPEC,                               "LoopSpec"                                 },
    { LLRP_IMPINJ_PARAM_GPS_NMEA_SENTENCES,                      "GPS NMEA sentences"                       },
    { LLRP_IMPINJ_PARAM_GGA_SENTENCE,                            "GGA sentence"                             },
    { LLRP_IMPINJ_PARAM_RMC_SENTENCE,                            "RMC sentence"                             },
    { LLRP_IMPINJ_PARAM_OPSPEC_RETRY_COUNT,                      "OpSpec retry count"                       },
    { LLRP_IMPINJ_PARAM_ADVANCE_GPO_CONFIG,                      "Advanced GPO configuration"               },
    { LLRP_IMPINJ_PARAM_ENABLE_OPTIM_READ,                       "Enable optimized read"                    },
    { LLRP_IMPINJ_PARAM_ACCESS_SPEC_ORDERING,                    "AccessSpec ordering"                      },
    { LLRP_IMPINJ_PARAM_ENABLE_RF_DOPPLER_FREQ,                  "Enable RF doppler frequency"              },
    { 0,                                                         NULL                                       }
};
static value_string_ext impinj_param_type_ext = VALUE_STRING_EXT_INIT(impinj_param_type);

/* Impinj requested data */
#define LLRP_IMPINJ_REQ_DATA_ALL_CAPABILITIES                1000
#define LLRP_IMPINJ_REQ_DATA_DETAILED_VERSION                1001
#define LLRP_IMPINJ_REQ_DATA_FREQUENCY_CAPABILITIES          1002
#define LLRP_IMPINJ_REQ_DATA_CONFIGURATION                   2000
#define LLRP_IMPINJ_REQ_DATA_SUB_REGULATORY_REGION           2001
#define LLRP_IMPINJ_REQ_DATA_FORKLIFT_CONFIGURATION          2002
#define LLRP_IMPINJ_REQ_DATA_GPI_DEBOUNCE_CONFIGURATION      2003
#define LLRP_IMPINJ_REQ_DATA_READER_TEMPERATURE              2004
#define LLRP_IMPINJ_REQ_DATA_LINK_MONITOR_CONFIGURATION      2005
#define LLRP_IMPINJ_REQ_DATA_REPORT_BUFFER_CONFIGURATION     2006
#define LLRP_IMPINJ_REQ_DATA_ACCESS_SPEC_CONFIGURATION       2007
#define LLRP_IMPINJ_REQ_DATA_GPS_NMEA_SENTENCES              2008


static const value_string impinj_req_data[] = {
    { LLRP_IMPINJ_REQ_DATA_ALL_CAPABILITIES,            "All capabilities"            },
    { LLRP_IMPINJ_REQ_DATA_DETAILED_VERSION,            "Detailed version"            },
    { LLRP_IMPINJ_REQ_DATA_FREQUENCY_CAPABILITIES,      "Frequency capabilities"      },
    { LLRP_IMPINJ_REQ_DATA_CONFIGURATION,               "Configuration"               },
    { LLRP_IMPINJ_REQ_DATA_SUB_REGULATORY_REGION,       "Sub regulatory region"       },
    { LLRP_IMPINJ_REQ_DATA_FORKLIFT_CONFIGURATION,      "Forklift configuration"      },
    { LLRP_IMPINJ_REQ_DATA_GPI_DEBOUNCE_CONFIGURATION,  "GPI debounce configuration"  },
    { LLRP_IMPINJ_REQ_DATA_READER_TEMPERATURE,          "Reader temperature"          },
    { LLRP_IMPINJ_REQ_DATA_LINK_MONITOR_CONFIGURATION,  "Link monitor configuration"  },
    { LLRP_IMPINJ_REQ_DATA_REPORT_BUFFER_CONFIGURATION, "Report buffer configuration" },
    { LLRP_IMPINJ_REQ_DATA_ACCESS_SPEC_CONFIGURATION,   "Access spec configuration"   },
    { LLRP_IMPINJ_REQ_DATA_GPS_NMEA_SENTENCES,          "GPS NMEA sentences"          },
    { 0,                                                NULL                          }
};
static value_string_ext impinj_req_data_ext = VALUE_STRING_EXT_INIT(impinj_req_data);

/* Impinj regulatory region */
#define LLRP_IMPINJ_REG_REGION_FCC_PART_15_247                   0
#define LLRP_IMPINJ_REG_REGION_ETSI_EN_300_220                   1
#define LLRP_IMPINJ_REG_REGION_ETSI_EN_302_208_WITH_LBT          2
#define LLRP_IMPINJ_REG_REGION_HONG_KONG_920_925_MHZ             3
#define LLRP_IMPINJ_REG_REGION_TAIWAN_922_928_MHZ                4
#define LLRP_IMPINJ_REG_REGION_JAPAN_952_954_MHZ                 5
#define LLRP_IMPINJ_REG_REGION_JAPAN_952_954_MHZ_LOW_POWER       6
#define LLRP_IMPINJ_REG_REGION_ETSI_EN_302_208_V1_2_1            7
#define LLRP_IMPINJ_REG_REGION_KOREA_910_914_MHZ                 8
#define LLRP_IMPINJ_REG_REGION_MALAYSIA_919_923_MHZ              9
#define LLRP_IMPINJ_REG_REGION_CHINA_920_925_MHZ                10
#define LLRP_IMPINJ_REG_REGION_JAPAN_952_954_MHZ_WITHOUT_LBT    11
#define LLRP_IMPINJ_REG_REGION_SOUTH_AFRICA_915_919_MHZ         12
#define LLRP_IMPINJ_REG_REGION_BRAZIL_902_907_AND_915_928_MHZ   13
#define LLRP_IMPINJ_REG_REGION_THAILAND_920_925_MHZ             14
#define LLRP_IMPINJ_REG_REGION_SINGAPORE_920_925_MHZ            15
#define LLRP_IMPINJ_REG_REGION_AUSTRALIA_920_926_MHZ            16
#define LLRP_IMPINJ_REG_REGION_INDIA_865_867_MHZ                17
#define LLRP_IMPINJ_REG_REGION_URUGUAY_916_928_MHZ              18
#define LLRP_IMPINJ_REG_REGION_VIETNAM_920_925_MHZ              19
#define LLRP_IMPINJ_REG_REGION_ISRAEL_915_917_MHZ               20

static const value_string impinj_reg_region[] = {
    { LLRP_IMPINJ_REG_REGION_FCC_PART_15_247,                "Fcc part 15 247"                },
    { LLRP_IMPINJ_REG_REGION_ETSI_EN_300_220,                "ETSI EN 300 220"                },
    { LLRP_IMPINJ_REG_REGION_ETSI_EN_302_208_WITH_LBT,       "ETSI EN 302 208 with LBT"       },
    { LLRP_IMPINJ_REG_REGION_HONG_KONG_920_925_MHZ,          "Hong kong 920-925 MHz"          },
    { LLRP_IMPINJ_REG_REGION_TAIWAN_922_928_MHZ,             "Taiwan 922-928 MHz"             },
    { LLRP_IMPINJ_REG_REGION_JAPAN_952_954_MHZ,              "Japan 952-954 MHz"              },
    { LLRP_IMPINJ_REG_REGION_JAPAN_952_954_MHZ_LOW_POWER,    "Japan 952-954 MHz low power"    },
    { LLRP_IMPINJ_REG_REGION_ETSI_EN_302_208_V1_2_1,         "ETSI EN 302 208 v1.2.1"         },
    { LLRP_IMPINJ_REG_REGION_KOREA_910_914_MHZ,              "Korea 910-914 MHz"              },
    { LLRP_IMPINJ_REG_REGION_MALAYSIA_919_923_MHZ,           "Malaysia 919-923 MHz"           },
    { LLRP_IMPINJ_REG_REGION_CHINA_920_925_MHZ,              "China 920-925 MHz"              },
    { LLRP_IMPINJ_REG_REGION_JAPAN_952_954_MHZ_WITHOUT_LBT,  "Japan 952-954 MHz without LBT"  },
    { LLRP_IMPINJ_REG_REGION_SOUTH_AFRICA_915_919_MHZ,       "South africa 915-919 MHz"       },
    { LLRP_IMPINJ_REG_REGION_BRAZIL_902_907_AND_915_928_MHZ, "Brazil 902-907 and 915-928 MHz" },
    { LLRP_IMPINJ_REG_REGION_THAILAND_920_925_MHZ,           "Thailand 920-925 MHz"           },
    { LLRP_IMPINJ_REG_REGION_SINGAPORE_920_925_MHZ,          "Singapore 920-925 MHz"          },
    { LLRP_IMPINJ_REG_REGION_AUSTRALIA_920_926_MHZ,          "Australia 920-926 MHz"          },
    { LLRP_IMPINJ_REG_REGION_INDIA_865_867_MHZ,              "India 865-867 MHz"              },
    { LLRP_IMPINJ_REG_REGION_URUGUAY_916_928_MHZ,            "Uruguay 916-928 MHz"            },
    { LLRP_IMPINJ_REG_REGION_VIETNAM_920_925_MHZ,            "Vietnam 920-925 MHz"            },
    { LLRP_IMPINJ_REG_REGION_ISRAEL_915_917_MHZ,             "Israel 915-917 MHz"             },
    { 0,                                                     NULL                             }
};
static value_string_ext impinj_reg_region_ext = VALUE_STRING_EXT_INIT(impinj_reg_region);

/* Impinj inventory search type */
#define LLRP_IMPINJ_SEARCH_TYPE_READER_SELECTED         0
#define LLRP_IMPINJ_SEARCH_TYPE_SINGLE_TARGET           1
#define LLRP_IMPINJ_SEARCH_TYPE_DUAL_TARGET             2
#define LLRP_IMPINJ_SEARCH_TYPE_SINGLE_TARGET_WITH_SUPP 3

static const value_string impinj_search_mode[] = {
    { LLRP_IMPINJ_SEARCH_TYPE_READER_SELECTED,           "Reader selected"                },
    { LLRP_IMPINJ_SEARCH_TYPE_SINGLE_TARGET,             "Single target"                  },
    { LLRP_IMPINJ_SEARCH_TYPE_DUAL_TARGET,               "Dual target"                    },
    { LLRP_IMPINJ_SEARCH_TYPE_SINGLE_TARGET_WITH_SUPP,   "Single target with suppression" },
    { 0,                                                 NULL                             }
};

/* Impinj antenna configuration */
#define LLRP_IMPINJ_ANT_CONF_DUAL   1
#define LLRP_IMPINJ_ANT_CONF_QUAD   2

static const value_string impinj_ant_conf[] = {
    { LLRP_IMPINJ_ANT_CONF_DUAL, "Dual antenna" },
    { LLRP_IMPINJ_ANT_CONF_QUAD, "Quad antenna" },
    { 0,                        NULL            }
};

/* Impinj tag direction */
#define LLRP_IMPINJ_TAG_DIR_INDETERMINED  0
#define LLRP_IMPINJ_TAG_DIR_FROM_2_TO_1   1
#define LLRP_IMPINJ_TAG_DIR_FROM_1_TO_2   2

static const value_string impinj_tag_dir[] = {
    { LLRP_IMPINJ_TAG_DIR_INDETERMINED, "Indeterminate"       },
    { LLRP_IMPINJ_TAG_DIR_FROM_2_TO_1,  "From side2 to side1" },
    { LLRP_IMPINJ_TAG_DIR_FROM_1_TO_2,  "From side1 to side2" },
    { 0,                                NULL                  }
};

/* Impinj fixed frequency mode */
#define LLRP_IMPINJ_FIX_FREQ_MODE_DISABLED      0
#define LLRP_IMPINJ_FIX_FREQ_MODE_AUTO_SELECT   1
#define LLRP_IMPINJ_FIX_FREQ_MODE_CHANNEL_LIST  2

static const value_string impinj_fix_freq_mode[] = {
    { LLRP_IMPINJ_FIX_FREQ_MODE_DISABLED,      "Disabled"     },
    { LLRP_IMPINJ_FIX_FREQ_MODE_AUTO_SELECT,   "Auto select"  },
    { LLRP_IMPINJ_FIX_FREQ_MODE_CHANNEL_LIST,  "Channel list" },
    { 0,                                       NULL           }
};

/* Impinj enabled/disabled */
#define LLRP_IMPINJ_BOOLEAN_DISABLED      0
#define LLRP_IMPINJ_BOOLEAN_ENABLED       1

static const value_string impinj_boolean[] = {
    { LLRP_IMPINJ_BOOLEAN_DISABLED, "Disabled" },
    { LLRP_IMPINJ_BOOLEAN_ENABLED,  "Enabled"  },
    { 0,                            NULL       }
};

/* Impinj report buffer mode */
#define LLRP_IMPINJ_REPORT_BUFF_MODE_NORMAL      0
#define LLRP_IMPINJ_REPORT_BUFF_MODE_LOW_LATENCY 1

static const value_string impinj_report_buff_mode[] = {
    { LLRP_IMPINJ_REPORT_BUFF_MODE_NORMAL,       "Normal"      },
    { LLRP_IMPINJ_REPORT_BUFF_MODE_LOW_LATENCY,  "Low latency" },
    { 0,                                         NULL          }
};

/* Impinj permalock operation result */
#define LLRP_IMPINJ_PERMALOCK_SUCCESS                      0
#define LLRP_IMPINJ_PERMALOCK_INSUFFICIENT_POWER           1
#define LLRP_IMPINJ_PERMALOCK_NONSPECIFIC_TAG_ERROR        2
#define LLRP_IMPINJ_PERMALOCK_NO_RESPONSE_FROM_TAG         3
#define LLRP_IMPINJ_PERMALOCK_NONSPECIFIC_READER_ERROR     4
#define LLRP_IMPINJ_PERMALOCK_INCORRECT_PASSWORD_ERROR     5
#define LLRP_IMPINJ_PERMALOCK_TAG_MEMORY_OVERRUN_ERROR     6

static const value_string impinj_permalock_result[] = {
    { LLRP_IMPINJ_PERMALOCK_SUCCESS,                  "Success"                  },
    { LLRP_IMPINJ_PERMALOCK_INSUFFICIENT_POWER,       "Insufficient power"       },
    { LLRP_IMPINJ_PERMALOCK_NONSPECIFIC_TAG_ERROR,    "Nonspecific tag error"    },
    { LLRP_IMPINJ_PERMALOCK_NO_RESPONSE_FROM_TAG,     "No response from tag"     },
    { LLRP_IMPINJ_PERMALOCK_NONSPECIFIC_READER_ERROR, "Nonspecific reader error" },
    { LLRP_IMPINJ_PERMALOCK_INCORRECT_PASSWORD_ERROR, "Incorrect password error" },
    { LLRP_IMPINJ_PERMALOCK_TAG_MEMORY_OVERRUN_ERROR, "Tag memory overrun error" },
    { 0,                                              NULL                       }
};
static value_string_ext impinj_permalock_result_ext = VALUE_STRING_EXT_INIT(impinj_permalock_result);

/* Impinj get block permalock operation result */
#define LLRP_IMPINJ_BLOCK_PERMALOCK_SUCCESS                      0
#define LLRP_IMPINJ_BLOCK_PERMALOCK_NONSPECIFIC_TAG_ERROR        1
#define LLRP_IMPINJ_BLOCK_PERMALOCK_NO_RESPONSE_FROM_TAG         2
#define LLRP_IMPINJ_BLOCK_PERMALOCK_NONSPECIFIC_READER_ERROR     3
#define LLRP_IMPINJ_BLOCK_PERMALOCK_INCORRECT_PASSWORD_ERROR     4
#define LLRP_IMPINJ_BLOCK_PERMALOCK_TAG_MEMORY_OVERRUN_ERROR     5

static const value_string impinj_block_permalock_result[] = {
    { LLRP_IMPINJ_BLOCK_PERMALOCK_SUCCESS,                  "Success"                  },
    { LLRP_IMPINJ_BLOCK_PERMALOCK_NONSPECIFIC_TAG_ERROR,    "Nonspecific tag error"    },
    { LLRP_IMPINJ_BLOCK_PERMALOCK_NO_RESPONSE_FROM_TAG,     "No response from tag"     },
    { LLRP_IMPINJ_BLOCK_PERMALOCK_NONSPECIFIC_READER_ERROR, "Nonspecific reader error" },
    { LLRP_IMPINJ_BLOCK_PERMALOCK_INCORRECT_PASSWORD_ERROR, "Incorrect password error" },
    { LLRP_IMPINJ_BLOCK_PERMALOCK_TAG_MEMORY_OVERRUN_ERROR, "Tag memory overrun error" },
    { 0,                                                    NULL                       }
};
static value_string_ext impinj_block_permalock_result_ext = VALUE_STRING_EXT_INIT(impinj_block_permalock_result);

/* Impinj data profile parameter */
#define LLRP_IMPINJ_DATA_PROFILE_UNKNOWN        0
#define LLRP_IMPINJ_DATA_PROFILE_PRIVATE        1
#define LLRP_IMPINJ_DATA_PROFILE_PUBLIC         2

static const value_string impinj_data_profile[] = {
    { LLRP_IMPINJ_DATA_PROFILE_UNKNOWN,  "Unknown" },
    { LLRP_IMPINJ_DATA_PROFILE_PRIVATE,  "Private" },
    { LLRP_IMPINJ_DATA_PROFILE_PUBLIC,   "Public"  },
    { 0,                                 NULL      }
};

/* Impinj access range parameter */
#define LLRP_IMPINJ_ACCESS_RANGE_UNKNOWN        0
#define LLRP_IMPINJ_ACCESS_RANGE_NORMAL_RANGE   1
#define LLRP_IMPINJ_ACCESS_RANGE_SHORT_RANGE    2

static const value_string impinj_access_range[] = {
    { LLRP_IMPINJ_ACCESS_RANGE_UNKNOWN,       "Unknown"      },
    { LLRP_IMPINJ_ACCESS_RANGE_NORMAL_RANGE,  "Normal range" },
    { LLRP_IMPINJ_ACCESS_RANGE_SHORT_RANGE,   "Short range"  },
    { 0,                                      NULL           }
};

/* Impinj persistence parameter */
#define LLRP_IMPINJ_PERSISTENCE_UNKNOWN     0
#define LLRP_IMPINJ_PERSISTENCE_TEMPORARY   1
#define LLRP_IMPINJ_PERSISTENCE_PERMANENT   2

static const value_string impinj_persistence[] = {
    { LLRP_IMPINJ_PERSISTENCE_UNKNOWN,     "Unknown"    },
    { LLRP_IMPINJ_PERSISTENCE_TEMPORARY,   "Temporary"  },
    { LLRP_IMPINJ_PERSISTENCE_PERMANENT,   "Permament"  },
    { 0,                                   NULL         }
};

/* Impinj set QT config result */
#define LLRP_IMPINJ_SET_QT_CONFIG_SUCCESS                      0
#define LLRP_IMPINJ_SET_QT_CONFIG_INSUFFICIENT_POWER           1
#define LLRP_IMPINJ_SET_QT_CONFIG_NONSPECIFIC_TAG_ERROR        2
#define LLRP_IMPINJ_SET_QT_CONFIG_NO_RESPONSE_FROM_TAG         3
#define LLRP_IMPINJ_SET_QT_CONFIG_NONSPECIFIC_READER_ERROR     4
#define LLRP_IMPINJ_SET_QT_CONFIG_INCORRECT_PASSWORD_ERROR     5

static const value_string impinj_set_qt_config_result[] = {
    { LLRP_IMPINJ_SET_QT_CONFIG_SUCCESS,                  "Success"                  },
    { LLRP_IMPINJ_SET_QT_CONFIG_INSUFFICIENT_POWER,       "Insufficient power"       },
    { LLRP_IMPINJ_SET_QT_CONFIG_NONSPECIFIC_TAG_ERROR,    "Nonspecific tag error"    },
    { LLRP_IMPINJ_SET_QT_CONFIG_NO_RESPONSE_FROM_TAG,     "No response from tag"     },
    { LLRP_IMPINJ_SET_QT_CONFIG_NONSPECIFIC_READER_ERROR, "Nonspecific reader error" },
    { LLRP_IMPINJ_SET_QT_CONFIG_INCORRECT_PASSWORD_ERROR, "Incorrect password error" },
    { 0,                                                  NULL                       }
};
static value_string_ext impinj_set_qt_config_result_ext = VALUE_STRING_EXT_INIT(impinj_set_qt_config_result);

/* Impinj get QT config result */
#define LLRP_IMPINJ_GET_QT_CONFIG_SUCCESS                      0
#define LLRP_IMPINJ_GET_QT_CONFIG_NONSPECIFIC_TAG_ERROR        1
#define LLRP_IMPINJ_GET_QT_CONFIG_NO_RESPONSE_FROM_TAG         2
#define LLRP_IMPINJ_GET_QT_CONFIG_NONSPECIFIC_READER_ERROR     3
#define LLRP_IMPINJ_GET_QT_CONFIG_INCORRECT_PASSWORD_ERROR     4

static const value_string impinj_get_qt_config_result[] = {
    { LLRP_IMPINJ_GET_QT_CONFIG_SUCCESS,                  "Success"                  },
    { LLRP_IMPINJ_GET_QT_CONFIG_NONSPECIFIC_TAG_ERROR,    "Nonspecific tag error"    },
    { LLRP_IMPINJ_GET_QT_CONFIG_NO_RESPONSE_FROM_TAG,     "No response from tag"     },
    { LLRP_IMPINJ_GET_QT_CONFIG_NONSPECIFIC_READER_ERROR, "Nonspecific reader error" },
    { LLRP_IMPINJ_GET_QT_CONFIG_INCORRECT_PASSWORD_ERROR, "Incorrect password error" },
    { 0,                                                  NULL                       }
};
static value_string_ext impinj_get_qt_config_result_ext = VALUE_STRING_EXT_INIT(impinj_get_qt_config_result);

/* Impinj access spec ordering */
#define LLRP_IMPINJ_ACCESS_SPEC_ORDERING_FIFO        0
#define LLRP_IMPINJ_ACCESS_SPEC_ORDERING_ASCENDING   1

static const value_string impinj_access_spec_ordering[] = {
    { LLRP_IMPINJ_ACCESS_SPEC_ORDERING_FIFO,       "FIFO"      },
    { LLRP_IMPINJ_ACCESS_SPEC_ORDERING_ASCENDING,  "Ascending" },
    { 0,                                           NULL        }
};

/* Impinj GPO mode */
#define LLRP_IMPINJ_GPO_MODE_NORMAL                         0
#define LLRP_IMPINJ_GPO_MODE_PULSED                         1
#define LLRP_IMPINJ_GPO_MODE_READER_OPERATIONAL_STATUS      2
#define LLRP_IMPINJ_GPO_MODE_LLRP_CONNECTION_STATUS         3
#define LLRP_IMPINJ_GPO_MODE_READER_INVENTORY_STATUS        4
#define LLRP_IMPINJ_GPO_MODE_NETWORK_CONNECTION_STATUS      5
#define LLRP_IMPINJ_GPO_MODE_READER_INVENTORY_TAGS_STATUS   6

static const value_string impinj_gpo_mode[] = {
    { LLRP_IMPINJ_GPO_MODE_NORMAL,                        "Normal"                       },
    { LLRP_IMPINJ_GPO_MODE_PULSED,                        "Pulsed"                       },
    { LLRP_IMPINJ_GPO_MODE_READER_OPERATIONAL_STATUS,     "Reader operational status"    },
    { LLRP_IMPINJ_GPO_MODE_LLRP_CONNECTION_STATUS,        "LLRP connection status"       },
    { LLRP_IMPINJ_GPO_MODE_READER_INVENTORY_STATUS,       "Reader inventory status"      },
    { LLRP_IMPINJ_GPO_MODE_NETWORK_CONNECTION_STATUS,     "Network connection status"    },
    { LLRP_IMPINJ_GPO_MODE_READER_INVENTORY_TAGS_STATUS,  "Reader inventory tags status" },
    { 0,                                                  NULL                           }
};
static value_string_ext impinj_gpo_mode_ext = VALUE_STRING_EXT_INIT(impinj_gpo_mode);

/* Misc */
#define LLRP_ROSPEC_ALL      0
#define LLRP_ANTENNA_ALL     0
#define LLRP_GPI_PORT_ALL    0
#define LLRP_GPO_PORT_ALL    0
#define LLRP_ACCESSSPEC_ALL  0
#define LLRP_TLV_LEN_MIN     4
#define LLRP_HEADER_LENGTH  10
#define LLRP_NO_LIMIT        0

/* Parameter list header field construction */

#define HF_LLRP_PARAMETER_LIST   \
    HF_PARAM(num_gpi, "Number of GPI ports",  FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(num_gpo, "Number of GPO ports",  FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(microseconds, "Microseconds",    FT_UINT64, BASE_DEC, NULL, 0) \
    HF_PARAM(max_supported_antenna, "Max number of antenna supported", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(can_set_antenna_prop, "Can set antenna properties", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x8000) \
    HF_PARAM(has_utc_clock, "Has UTC clock capabilities", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x4000) \
    HF_PARAM(device_manufacturer, "Device manufacturer name", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(model, "Model name", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(firmware_version, "Reader firmware version", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(max_receive_sense, "Maximum sensitivity value", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(index, "Index", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(receive_sense, "Receive sensitivity value", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(receive_sense_index_min, "Receive sensitivity index min", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(receive_sense_index_max, "Receive sensitivity index max", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(num_protocols, "Number of protocols", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(protocol_id, "Protocol ID", FT_UINT16, BASE_DEC | BASE_RANGE_STRING, RVALS(protocol_id), 0) \
    HF_PARAM(can_do_survey, "Can do RF survey", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(can_report_buffer_warning, "Can report buffer fill warning", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x40) \
    HF_PARAM(support_client_opspec, "Support client request OpSpec", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x20) \
    HF_PARAM(can_stateaware, "Can do tag inventory state aware singulation", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x10) \
    HF_PARAM(support_holding, "Support event and report holding", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x08) \
    HF_PARAM(max_priority_supported, "Max priority level supported", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(client_opspec_timeout, "Client request OpSpec timeout", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(max_num_rospec, "Maximum number of ROSpecs", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(max_num_spec_per_rospec, "Maximum number of spec per ROSpec", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(max_num_inventory_per_aispec, "Maximum number of Inventory Spec per AISpec", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(max_num_accessspec, "Maximum number of AccessSpec", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(max_num_opspec_per_accressspec, "Maximum number of OpSpec per AccessSpec", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(country_code, "Contry code", FT_UINT16, BASE_DEC, NULL, 0) /* TODO add translation */ \
    HF_PARAM(comm_standard, "Communication standard", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &comm_standard_ext, 0) \
    HF_PARAM(transmit_power, "Transmit power value", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(hopping, "Hopping", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(hop_table_id, "Hop table ID", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(rfu, "Reserved for future use", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(num_hops, "Number of hops", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(frequency, "Frequency", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(num_freqs, "Number of frequencies", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(min_freq, "Minimum frequency", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(max_freq, "Maximum frequency", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(rospec_id, "ROSpec ID", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(priority, "Priority", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(cur_state, "Current state", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(rospec_start_trig_type, "ROSpec start trigger type", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(offset, "Offset", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(period, "Period", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(gpi_event, "GPI event", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(timeout, "Timeout", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(rospec_stop_trig_type, "ROSpec stop trigger type", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(duration_trig, "Duration trigger value", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(antenna_count, "Antenna count", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(antenna, "Antenna ID", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(aispec_stop_trig_type, "AISpec stop trigger type", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(trig_type, "Trigger type", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(number_of_tags, "Number of tags", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(number_of_attempts, "Number of attempts", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(t, "T", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(inventory_spec_id, "Inventory parameter spec id", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(start_freq, "Start frequency", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(stop_freq, "Stop frequency", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(stop_trig_type, "Stop trigger type", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(n_4, "N", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(duration, "Duration", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(accessspec_id, "AccessSpec ID", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(access_cur_state, "Current state", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(access_stop_trig_type, "AccessSpec Stop trigger", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(operation_count, "Operation count value", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(opspec_id, "OpSpec ID", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(conf_value, "Configuration value", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(id_type, "ID type", FT_UINT8, BASE_DEC, VALS(id_type), 0) \
    HF_PARAM(reader_id, "Reader ID", FT_UINT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(gpo_data, "GPO data", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(keepalive_trig_type, "KeepAlive trigger type", FT_UINT8, BASE_DEC, VALS(keepalive_type), 0) \
    HF_PARAM(time_iterval, "Time interval", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(antenna_connected, "Antenna connected", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(antenna_gain, "Antenna gain", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(receiver_sense, "Receiver sensitivity", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(channel_idx, "Channel index", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(gpi_config, "GPI config", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(gpi_state, "GPI state", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(hold_events_and_reports, "Hold events and reports upon reconnect", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(ro_report_trig, "RO report trigger", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(n_2, "N", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(enable_rospec_id, "Enable ROSpec ID", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x8000) \
    HF_PARAM(enable_spec_idx, "Enable spec index", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x4000) \
    HF_PARAM(enable_inv_spec_id, "Enable inventory spec ID", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x2000) \
    HF_PARAM(enable_antenna_id, "Enable antenna ID", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x1000) \
    HF_PARAM(enable_channel_idx, "Enable channel index", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x0800) \
    HF_PARAM(enable_peak_rssi, "Enable peak RSSI", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x0400) \
    HF_PARAM(enable_first_seen, "Enable first seen timestamp", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x0200) \
    HF_PARAM(enable_last_seen, "Enable last seen timestamp", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x0100) \
    HF_PARAM(enable_seen_count, "Enable tag seen count", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x0080) \
    HF_PARAM(enable_accessspec_id, "Enable AccessSpec ID", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x0040) \
    HF_PARAM(access_report_trig, "Access report trigger", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(length_bits, "Bit field length (bits)", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(epc, "EPC", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(spec_idx, "Spec index", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(peak_rssi, "Peak RSSI", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(tag_count, "Tag count", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(bandwidth, "Bandwidth", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(average_rssi, "Average RSSI", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(notif_state, "Notification state", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(event_type, "Event type", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &event_type_ext, 0) \
    HF_PARAM(next_chan_idx, "Next channel index", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(roevent_type, "Event type", FT_UINT8, BASE_DEC, VALS(roevent_type), 0) \
    HF_PARAM(prem_rospec_id, "Preempting ROSpec ID", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(buffer_full_percentage, "Report Buffer percentage full", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(message, "Message", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(rfevent_type, "Event type", FT_UINT8, BASE_DEC, VALS(rfevent_type), 0) \
    HF_PARAM(aievent_type, "Event type", FT_UINT8, BASE_DEC, VALS(aievent_type), 0) \
    HF_PARAM(antenna_event_type, "Event type", FT_UINT8, BASE_DEC, VALS(antenna_event_type), 0) \
    HF_PARAM(conn_status, "Status", FT_UINT16, BASE_DEC, VALS(connection_status), 0) \
    HF_PARAM(loop_count, "Loop count", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(status_code, "Status code", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &status_code_ext, 0) \
    HF_PARAM(error_desc, "Error Description", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(field_num, "Field number", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(error_code, "Error code", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &status_code_ext, 0) \
    HF_PARAM(parameter_type, "Parameter type", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &tlv_type_ext, 0) \
    HF_PARAM(can_support_block_erase, "Can support block erase", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(can_support_block_write, "Can support block write", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x40) \
    HF_PARAM(can_support_block_permalock, "Can support block permalock", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x20) \
    HF_PARAM(can_support_tag_recomm, "Can support tag recommisioning", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x10) \
    HF_PARAM(can_support_UMI_method2, "Can support UMI method 2", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x08) \
    HF_PARAM(can_support_XPC, "Can support XPC", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x04) \
    HF_PARAM(max_num_filter_per_query, "Maximum number of select filters per query", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(mode_ident, "Mode identifier", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(DR, "DR", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(hag_conformance, "EPC HAG T&C Conformance", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x40) \
    HF_PARAM(mod, "M", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(flm, "Forward link modulation", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(m, "Spectral mask indicator", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(bdr, "BDR", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(pie, "PIE", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(min_tari, "Minimum tari", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(max_tari, "Maximum tari", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(step_tari, "Tari step", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(inventory_state_aware, "Tag inventory state aware", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(trunc, "T", FT_UINT8, BASE_DEC, NULL, 0xC0) \
    HF_PARAM(mb, "MB", FT_UINT8, BASE_DEC, NULL, 0xC0) \
    HF_PARAM(pointer, "Pointer", FT_UINT16, BASE_DEC_HEX, NULL, 0) \
    HF_PARAM(tag_mask, "Tag mask", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(aware_filter_target, "Target", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(aware_filter_action, "Action", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(unaware_filter_action, "Action", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(mode_idx, "Mode index", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(tari, "Tari", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(session, "Session", FT_UINT8, BASE_DEC, NULL, 0xC0) \
    HF_PARAM(tag_population, "Tag population", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(tag_transit_time, "Tag tranzit time", FT_UINT32, BASE_DEC, NULL, 0) \
    HF_PARAM(sing_i, "I", FT_BOOLEAN, 8, TFS(&tfs_state_a_b), 0x80) \
    HF_PARAM(sing_s, "S", FT_BOOLEAN, 8, TFS(&tfs_sl), 0x40) \
    HF_PARAM(sing_a, "S_All", FT_BOOLEAN, 8, TFS(&tfs_all_no), 0x20) \
    HF_PARAM(match, "Match", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x20) \
    HF_PARAM(tag_data, "Tag data", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(access_pass, "Access password", FT_UINT32, BASE_DEC_HEX, NULL, 0) \
    HF_PARAM(word_pointer, "Word pointer", FT_UINT16, BASE_DEC_HEX, NULL, 0) \
    HF_PARAM(word_count, "Word count", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(write_data, "Write data", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(kill_pass, "Killpassword", FT_UINT32, BASE_DEC_HEX, NULL, 0) \
    HF_PARAM(kill_3, "3", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x04) \
    HF_PARAM(kill_2, "2", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x02) \
    HF_PARAM(kill_l, "L", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x01) \
    HF_PARAM(privilege, "Privilege", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(data_field, "Data field", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(block_pointer, "Block pointer", FT_UINT16, BASE_DEC_HEX, NULL, 0) \
    HF_PARAM(block_mask, "Block mask", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(length_words, "Field Length (words)", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(block_range, "Block range", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(enable_crc, "Enable CRC", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(enable_pc, "Enable PC bits", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x40) \
    HF_PARAM(enable_xpc, "Enable XPC bits", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x20) \
    HF_PARAM(pc_bits, "PC bits", FT_UINT16, BASE_HEX, NULL, 0) \
    HF_PARAM(xpc_w1, "XPC-W1", FT_UINT16, BASE_HEX, NULL, 0) \
    HF_PARAM(xpc_w2, "XPC-W2", FT_UINT16, BASE_HEX, NULL, 0) \
    HF_PARAM(crc, "CRC", FT_UINT16, BASE_HEX, NULL, 0) \
    HF_PARAM(num_coll, "Number of collisions", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(num_empty, "Number of empty slots", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(access_result, "Result", FT_UINT8, BASE_DEC, NULL, 0) \
    HF_PARAM(read_data, "Read data", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(num_words_written, "Number of words written", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(permlock_status, "Read data", FT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(vendor_id, "Vendor ID", FT_UINT32, BASE_DEC, VALS(llrp_vendors), 0) \
    HF_PARAM(impinj_param_type, "Impinj parameter subtype", FT_UINT32, BASE_DEC | BASE_EXT_STRING, &impinj_param_type_ext, 0) \
    HF_PARAM(save_config, "Save configuration", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80) \
    HF_PARAM(impinj_req_data, "Requested data", FT_UINT32, BASE_DEC | BASE_EXT_STRING, &impinj_req_data_ext, 0) \
    HF_PARAM(impinj_reg_region, "Regulatory region", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &impinj_reg_region_ext, 0) \
    HF_PARAM(impinj_search_mode, "Inventory search mode", FT_UINT16, BASE_DEC, VALS(impinj_search_mode), 0) \
    HF_PARAM(impinj_en_tag_dir, "Enable tag direction", FT_BOOLEAN, 16, TFS(&tfs_yes_no), 0x8000) \
    HF_PARAM(impinj_antenna_conf, "Antenna configuration", FT_UINT16, BASE_DEC, VALS(impinj_ant_conf), 0) \
    HF_PARAM(decision_time, "Decision timestamp", FT_UINT64, BASE_DEC, NULL, 0) \
    HF_PARAM(impinj_tag_dir, "Tag direction", FT_UINT16, BASE_DEC, VALS(impinj_tag_dir), 0) \
    HF_PARAM(confidence, "Confidence", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(impinj_fix_freq_mode, "Fixed frequency mode", FT_UINT16, BASE_DEC, VALS(impinj_fix_freq_mode), 0) \
    HF_PARAM(num_channels, "Number of channels", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(channel, "Channel", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(impinj_reduce_power_mode, "Recuced power mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(impinj_low_duty_mode, "Low duty cycle mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(empty_field_timeout, "Empty field timeout", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(field_ping_interval, "Field ping interval", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(model_name, "Model name", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(serial_number, "Serial number", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(soft_ver, "Softwave version", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(firm_ver, "Firmware version", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(fpga_ver, "FPGA version", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(pcba_ver, "PCBA version", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(height_thresh, "Height threshold", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(zero_motion_thresh, "Zero motion threshold", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(board_manufacturer, "Board manufacturer", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(fw_ver_hex, "Firmware version", FT_UINT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(hw_ver_hex, "Hardware version", FT_UINT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(gpi_debounce, "GPI debounce timer Msec", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(temperature, "Temperature", FT_INT16, BASE_DEC, NULL, 0) \
    HF_PARAM(impinj_link_monitor_mode, "Link monitor mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(link_down_thresh, "Link down threshold", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(impinj_report_buff_mode, "Report buffer mode", FT_UINT16, BASE_DEC, VALS(impinj_report_buff_mode), 0) \
    HF_PARAM(permalock_result, "Result", FT_UINT8, BASE_DEC | BASE_EXT_STRING, &impinj_permalock_result_ext, 0) \
    HF_PARAM(block_permalock_result, "Result", FT_UINT8, BASE_DEC | BASE_EXT_STRING, &impinj_block_permalock_result_ext, 0) \
    HF_PARAM(impinj_data_profile, "Data profile", FT_UINT8, BASE_DEC, VALS(impinj_data_profile), 0) \
    HF_PARAM(impinj_access_range, "Access range", FT_UINT8, BASE_DEC, VALS(impinj_access_range), 0) \
    HF_PARAM(impinj_persistence, "Persistence", FT_UINT8, BASE_DEC, VALS(impinj_persistence), 0) \
    HF_PARAM(set_qt_config_result, "Result", FT_UINT8, BASE_DEC | BASE_EXT_STRING, &impinj_set_qt_config_result_ext, 0) \
    HF_PARAM(get_qt_config_result, "Result", FT_UINT8, BASE_DEC | BASE_EXT_STRING, &impinj_get_qt_config_result_ext, 0) \
    HF_PARAM(impinj_serialized_tid_mode, "Serialized TID Mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(impinj_rf_phase_mode, "RF phase angle mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(impinj_peak_rssi_mode, "Peak RSSI mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(impinj_gps_coordinates_mode, "GPS coordinates mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(impinj_tid, "TID", FT_UINT_BYTES, BASE_NONE, NULL, 0) \
    HF_PARAM(phase_angle, "Phase angle", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(rssi, "RSSI", FT_INT16, BASE_DEC, NULL, 0) \
    HF_PARAM(latitude, "Latitude", FT_INT32, BASE_DEC, NULL, 0) \
    HF_PARAM(longitude, "Longitude", FT_INT32, BASE_DEC, NULL, 0) \
    HF_PARAM(gga_sentence, "GGA sentence", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(rmc_sentence, "RMC sentence", FT_UINT_STRING, BASE_NONE, NULL, 0) \
    HF_PARAM(impinj_optim_read_mode, "Optimized read mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(impinj_rf_doppler_mode, "RF doppler frequency mode", FT_UINT16, BASE_DEC, VALS(impinj_boolean), 0) \
    HF_PARAM(retry_count, "Retry count", FT_UINT16, BASE_DEC, NULL, 0) \
    HF_PARAM(impinj_access_spec_ordering, "AccessSpec ordering", FT_UINT16, BASE_DEC, VALS(impinj_access_spec_ordering), 0) \
    HF_PARAM(impinj_gpo_mode, "GPO mode", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &impinj_gpo_mode_ext, 0) \
    HF_PARAM(gpo_pulse_dur, "GPO pulse duration", FT_UINT32, BASE_DEC, NULL, 0) \

/* Construct the hfindex variables */
#define HF_PARAM(name, ...) static int hf_llrp_##name = -1;
HF_LLRP_PARAMETER_LIST
#undef HF_PARAM

static guint
dissect_llrp_parameters(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
        guint offset, const guint end);

static guint dissect_llrp_utf8_parameter(tvbuff_t * const tvb, packet_info *pinfo,
        proto_tree * const tree, const guint hfindex, const guint offset)
{
    gint len;

    len = tvb_get_ntohs(tvb, offset);
    if(tvb_reported_length_remaining(tvb, offset) < len) {
        expert_add_info_format(pinfo, tree, PI_MALFORMED, PI_ERROR,
            "invalid length of string: claimed %u, available %u.",
            len, tvb_reported_length_remaining(tvb, offset));
        return offset + 2;
    }
    proto_tree_add_item(tree, hfindex, tvb,
            offset, 2, ENC_BIG_ENDIAN | ENC_UTF_8);

    return offset + len + 2;
}

static guint dissect_llrp_bit_field(tvbuff_t * const tvb,
        proto_tree * const tree, const guint hfindex, const guint offset)
{
    guint len;

    len = tvb_get_ntohs(tvb, offset);
    len = (len + 7) / 8;
    proto_tree_add_item(tree, hf_llrp_length_bits, tvb,
            offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(tree, hfindex, tvb,
            offset + 2, len, ENC_NA);
    return offset + len + 2;
}

static guint dissect_llrp_word_array(tvbuff_t * const tvb,
        proto_tree * const tree, const guint hfindex, const guint offset)
{
    guint len;

    len = tvb_get_ntohs(tvb, offset);
    len *= 2;
    proto_tree_add_item(tree, hf_llrp_length_words, tvb,
            offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(tree, hfindex, tvb,
            offset + 2, len, ENC_NA);
    return offset + len + 2;
}

static guint dissect_llrp_item_array(tvbuff_t * const tvb, packet_info *pinfo,
        proto_tree * const tree, const guint hfindex_number,
        const guint hfindex_item, const guint item_size, guint offset)
{
    guint num;

    num = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(tree, hfindex_number, tvb,
            offset, 2, ENC_BIG_ENDIAN);
    offset += 2;
    if(tvb_reported_length_remaining(tvb, offset) < ((gint)(num*item_size))) {
        expert_add_info_format(pinfo, tree, PI_MALFORMED, PI_ERROR,
                "Array longer than message");
        return offset + tvb_reported_length_remaining(tvb, offset);
    }
    while(num--) {
        proto_tree_add_item(tree, hfindex_item, tvb,
                offset, item_size, item_size == 1 ? ENC_NA : ENC_BIG_ENDIAN);
        offset += item_size;
    }
    return offset;
}

#define PARAM_TREE_ADD_STAY(hfindex, length, flag) \
            proto_tree_add_item(param_tree, hf_llrp_##hfindex, tvb, \
                    suboffset, length, flag)

#define PARAM_TREE_ADD(hfindex, length, flag) \
            PARAM_TREE_ADD_STAY(hfindex, length, flag); \
            suboffset += length

#define PARAM_TREE_ADD_SPEC_STAY(type, hfindex, length, number, string) \
            proto_tree_add_##type(param_tree, hf_llrp_##hfindex, tvb, \
                    suboffset, length, number, string, number)

#define PARAM_TREE_ADD_SPEC(type, hfindex, length, number, string) \
            PARAM_TREE_ADD_SPEC_STAY(type, hfindex, length, number, string); \
            suboffset += length

static guint
dissect_llrp_impinj_parameter(tvbuff_t *tvb, packet_info *pinfo, proto_tree *param_tree,
        guint suboffset, const guint param_end)
{
    guint32 subtype;

    subtype = tvb_get_ntohl(tvb, suboffset);
    proto_item_append_text(param_tree, " (Impinj - %s)",
            val_to_str_ext(subtype, &impinj_param_type_ext, "Unknown Type: %d"));
    proto_tree_add_item(param_tree, hf_llrp_impinj_param_type, tvb, suboffset, 4, ENC_BIG_ENDIAN);
    suboffset += 4;

    switch(subtype) {
    case LLRP_IMPINJ_PARAM_TAG_INFORMATION:
    case LLRP_IMPINJ_PARAM_FORKLIFT_CONFIGURATION:
    case LLRP_IMPINJ_PARAM_ACCESS_SPEC_CONFIGURATION:
    case LLRP_IMPINJ_PARAM_TAG_REPORT_CONTENT_SELECTOR:
    case LLRP_IMPINJ_PARAM_GPS_NMEA_SENTENCES:
        /* Just parameters */
        break;
    case LLRP_IMPINJ_PARAM_REQUESTED_DATA:
        PARAM_TREE_ADD(impinj_req_data, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_SUBREGULATORY_REGION:
        PARAM_TREE_ADD(impinj_reg_region, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_INVENTORY_SEARCH_MODE:
        PARAM_TREE_ADD(impinj_search_mode, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_TAG_DIRECTION_REPORTING:
        PARAM_TREE_ADD(impinj_en_tag_dir, 2, ENC_NA);
        PARAM_TREE_ADD(impinj_antenna_conf, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(rfu, 4, ENC_NA);
        break;
    case LLRP_IMPINJ_PARAM_TAG_DIRECTION:
        PARAM_TREE_ADD(decision_time, 8, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(impinj_tag_dir, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(confidence, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_FIXED_FREQUENCY_LIST:
        PARAM_TREE_ADD(impinj_fix_freq_mode, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(rfu, 2, ENC_NA);
        suboffset = dissect_llrp_item_array(tvb, pinfo, param_tree,
                hf_llrp_num_channels, hf_llrp_channel, 2, suboffset);
        break;
    case LLRP_IMPINJ_PARAM_REDUCED_POWER_FREQUENCY_LIST:
        PARAM_TREE_ADD(impinj_reduce_power_mode, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(rfu, 2, ENC_NA);
        suboffset = dissect_llrp_item_array(tvb, pinfo, param_tree,
                hf_llrp_num_channels, hf_llrp_channel, 2, suboffset);
        break;
    case LLRP_IMPINJ_PARAM_LOW_DUTY_CYCLE:
        PARAM_TREE_ADD(impinj_low_duty_mode, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(empty_field_timeout, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(field_ping_interval, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_DETAILED_VERSION:
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_model_name, suboffset);
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_serial_number, suboffset);
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_soft_ver, suboffset);
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_firm_ver, suboffset);
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_fpga_ver, suboffset);
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_pcba_ver, suboffset);
        break;
    case LLRP_IMPINJ_PARAM_FREQUENCY_CAPABILITIES:
        suboffset = dissect_llrp_item_array(tvb, pinfo, param_tree,
                hf_llrp_num_freqs, hf_llrp_frequency, 4, suboffset);
        break;
    case LLRP_IMPINJ_PARAM_FORKLIFT_HEIGHT_THRESHOLD:
        PARAM_TREE_ADD(height_thresh, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_FORKLIFT_ZEROMOTION_TIME_THRESHOLD:
        PARAM_TREE_ADD(zero_motion_thresh, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_FORKLIFT_COMPANION_BOARD_INFO:
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_board_manufacturer, suboffset);
        PARAM_TREE_ADD(fw_ver_hex, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(hw_ver_hex, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_GPI_DEBOUNCE_CONFIGURATION:
        PARAM_TREE_ADD(gpi_port, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(gpi_debounce, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_READER_TEMPERATURE:
        PARAM_TREE_ADD(temperature, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_LINK_MONITOR_CONFIGURATION:
        PARAM_TREE_ADD(impinj_link_monitor_mode, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(link_down_thresh, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_REPORT_BUFFER_CONFIGURATION:
        PARAM_TREE_ADD(impinj_report_buff_mode, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_BLOCK_WRITE_WORD_COUNT:
        PARAM_TREE_ADD(word_count, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_BLOCK_PERMALOCK:
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(mb, 1, ENC_NA);
        PARAM_TREE_ADD(block_pointer, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(block_mask, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_BLOCK_PERMALOCK_OPSPEC_RESULT:
        PARAM_TREE_ADD(permalock_result, 1, ENC_NA);
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_GET_BLOCK_PERMALOCK_STATUS:
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(mb, 1, ENC_NA);
        PARAM_TREE_ADD(block_pointer, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(block_range, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_GET_BLOCK_PERMALOCK_STATUS_OPSPEC_RESULT:
        PARAM_TREE_ADD(block_permalock_result, 1, ENC_NA);
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_SET_QT_CONFIG:
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(impinj_data_profile, 1, ENC_NA);
        PARAM_TREE_ADD(impinj_access_range, 1, ENC_NA);
        PARAM_TREE_ADD(impinj_persistence, 1, ENC_NA);
        PARAM_TREE_ADD(rfu, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_SET_QT_CONFIG_OPSPEC_RESULT:
        PARAM_TREE_ADD(set_qt_config_result, 1, ENC_NA);
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_GET_QT_CONFIG:
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_GET_QT_CONFIG_OPSPEC_RESULT:
        PARAM_TREE_ADD(get_qt_config_result, 1, ENC_NA);
        PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(impinj_data_profile, 1, ENC_NA);
        PARAM_TREE_ADD(impinj_access_range, 1, ENC_NA);
        PARAM_TREE_ADD(rfu, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ENABLE_SERIALIZED_TID:
        PARAM_TREE_ADD(impinj_serialized_tid_mode, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ENABLE_RF_PHASE_ANGLE:
        PARAM_TREE_ADD(impinj_rf_phase_mode, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ENABLE_PEAK_RSSI:
        PARAM_TREE_ADD(impinj_peak_rssi_mode, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ENABLE_GPS_COORDINATES:
        PARAM_TREE_ADD(impinj_gps_coordinates_mode, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_SERIALIZED_TID:
        PARAM_TREE_ADD(impinj_tid, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_RF_PHASE_ANGLE:
        PARAM_TREE_ADD(phase_angle, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_PEAK_RSSI:
        PARAM_TREE_ADD(rssi, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_GPS_COORDINATES:
        PARAM_TREE_ADD(latitude, 4, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(longitude, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_LOOP_SPEC:
        PARAM_TREE_ADD(loop_count, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_GGA_SENTENCE:
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_gga_sentence, suboffset);
        break;
    case LLRP_IMPINJ_PARAM_RMC_SENTENCE:
        suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_rmc_sentence, suboffset);
        break;
    case LLRP_IMPINJ_PARAM_OPSPEC_RETRY_COUNT:
        PARAM_TREE_ADD(retry_count, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ADVANCE_GPO_CONFIG:
        PARAM_TREE_ADD(gpo_port, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(impinj_gpo_mode, 2, ENC_BIG_ENDIAN);
        PARAM_TREE_ADD(gpo_pulse_dur, 4, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ENABLE_OPTIM_READ:
        PARAM_TREE_ADD(impinj_optim_read_mode, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ACCESS_SPEC_ORDERING:
        PARAM_TREE_ADD(impinj_access_spec_ordering, 2, ENC_BIG_ENDIAN);
        break;
    case LLRP_IMPINJ_PARAM_ENABLE_RF_DOPPLER_FREQ:
        PARAM_TREE_ADD(impinj_rf_doppler_mode, 2, ENC_BIG_ENDIAN);
        break;
    default:
        return suboffset;
        break;
    }
    /* Each custom parameters ends with optional custom parameter, disscect it */
    return dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
}

static guint
dissect_llrp_parameters(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
        guint offset, const guint end)
{
    guint8      has_length;
    guint16     len, type;
    guint       real_len, param_end;
    guint       suboffset;
    guint       num;
    proto_item *ti;
    proto_tree *param_tree;
    guint (*dissect_custom_parameter)(tvbuff_t *tvb,
            packet_info *pinfo, proto_tree *tree, guint offset, guint param_end);

    while (((gint)(end - offset)) > 0)
    {
        has_length = !(tvb_get_guint8(tvb, offset) & 0x80);

        if (has_length)
        {
            type = tvb_get_ntohs(tvb, offset);
            len = tvb_get_ntohs(tvb, offset + 2);
            
            param_end = offset + len;

            if (len < LLRP_TLV_LEN_MIN)
                real_len = LLRP_TLV_LEN_MIN;
            else if (len > tvb_reported_length_remaining(tvb, offset))
                real_len = tvb_reported_length_remaining(tvb, offset);
            else
                real_len = len;

            param_end = offset + real_len;

            ti = proto_tree_add_none_format(tree, hf_llrp_param, tvb,
                    offset, real_len, "TLV Parameter: %s",
                    val_to_str_ext(type, &tlv_type_ext, "Unknown Type: %d"));
            param_tree = proto_item_add_subtree(ti, ett_llrp_param);

            proto_tree_add_item(param_tree, hf_llrp_tlv_type, tvb,
                    offset, 2, ENC_BIG_ENDIAN);
            offset += 2;

            ti = proto_tree_add_item(param_tree, hf_llrp_tlv_len, tvb,
                    offset, 2, ENC_BIG_ENDIAN);
            if (len != real_len)
                expert_add_info_format(pinfo, ti, PI_MALFORMED, PI_ERROR,
                        "Invalid length field: claimed %u, should be %u.",
                        len, real_len);
            offset += 2;

            suboffset = offset;
            switch(type) {
            case LLRP_TLV_RO_BOUND_SPEC:
            case LLRP_TLV_UHF_CAPABILITIES:
            case LLRP_TLV_ACCESS_COMMAND:
            case LLRP_TLV_TAG_REPORT_DATA:
            case LLRP_TLV_RF_SURVEY_REPORT_DATA:
            case LLRP_TLV_READER_EVENT_NOTI_SPEC:
            case LLRP_TLV_READER_EVENT_NOTI_DATA:
            case LLRP_TLV_C1G2_UHF_RF_MD_TBL:
            case LLRP_TLV_C1G2_TAG_SPEC:
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_UTC_TIMESTAMP:
            case LLRP_TLV_UPTIME:
                PARAM_TREE_ADD(microseconds, 8, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_GENERAL_DEVICE_CAP:
                PARAM_TREE_ADD_STAY(max_supported_antenna, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(can_set_antenna_prop, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(has_utc_clock, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(device_manufacturer, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(model, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_firmware_version, suboffset);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_MAX_RECEIVE_SENSE:
                PARAM_TREE_ADD(max_receive_sense, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_RECEIVE_SENSE_ENTRY:
                PARAM_TREE_ADD(index, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(receive_sense, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_ANTENNA_RCV_SENSE_RANGE:
                PARAM_TREE_ADD(antenna_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(receive_sense_index_min, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(receive_sense_index_max, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_ANTENNA_AIR_PROTO:
                PARAM_TREE_ADD(antenna_id, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_item_array(tvb, pinfo, param_tree,
                        hf_llrp_num_protocols, hf_llrp_protocol_id, 1, suboffset);
                break;
            case LLRP_TLV_GPIO_CAPABILITIES:
                PARAM_TREE_ADD(num_gpi, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(num_gpo, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_LLRP_CAPABILITIES:
                PARAM_TREE_ADD_STAY(can_do_survey, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(can_report_buffer_warning, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(support_client_opspec, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(can_stateaware, 1, ENC_NA);
                PARAM_TREE_ADD(support_holding, 1, ENC_NA);
                PARAM_TREE_ADD(max_priority_supported, 1, ENC_NA);
                PARAM_TREE_ADD(client_opspec_timeout, 2, ENC_BIG_ENDIAN);
                num = tvb_get_ntohl(tvb, suboffset);
                if(num == LLRP_NO_LIMIT)
                    PARAM_TREE_ADD_SPEC_STAY(uint_format_value, max_num_rospec, 4, num, "No limit (%u)");
                else
                    PARAM_TREE_ADD_STAY(max_num_rospec, 4, ENC_BIG_ENDIAN);
                suboffset += 4;
                num = tvb_get_ntohl(tvb, suboffset);
                if(num == LLRP_NO_LIMIT)
                    PARAM_TREE_ADD_SPEC_STAY(uint_format_value, max_num_spec_per_rospec, 4, num, "No limit (%u)");
                else
                    PARAM_TREE_ADD_STAY(max_num_spec_per_rospec, 4, ENC_BIG_ENDIAN);
                suboffset += 4;
                num = tvb_get_ntohl(tvb, suboffset);
                if(num == LLRP_NO_LIMIT)
                    PARAM_TREE_ADD_SPEC_STAY(uint_format_value, max_num_inventory_per_aispec, 4, num, "No limit (%u)");
                else
                    PARAM_TREE_ADD_STAY(max_num_inventory_per_aispec, 4, ENC_BIG_ENDIAN);
                suboffset += 4;
                num = tvb_get_ntohl(tvb, suboffset);
                if(num == LLRP_NO_LIMIT)
                    PARAM_TREE_ADD_SPEC_STAY(uint_format_value, max_num_accessspec, 4, num, "No limit (%u)");
                else
                    PARAM_TREE_ADD_STAY(max_num_accessspec, 4, ENC_BIG_ENDIAN);
                suboffset += 4;
                num = tvb_get_ntohl(tvb, suboffset);
                if(num == LLRP_NO_LIMIT)
                    PARAM_TREE_ADD_SPEC_STAY(uint_format_value, max_num_opspec_per_accressspec, 4, num, "No limit (%u)");
                else
                    PARAM_TREE_ADD_STAY(max_num_opspec_per_accressspec, 4, ENC_BIG_ENDIAN);
                suboffset += 4;
                break;
            case LLRP_TLV_REGU_CAPABILITIES:
                PARAM_TREE_ADD(country_code, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(comm_standard, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_XMIT_POWER_LEVEL_ENTRY:
                PARAM_TREE_ADD(index, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(transmit_power, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_FREQ_INFORMATION:
                PARAM_TREE_ADD(hopping, 1, ENC_NA);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_FREQ_HOP_TABLE:
                PARAM_TREE_ADD(hop_table_id, 1, ENC_NA);
                PARAM_TREE_ADD(rfu, 1, ENC_NA);
                suboffset = dissect_llrp_item_array(tvb, pinfo, param_tree,
                        hf_llrp_num_hops, hf_llrp_frequency, 4, suboffset);
                break;
            case LLRP_TLV_FIXED_FREQ_TABLE:
                suboffset = dissect_llrp_item_array(tvb, pinfo, param_tree,
                        hf_llrp_num_freqs, hf_llrp_frequency, 4, suboffset);
                break;
            case LLRP_TLV_RF_SURVEY_FREQ_CAP:
                PARAM_TREE_ADD(min_freq, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(max_freq, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_RO_SPEC:
                PARAM_TREE_ADD(rospec_id, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(priority, 1, ENC_NA);
                PARAM_TREE_ADD(cur_state, 1, ENC_NA);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_RO_SPEC_START_TRIGGER:
                PARAM_TREE_ADD(rospec_start_trig_type, 1, ENC_NA);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_PER_TRIGGER_VAL:
                PARAM_TREE_ADD(offset, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(period, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_GPI_TRIGGER_VAL:
                PARAM_TREE_ADD(gpi_port, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(gpi_event, 1, ENC_NA);
                PARAM_TREE_ADD(timeout, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_RO_SPEC_STOP_TRIGGER:
                PARAM_TREE_ADD(rospec_stop_trig_type, 1, ENC_NA);
                PARAM_TREE_ADD(duration_trig, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_AI_SPEC:
                suboffset = dissect_llrp_item_array(tvb, pinfo, param_tree,
                        hf_llrp_antenna_count, hf_llrp_antenna, 2, suboffset);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_AI_SPEC_STOP:
                PARAM_TREE_ADD(aispec_stop_trig_type, 1, ENC_NA);
                PARAM_TREE_ADD(duration_trig, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_TAG_OBSERV_TRIGGER:
                PARAM_TREE_ADD(trig_type, 1, ENC_NA);
                PARAM_TREE_ADD(rfu, 1, ENC_NA);
                PARAM_TREE_ADD(number_of_tags, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(number_of_attempts, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(t, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(timeout, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_INVENTORY_PARAM_SPEC:
                PARAM_TREE_ADD(inventory_spec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(protocol_id, 1, ENC_NA);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_RF_SURVEY_SPEC:
                PARAM_TREE_ADD(antenna_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(start_freq, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(stop_freq, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_RF_SURVEY_SPEC_STOP_TR:
                PARAM_TREE_ADD(stop_trig_type, 1, ENC_NA);
                PARAM_TREE_ADD(duration, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(n_4, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_LOOP_SPEC:
                PARAM_TREE_ADD(loop_count, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_ACCESS_SPEC:
                PARAM_TREE_ADD(accessspec_id, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(antenna_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(protocol_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(access_cur_state, 1, ENC_NA);
                PARAM_TREE_ADD(rospec_id, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_ACCESS_SPEC_STOP_TRIG:
                PARAM_TREE_ADD(access_stop_trig_type, 1, ENC_NA);
                PARAM_TREE_ADD(operation_count, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_CLIENT_REQ_OP_SPEC:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_CLIENT_REQ_RESPONSE:
                PARAM_TREE_ADD(accessspec_id, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_LLRP_CONF_STATE_VAL:
                PARAM_TREE_ADD(conf_value, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_IDENT:
                PARAM_TREE_ADD(id_type, 1, ENC_NA);
                num = tvb_get_ntohs(tvb, suboffset);
                PARAM_TREE_ADD(reader_id, 2, ENC_BIG_ENDIAN);
                suboffset += num;
                break;
            case LLRP_TLV_GPO_WRITE_DATA:
                PARAM_TREE_ADD(gpo_port, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(gpo_data, 1, ENC_NA);
                break;
            case LLRP_TLV_KEEPALIVE_SPEC:
                PARAM_TREE_ADD(keepalive_trig_type, 1, ENC_NA);
                PARAM_TREE_ADD(time_iterval, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_ANTENNA_PROPS:
                PARAM_TREE_ADD(antenna_connected, 1, ENC_NA);
                PARAM_TREE_ADD(antenna_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(antenna_gain, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_ANTENNA_CONF:
                PARAM_TREE_ADD(antenna_id, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_RF_RECEIVER:
                PARAM_TREE_ADD(receiver_sense, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_RF_TRANSMITTER:
                PARAM_TREE_ADD(hop_table_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(channel_idx, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(transmit_power, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_GPI_PORT_CURRENT_STATE:
                PARAM_TREE_ADD(gpi_port, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(gpi_config, 1, ENC_NA);
                PARAM_TREE_ADD(gpi_state, 1, ENC_NA);
                break;
            case LLRP_TLV_EVENTS_AND_REPORTS:
                PARAM_TREE_ADD(hold_events_and_reports, 1, ENC_NA);
                break;
            case LLRP_TLV_RO_REPORT_SPEC:
                PARAM_TREE_ADD(ro_report_trig, 1, ENC_NA);
                PARAM_TREE_ADD(n_2, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_TAG_REPORT_CONTENT_SEL:
                PARAM_TREE_ADD_STAY(enable_rospec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_spec_idx, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_inv_spec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_antenna_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_channel_idx, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_peak_rssi, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_first_seen, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_last_seen, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(enable_seen_count, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(enable_accessspec_id, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_ACCESS_REPORT_SPEC:
                PARAM_TREE_ADD(access_report_trig, 1, ENC_NA);
                break;
            case LLRP_TLV_EPC_DATA:
                suboffset = dissect_llrp_bit_field(tvb, param_tree, hf_llrp_epc, suboffset);
                break;
            case LLRP_TLV_FREQ_RSSI_LEVEL_ENTRY:
                PARAM_TREE_ADD(frequency, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(bandwidth, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(average_rssi, 1, ENC_NA);
                PARAM_TREE_ADD(peak_rssi, 1, ENC_NA);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_EVENT_NOTIF_STATE:
                PARAM_TREE_ADD(event_type, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(notif_state, 1, ENC_NA);
                break;
            case LLRP_TLV_HOPPING_EVENT:
                PARAM_TREE_ADD(hop_table_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(next_chan_idx, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_GPI_EVENT:
                PARAM_TREE_ADD(gpi_port, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(gpi_event, 1, ENC_NA);
                break;
            case LLRP_TLV_RO_SPEC_EVENT:
                PARAM_TREE_ADD(roevent_type, 1, ENC_NA);
                PARAM_TREE_ADD(rospec_id, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(prem_rospec_id, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_REPORT_BUF_LEVEL_WARN:
                PARAM_TREE_ADD(buffer_full_percentage, 1, ENC_NA);
                break;
            case LLRP_TLV_REPORT_BUF_OVERFLOW_ERR:
                break;
            case LLRP_TLV_READER_EXCEPTION_EVENT:
                suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_message, suboffset);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_RF_SURVEY_EVENT:
                PARAM_TREE_ADD(rfevent_type, 1, ENC_NA);
                PARAM_TREE_ADD(rospec_id, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(spec_idx, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_AI_SPEC_EVENT:
                PARAM_TREE_ADD(aievent_type, 1, ENC_NA);
                PARAM_TREE_ADD(rospec_id, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(spec_idx, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_ANTENNA_EVENT:
                PARAM_TREE_ADD(antenna_event_type, 1, ENC_NA);
                PARAM_TREE_ADD(antenna_id, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_CONN_ATTEMPT_EVENT:
                PARAM_TREE_ADD(conn_status, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_CONN_CLOSE_EVENT:
                break;
            case LLRP_TLV_SPEC_LOOP_EVENT:
                PARAM_TREE_ADD(rospec_id, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(loop_count, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_LLRP_STATUS:
                PARAM_TREE_ADD(status_code, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_utf8_parameter(tvb, pinfo, param_tree, hf_llrp_error_desc, suboffset);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_FIELD_ERROR:
                PARAM_TREE_ADD(field_num, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(error_code, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_PARAM_ERROR:
                PARAM_TREE_ADD(parameter_type, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(error_code, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_C1G2_LLRP_CAP:
                PARAM_TREE_ADD_STAY(can_support_block_erase, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(can_support_block_write, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(can_support_block_permalock, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(can_support_tag_recomm, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(can_support_UMI_method2, 1, ENC_NA);
                PARAM_TREE_ADD(can_support_XPC, 1, ENC_NA);
                num = tvb_get_ntohs(tvb, suboffset);
                if(num == LLRP_NO_LIMIT)
                    PARAM_TREE_ADD_SPEC_STAY(uint_format_value, max_num_filter_per_query, 2, num, "No limit (%u)");
                else
                    PARAM_TREE_ADD_STAY(max_num_filter_per_query, 2, ENC_BIG_ENDIAN);
                suboffset += 2;
                break;
            case LLRP_TLV_C1G2_UHF_RF_MD_TBL_ENT:
                PARAM_TREE_ADD(mode_ident, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(DR, 1, ENC_NA);
                PARAM_TREE_ADD(hag_conformance, 1, ENC_NA);
                PARAM_TREE_ADD(mod, 1, ENC_NA);
                PARAM_TREE_ADD(flm, 1, ENC_NA);
                PARAM_TREE_ADD(m, 1, ENC_NA);
                PARAM_TREE_ADD(bdr, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(pie, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(min_tari, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(max_tari, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(step_tari, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_C1G2_INVENTORY_COMMAND:
                PARAM_TREE_ADD(inventory_state_aware, 1, ENC_NA);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_C1G2_FILTER:
                PARAM_TREE_ADD(trunc, 1, ENC_NA);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_C1G2_TAG_INV_MASK:
                PARAM_TREE_ADD(mb, 1, ENC_NA);
                PARAM_TREE_ADD(pointer, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_bit_field(tvb, param_tree, hf_llrp_tag_mask, suboffset);
                break;
            case LLRP_TLV_C1G2_TAG_INV_AWARE_FLTR:
                PARAM_TREE_ADD(aware_filter_target, 1, ENC_NA);
                PARAM_TREE_ADD(aware_filter_action, 1, ENC_NA);
                break;
            case LLRP_TLV_C1G2_TAG_INV_UNAWR_FLTR:
                PARAM_TREE_ADD(unaware_filter_action, 1, ENC_NA);
                break;
            case LLRP_TLV_C1G2_RF_CONTROL:
                PARAM_TREE_ADD(mode_idx, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(tari, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_C1G2_SINGULATION_CTRL:
                PARAM_TREE_ADD(session, 1, ENC_NA);
                PARAM_TREE_ADD(tag_population, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(tag_transit_time, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_C1G2_TAG_INV_AWARE_SING:
                PARAM_TREE_ADD_STAY(sing_i, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(sing_s, 1, ENC_NA);
                PARAM_TREE_ADD(sing_a, 1, ENC_NA);
                break;
            case LLRP_TLV_C1G2_TARGET_TAG:
                PARAM_TREE_ADD_STAY(mb, 1, ENC_NA);
                PARAM_TREE_ADD(match, 1, ENC_NA);
                PARAM_TREE_ADD(pointer, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_bit_field(tvb, param_tree, hf_llrp_tag_mask, suboffset);
                suboffset = dissect_llrp_bit_field(tvb, param_tree, hf_llrp_tag_data, suboffset);
                break;
            case LLRP_TLV_C1G2_READ:
            case LLRP_TLV_C1G2_BLK_ERASE:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(mb, 1, ENC_NA);
                PARAM_TREE_ADD(word_pointer, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(word_count, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_C1G2_WRITE:
            case LLRP_TLV_C1G2_BLK_WRITE:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(mb, 1, ENC_NA);
                PARAM_TREE_ADD(word_pointer, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_word_array(tvb, param_tree, hf_llrp_write_data, suboffset);
                break;
            case LLRP_TLV_C1G2_KILL:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(kill_pass, 4, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_C1G2_RECOMMISSION:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(kill_pass, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD_STAY(kill_3, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(kill_2, 1, ENC_NA);
                PARAM_TREE_ADD(kill_l, 1, ENC_NA);
                break;
            case LLRP_TLV_C1G2_LOCK:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_parameters(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            case LLRP_TLV_C1G2_LOCK_PAYLOAD:
                PARAM_TREE_ADD(privilege, 1, ENC_NA);
                PARAM_TREE_ADD(data_field, 1, ENC_NA);
                break;
            case LLRP_TLV_C1G2_BLK_PERMALOCK:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(mb, 1, ENC_NA);
                PARAM_TREE_ADD(block_pointer, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_word_array(tvb, param_tree, hf_llrp_block_mask, suboffset);
                break;
            case LLRP_TLV_C1G2_GET_BLK_PERMALOCK:
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(access_pass, 4, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(mb, 1, ENC_NA);
                PARAM_TREE_ADD(block_pointer, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(block_range, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_C1G2_EPC_MEMORY_SLCTOR:
                PARAM_TREE_ADD_STAY(enable_crc, 1, ENC_NA);
                PARAM_TREE_ADD_STAY(enable_pc, 1, ENC_NA);
                PARAM_TREE_ADD(enable_xpc, 1, ENC_NA);
                break;
            case LLRP_TLV_C1G2_READ_OP_SPEC_RES:
                PARAM_TREE_ADD(access_result, 1, ENC_NA);
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_word_array(tvb, param_tree, hf_llrp_read_data, suboffset);
                break;
            case LLRP_TLV_C1G2_WRT_OP_SPEC_RES:
            case LLRP_TLV_C1G2_BLK_WRT_OP_SPC_RES:
                PARAM_TREE_ADD(access_result, 1, ENC_NA);
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                PARAM_TREE_ADD(num_words_written, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_C1G2_KILL_OP_SPEC_RES:
            case LLRP_TLV_C1G2_RECOM_OP_SPEC_RES:
            case LLRP_TLV_C1G2_LOCK_OP_SPEC_RES:
            case LLRP_TLV_C1G2_BLK_ERS_OP_SPC_RES:
            case LLRP_TLV_C1G2_BLK_PRL_OP_SPC_RES:
                PARAM_TREE_ADD(access_result, 1, ENC_NA);
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                break;
            case LLRP_TLV_C1G2_BLK_PRL_STAT_RES:
                PARAM_TREE_ADD(access_result, 1, ENC_NA);
                PARAM_TREE_ADD(opspec_id, 2, ENC_BIG_ENDIAN);
                suboffset = dissect_llrp_word_array(tvb, param_tree, hf_llrp_permlock_status, suboffset);
                break;
            case LLRP_TLV_CUSTOM_PARAMETER:
                num = tvb_get_ntohl(tvb, suboffset);
                PARAM_TREE_ADD(vendor_id, 4, ENC_BIG_ENDIAN);
                switch(num) {
                case LLRP_VENDOR_IMPINJ:
                    dissect_custom_parameter = dissect_llrp_impinj_parameter;
                    break;
                }
                suboffset = dissect_custom_parameter(tvb, pinfo, param_tree, suboffset, param_end);
                break;
            }
            /* Have we decoded exactly the number of bytes declared in the parameter? */
            if(suboffset != param_end) {
                /* Report problem */
                expert_add_info_format(pinfo, param_tree, PI_MALFORMED, PI_ERROR,
                        "Incorrect length of parameter: %u bytes decoded, but %u bytes claimed.",
                        suboffset - offset + 4, real_len);
            }
            /* The len field includes the 4-byte parameter header that we've
             * already accounted for in offset */
            offset += real_len - 4;
        }
        else
        {
            type = tvb_get_guint8(tvb, offset) & 0x7F;

            switch (type)
            {
                case LLRP_TV_ANTENNA_ID:
                    real_len = LLRP_TV_LEN_ANTENNA_ID; break;
                case LLRP_TV_FIRST_SEEN_TIME_UTC:
                    real_len = LLRP_TV_LEN_FIRST_SEEN_TIME_UTC; break;
                case LLRP_TV_FIRST_SEEN_TIME_UPTIME:
                    real_len = LLRP_TV_LEN_FIRST_SEEN_TIME_UPTIME; break;
                case LLRP_TV_LAST_SEEN_TIME_UTC:
                    real_len = LLRP_TV_LEN_LAST_SEEN_TIME_UTC; break;
                case LLRP_TV_LAST_SEEN_TIME_UPTIME:
                    real_len = LLRP_TV_LEN_LAST_SEEN_TIME_UPTIME; break;
                case LLRP_TV_PEAK_RSSI:
                    real_len = LLRP_TV_LEN_PEAK_RSSI; break;
                case LLRP_TV_CHANNEL_INDEX:
                    real_len = LLRP_TV_LEN_CHANNEL_INDEX; break;
                case LLRP_TV_TAG_SEEN_COUNT:
                    real_len = LLRP_TV_LEN_TAG_SEEN_COUNT; break;
                case LLRP_TV_RO_SPEC_ID:
                    real_len = LLRP_TV_LEN_RO_SPEC_ID; break;
                case LLRP_TV_INVENTORY_PARAM_SPEC_ID:
                    real_len = LLRP_TV_LEN_INVENTORY_PARAM_SPEC_ID; break;
                case LLRP_TV_C1G2_CRC:
                    real_len = LLRP_TV_LEN_C1G2_CRC; break;
                case LLRP_TV_C1G2_PC:
                    real_len = LLRP_TV_LEN_C1G2_PC; break;
                case LLRP_TV_EPC96:
                    real_len = LLRP_TV_LEN_EPC96; break;
                case LLRP_TV_SPEC_INDEX:
                    real_len = LLRP_TV_LEN_SPEC_INDEX; break;
                case LLRP_TV_CLIENT_REQ_OP_SPEC_RES:
                    real_len = LLRP_TV_LEN_CLIENT_REQ_OP_SPEC_RES; break;
                case LLRP_TV_ACCESS_SPEC_ID:
                    real_len = LLRP_TV_LEN_ACCESS_SPEC_ID; break;
                case LLRP_TV_OP_SPEC_ID:
                    real_len = LLRP_TV_LEN_OP_SPEC_ID; break;
                case LLRP_TV_C1G2_SINGULATION_DET:
                    real_len = LLRP_TV_LEN_C1G2_SINGULATION_DET; break;
                case LLRP_TV_C1G2_XPC_W1:
                    real_len = LLRP_TV_LEN_C1G2_XPC_W1; break;
                case LLRP_TV_C1G2_XPC_W2:
                    real_len = LLRP_TV_LEN_C1G2_XPC_W2; break;
                default:
                    /* ???
                     * No need to mark it, since the hf_llrp_tv_type field
                     * will already show up as 'unknown'. */
                    real_len = 0;
                    break;
            };

            ti = proto_tree_add_none_format(tree, hf_llrp_param, tvb,
                    offset, real_len + 1, "TV Parameter : %s",
                    val_to_str_ext(type, &tv_type_ext, "Unknown Type: %d"));
            param_tree = proto_item_add_subtree(ti, ett_llrp_param);

            proto_tree_add_item(param_tree, hf_llrp_tv_type, tvb,
                    offset, 1, ENC_NA);
            offset++;

            suboffset = offset;
            switch (type)
            {
                case LLRP_TV_ANTENNA_ID:
                    PARAM_TREE_ADD_STAY(antenna_id, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_FIRST_SEEN_TIME_UTC:
                case LLRP_TV_FIRST_SEEN_TIME_UPTIME:
                case LLRP_TV_LAST_SEEN_TIME_UTC:
                case LLRP_TV_LAST_SEEN_TIME_UPTIME:
                    PARAM_TREE_ADD_STAY(microseconds, 8, ENC_BIG_ENDIAN); break;
                case LLRP_TV_PEAK_RSSI:
                    PARAM_TREE_ADD_STAY(peak_rssi, 1, ENC_NA); break;
                case LLRP_TV_CHANNEL_INDEX:
                    PARAM_TREE_ADD_STAY(channel_idx, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_TAG_SEEN_COUNT:
                    PARAM_TREE_ADD_STAY(tag_count, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_RO_SPEC_ID:
                    PARAM_TREE_ADD_STAY(rospec_id, 4, ENC_BIG_ENDIAN); break;
                case LLRP_TV_INVENTORY_PARAM_SPEC_ID:
                    PARAM_TREE_ADD_STAY(inventory_spec_id, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_C1G2_CRC:
                    PARAM_TREE_ADD_STAY(crc, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_C1G2_PC:
                    PARAM_TREE_ADD_STAY(pc_bits, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_EPC96:
                    PARAM_TREE_ADD_STAY(epc, 96/8, ENC_NA); break;
                case LLRP_TV_SPEC_INDEX:
                    PARAM_TREE_ADD_STAY(spec_idx, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_CLIENT_REQ_OP_SPEC_RES:
                    PARAM_TREE_ADD_STAY(opspec_id, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_ACCESS_SPEC_ID:
                    PARAM_TREE_ADD_STAY(accessspec_id, 4, ENC_BIG_ENDIAN); break;
                case LLRP_TV_OP_SPEC_ID:
                    PARAM_TREE_ADD_STAY(opspec_id, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_C1G2_SINGULATION_DET:
                    PARAM_TREE_ADD_STAY(num_coll, 2, ENC_BIG_ENDIAN);
                    PARAM_TREE_ADD_STAY(num_empty, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_C1G2_XPC_W1:
                    PARAM_TREE_ADD_STAY(xpc_w1, 2, ENC_BIG_ENDIAN); break;
                case LLRP_TV_C1G2_XPC_W2:
                    PARAM_TREE_ADD_STAY(xpc_w2, 2, ENC_BIG_ENDIAN); break;
                    break;
            };
            /* Unlike for TLV's, real_len for TV's doesn't include the standard
             * header length, so just add it straight to the offset. */
            offset += real_len;
        }
    }
    return offset;
}

#undef PARAM_TREE_ADD_STAY
#undef PARAM_TREE_ADD
#undef PARAM_TREE_ADD_SPEC_STAY
#undef PARAM_TREE_ADD_SPEC

static guint
dissect_llrp_impinj_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, guint offset)
{
    guint8 subtype;

    subtype = tvb_get_guint8(tvb, offset);

    col_append_fstr(pinfo->cinfo, COL_INFO, " (Impinj - %s)",
            val_to_str_ext(subtype, &impinj_msg_subtype_ext, "Unknown Type: %d"));
    proto_tree_add_item(tree, hf_llrp_impinj_msg_type, tvb, offset, 1, ENC_NA);
    offset += 1;

    switch(subtype) {
    case LLRP_IMPINJ_TYPE_ENABLE_EXTENSIONS:
        proto_tree_add_item(tree, hf_llrp_rfu, tvb, offset, 4, ENC_NA);
        offset += 4;
        break;
    case LLRP_IMPINJ_TYPE_ENABLE_EXTENSIONS_RESPONSE:
        /* Just parameters */
        break;
    case LLRP_IMPINJ_TYPE_SAVE_SETTINGS:
        proto_tree_add_item(tree, hf_llrp_save_config, tvb, offset, 1, ENC_NA);
        offset += 1;
        break;
    case LLRP_IMPINJ_TYPE_SAVE_SETTINGS_RESPONSE:
        /* Just parameters */
        break;
    }
    /* Just return offset, parameters will be dissected by our callee */
    return offset;
}

static void
dissect_llrp_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
        guint16 type, guint offset)
{
    gboolean    ends_with_parameters;
    guint8      requested_data;
    guint16     antenna_id, gpi_port, gpo_port;
    guint32     spec_id, vendor;
    proto_item *request_item, *antenna_item, *gpi_item, *gpo_item;
    guint (*dissect_custom_message)(tvbuff_t *tvb,
            packet_info *pinfo, proto_tree *tree, guint offset);

    ends_with_parameters = FALSE;
    switch (type)
    {
        /* Simple cases just have normal TLV or TV parameters */
        case LLRP_TYPE_CLOSE_CONNECTION_RESPONSE:
        case LLRP_TYPE_GET_READER_CAPABILITES_RESPONSE:
        case LLRP_TYPE_ADD_ROSPEC:
        case LLRP_TYPE_ADD_ROSPEC_RESPONSE:
        case LLRP_TYPE_DELETE_ROSPEC_RESPONSE:
        case LLRP_TYPE_START_ROSPEC_RESPONSE:
        case LLRP_TYPE_STOP_ROSPEC_RESPONSE:
        case LLRP_TYPE_ENABLE_ROSPEC_RESPONSE:
        case LLRP_TYPE_DISABLE_ROSPEC_RESPONSE:
        case LLRP_TYPE_GET_ROSPECS_RESPONSE:
        case LLRP_TYPE_ADD_ACCESSSPEC:
        case LLRP_TYPE_ADD_ACCESSSPEC_RESPONSE:
        case LLRP_TYPE_DELETE_ACCESSSPEC_RESPONSE:
        case LLRP_TYPE_ENABLE_ACCESSSPEC_RESPONSE:
        case LLRP_TYPE_DISABLE_ACCESSSPEC_RESPONSE:
        case LLRP_TYPE_GET_ACCESSSPECS:
        case LLRP_TYPE_CLIENT_REQUEST_OP:
        case LLRP_TYPE_CLIENT_RESQUEST_OP_RESPONSE:
        case LLRP_TYPE_RO_ACCESS_REPORT:
        case LLRP_TYPE_READER_EVENT_NOTIFICATION:
        case LLRP_TYPE_ERROR_MESSAGE:
        case LLRP_TYPE_GET_READER_CONFIG_RESPONSE:
        case LLRP_TYPE_SET_READER_CONFIG_RESPONSE:
        case LLRP_TYPE_SET_PROTOCOL_VERSION_RESPONSE:
        case LLRP_TYPE_GET_ACCESSSPECS_RESPONSE:
        case LLRP_TYPE_GET_REPORT:
        case LLRP_TYPE_ENABLE_EVENTS_AND_REPORTS:
            ends_with_parameters = TRUE;
            break;
        /* Some just have an ROSpec ID */
        case LLRP_TYPE_START_ROSPEC:
        case LLRP_TYPE_STOP_ROSPEC:
        case LLRP_TYPE_ENABLE_ROSPEC:
        case LLRP_TYPE_DISABLE_ROSPEC:
        case LLRP_TYPE_DELETE_ROSPEC:
            spec_id = tvb_get_ntohl(tvb, offset);
            if (spec_id == LLRP_ROSPEC_ALL)
                proto_tree_add_uint_format(tree, hf_llrp_rospec, tvb,
                        offset, 4, spec_id, "All ROSpecs (%u)", spec_id);
            else
                proto_tree_add_item(tree, hf_llrp_rospec, tvb,
                        offset, 4, ENC_BIG_ENDIAN);
            offset += 4;
            break;
        /* Some just have an AccessSpec ID */
        case LLRP_TYPE_ENABLE_ACCESSSPEC:
        case LLRP_TYPE_DELETE_ACCESSSPEC:
        case LLRP_TYPE_DISABLE_ACCESSSPEC:
            spec_id = tvb_get_ntohl(tvb, offset);
            if (spec_id == LLRP_ACCESSSPEC_ALL)
                proto_tree_add_uint_format(tree, hf_llrp_accessspec, tvb,
                        offset, 4, spec_id, "All Access Specs (%u)", spec_id);
            else
                proto_tree_add_item(tree, hf_llrp_accessspec, tvb,
                        offset, 4, ENC_BIG_ENDIAN);
            offset += 4;
            break;
        case LLRP_TYPE_GET_READER_CAPABILITES:
            proto_tree_add_item(tree, hf_llrp_req_cap, tvb, offset, 1, ENC_NA);
            offset++;
            ends_with_parameters = TRUE;
            break;
        /* GET_READER_CONFIG is more complicated */
        case LLRP_TYPE_GET_READER_CONFIG:
            antenna_id = tvb_get_ntohs(tvb, offset);
            if (antenna_id == LLRP_ANTENNA_ALL)
                antenna_item = proto_tree_add_uint_format(tree, hf_llrp_antenna_id, tvb,
                        offset, 2, antenna_id, "All Antennas (%u)", antenna_id);
            else
                antenna_item = proto_tree_add_item(tree, hf_llrp_antenna_id, tvb,
                        offset, 2, ENC_BIG_ENDIAN);
            offset += 2;

            requested_data = tvb_get_guint8(tvb, offset);
            request_item = proto_tree_add_item(tree, hf_llrp_req_conf, tvb,
                    offset, 1, ENC_NA);
            offset++;

            gpi_port = tvb_get_ntohs(tvb, offset);
            if (gpi_port == LLRP_GPI_PORT_ALL)
                gpi_item = proto_tree_add_uint_format(tree, hf_llrp_gpi_port, tvb,
                        offset, 2, gpi_port, "All GPI Ports (%u)", gpi_port);
            else
                gpi_item = proto_tree_add_item(tree, hf_llrp_gpi_port, tvb,
                        offset, 2, ENC_BIG_ENDIAN);
            offset += 2;

            gpo_port = tvb_get_ntohs(tvb, offset);
            if (gpo_port == LLRP_GPO_PORT_ALL)
                gpo_item = proto_tree_add_uint_format(tree, hf_llrp_gpo_port, tvb,
                        offset, 2, gpo_port, "All GPO Ports (%u)", gpo_port);
            else
                gpo_item = proto_tree_add_item(tree, hf_llrp_gpo_port, tvb,
                        offset, 2, ENC_BIG_ENDIAN);
            offset += 2;

            switch (requested_data)
            {
                case LLRP_CONF_ALL:
                    break;
                case LLRP_CONF_ANTENNA_PROPERTIES:
                case LLRP_CONF_ANTENNA_CONFIGURATION:
                    /* Ignore both GPI and GPO ports */
                    proto_item_append_text(gpi_item, " (Ignored)");
                    proto_item_append_text(gpo_item, " (Ignored)");
                    break;
                case LLRP_CONF_IDENTIFICATION:
                case LLRP_CONF_RO_REPORT_SPEC:
                case LLRP_CONF_READER_EVENT_NOTIFICATION_SPEC:
                case LLRP_CONF_ACCESS_REPORT_SPEC:
                case LLRP_CONF_LLRP_CONFIGURATION_STATE:
                case LLRP_CONF_KEEPALIVE_SPEC:
                case LLRP_CONF_EVENTS_AND_REPORTS:
                    /* Ignore antenna ID */
                    proto_item_append_text(antenna_item, " (Ignored)");
                    /* Ignore both GPI and GPO ports */
                    proto_item_append_text(gpi_item, " (Ignored)");
                    proto_item_append_text(gpo_item, " (Ignored)");
                    break;
                case LLRP_CONF_GPI_PORT_CURRENT_STATE:
                    /* Ignore antenna ID */
                    proto_item_append_text(antenna_item, " (Ignored)");
                    /* Ignore GPO port */
                    proto_item_append_text(gpo_item, " (Ignored)");
                    break;
                case LLRP_CONF_GPO_WRITE_DATA:
                    /* Ignore antenna ID */
                    proto_item_append_text(antenna_item, " (Ignored)");
                    /* Ignore GPI port */
                    proto_item_append_text(gpi_item, " (Ignored)");
                    break;
                default:
                    /* Ignore antenna ID */
                    proto_item_append_text(antenna_item, " (Ignored)");
                    /* Tell the user that we are confused */
                    expert_add_info_format(pinfo, request_item, PI_MALFORMED, PI_ERROR,
                            "Unrecognized configuration request: %u",
                            requested_data);
                    /* Ignore both GPI and GPO ports */
                    proto_item_append_text(gpi_item, " (Ignored)");
                    proto_item_append_text(gpo_item, " (Ignored)");
                    break;
            };
            ends_with_parameters = TRUE;
            break;
        /* END GET_READER_CONFIG */
        /* Misc */
        case LLRP_TYPE_SET_READER_CONFIG:
            proto_tree_add_item(tree, hf_llrp_rest_fact, tvb, offset, 1, ENC_NA);
            offset++;
            ends_with_parameters = TRUE;
            break;
        case LLRP_TYPE_SET_PROTOCOL_VERSION:
            proto_tree_add_item(tree, hf_llrp_version, tvb, offset, 1, ENC_NA);
            break;
        case LLRP_TYPE_GET_SUPPORTED_VERSION_RESPONSE:
            proto_tree_add_item(tree, hf_llrp_cur_ver, tvb, offset, 1, ENC_NA);
            offset++;
            proto_tree_add_item(tree, hf_llrp_sup_ver, tvb, offset, 1, ENC_NA);
            offset++;
            ends_with_parameters = TRUE;
            break;
        case LLRP_TYPE_CUSTOM_MESSAGE:
            vendor = tvb_get_ntohl(tvb, offset);
            proto_tree_add_item(tree, hf_llrp_vendor, tvb, offset, 4, ENC_BIG_ENDIAN);
            offset += 4;
            /* Do vendor specific dissection */
            switch(vendor) {
            case LLRP_VENDOR_IMPINJ:
                dissect_custom_message = dissect_llrp_impinj_message;
                ends_with_parameters = TRUE;
                break;
            }
            offset = dissect_custom_message(tvb, pinfo, tree, offset);
            break;
        /* Some have no extra data expected */
        case LLRP_TYPE_KEEPALIVE:
        case LLRP_TYPE_KEEPALIVE_ACK:
        case LLRP_TYPE_CLOSE_CONNECTION:
        case LLRP_TYPE_GET_ROSPECS:
        case LLRP_TYPE_GET_SUPPORTED_VERSION:
            break;
        default:
            /* We shouldn't be called if we don't already recognize the value */
            DISSECTOR_ASSERT_NOT_REACHED();
    };
    if(ends_with_parameters) {
        offset = dissect_llrp_parameters(tvb, pinfo, tree, offset, tvb_reported_length(tvb));
    }
    if(tvb_reported_length_remaining(tvb, offset) != 0) {
        /* Report problem */
        expert_add_info_format(pinfo, tree, PI_MALFORMED, PI_ERROR,
                "Incorrect length of message: %u bytes decoded, but %u bytes available.",
                offset, tvb_reported_length(tvb));
    }
}

/* Code to actually dissect the packets */
static void
dissect_llrp_packet(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    proto_item *ti;
    proto_tree *llrp_tree;
    guint16     type;
    guint32     len;
    guint       offset = 0;

    /* Check that there's enough data */
    DISSECTOR_ASSERT(tvb_reported_length(tvb) >= LLRP_HEADER_LENGTH);

    /* Make entries in Protocol column and Info column on summary display */
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "LLRP");

    col_set_str(pinfo->cinfo, COL_INFO, "LLRP Message");

    type = tvb_get_ntohs(tvb, offset) & 0x03FF;

    col_append_fstr(pinfo->cinfo, COL_INFO, " (%s)",
                    val_to_str_ext(type, &message_types_ext, "Unknown Type: %d"));

    ti = proto_tree_add_item(tree, proto_llrp, tvb, offset, -1, ENC_NA);
    llrp_tree = proto_item_add_subtree(ti, ett_llrp);

    proto_tree_add_item(llrp_tree, hf_llrp_version, tvb, offset, 1, ENC_NA);
    proto_tree_add_item(llrp_tree, hf_llrp_type, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    ti = proto_tree_add_item(llrp_tree, hf_llrp_length, tvb, offset, 4, ENC_BIG_ENDIAN);
    len = tvb_get_ntohl(tvb, offset);
    if (len != tvb_reported_length(tvb))
    {
        expert_add_info_format(pinfo, ti, PI_MALFORMED, PI_ERROR,
                               "Incorrect length field: claimed %u, but have %u.",
                               len, tvb_reported_length(tvb));
    }
    offset += 4;

    proto_tree_add_item(llrp_tree, hf_llrp_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (match_strval_ext(type, &message_types_ext))
        dissect_llrp_message(tvb, pinfo, llrp_tree, type, offset);
}

/* Determine length of LLRP message */
static guint
get_llrp_message_len(packet_info *pinfo _U_, tvbuff_t *tvb, int offset)
{
    /* Peek into the header to determine the total message length */
    return (guint)tvb_get_ntohl(tvb, offset+2);
}

/* The main dissecting routine */
static void
dissect_llrp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    tcp_dissect_pdus(tvb, pinfo, tree, TRUE, LLRP_HEADER_LENGTH,
        get_llrp_message_len, dissect_llrp_packet);
}

void
proto_register_llrp(void)
{
    /* Setup list of header fields  See Section 1.6.1 for details */
    static hf_register_info hf[] = {
        { &hf_llrp_version,
        { "Version", "llrp.version", FT_UINT8, BASE_DEC, VALS(llrp_versions), 0x1C,
          NULL, HFILL }},

        { &hf_llrp_type,
        { "Type", "llrp.type", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &message_types_ext, 0x03FF,
          NULL, HFILL }},

        { &hf_llrp_length,
        { "Length", "llrp.length", FT_UINT32, BASE_DEC, NULL, 0,
          NULL, HFILL }},

        { &hf_llrp_id,
        { "ID", "llrp.id", FT_UINT32, BASE_DEC, NULL, 0,
          NULL, HFILL }},

        { &hf_llrp_cur_ver,
        { "Current Version", "llrp.cur_ver", FT_UINT8, BASE_DEC, VALS(llrp_versions), 0,
          NULL, HFILL }},

        { &hf_llrp_sup_ver,
        { "Supported Version", "llrp.sup_ver", FT_UINT8, BASE_DEC, VALS(llrp_versions), 0,
          "The max supported protocol version.", HFILL }},

        { &hf_llrp_req_cap,
        { "Requested Capabilities", "llrp.req_cap", FT_UINT8, BASE_DEC, VALS(capabilities_request), 0,
          NULL, HFILL }},

        { &hf_llrp_req_conf,
        { "Requested Configuration", "llrp.req_conf", FT_UINT8, BASE_DEC | BASE_EXT_STRING, &config_request_ext, 0,
          NULL, HFILL }},

        { &hf_llrp_rospec,
        { "ROSpec ID", "llrp.rospec", FT_UINT32, BASE_DEC, NULL, 0,
          NULL, HFILL }},

        { &hf_llrp_antenna_id,
        { "Antenna ID", "llrp.antenna_id", FT_UINT16, BASE_DEC, NULL, 0,
          NULL, HFILL }},

        { &hf_llrp_gpi_port,
        { "GPI Port Number", "llrp.gpi_port", FT_UINT16, BASE_DEC, NULL, 0,
          NULL, HFILL }},

        { &hf_llrp_gpo_port,
        { "GPO Port Number", "llrp.gpo_port", FT_UINT16, BASE_DEC, NULL, 0,
          NULL, HFILL }},

        { &hf_llrp_rest_fact,
        { "Restore Factory Settings", "llrp.rest_fact", FT_BOOLEAN, 8, TFS(&tfs_yes_no), 0x80,
          NULL, HFILL }},

        { &hf_llrp_accessspec,
        { "Access Spec ID", "llrp.accessspec", FT_UINT32, BASE_DEC, NULL, 0,
          NULL, HFILL }},

        { &hf_llrp_vendor,
        { "Vendor ID", "llrp.vendor", FT_UINT32, BASE_DEC, VALS(llrp_vendors), 0,
          NULL, HFILL }},

        { &hf_llrp_impinj_msg_type,
        { "Subtype", "llrp.impinj.type", FT_UINT8, BASE_DEC | BASE_EXT_STRING, &impinj_msg_subtype_ext, 0,
          "Subtype specified by vendor", HFILL }},

        { &hf_llrp_tlv_type,
        { "Type", "llrp.tlv_type", FT_UINT16, BASE_DEC | BASE_EXT_STRING, &tlv_type_ext, 0x03FF,
          "The type of TLV.", HFILL }},

        { &hf_llrp_tv_type,
        { "Type", "llrp.tv_type", FT_UINT8, BASE_DEC | BASE_EXT_STRING, &tv_type_ext, 0x7F,
          "The type of TV.", HFILL }},

        { &hf_llrp_tlv_len,
        { "Length", "llrp.tlv_len", FT_UINT16, BASE_DEC, NULL, 0,
          "The length of this TLV.", HFILL }},

        { &hf_llrp_param,
        { "Parameter", "llrp.param", FT_NONE, BASE_NONE, NULL, 0,
          NULL, HFILL }},

#define HF_PARAM(name, desc, ft, enc, trans, bit) {&hf_llrp_##name, \
                                                  {desc, "llrp.param." #name, ft, enc, trans, bit, NULL, HFILL }},
        HF_LLRP_PARAMETER_LIST
#undef HF_PARAM
    };

    /* Setup protocol subtree array */
    static gint *ett[] = {
        &ett_llrp,
        &ett_llrp_param
    };

    /* Register the protocol name and description */
    proto_llrp = proto_register_protocol("Low Level Reader Protocol",
            "LLRP", "llrp");

    /* Required function calls to register the header fields and subtrees used */
    proto_register_field_array(proto_llrp, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
}

void
proto_reg_handoff_llrp(void)
{
    dissector_handle_t llrp_handle;

    llrp_handle = create_dissector_handle(dissect_llrp, proto_llrp);
    dissector_add_uint("tcp.port", LLRP_PORT, llrp_handle);
}


/*
 * Editor modelines  -  http://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
