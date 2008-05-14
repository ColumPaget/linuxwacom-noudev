/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
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

#include "xf86Wacom.h"
#include "wcmSerial.h"
#include "wcmFilter.h"

/* Serial Support */
static Bool serialDetect(LocalDevicePtr pDev);
static Bool serialInit(LocalDevicePtr pDev, char* id, float *version);

static int serialInitTablet(LocalDevicePtr local, char* id, float *version);
static void serialInitIntuos(WacomCommonPtr common, const char* id, float version);
static void serialInitIntuos2(WacomCommonPtr common, const char* id, float version);
static void serialInitCintiq(WacomCommonPtr common, const char* id, float version);
static void serialInitPenPartner(WacomCommonPtr common, const char* id, float version);
static void serialInitGraphire(WacomCommonPtr common, const char* id, float version);
static void serialInitProtocol4(WacomCommonPtr common, const char* id, float version);
static void serialGetResolution(LocalDevicePtr local);
static int serialGetRanges(LocalDevicePtr local);
static int serialResetIntuos(LocalDevicePtr local);
static int serialResetCintiq(LocalDevicePtr local);
static int serialResetPenPartner(LocalDevicePtr local);
static int serialResetProtocol4(LocalDevicePtr local);
static int serialEnableTiltProtocol4(LocalDevicePtr local);
static int serialEnableSuppressProtocol4(LocalDevicePtr local);
static int serialSetLinkSpeedIntuos(LocalDevicePtr local);
static int serialSetLinkSpeedProtocol5(LocalDevicePtr local);
static int serialStartTablet(LocalDevicePtr local);
static int serialParseCintiq(LocalDevicePtr local, const unsigned char* data);
static int serialParseGraphire(LocalDevicePtr local, const unsigned char* data);
static int serialParseProtocol4(LocalDevicePtr local, const unsigned char* data);
static int serialParseProtocol5(LocalDevicePtr local, const unsigned char* data);
static void serialParseP4Common(LocalDevicePtr local, const unsigned char* data, 
	WacomDeviceState* last, WacomDeviceState* ds);

/*****************************************************************************
 * Global Structures
 ****************************************************************************/

	WacomDeviceClass gWacomSerialDevice =
	{
		serialDetect,
		serialInit,
		xf86WcmReadPacket,
	};

/*****************************************************************************
 * Static Structures
 ****************************************************************************/

	static WacomModel serialIntuos =
	{
		"Serial Intuos",
		serialInitIntuos,
		NULL,           /* resolution not queried */
		serialGetRanges,
		serialResetIntuos,
		NULL,           /* tilt automatically enabled */
		NULL,           /* suppress implemented in software */
		serialSetLinkSpeedIntuos,
		serialStartTablet,
		serialParseProtocol5,
		xf86WcmFilterIntuos,
	};

	static WacomModel serialIntuos2 =
	{
		"Serial Intuos2",
		serialInitIntuos2,
		NULL,                 /* resolution not queried */
		serialGetRanges,
		serialResetIntuos,    /* same as Intuos */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		serialSetLinkSpeedProtocol5,
		serialStartTablet,
		serialParseProtocol5,
		xf86WcmFilterIntuos,
	};

	static WacomModel serialCintiq =
	{
		"Serial Cintiq",
		serialInitCintiq,
		serialGetResolution,
		serialGetRanges,
		serialResetCintiq,
		serialEnableTiltProtocol4,
		serialEnableSuppressProtocol4,
		NULL,               /* link speed cannot be changed */
		serialStartTablet,
		serialParseCintiq,
	};

	static WacomModel serialPenPartner =
	{
		"Serial PenPartner",
		serialInitPenPartner,
		NULL,               /* resolution not queried */
		serialGetRanges,
		serialResetPenPartner,
		serialEnableTiltProtocol4,
		serialEnableSuppressProtocol4,
		NULL,              /* link speed cannot be changed */
		serialStartTablet,
		serialParseProtocol4,
		xf86WcmFilterCoord,
	};	

	static WacomModel serialGraphire =
	{
		"Serial Graphire",
		serialInitGraphire,
		NULL,                     /* resolution not queried */
		NULL,                     /* ranges not supported */
		serialResetPenPartner,    /* functionally very similar */
		serialEnableTiltProtocol4,
		serialEnableSuppressProtocol4,
		NULL,                    /* link speed cannot be changed */
		serialStartTablet,
		serialParseGraphire,
		xf86WcmFilterCoord,
	};

	static WacomModel serialProtocol4 =
	{
		"Serial UD",
		serialInitProtocol4,
		serialGetResolution,
		serialGetRanges,
		serialResetProtocol4,
		serialEnableTiltProtocol4,
		serialEnableSuppressProtocol4,
		NULL,               /* link speed cannot be changed */
		serialStartTablet,
		serialParseProtocol4,
	};

/*****************************************************************************
 * Setup strings
 ****************************************************************************/

	static const char * setup_string = WC_MULTI WC_UPPER_ORIGIN
		WC_ALL_MACRO WC_NO_MACRO1 WC_RATE WC_NO_INCREMENT
		WC_STREAM_MODE WC_ZFILTER;
	static const char * pl_setup_string = WC_UPPER_ORIGIN WC_RATE
		WC_STREAM_MODE;
	static const char * penpartner_setup_string = WC_PRESSURE_MODE
		WC_START;
	static const char * intuos_setup_string = WC_V_MULTI WC_V_ID WC_RATE
		WC_START;

	/* PROTOCOL 4 */

	/* Format of 7 bytes data packet for Wacom Tablets
	Byte 1
	bit 7  Sync bit always 1
	bit 6  Pointing device detected
	bit 5  Cursor = 0 / Stylus = 1
	bit 4  Reserved
	bit 3  1 if a button on the pointing device has been pressed
	bit 2  Reserved
	bit 1  X15
	bit 0  X14

	Byte 2
	bit 7  Always 0
	bits 6-0 = X13 - X7

	Byte 3
	bit 7  Always 0
	bits 6-0 = X6 - X0

	Byte 4
	bit 7  Always 0
	bit 6  B3
	bit 5  B2
	bit 4  B1
	bit 3  B0
	bit 2  P0
	bit 1  Y15
	bit 0  Y14

	Byte 5
	bit 7  Always 0
	bits 6-0 = Y13 - Y7

	Byte 6
	bit 7  Always 0
	bits 6-0 = Y6 - Y0

	Byte 7
	bit 7 Always 0
	bit 6  Sign of pressure data
	bit 5  P6
	bit 4  P5
	bit 3  P4
	bit 2  P3
	bit 1  P2
	bit 0  P1

	byte 8 and 9 are optional and present only
	in tilt mode.

	Byte 8
	bit 7 Always 0
	bit 6 Sign of tilt X
	bit 5  Xt6
	bit 4  Xt5
	bit 3  Xt4
	bit 2  Xt3
	bit 1  Xt2
	bit 0  Xt1

	Byte 9
	bit 7 Always 0
	bit 6 Sign of tilt Y
	bit 5  Yt6
	bit 4  Yt5
	bit 3  Yt4
	bit 2  Yt3
	bit 1  Yt2
	bit 0  Yt1
	*/

/*****************************************************************************
 * xf86WcmWrite --
 *   send a request 
 ****************************************************************************/

int xf86WcmWriteWait(int fd, const char* request)
{
	int len, maxtry = MAXTRY;

	/* send request string */
	do
	{
		len = xf86WriteSerial(fd, request, strlen(request));
		if ((len == -1) && (errno != EAGAIN))
		{
			ErrorF("Wacom xf86WcmWriteWait error : %s", strerror(errno));
			return 0;
		}

		maxtry--;

	} while ((len <= 0) && maxtry);

	return maxtry;
}

/*****************************************************************************
 * xf86WcmWaitForTablet --
 *   wait for tablet data 
 ****************************************************************************/

int xf86WcmWaitForTablet(int fd, char* answer, int size)
{
	int len, maxtry = MAXTRY;

	/* Read size bytes of the answer */
	do
	{
		if ((len = xf86WaitForInput(fd, 1000000)) > 0)
		{
			len = xf86WcmRead(fd, answer, size);
			if ((len == -1) && (errno != EAGAIN))
			{
				ErrorF("Wacom xf86WcmRead error : %s\n",
						strerror(errno));
				return 0;
			}
		}
		maxtry--;
	} while ((len <= 0) && maxtry);

	return maxtry;
}

/*****************************************************************************
 * xf86WcmSendRequest --
 *   send a request and wait for the answer.
 *   the answer must begin with the first two chars of the request.
 *   The last character in the answer string is replaced by a \0.
 ****************************************************************************/

char* xf86WcmSendRequest(int fd, const char* request, char* answer, int maxlen)
{
	int len, nr = 0;
	int maxtry = MAXTRY;

	if (maxlen < 3)
		return NULL;
  
	/* wait for request return */
	if (!xf86WcmWriteWait(fd, request))
	{
		ErrorF("Wacom unable to xf86WcmWrite request string '%s' "
				"after %d tries\n", request, MAXTRY);
		return NULL;
	}

	do
	{
		/* Read the first byte of the answer which must
		 * be equal to the first byte of the request.
		 */
		maxtry = xf86WcmWaitForTablet(fd, answer, 1);
		if (answer[0] != request[0])
		{
			ErrorF("Wacom unable to read first byte of "
					"request '%c%c' answer after %d tries\n",
					request[0], request[1], maxtry);
			return NULL;
		}

		/* Read the second byte of the answer which must be equal
		 * to the second byte of the request. */
		if (!xf86WcmWaitForTablet(fd, answer+1, 1))
		{
			ErrorF("Wacom unable to read second byte of "
				"request '%c%c' answer after %d "
				"tries\n", request[0], request[1], maxtry);
			return NULL;
		}

		if (answer[1] != request[1])
			answer[0] = answer[1];

	} while ((answer[0] == request[0]) && (answer[1] != request[1]));


	/* Read until we don't get anything or timeout. */

	len = 2;
	do
	{
		if (len == 2)
		{
			if (!xf86WcmWaitForTablet(fd, answer+len, 1))
			{
				ErrorF("Wacom unable to read last byte of request '%c%c' "
					"answer after %d tries\n",
					request[0], request[1], MAXTRY);
				return NULL;
			}
			len++;
		}

		if ((nr = xf86WaitForInput(fd, 1000000)) > 0)
		{
			nr = xf86WcmRead(fd, answer+len, 1);
			if ((nr == -1) && (errno != EAGAIN))
			{
				ErrorF("Wacom xf86WcmRead in xf86WcmSendRequest error : %s\n",
						strerror(errno));
				return NULL;
			}
		}

		if (nr > 0)
		{
			len += nr;
			if (len >= (maxlen - 1))
				return NULL;
		}

	} while (nr > 0);

	if (len <= 3)
		return NULL;

	answer[len-1] = '\0';

	return answer;
}

static Bool serialDetect(LocalDevicePtr pDev)
{
	return 1;
}

static Bool serialInit(LocalDevicePtr local, char* id, float *version)
{
	int err;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(1, priv->debugLevel, ErrorF("initializing serial tablet\n"));

	/* Set the speed of the serial link to 38400 */
	if (xf86WcmSetSerialSpeed(local->fd, 38400) < 0)
		return !Success;

	/* Send reset to the tablet */
	err = xf86WcmWrite(local->fd, WC_RESET_BAUD,
		strlen(WC_RESET_BAUD));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;

	/* Send reset to the tablet */
	err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 75 mSecs */
	if (xf86WcmWait(75))
		return !Success;

	/* Set the speed of the serial link to 19200 */
	if (xf86WcmSetSerialSpeed(local->fd, 19200) < 0)
		return !Success;

	/* Send reset to the tablet */
	err = xf86WcmWrite(local->fd, WC_RESET_BAUD,
		strlen(WC_RESET_BAUD));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;

	/* Send reset to the tablet */
	err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 75 mSecs */
	if (xf86WcmWait(75))
		return !Success;

	/* Set the speed of the serial link to 9600 */
	if (xf86WcmSetSerialSpeed(local->fd, 9600) < 0)
		return !Success;

	/* Send reset to the tablet */
	err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;

	err = xf86WcmWrite(local->fd, WC_STOP, strlen(WC_STOP));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 30 mSecs */
	if (xf86WcmWait(30))
		return !Success;

	xf86WcmFlushTablet(local->fd);

	return serialInitTablet(local, id, version);
}

/*****************************************************************************
 * serialInitTablet --
 *   Initialize the tablet
 ****************************************************************************/

static int serialInitTablet(LocalDevicePtr local, char* id, float *version)
{
	int loop, idx;
	char getID[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(2, priv->debugLevel, ErrorF("reading model\n"));

	if (!xf86WcmSendRequest(local->fd, WC_MODEL, getID, sizeof(getID)))
		return !Success;

	DBG(2, priv->debugLevel, ErrorF("%s\n", getID));

	if (xf86Verbose)
		ErrorF("%s Wacom tablet model : %s\n",
				XCONFIG_PROBED, getID+2);

	/* Answer is in the form ~#Tablet-Model VRom_Version 
	 * look for the first V from the end of the string
	 * this seems to be the better way to find the version
	 * of the ROM */
	for(loop=strlen(getID); loop>=0 && *(getID+loop) != 'V'; loop--);
	for(idx=loop; idx<strlen(getID) && *(getID+idx) != '-'; idx++);
	*(getID+idx) = '\0';

	/* Extract version numbers */
	sscanf(getID+loop+1, "%f", version);

	/* Detect tablet model based on identifier */
	if (getID[2] == 'G' && getID[3] == 'D')
	{
		common->wcmModel = &serialIntuos;
		common->tablet_id = 0x20;
	}
	else if (getID[2] == 'X' && getID[3] == 'D')
	{
		common->wcmModel = &serialIntuos2;
		common->tablet_id = 0x40;
	}
	else if ( (getID[2] == 'P' && getID[3] == 'L') ||
		(getID[2] == 'D' && getID[3] == 'T') )
	{
		common->wcmModel = &serialCintiq;
		common->tablet_id = 0x30;
	}
	else if (getID[2] == 'C' && getID[3] == 'T')
	{
		common->wcmModel = &serialPenPartner;
		common->tablet_id = 0x00;
	}
	else if (getID[2] == 'E' && getID[3] == 'T')
	{
		common->wcmModel = &serialGraphire;
		common->tablet_id = 0x10;
	}
	else
	{
		common->wcmModel = &serialProtocol4;
		common->tablet_id = 0x03;
	}
	sprintf(id, "%s", getID);

	return Success;
}

static int serialParseGraphire(LocalDevicePtr local, const unsigned char* data)
{
	int n;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* ds;

	/* positive value is skip */
	if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[0].work;
	RESET_RELATIVE(*ds);

	/* get pressure */
	ds->pressure = ((data[6]&ZAXIS_BITS) << 2 ) +
		((data[3]&ZAXIS_BIT) >> 1) +
		((data[0]&ZAXIS_BIT) >> 6) +
		((data[6]&ZAXIS_SIGN_BIT) ? 0 : 0x100);

	/* get buttons */
	ds->buttons = (data[3] & BUTTONS_BITS) >> 3;
	
	/* requires button info, so it goes down here. */
	serialParseP4Common(local, data, last, ds);

	/* handle relative wheel for non-stylus device */
	if (ds->device_type == CURSOR_ID)
	{
		ds->relwheel = (data[6] & 0x30) >> 4;
		if (data[6] & 0x40)
			ds->relwheel = -ds->relwheel;
	}

	xf86WcmEvent(common,0,ds);
	return common->wcmPktLength;
}

static int serialParseCintiq(LocalDevicePtr local, const unsigned char* data)
{
	int n;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* ds;

	/* positive value is skip */
	if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[0].work;
	RESET_RELATIVE(*ds);

	/* get pressure */
	if (common->wcmMaxZ == 255)
	{
		ds->pressure = ((data[6] & ZAXIS_BITS) << 1 ) |
			((data[3] & ZAXIS_BIT) >> 2) |
			((data[6] & ZAXIS_SIGN_BIT) ? 0 : 0x80);
	}
	else
	{
		/* which tablets use this? */
		/* PL550 and PL800 apparently */
		ds->pressure = ((data[6]&ZAXIS_BITS) << 2 ) +
			((data[3]&ZAXIS_BIT) >> 1) +
			((data[0]&ZAXIS_BIT) >> 6) +
			((data[6]&ZAXIS_SIGN_BIT) ? 0 : 0x100);
	}

	/* get buttons */
	ds->buttons = (data[3] & BUTTONS_BITS) >> 3;

	/* requires button info, so it goes down here. */
	serialParseP4Common(local, data, last, ds);

	xf86WcmEvent(common,0,ds);
	return common->wcmPktLength;
}

static int serialParseProtocol4(LocalDevicePtr local, const unsigned char* data)
{
	int n;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* ds;

	DBG(10, common->debugLevel, ErrorF("serialParseProtocol4 \n"));
	/* positive value is skip */
	if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[0].work;
	RESET_RELATIVE(*ds);

	/* get pressure */
	if (common->wcmMaxZ == 255)
		ds->pressure = ((data[6] & ZAXIS_BITS) << 1 ) |
			((data[3] & ZAXIS_BIT) >> 2) |
			((data[6] & ZAXIS_SIGN_BIT) ? 0 : 0x80);

	else
		ds->pressure = (data[6] & ZAXIS_BITS) |
			((data[6] & ZAXIS_SIGN_BIT) ? 0 : 0x40);

	/* get button state */
	ds->buttons = (data[3] & BUTTONS_BITS) >> 3;

	/* requires button info, so it goes down here. */
	serialParseP4Common(local, data, last, ds);

	xf86WcmEvent(common,0,ds);
	return common->wcmPktLength;
}

static int serialParseProtocol5(LocalDevicePtr local, const unsigned char* data)
{
	int n;
	int have_data=0;
	int channel;
	WacomDeviceState* ds;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(10, common->debugLevel, ErrorF("serialParseProtocol5 \n"));
	/* positive value is skip */
	if ((n = xf86WcmSerialValidate(common,data)) > 0) return n;

	/* Protocol 5 devices support 2 data channels */
	channel = data[0] & 0x01;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[channel].work;
	RESET_RELATIVE(*ds);

	DBG(7, common->debugLevel, ErrorF("packet header = %x\n", data[0]));

	/* Device ID packet */
	if ((data[0] & 0xfc) == 0xc0)
	{
		/* start from scratch */
		memset(ds, 0, sizeof(*ds));

		ds->proximity = 1;
		ds->device_id = (((data[1] & 0x7f) << 5) |
				((data[2] & 0x7c) >> 2));
		ds->serial_num = (((data[2] & 0x03) << 30) |
				((data[3] & 0x7f) << 23) |
				((data[4] & 0x7f) << 16) |
				((data[5] & 0x7f) << 9) |
				((data[6] & 0x7f) << 2) |
				((data[7] & 0x60) >> 5));

		if ((ds->device_id & 0xf06) != 0x802)
			ds->discard_first = 1;

		if (STYLUS_TOOL(ds))
			ds->device_type = STYLUS_ID;
		else if (CURSOR_TOOL(ds))
			ds->device_type = CURSOR_ID;
		else
			ds->device_type = ERASER_ID;

		DBG(6, common->debugLevel, ErrorF(
			"device_id=%x serial_num=%u type=%s\n",
			ds->device_id, ds->serial_num,
			(ds->device_type == STYLUS_ID) ? "stylus" :
			(ds->device_type == CURSOR_ID) ? "cursor" :
			"eraser"));
	}

	/* Out of proximity packet */
	else if ((data[0] & 0xfe) == 0x80)
	{
		ds->proximity = 0;
		have_data = 1;
	}

	/* General pen packet or eraser packet or airbrush first packet
	 * airbrush second packet */
	else if (((data[0] & 0xb8) == 0xa0) ||
			((data[0] & 0xbe) == 0xb4))
	{
		ds->x = (((data[1] & 0x7f) << 9) |
				((data[2] & 0x7f) << 2) |
				((data[3] & 0x60) >> 5));
		ds->y = (((data[3] & 0x1f) << 11) |
				((data[4] & 0x7f) << 4) |
				((data[5] & 0x78) >> 3));
		if ((data[0] & 0xb8) == 0xa0)
		{
			ds->pressure = (((data[5] & 0x07) << 7) |
				(data[6] & 0x7f));
			ds->buttons = (data[0] & 0x06);
		}
		else
		{
			/* 10 bits for absolute wheel position */
			ds->abswheel = (((data[5] & 0x07) << 7) |
				(data[6] & 0x7f));
		}
		ds->tiltx = (data[7] & TILT_BITS);
		ds->tilty = (data[8] & TILT_BITS);
		if (data[7] & TILT_SIGN_BIT)
			ds->tiltx -= (TILT_BITS + 1);
		if (data[8] & TILT_SIGN_BIT)
			ds->tilty -= (TILT_BITS + 1);
		ds->proximity = (data[0] & PROXIMITY_BIT);
		have_data = 1;
	} /* end pen packet */

	/* 4D mouse 1st packet or Lens cursor packet or 2D mouse packet*/
	else if (((data[0] & 0xbe) == 0xa8) ||
			((data[0] & 0xbe) == 0xb0))
	{
		ds->x = (((data[1] & 0x7f) << 9) |
				((data[2] & 0x7f) << 2) |
				((data[3] & 0x60) >> 5));
		ds->y = (((data[3] & 0x1f) << 11) |
				((data[4] & 0x7f) << 4) |
				((data[5] & 0x78) >> 3));
		ds->tilty = 0;

		/* 4D mouse */
		if (MOUSE_4D(ds))
		{
			ds->throttle = (((data[5] & 0x07) << 7) |
				(data[6] & 0x7f));
			if (data[8] & 0x08)
				ds->throttle = -ds->throttle;
			ds->buttons = (((data[8] & 0x70) >> 1) |
				(data[8] & 0x07));
			have_data = !ds->discard_first;
		}

		/* Lens cursor */
		else if (LENS_CURSOR(ds))
		{
			ds->buttons = data[8];
			have_data = 1;
		}

		/* 2D mouse */
		else if (MOUSE_2D(ds))
		{
			ds->buttons = (data[8] & 0x1C) >> 2;
			ds->relwheel = - (data[8] & 1) +
					((data[8] & 2) >> 1);
			have_data = 1; /* must send since relwheel is reset */
		}

		ds->proximity = (data[0] & PROXIMITY_BIT);
	} /* end 4D mouse 1st packet */

	/* 4D mouse 2nd packet */
	else if ((data[0] & 0xbe) == 0xaa)
	{
		ds->x = (((data[1] & 0x7f) << 9) |
			((data[2] & 0x7f) << 2) |
			((data[3] & 0x60) >> 5));
		ds->y = (((data[3] & 0x1f) << 11) |
			((data[4] & 0x7f) << 4) |
			((data[5] & 0x78) >> 3));
		ds->rotation = (((data[6] & 0x0f) << 7) |
			(data[7] & 0x7f));
		if (ds->rotation < 900) ds->rotation = -ds->rotation;
		else ds->rotation = 1799 - ds->rotation;
		ds->proximity = (data[0] & PROXIMITY_BIT);
		have_data = 1;
		ds->discard_first = 0;
	}
	else
	{
		DBG(10, common->debugLevel, ErrorF("unknown wacom V packet %x\n",data[0]));
	}

	/* if new data is available, send it */
	if (have_data)
	{
	       	xf86WcmEvent(common,channel,ds);
	}
	return common->wcmPktLength;
}

/*****************************************************************************
 * Model-specific functions
 ****************************************************************************/

static void serialInitIntuos(WacomCommonPtr common, const char* id, float version)
{
	common->wcmProtocolLevel = 5;
	common->wcmVersion = version;

	common->wcmMaxZ = 1023;   /* max Z value */
	common->wcmResolX = 2540; /* tablet X resolution in points/inch */
	common->wcmResolY = 2540; /* tablet Y resolution in points/inch */
	common->wcmPktLength = 9; /* length of a packet */
	common->wcmFlags |= TILT_ENABLED_FLAG;
}

static void serialInitIntuos2(WacomCommonPtr common, const char* id, float version)
{
	common->wcmProtocolLevel = 5;
	common->wcmVersion = version;

	common->wcmMaxZ = 1023;       /* max Z value */
	common->wcmResolX = 2540;     /* tablet X resolution in points/inch */
	common->wcmResolY = 2540;     /* tablet Y resolution in points/inch */
	common->wcmPktLength = 9;     /* length of a packet */
	common->wcmFlags |= TILT_ENABLED_FLAG;
}

static void serialInitCintiq(WacomCommonPtr common, const char* id, float version)
{
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = 7;
	common->wcmVersion = version;
	common->wcmResolX = 508; /* tablet X resolution in points/inch */
	common->wcmResolY = 508; /* tablet Y resolution in points/inch */

	if (id[5] == '2')
	{
		/* PL-250  */
		if ( id[6] == '5' )
		{
			common->wcmMaxZ = 255;
		}
		/* PL-270  */
		else
		{
			common->wcmMaxZ = 255;
		}
	}
	else if (id[5] == '3')
	{
		/* PL-300  */
		common->wcmMaxZ = 255;
	}
	else if (id[5] == '4')
	{
		/* PL-400  */
		common->wcmMaxZ = 255;
	}
	else if (id[5] == '5')
	{
		/* PL-550  */
		if ( id[6] == '5' )
		{
			common->wcmMaxZ = 511;
		}
		/* PL-500  */
		else
		{
			common->wcmMaxZ = 255;
		}
	}
	else if (id[5] == '6')
	{
		/* PL-600SX  */
		if ( id[8] == 'S' )
		{
			common->wcmMaxZ = 255;
		}
		/* PL-600  */
		else
		{
			common->wcmMaxZ = 255;
		}
	}
	else if (id[5] == '7')
	{
		/* PL-710  */
		common->wcmMaxZ = 511;
		common->wcmResolX = 2540;
		common->wcmResolY = 2540;
	}
	else if (id[5] == '8')
	{
		/* PL-800  */
		common->wcmMaxZ = 511;
	}
}

static void serialInitPenPartner(WacomCommonPtr common, const char* id, float version)
{
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = 7;
	common->wcmVersion = version;

	common->wcmMaxZ = 255;
	common->wcmResolX = 1000; /* tablet X resolution in points/inch */
	common->wcmResolY = 1000; /* tablet Y resolution in points/inch */
}

static void serialInitGraphire(WacomCommonPtr common, const char* id, float version)
{
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = 7;
	common->wcmVersion = version;

	/* Graphire models don't answer WC_COORD requests */
	common->wcmMaxX = 5103;
	common->wcmMaxY = 3711;
	common->wcmMaxZ = 511;
	common->wcmResolX = 1016; /* tablet X resolution in points/inch */
	common->wcmResolY = 1016; /* tablet Y resolution in points/inch */
}

static void serialInitProtocol4(WacomCommonPtr common, const char* id, float version)
{
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = 7;
	common->wcmVersion = version;

	/* If no maxZ is set, determine from version */
	if (!common->wcmMaxZ)
	{
		/* the rom version determines the max z */
		if (version >= (float)1.2)
			common->wcmMaxZ = 255;
		else
			common->wcmMaxZ = 120;
	}
}

static void serialGetResolution(LocalDevicePtr local)
{
	int a, b;
	char buffer[BUFFER_SIZE], header[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	if (!(common->wcmResolX && common->wcmResolY))
	{
		DBG(2, priv->debugLevel, ErrorF(
			"Requesting resolution from device\n"));
		if (xf86WcmSendRequest(local->fd, WC_CONFIG, buffer,
			sizeof(buffer)))
		{
			DBG(2, priv->debugLevel, ErrorF("%s\n", buffer));
			/* The header string is simply a place to put the
			 * unwanted config header don't use buffer+xx because
			 * the header size varies on different tablets */

			if (sscanf(buffer, "%[^,],%d,%d,%d,%d", header,
				 &a, &b, &common->wcmResolX,
				 &common->wcmResolY) == 5)
			{
				DBG(6, priv->debugLevel, ErrorF(
					"WC_CONFIG Header = %s\n", header));
			}
			else
			{
				ErrorF("WACOM: unable to parse resolution. "
					"Using default.\n");
				common->wcmResolX = common->wcmResolY = 1270;
			}
		}
		else
		{
			ErrorF("WACOM: unable to read resolution. "
				"Using default.\n");
			common->wcmResolX = common->wcmResolY = 1270;
		}
	}

	DBG(2, priv->debugLevel, ErrorF("serialGetResolution: ResolX=%d ResolY=%d\n",
		common->wcmResolX, common->wcmResolY));
}

static int serialGetRanges(LocalDevicePtr local)
{
	char buffer[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	if (!(common->wcmMaxX && common->wcmMaxY))
	{
		DBG(2, priv->debugLevel, ErrorF("Requesting max coordinates\n"));
		if (!xf86WcmSendRequest(local->fd, WC_COORD, buffer,
			sizeof(buffer)))
		{
			ErrorF("WACOM: unable to read max coordinates. "
				"Use the MaxX and MaxY options.\n");
			return !Success;
		}
		DBG(2, priv->debugLevel, ErrorF("%s\n", buffer));
		if (sscanf(buffer+2, "%d,%d", &common->wcmMaxX,
			&common->wcmMaxY) != 2)
		{
			ErrorF("WACOM: unable to parse max coordinates. "
				"Use the MaxX and MaxY options.\n");
			return !Success;
		}
	}

	DBG(2, priv->debugLevel, ErrorF("serialGetRanges: maxX=%d maxY=%d (%g,%g in)\n",
		common->wcmMaxX, common->wcmMaxY,
		(double)common->wcmMaxX / common->wcmResolX,
		(double)common->wcmMaxY / common->wcmResolY));

	return Success;
}

static int serialResetIntuos(LocalDevicePtr local)
{
	int err;
	err = xf86WcmWrite(local->fd, intuos_setup_string,
		strlen(intuos_setup_string));
	return (err == -1) ? !Success : Success;
}

static int serialResetCintiq(LocalDevicePtr local)
{
	int err;

	err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET));

	if (xf86WcmWait(75)) return !Success;

	err = xf86WcmWrite(local->fd, pl_setup_string,
		strlen(pl_setup_string));
	if (err == -1) return !Success;

	err = xf86WcmWrite(local->fd, penpartner_setup_string,
		strlen(penpartner_setup_string));

	return (err == -1) ? !Success : Success;
}

static int serialResetPenPartner(LocalDevicePtr local)
{
	int err;
	err = xf86WcmWrite(local->fd, penpartner_setup_string,
		strlen(penpartner_setup_string));
	return (err == -1) ? !Success : Success;
}

static int serialResetProtocol4(LocalDevicePtr local)
{
	int err;

	err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET));

	if (xf86WcmWait(75)) return !Success;

	err = xf86WcmWrite(local->fd, setup_string,
		strlen(setup_string));
	if (err == -1) return !Success;

	err = xf86WcmWrite(local->fd, penpartner_setup_string,
		strlen(penpartner_setup_string));
	return (err == -1) ? !Success : Success;
}

static int serialEnableTiltProtocol4(LocalDevicePtr local)
{
	return Success;
}

static int serialEnableSuppressProtocol4(LocalDevicePtr local)
{
	char buf[20];
	int err;
	WacomCommonPtr common =	((WacomDevicePtr)(local->private))->common;

	sprintf(buf, "%s%d\r", WC_SUPPRESS, common->wcmSuppress);
	err = xf86WcmWrite(local->fd, buf, strlen(buf));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n",
			strerror(errno));
		return !Success;
	}
	return Success;
}

static int serialSetLinkSpeedIntuos(LocalDevicePtr local)
{
	WacomCommonPtr common =	((WacomDevicePtr)(local->private))->common;

	if ((common->wcmLinkSpeed == 38400) &&
		(common->wcmVersion < 2.0F))
	{
		ErrorF("Wacom: 38400 speed not supported with this Intuos "
			"firmware (%f)\n", common->wcmVersion);
		ErrorF("Switching to 19200\n");
		common->wcmLinkSpeed = 19200;
	}
	return serialSetLinkSpeedProtocol5(local);
}

static int serialSetLinkSpeedProtocol5(LocalDevicePtr local)
{
	int err;
	char* speed_init_string;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common =	priv->common;

	DBG(1, priv->debugLevel, ErrorF("Switching serial link to %d\n",
		common->wcmLinkSpeed));

	/* set init string according to speed */
	speed_init_string = (common->wcmLinkSpeed == 38400) ?
		WC_V_38400 : WC_V_19200;

	/* Switch the tablet to the requested speed */
	err = xf86WcmWrite(local->fd, speed_init_string,
		strlen(speed_init_string));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 75 mSecs */
	if (xf86WcmWait(75))
		return !Success;

	/* Set speed of serial link to requested speed */
	if (xf86WcmSetSerialSpeed(local->fd, common->wcmLinkSpeed) < 0)
		return !Success;

	return Success;
}

static int serialStartTablet(LocalDevicePtr local)
{
	int err;

	/* Tell the tablet to start sending coordinates */
	err = xf86WcmWrite(local->fd, WC_START, strlen(WC_START));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	return Success;
}

static void serialParseP4Common(LocalDevicePtr local,
	const unsigned char* data, WacomDeviceState* last,
	WacomDeviceState* ds)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int is_stylus = (data[0] & POINTER_BIT);
	int cur_type = is_stylus ?
		((ds->buttons & 4) ? ERASER_ID : STYLUS_ID) :
		CURSOR_ID;

	/* for Graphire eraser */
	if(ds->buttons & 8) cur_type = ERASER_ID;

	/* proximity bit */
	ds->proximity = (data[0] & PROXIMITY_BIT);

	/* x and y coordinates */
	ds->x = (((data[0] & 0x3) << 14) + (data[1] << 7) + data[2]);
	ds->y = (((data[3] & 0x3) << 14) + (data[4] << 7) + data[5]);

	/* handle tilt values only for stylus */
	if (HANDLE_TILT(common) && is_stylus)
	{
		ds->tiltx = (data[7] & TILT_BITS);
		ds->tilty = (data[8] & TILT_BITS);
		if (data[7] & TILT_SIGN_BIT)
			ds->tiltx -= common->wcmMaxtiltX/2;
		if (data[8] & TILT_SIGN_BIT)
			ds->tilty -= common->wcmMaxtiltY/2;
	}

	/* first time into prox */
	if (!last->proximity && ds->proximity)
		ds->device_type = cur_type;
	/* check on previous proximity */
	else if (ds->buttons && ds->proximity)
	{
		/* we might have been fooled by tip and second
		 * sideswitch when it came into prox */
		if ((ds->device_type != cur_type) &&
			(ds->device_type == ERASER_ID))
		{
			/* send a prox-out for old device */
			WacomDeviceState out = { 0 };
			xf86WcmEvent(common, 0, &out);
			ds->device_type = cur_type;
		}
	}

	ds->device_id = (ds->device_type == CURSOR_ID) ? CURSOR_DEVICE_ID : STYLUS_DEVICE_ID;

	/* don't send button event for eraser 
	 * button 1 event will be sent by testing presure level
	 */
	if (ds->device_type == ERASER_ID) {
		ds->buttons = 0;
		ds->device_id = ERASER_DEVICE_ID;
	}

	DBG(8, common->debugLevel, ErrorF("serialParseP4Common %s\n",
		ds->device_type == CURSOR_ID ? "CURSOR" :
		ds->device_type == ERASER_ID ? "ERASER " :
		ds->device_type == STYLUS_ID ? "STYLUS" : "NONE"));
}

/*****************************************************************************
 * xf86WcmSerialValidate -- validates serial packet; returns 0 on success,
 *   positive number of bytes to skip on error.
 ****************************************************************************/

int xf86WcmSerialValidate(WacomCommonPtr common, const unsigned char* data)
{
	int i, bad = 0;

	/* check magic */
	for (i=0; i<common->wcmPktLength; ++i)
	{
		if ( ((i==0) && !(data[i] & HEADER_BIT)) || 
				((i!=0) && (data[i] & HEADER_BIT)) )
		{
			bad = 1;
			if (i!=1)
				ErrorF("xf86WcmSerialValidate: bad magic at %d "
					"v=%x l=%d\n", i, data[i], common->wcmPktLength);
			if (i!=0 && (data[i] & HEADER_BIT)) return i;
		}
	}
	if (bad) return common->wcmPktLength;
	else return 0;
}

