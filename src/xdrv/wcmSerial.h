/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2008 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XF86_WCMSERIAL_H
#define __XF86_WCMSERIAL_H

#include "xf86Wacom.h"

#define WC_RESET "\r#"           /* reset wacom IV or wacom V command set */
#define WC_RESET_BAUD "\r$"      /* reset baud rate to default (wacom V) ors
                                  * switch to wacom IIs (wacom IV) */
#define WC_CONFIG "~R\r"         /* request a configuration string */
#define WC_COORD "~C\r"          /* request max coordinates */
#define WC_MODEL "~#\r"          /* request model and ROM version */
#define WC_ISDV4_QUERY "*"       /* ISDV4 query command */
#define WC_ISDV4_TOUCH_QUERY "%" /* ISDV4 touch query command */
#define WC_ISDV4_STOP "0"        /* ISDV4 stop command */
#define WC_ISDV4_SAMPLING "1"    /* ISDV4 sampling command */

#define WC_MULTI "MU1\r"         /* multi mode input */
#define WC_UPPER_ORIGIN "OC1\r"  /* origin in upper left */
#define WC_SUPPRESS "SU"         /* suppress mode */
#define WC_ALL_MACRO "~M0\r"     /* enable all macro buttons */
#define WC_NO_MACRO1 "~M1\r"     /* disable macro buttons of group 1 */
#define WC_RATE  "IT0\r"         /* max transmit rate (unit of 5 ms) */
#define WC_TILT_MODE "FM1\r"     /* enable extra protocol for tilt management */
#define WC_NO_INCREMENT "IN0\r"  /* do not enable increment mode */
#define WC_STREAM_MODE "SR\r"    /* enable continuous mode */
#define WC_PRESSURE_MODE "PH1\r" /* enable pressure mode */
#define WC_ZFILTER "ZF1\r"       /* stop sending coordinates */
#define WC_STOP  "\nSP\r"        /* stop sending coordinates */
#define WC_START "ST\r"          /* start sending coordinates */
#define WC_NEW_RESOLUTION "NR"   /* change the resolution */

#define WC_V_SINGLE "MT0\r"
#define WC_V_MULTI "MT1\r"
#define WC_V_HEIGHT "HT1\r"
#define WC_V_ID  "ID1\r"
#define WC_V_19200 "BA19\r"
#define WC_V_38400 "BA38\r"
/*  #define WC_V_9600 "BA96\r" */
#define WC_V_9600 "$\r"

#define WC_RESET_19200 "\r$"     /* reset to 9600 baud */
#define WC_RESET_19200_IV "\r#"

char* xf86WcmSendRequest(int fd, const char* request, char* answer, int maxlen);
int xf86WcmSerialValidate(WacomCommonPtr common, const unsigned char* data);

#endif /* __XF86_WCMSERIAL_H */

