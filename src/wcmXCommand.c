/*
 * Copyright 2007-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <wacom-properties.h>

#include "xf86Wacom.h"
#include "wcmFilter.h"
#include <exevents.h>

/*****************************************************************************
* wcmDevSwitchModeCall --
*****************************************************************************/

int wcmDevSwitchModeCall(InputInfoPtr pInfo, int mode)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(3, priv, "to mode=%d\n", mode);

	/* Pad is always in absolute mode.*/
	if (IsPad(priv))
		return (mode == Absolute) ? Success : XI_BadMode;

	if ((mode == Absolute) && !is_absolute(pInfo))
		set_absolute(pInfo, TRUE);
	else if ((mode == Relative) && is_absolute(pInfo))
		set_absolute(pInfo, FALSE);
	else if ( (mode != Absolute) && (mode != Relative))
	{
		DBG(10, priv, "invalid mode=%d\n", mode);
		return XI_BadMode;
	}

	return Success;
}

/*****************************************************************************
* wcmDevSwitchMode --
*****************************************************************************/

int wcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	InputInfoPtr pInfo = (InputInfoPtr)dev->public.devicePrivate;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(3, priv, "dev=%p mode=%d\n",
		(void *)dev, mode);
#endif
	/* Share this call with sendAButton in wcmCommon.c */
	return wcmDevSwitchModeCall(pInfo, mode);
}

Atom prop_rotation;
Atom prop_tablet_area;
Atom prop_pressurecurve;
Atom prop_serials;
Atom prop_strip_buttons;
Atom prop_wheel_buttons;
Atom prop_tv_resolutions;
Atom prop_cursorprox;
Atom prop_capacity;
Atom prop_threshold;
Atom prop_suppress;
Atom prop_touch;
Atom prop_gesture;
Atom prop_gesture_param;
Atom prop_hover;
Atom prop_tooltype;
Atom prop_btnactions;
#ifdef DEBUG
Atom prop_debuglevels;
#endif

/* Special case: format -32 means type is XA_ATOM */
static Atom InitWcmAtom(DeviceIntPtr dev, char *name, int format, int nvalues, int *values)
{
	int i;
	Atom atom;
	uint8_t val_8[WCM_MAX_MOUSE_BUTTONS];
	uint16_t val_16[WCM_MAX_MOUSE_BUTTONS];
	uint32_t val_32[WCM_MAX_MOUSE_BUTTONS];
	pointer converted = val_32;
	Atom type = XA_INTEGER;

	if (format == -32)
	{
		type = XA_ATOM;
		format = 32;
	}

	for (i = 0; i < nvalues; i++)
	{
		switch(format)
		{
			case 8:  val_8[i]  = values[i]; break;
			case 16: val_16[i] = values[i]; break;
			case 32: val_32[i] = values[i]; break;
		}
	}

	switch(format)
	{
		case 8: converted = val_8; break;
		case 16: converted = val_16; break;
		case 32: converted = val_32; break;
	}

	atom = MakeAtom(name, strlen(name), TRUE);
	XIChangeDeviceProperty(dev, atom, type, format,
			PropModeReplace, nvalues,
			converted, FALSE);
	XISetDevicePropertyDeletable(dev, atom, FALSE);
	return atom;
}

void InitWcmDeviceProperties(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;
	int values[WCM_MAX_MOUSE_BUTTONS];

	DBG(10, priv, "\n");

	if (!IsPad(priv)) {
		values[0] = priv->topX;
		values[1] = priv->topY;
		values[2] = priv->bottomX;
		values[3] = priv->bottomY;
		prop_tablet_area = InitWcmAtom(pInfo->dev, WACOM_PROP_TABLET_AREA, 32, 4, values);
	}

	values[0] = common->wcmRotate;
	prop_rotation = InitWcmAtom(pInfo->dev, WACOM_PROP_ROTATION, 8, 1, values);

	if (IsStylus(priv) || IsEraser(priv)) {
		values[0] = priv->nPressCtrl[0];
		values[1] = priv->nPressCtrl[1];
		values[2] = priv->nPressCtrl[2];
		values[3] = priv->nPressCtrl[3];
		prop_pressurecurve = InitWcmAtom(pInfo->dev, WACOM_PROP_PRESSURECURVE, 32, 4, values);
	}

	values[0] = common->tablet_id;
	values[1] = priv->old_serial;
	values[2] = priv->old_device_id;
	values[3] = priv->serial;
	prop_serials = InitWcmAtom(pInfo->dev, WACOM_PROP_SERIALIDS, 32, 4, values);

	if (IsPad(priv)) {
		values[0] = priv->striplup;
		values[1] = priv->stripldn;
		values[2] = priv->striprup;
		values[3] = priv->striprdn;
		prop_strip_buttons = InitWcmAtom(pInfo->dev, WACOM_PROP_STRIPBUTTONS, 8, 4, values);
	}

	if (IsPad(priv) || IsCursor(priv))
	{
		values[0] = priv->relup;
		values[1] = priv->reldn;
		values[2] = priv->wheelup;
		values[3] = priv->wheeldn;
		prop_wheel_buttons = InitWcmAtom(pInfo->dev, WACOM_PROP_WHEELBUTTONS, 8, 4, values);
	}

	if (IsCursor(priv)) {
		values[0] = common->wcmCursorProxoutDist;
		prop_cursorprox = InitWcmAtom(pInfo->dev, WACOM_PROP_PROXIMITY_THRESHOLD, 32, 1, values);
	}

	values[0] = common->wcmCapacity;
	prop_capacity = InitWcmAtom(pInfo->dev, WACOM_PROP_CAPACITY, 32, 1, values);

	values[0] = (!common->wcmMaxZ) ? 0 : common->wcmThreshold;
	prop_threshold = InitWcmAtom(pInfo->dev, WACOM_PROP_PRESSURE_THRESHOLD, 32, 1, values);

	values[0] = common->wcmSuppress;
	values[1] = common->wcmRawSample;
	prop_suppress = InitWcmAtom(pInfo->dev, WACOM_PROP_SAMPLE, 32, 2, values);

	values[0] = common->wcmTouch;
	prop_touch = InitWcmAtom(pInfo->dev, WACOM_PROP_TOUCH, 8, 1, values);

	if (IsStylus(priv)) {
		values[0] = !common->wcmTPCButton;
		prop_hover = InitWcmAtom(pInfo->dev, WACOM_PROP_HOVER, 8, 1, values);
	}

	values[0] = common->wcmGesture;
	prop_gesture = InitWcmAtom(pInfo->dev, WACOM_PROP_ENABLE_GESTURE, 8, 1, values);

	values[0] = common->wcmGestureParameters.wcmZoomDistance;
	values[1] = common->wcmGestureParameters.wcmScrollDistance;
	values[2] = common->wcmGestureParameters.wcmTapTime;
	prop_gesture_param = InitWcmAtom(pInfo->dev, WACOM_PROP_GESTURE_PARAMETERS, 32, 3, values);

	values[0] = MakeAtom(pInfo->type_name, strlen(pInfo->type_name), TRUE);
	prop_tooltype = InitWcmAtom(pInfo->dev, WACOM_PROP_TOOL_TYPE, -32, 1, values);

	/* default to no actions */
	memset(values, 0, sizeof(values));
	prop_btnactions = InitWcmAtom(pInfo->dev, WACOM_PROP_BUTTON_ACTIONS, -32, WCM_MAX_MOUSE_BUTTONS, values);

#ifdef DEBUG
	values[0] = priv->debugLevel;
	values[1] = common->debugLevel;
	prop_debuglevels = InitWcmAtom(pInfo->dev, WACOM_PROP_DEBUGLEVELS, 8, 2, values);
#endif
}

/* Returns the offset of the property in the list given. If the property is
 * not found, a negative error code is returned. */
static int wcmFindProp(Atom property, Atom *prop_list, int nprops)
{
	int i;

	/* check all properties used for button actions */
	for (i = 0; i < nprops; i++)
		if (prop_list[i] == property)
			break;

	if (i >= nprops)
		return -BadAtom;

	return i;
}

static int wcmSanityCheckProperty(XIPropertyValuePtr prop)
{
	CARD32 *data;
	int j;

	if (prop->size >= 255 || prop->format != 32 || prop->type != XA_INTEGER)
		return BadMatch;

	data = (CARD32*)prop->data;

	for (j = 0; j < prop->size; j++)
	{
		int code = data[j] & AC_CODE;
		int type = data[j] & AC_TYPE;

		switch(type)
		{
			case AC_KEY:
				break;
			case AC_BUTTON:
				if (code > WCM_MAX_MOUSE_BUTTONS)
					return BadValue;
				break;
			case AC_DISPLAYTOGGLE:
			case AC_MODETOGGLE:
				break;
			default:
				return BadValue;
		}
	}

	return Success;
}

/**
 * Store the new value of the property in one of the driver's internal
 * property handler lists. Properties stored there will be checked for value
 * changes whenever updated.
 */
static void wcmUpdateActionPropHandlers(XIPropertyValuePtr prop, Atom *handlers)
{
	int i;
	CARD32 *values = (CARD32*)prop->data;

	/* any action property needs to be registered for this handler. */
	for (i = 0; i < prop->size; i++)
		handlers[i] = values[i];
}

static void wcmUpdateButtonKeyActions(DeviceIntPtr dev, XIPropertyValuePtr prop,
					unsigned int (*keys)[256], int skeys)
{
	Atom *values = (Atom*)prop->data;
	XIPropertyValuePtr val;
	int i, j;

	for (i = 0; i < prop->size; i++)
	{
		/* keys is one based array to align with X buttons */
		memset(keys[i+1], 0, sizeof(keys[i+1]));

		if (!values[i])
			continue;

		XIGetDeviceProperty(dev, values[i], &val);

		for (j = 0; j < val->size; j++)
			keys[i+1][j] = ((unsigned int*)val->data)[j];
	}
}

/* Change the properties that hold the actual button actions */
static int wcmSetActionProperties(DeviceIntPtr dev, Atom property,
				  XIPropertyValuePtr prop, BOOL checkonly)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int i;
	int rc;


	DBG(10, priv, "\n");

	rc = wcmSanityCheckProperty(prop);
	if (rc != Success)
		return rc;

	i = wcmFindProp(property, priv->btn_actions, ARRAY_SIZE(priv->btn_actions));
	if (i >= 0)
	{
		if (!checkonly)
		{
			XIGetDeviceProperty(dev, prop_btnactions, &prop);
			wcmUpdateButtonKeyActions(dev, prop, priv->keys, ARRAY_SIZE(priv->keys));
		}
	} else
	{
		i = wcmFindProp(property, priv->wheel_actions,
					ARRAY_SIZE(priv->wheel_actions));
		if (i >= 0) {
			if (!checkonly)
			{
				XIGetDeviceProperty(dev, prop_wheel_buttons, &prop);
				wcmUpdateButtonKeyActions(dev, prop,
						priv->wheel_keys,
						ARRAY_SIZE(priv->wheel_keys));
			}
		} else
		{
			i = wcmFindProp(property, priv->strip_actions, ARRAY_SIZE(priv->strip_actions));
			if (i >= 0 && !checkonly)
			{
				XIGetDeviceProperty(dev, prop_strip_buttons, &prop);
				wcmUpdateButtonKeyActions(dev, prop, priv->strip_keys, ARRAY_SIZE(priv->strip_keys));
			}
		}
	}

	return abs(i);
}

static int wcmCheckActionProp(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	XIPropertyValuePtr val;
	Atom *values = (Atom*)prop->data;
	int i;

	for (i = 0; i < prop->size; i++)
	{
		if (!values[i])
			continue;

		if (values[i] == property || !ValidAtom(values[i]))
			return BadValue;

		if (XIGetDeviceProperty(pInfo->dev, values[i], &val) != Success)
			return BadValue;
	}

	return Success;
}

/* Change the property that refers to which properties the actual button
 * actions are stored in */
static int wcmSetPropertyButtonActions(DeviceIntPtr dev, Atom property,
				       XIPropertyValuePtr prop, BOOL checkonly)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int rc;

	DBG(10, priv, "\n");

	if (prop->format != 32 || prop->type != XA_ATOM)
		return BadMatch;

	/* How this works:
	 * prop_btnactions has a list of atoms stored. Any atom references
	 * another property on that device that contains the actual action.
	 * If this property changes, all action-properties are queried for
	 * their value and their value is stored in priv->key[button].
	 *
	 * If the button is pressed, the actions are executed.
	 *
	 * Any button action property needs to be monitored by this property
	 * handler too.
	 */

	rc = wcmCheckActionProp(dev, property, prop);
	if (rc != Success)
		return rc;

	if (!checkonly)
	{
		wcmUpdateActionPropHandlers(prop, priv->btn_actions);
		wcmUpdateButtonKeyActions(dev, prop, priv->keys, ARRAY_SIZE(priv->keys));

	}
	return Success;
}

struct wheel_strip_update_t {
	/* for CARD8 values, points to fields in struct to be updated */
	int *up1;
	int *dn1;
	int *up2;
	int *dn2;

	/* for CARD32 values, points to atom array of atoms to be
	 * monitored.*/
	Atom *handlers;
	/* for CARD32 values, points to key array that keeps the actual
	   actions.*/
	int skeys;  /* size of first keys dimensions */
	unsigned int (*keys)[256];
};

static int wcmSetWheelOrStripProperty(DeviceIntPtr dev, Atom property,
				      XIPropertyValuePtr prop, BOOL checkonly,
				      struct wheel_strip_update_t *wsup)
{
	int rc;

	union multival {
		CARD8 *v8;
		CARD32 *v32;
	} values;

	if (prop->size != 4)
		return BadValue;

	/* see wcmSetPropertyButtonActions for how this works. The wheel is
	 * slightly different in that it allows for 8 bit properties for
	 * pure buttons too */

	values.v8 = (CARD8*)prop->data;

	switch (prop->format)
	{
		case 8:
			if (values.v8[0] > WCM_MAX_MOUSE_BUTTONS ||
			    values.v8[1] > WCM_MAX_MOUSE_BUTTONS ||
			    values.v8[2] > WCM_MAX_MOUSE_BUTTONS ||
			    values.v8[3] > WCM_MAX_MOUSE_BUTTONS)
				return BadValue;

			if (!checkonly) {
				*wsup->up1 = values.v8[0];
				*wsup->dn1 = values.v8[1];
				*wsup->up2 = values.v8[2];
				*wsup->dn2 = values.v8[3];
			}
			break;
		case 32:
			rc = wcmCheckActionProp(dev, property, prop);
			if (rc != Success)
				return rc;

			if (!checkonly)
			{
				wcmUpdateActionPropHandlers(prop, wsup->handlers);
				wcmUpdateButtonKeyActions(dev, prop, wsup->keys,
						          wsup->skeys);
			}

			break;
		default:
			return BadMatch;
	}

	return Success;
}


static int wcmSetWheelProperty(DeviceIntPtr dev, Atom property,
			       XIPropertyValuePtr prop, BOOL checkonly)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;

	struct wheel_strip_update_t wsup = {
		.up1 = &priv->relup,
		.dn1 = &priv->reldn,
		.up2 = &priv->wheelup,
		.dn2 = &priv->wheeldn,

		.handlers = priv->wheel_actions,
		.keys	  = priv->wheel_keys,
		.skeys    = 4,
	};

	return wcmSetWheelOrStripProperty(dev, property, prop, checkonly, &wsup);
}

static int wcmSetStripProperty(DeviceIntPtr dev, Atom property,
			       XIPropertyValuePtr prop, BOOL checkonly)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;

	struct wheel_strip_update_t wsup = {
		.up1 = &priv->striplup,
		.dn1 = &priv->stripldn,
		.up2 = &priv->striprup,
		.dn2 = &priv->striprdn,

		.handlers = priv->strip_actions,
		.keys	  = priv->strip_keys,
		.skeys    = 4,
	};

	return wcmSetWheelOrStripProperty(dev, property, prop, checkonly, &wsup);
}

/**
 * Update the rotation property for all tools on the same physical tablet as
 * pInfo.
 */
void wcmUpdateRotationProperty(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomDevicePtr other;
	char rotation = common->wcmRotate;

	for (other = common->wcmDevices; other; other = other->next)
	{
		InputInfoPtr pInfo;
		DeviceIntPtr dev;

		if (other == priv)
			continue;

		pInfo = other->pInfo;
		dev = pInfo->dev;

		XIChangeDeviceProperty(dev, prop_rotation, XA_INTEGER, 8,
				       PropModeReplace, 1, &rotation,
				       TRUE);
	}
}

/**
 * Only allow deletion of a property if it is not being used by any of the
 * button actions.
 */
int wcmDeleteProperty(DeviceIntPtr dev, Atom property)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int i;

	i = wcmFindProp(property, priv->btn_actions, ARRAY_SIZE(priv->btn_actions));
	if (i < 0)
		i = wcmFindProp(property, priv->wheel_actions,
				ARRAY_SIZE(priv->wheel_actions));
	if (i < 0)
		i = wcmFindProp(property, priv->strip_actions,
				ARRAY_SIZE(priv->strip_actions));

	return (i >= 0) ? BadAccess : Success;
}

int wcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
		BOOL checkonly)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(10, priv, "\n");

	if (property == prop_tablet_area)
	{
		INT32 *values = (INT32*)prop->data;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		if (!checkonly)
		{
			if ((values[0] == -1) && (values[1] == -1) &&
					(values[2] == -1) && (values[3] == -1))
			{
				values[0] = 0;
				values[1] = 0;
				values[2] = priv->maxX;
				values[3] = priv->maxY;
			}

			priv->topX = values[0];
			priv->topY = values[1];
			priv->bottomX = values[2];
			priv->bottomY = values[3];
		}
	} else if (property == prop_pressurecurve)
	{
		INT32 *pcurve;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		pcurve = (INT32*)prop->data;

		if (!wcmCheckPressureCurveValues(pcurve[0], pcurve[1],
						 pcurve[2], pcurve[3]))
			return BadValue;

		if (IsCursor(priv) || IsPad (priv) || IsTouch (priv))
			return BadValue;

		if (!checkonly)
			wcmSetPressureCurve (priv, pcurve[0], pcurve[1],
					pcurve[2], pcurve[3]);
	} else if (property == prop_suppress)
	{
		CARD32 *values;

		if (prop->size != 2 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		if ((values[0] < 0) || (values[0] > 100))
			return BadValue;

		if ((values[1] < 1) || (values[1] > MAX_SAMPLES))
			return BadValue;

		if (!checkonly)
		{
			common->wcmSuppress = values[0];
			common->wcmRawSample = values[1];
		}
	} else if (property == prop_rotation)
	{
		CARD8 value;
		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		value = *(CARD8*)prop->data;

		if (value > 3)
			return BadValue;

		if (!checkonly && common->wcmRotate != value)
			wcmRotateTablet(pInfo, value);

	} else if (property == prop_serials)
	{
		return BadValue; /* Read-only */
	} else if (property == prop_strip_buttons)
		return wcmSetStripProperty(dev, property, prop, checkonly);
	else if (property == prop_wheel_buttons)
		return wcmSetWheelProperty(dev, property, prop, checkonly);
	else if (property == prop_cursorprox)
	{
		CARD32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		if (!IsCursor (priv))
			return BadValue;

		value = *(CARD32*)prop->data;

		if (value > common->wcmMaxDist)
			return BadValue;

		if (!checkonly)
			common->wcmCursorProxoutDist = value;
	} else if (property == prop_capacity)
	{
		INT32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		value = *(INT32*)prop->data;

		if ((value < -1) || (value > 5))
			return BadValue;

		if (!checkonly)
			common->wcmCapacity = value;

	} else if (property == prop_threshold)
	{
		CARD32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		value = *(CARD32*)prop->data;

		if ((value < 1) || (value > FILTER_PRESSURE_RES))
			return BadValue;

		if (!checkonly)
			common->wcmThreshold = value;
	} else if (property == prop_touch)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmTouch != values[0])
			common->wcmTouch = values[0];
	} else if (property == prop_gesture)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmGesture != values[0])
			common->wcmGesture = values[0];
	} else if (property == prop_gesture_param)
	{
		CARD32 *values;

		if (prop->size != 3 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		if (!checkonly)
		{
			if (common->wcmGestureParameters.wcmZoomDistance != values[0])
				common->wcmGestureParameters.wcmZoomDistance = values[0];
			if (common->wcmGestureParameters.wcmScrollDistance != values[1])
				common->wcmGestureParameters.wcmScrollDistance = values[1];
			if (common->wcmGestureParameters.wcmTapTime != values[2])
				common->wcmGestureParameters.wcmTapTime = values[2];
		}
	} else if (property == prop_hover)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!IsStylus(priv))
			return BadMatch;

		if (!checkonly)
			common->wcmTPCButton = !values[0];
#ifdef DEBUG
	} else if (property == prop_debuglevels)
	{
		CARD8 *values;

		if (prop->size != 2 || prop->format != 8)
			return BadMatch;

		values = (CARD8*)prop->data;
		if (values[0] > 12 || values[1] > 12)
			return BadValue;

		if (!checkonly)
		{
			priv->debugLevel = values[0];
			common->debugLevel = values[1];
		}
#endif
	} else if (property == prop_btnactions)
	{
		if (prop->size != WCM_MAX_MOUSE_BUTTONS)
			return BadMatch;
		wcmSetPropertyButtonActions(dev, property, prop, checkonly);
	} else
		wcmSetActionProperties(dev, property, prop, checkonly);

	return Success;
}
/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
