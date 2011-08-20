
/*!
 * \file 	sccp_config.c
 * \brief 	SCCP Config Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \note 	To find out more about the reload function see \ref sccp_config_reload
 * \remarks     Only methods directly related to chan-sccp configuration should be stored in this source file.
 *
 * $Date: 2010-11-17 18:10:34 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2154 $
 */

/*!
 * \file
 * \ref sccp_config
 *
 * \page sccp_config Loading sccp.conf/realtime configuration implementation
 *
 * \section sccp_config_reload How was the new cli command "sccp reload" implemented
 *
 * \code
 * sccp_cli.c
 * 	new implementation of cli reload command
 * 		checks if no other reload command is currently running
 * 		starts loading global settings from sccp.conf (sccp_config_general)
 * 		starts loading devices and lines from sccp.conf(sccp_config_readDevicesLines)
 *
 * sccp_config.c
 * 	modified sccp_config_general
 * 		skips reading new variable values for bindaddr, port, debug when reloading
 *
 * 	modified sccp_config_readDevicesLines
 * 		sets pendingDelete for
 * 			devices (via sccp_device_pre_reload),
 * 			lines (via sccp_line_pre_reload)
 * 			softkey (via sccp_softkey_pre_reload)
 *
 * 		calls sccp_config_buildDevice as usual
 * 			calls sccp_config_buildDevice as usual
 * 				find device
 * 					makes clone of device if exists (sccp_device_clone)
 * 				parses sccp.conf for device
 * 				checks if clone and device are still the same for critical values (via sccp_device_changed)
 * 				if not it sets pendingUpdate on device
 * 			calls sccp_config_buildLine as usual
 * 				find line
 * 					makes clone of line if exists (sccp_line_clone)
 * 				or create new line
 * 				parses sccp.conf for line
 * 				checks if clone and line are still the same for critical values (via sccp_line_changed)
 *			 	if not it sets pendingUpdate on line
 * 			calls sccp_config_softKeySet as usual ***
 * 				find softKeySet
 * 					makes clone of softKeySet if exists (sccp_softkeyset_clone) ***
 *				or create new softKeySet
 *			 	parses sccp.conf for softKeySet
 * 				checks if clone and softKeySet are still the same for critical values (via sccp_softkeyset_changed) ***
 * 				if not it sets pendingUpdate on softKeySet[A
 *
 * 		checks pendingDelete and pendingUpdate for
 *			skip when call in progress
 * 			devices (via sccp_device_post_reload),
 * 				resets GLOB(device) if pendingUpdate
 * 				removes GLOB(devices) with pendingDelete
 * 			lines (via sccp_line_post_reload)
 * 				resets GLOB(lines) if pendingUpdate
 * 				removes GLOB(lines) with pendingDelete
 * 			softkey (via sccp_softkey_post_reload) ***
 * 				resets GLOB(softkeyset) if pendingUpdate ***
 * 				removes GLOB(softkeyset) with pendingDelete ***
 *
 * channel.c
 * 	sccp_channel_endcall ***
 *		reset device if still device->pendingUpdate,line->pendingUpdate or softkeyset->pendingUpdate
 *
 * \endcode
 *
 * lines marked with "***" still need be implemented
 *
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2154 $")

struct ast_config *sccp_config_getConfig(void);

#ifdef CS_DYNAMIC_CONFIG

/*!
 * \brief add a Button to a device
 * \param device the SCCP Device where to add the button
 * \param index         the index of the button (-1 to add the line at the end)
 * \param type          type of button
 * \param name          name
 * \param options       options
 * \param args          args
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device->buttonconfig
 * 	  - globals
 * 	- device->devstateSpecifiers
 */
void sccp_config_addButton(sccp_device_t * device, int index, button_type_t type, const char *name, const char *options, const char *args)
{
	sccp_buttonconfig_t *config = NULL;

	boolean_t new = FALSE;

	uint16_t highest_index = 0;

#    ifdef CS_DEVSTATE_FEATURE
	sccp_devstate_specifier_t *dspec;

	dspec = ast_calloc(1, sizeof(sccp_devstate_specifier_t));
#    endif

	sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "%s: Loading/Checking ButtonConfig\n", device->id);
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		/* If the index is < 0, we will add the new button at the
		 * bottom of the list, so we have to calculate the highest ID. */
		if (index < 0 && config->index > highest_index)
			highest_index = config->index;
		/* In other case, we try to find the button at this index. */
		else if (config->index == index) {
			sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Found Existing button at %d\n", config->index);
			break;
		}
	}

	if (index < 0) {
		index = highest_index + 1;
		config = NULL;
	}

	/* If a config at this position is not found, recreate one. */
	if (!config || config->index != index) {
		config = ast_calloc(1, sizeof(*config));
		if (!config)
			return;

		config->index = index;
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "New %s Button %s at : %d:%d\n", sccp_buttontype2str(type), name, index, config->index);
		SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
		new = TRUE;
	} else
		config->pendingDelete = 0;
	SCCP_LIST_UNLOCK(&device->buttonconfig);
	//taken outside the loop, reasons: locking order and retaking device lock and releasing to add buttons to the same device.
#    ifdef CS_DYNAMIC_CONFIG
	if (TRUE == new) {
		ast_mutex_lock(&GLOB(lock));
		if (GLOB(reload_in_progress) == TRUE)
			device->pendingUpdate = 1;
		ast_mutex_unlock(&GLOB(lock));
	}
#    endif

	if (sccp_strlen_zero(name) || (type != LINE && !options) ) {
		sccp_log(0) (VERBOSE_PREFIX_1 "%s: Faulty Button Configutation found at index: %d", device->id, config->index);
		type = EMPTY;
	}

	switch (type) {
	case LINE:
	{
		struct composedId composedLineRegistrationId;

		memset(&composedLineRegistrationId, 0, sizeof(struct composedId));
		composedLineRegistrationId = sccp_parseComposedId(name, 80);

		config->type = LINE;
		if (new)
			device->configurationStatistic.numberOfLines++;

		sccp_copy_string(config->button.line.name, composedLineRegistrationId.mainId, sizeof(config->button.line.name));
		sccp_copy_string(config->button.line.subscriptionId.number, composedLineRegistrationId.subscriptionId.number, sizeof(config->button.line.subscriptionId.number));
		sccp_copy_string(config->button.line.subscriptionId.name, composedLineRegistrationId.subscriptionId.name, sizeof(config->button.line.subscriptionId.name));
		sccp_copy_string(config->button.line.subscriptionId.aux, composedLineRegistrationId.subscriptionId.aux, sizeof(config->button.line.subscriptionId.aux));

		if (options) {
			sccp_copy_string(config->button.line.options, options, sizeof(config->button.line.options));
		}
		break;
	}
	case SPEEDDIAL:
		config->type = SPEEDDIAL;

		sccp_copy_string(config->button.speeddial.label, name, sizeof(config->button.speeddial.label));
		sccp_copy_string(config->button.speeddial.ext, options, sizeof(config->button.speeddial.ext));
		if (args) {
			sccp_copy_string(config->button.speeddial.hint, args, sizeof(config->button.speeddial.hint));
		}
		break;
	case SERVICE:
		config->type = SERVICE;

		sccp_copy_string(config->button.service.label, name, sizeof(config->button.service.label));
		sccp_copy_string(config->button.service.url, options, sizeof(config->button.service.url));
		break;
	case FEATURE:
		config->type = FEATURE;

		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_FEATURE | DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "featureID: %s\n", options);

		sccp_copy_string(config->button.feature.label, name, sizeof(config->button.feature.label));
		config->button.feature.id = sccp_featureStr2featureID(options);

		if (args) {
			sccp_copy_string(config->button.feature.options, args, sizeof(config->button.feature.options));
			sccp_log(0) (VERBOSE_PREFIX_3 "Arguments present on feature button: %d\n", config->instance);

#    ifdef CS_DEVSTATE_FEATURE
			/* Check for the presence of a devicestate specifier and register in device list. */
			if ((SCCP_FEATURE_DEVSTATE == config->button.feature.id) && (strncmp("", config->button.feature.options, 254))) {
				sccp_log(0) (VERBOSE_PREFIX_3 "Recognized devstate feature button: %d\n", config->instance);
				SCCP_LIST_LOCK(&device->devstateSpecifiers);
				sccp_copy_string(dspec->specifier, config->button.feature.options, sizeof(config->button.feature.options));
				SCCP_LIST_INSERT_TAIL(&device->devstateSpecifiers, dspec, list);
				SCCP_LIST_UNLOCK(&device->devstateSpecifiers);
			}
#    endif
		}

		sccp_log((DEBUGCAT_FEATURE | DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "Configured feature button with featureID: %s args: %s\n", options, args);

		break;
	case EMPTY:
		config->type = EMPTY;
		break;
	}
}
#else										/* CS_DYNAMIC_CONFIG */

/*!
 * \brief Add Line to device.
 * \param device Device
 * \param lineName Name of line
 * \param options  Line Options
 * \param instance Line Instance as uint16
 * 
 * \lock
 * 	- device->buttonconfig
 */
void sccp_config_addLine(sccp_device_t * device, char *lineName, char *options, uint16_t instance)
{
	struct composedId composedLineRegistrationId;

	sccp_buttonconfig_t *config;

	config = ast_calloc(1, sizeof(sccp_buttonconfig_t));
	if (!config)
		return;

	memset(&composedLineRegistrationId, 0, sizeof(struct composedId));

	ast_strip(lineName);

	if (sccp_strlen_zero(lineName)) {
		config->type = EMPTY;
	} else {
		composedLineRegistrationId = sccp_parseComposedId(lineName, 80);

		config->type = LINE;

		sccp_copy_string(config->button.line.name, composedLineRegistrationId.mainId, sizeof(config->button.line.name));
		sccp_copy_string(config->button.line.subscriptionId.number, composedLineRegistrationId.subscriptionId.number, sizeof(config->button.line.subscriptionId.number));
		sccp_copy_string(config->button.line.subscriptionId.name, composedLineRegistrationId.subscriptionId.name, sizeof(config->button.line.subscriptionId.name));
		sccp_copy_string(config->button.line.subscriptionId.aux, composedLineRegistrationId.subscriptionId.aux, sizeof(config->button.line.subscriptionId.aux));

		if (options) {
			sccp_copy_string(config->button.line.options, options, sizeof(config->button.line.options));
		}
	}

	sccp_log(0) (VERBOSE_PREFIX_3 "Add line button on position: %d\n", config->instance);
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	device->configurationStatistic.numberOfLines++;
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}

/**
 * Add an Empty Button to device.
 * \param device SCCP Device
 * \param instance Index Preferred Button Position as int
 * 
 * \lock
 * 	- device->buttonconfig
 */
void sccp_config_addEmpty(sccp_device_t * device, uint16_t instance)
{
	sccp_buttonconfig_t *config;

	config = ast_calloc(1, sizeof(sccp_buttonconfig_t));
	if (!config)
		return;

	config->type = EMPTY;
	sccp_log(0) (VERBOSE_PREFIX_3 "Add empty button on position: %d\n", config->instance);
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}

/*!
 * Add an SpeedDial to Device.
 * \param device SCCP Device
 * \param label Label as char
 * \param extension Extension as char
 * \param hint Hint as char
 * \param instance Index Preferred Button Position as int
 * 
 * \lock
 * 	- device->buttonconfig
 */
void sccp_config_addSpeeddial(sccp_device_t * device, char *label, char *extension, char *hint, uint16_t instance)
{
	sccp_buttonconfig_t *config;

	config = ast_calloc(1, sizeof(sccp_buttonconfig_t));
	if (!config)
		return;

	config->type = SPEEDDIAL;

	sccp_copy_string(config->button.speeddial.label, ast_strip(label), sizeof(config->button.speeddial.label));
	sccp_copy_string(config->button.speeddial.ext, ast_strip(extension), sizeof(config->button.speeddial.ext));
	if (hint) {
		sccp_copy_string(config->button.speeddial.hint, ast_strip(hint), sizeof(config->button.speeddial.hint));
	}

	sccp_log(0) (VERBOSE_PREFIX_3 "Add empty button on position: %d\n", config->instance);
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	device->configurationStatistic.numberOfSpeeddials++;
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}

/**
 * \brief Add Feature to Device.
 * \param device SCCP Device
 * \param label Label as char
 * \param featureID featureID as char
 * \param args Arguments as char
 * \param instance Index Preferred Button Position as int
 * 
 * \lock
 * 	- device->devstateSpecifiers
 * 	- device->buttonconfig
 */
void sccp_config_addFeature(sccp_device_t * device, char *label, char *featureID, char *args, uint16_t instance)
{
	sccp_buttonconfig_t *config;

#    ifdef CS_DEVSTATE_FEATURE
	sccp_devstate_specifier_t *dspec;

	dspec = ast_calloc(1, sizeof(sccp_devstate_specifier_t));
#    endif

	config = ast_calloc(1, sizeof(sccp_buttonconfig_t));

	if (!config)
		return;

	if (sccp_strlen_zero(label)) {
		config->type = EMPTY;
	} else {
		config->type = FEATURE;
		sccp_copy_string(config->button.feature.label, ast_strip(label), sizeof(config->button.feature.label));
		config->button.feature.id = sccp_featureStr2featureID(featureID);

		if (args) {
			sccp_copy_string(config->button.feature.options, (args) ? ast_strip(args) : "", sizeof(config->button.feature.options));
			sccp_log(0) (VERBOSE_PREFIX_3 "Arguments present on feature button: %d\n", config->instance);

#    ifdef CS_DEVSTATE_FEATURE
			/* Check for the presence of a devicestate specifier and register in device list.
			 */
			if ((SCCP_FEATURE_DEVSTATE == config->button.feature.id) && (strncmp("", config->button.feature.options, 254))) {
				sccp_log(0) (VERBOSE_PREFIX_3 "Recognized devstate feature button: %d\n", config->instance);
				SCCP_LIST_LOCK(&device->devstateSpecifiers);
				sccp_copy_string(dspec->specifier, config->button.feature.options, sizeof(config->button.feature.options));
				SCCP_LIST_INSERT_TAIL(&device->devstateSpecifiers, dspec, list);
				SCCP_LIST_UNLOCK(&device->devstateSpecifiers);
			}
#    endif
		}

		sccp_log((DEBUGCAT_FEATURE | DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "featureID: %s args: %s\n", featureID, (config->button.feature.options) ? (config->button.feature.options) : (""));

	}

	sccp_log(0) (VERBOSE_PREFIX_3 "Add FEATURE button on position: %d, featureID: %d, args: %s\n", config->instance, config->button.feature.id, (config->button.feature.options) ? config->button.feature.options : "(none)");
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	device->configurationStatistic.numberOfFeatures++;
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}

/**
 * Add an Service to Device.
 * \param device SCCP Device
 * \param label Label as char
 * \param url URL as char
 * \param instance Index Preferred Button Position as int
 * 
 * \lock
 * 	- device->buttonconfig
 */
void sccp_config_addService(sccp_device_t * device, char *label, char *url, uint16_t instance)
{
	sccp_buttonconfig_t *config;

	config = ast_calloc(1, sizeof(sccp_buttonconfig_t));

	if (!config)
		return;

	if (sccp_strlen_zero(label) || sccp_strlen_zero(url)) {
		config->type = EMPTY;
	} else {
		config->type = SERVICE;
		sccp_copy_string(config->button.service.label, ast_strip(label), sizeof(config->button.service.label));
		sccp_copy_string(config->button.service.url, ast_strip(url), sizeof(config->button.service.url));
	}

	sccp_log(0) (VERBOSE_PREFIX_3 "Add SERVICE button on position: %d\n", config->instance);
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	device->configurationStatistic.numberOfServices++;
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}
#endif										/* CS_DYNAMIC_CONFIG */

/**
 * Create a device from configuration variable.
 *
 * \param variable asterisk configuration variable
 * \param deviceName name of device e.g. SEP0123455689
 * \param isRealtime configuration is generated by realtime
 * \since 10.01.2008 - branche V3
 * \author Marcello Ceschia
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	  - see sccp_device_changed()
 */
sccp_device_t *sccp_config_buildDevice(struct ast_variable *variable, const char *deviceName, boolean_t isRealtime)
{
	sccp_device_t *d = NULL;

	int res;

#ifdef CS_DYNAMIC_CONFIG
	boolean_t is_new = FALSE;
#endif
	char message[256] = "";							/*!< device message */

	// Try to find out if we have the device already on file.
	// However, do not look into realtime, since
	// we might have been asked to create a device for realtime addition,
	// thus causing an infinite loop / recursion.
	d = sccp_device_find_byid(deviceName, FALSE);
	if (d
#ifdef CS_DYNAMIC_CONFIG
	    && !d->pendingDelete && !d->realtime
#endif
	    ) {
		ast_log(LOG_WARNING, "SCCP: Device '%s' already exists\n", deviceName);
		return d;
	}

	/* create new device with default values */
	if (!d) {
		d = sccp_device_create();
		sccp_copy_string(d->id, deviceName, sizeof(d->id));		/* set device name */
#ifdef CS_DYNAMIC_CONFIG
		is_new = TRUE;
#endif
	}
#ifdef CS_DYNAMIC_CONFIG
	/* clone d to temp_d */
	sccp_device_t *temp_d = NULL;

	if (d->pendingDelete) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: cloning device\n", d->id);
		temp_d = sccp_clone_device(d);
	} else if (d->realtime) {
		/* if the device was realtime (for example an hotline), force
		 * reset of the device.
		 */
		d->defaultLineInstance = 0;
		d->isAnonymous = FALSE;
		d->pendingUpdate = 1;
	}
#endif										/* CS_DYNAMIC_CONFIG */

	d = sccp_config_applyDeviceConfiguration(d, variable);			/* apply configuration using variable */

#ifdef CS_DYNAMIC_CONFIG
	/* compare temporairy temp_d to d */
	if (temp_d) {
		sccp_device_lock(d);
		sccp_device_lock(temp_d);
		switch (sccp_device_changed(temp_d, d)) {
		case CHANGES_NEED_RESET:{
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: major changes detected, reset required -> pendingUpdate=1\n", temp_d->id);
				d->pendingUpdate = 1;
				break;
			}
		case MINOR_CHANGES:{
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: minor changes detected, no reset required\n", temp_d->id);
				break;
			}
		case NO_CHANGES:{
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: no changes detected\n", temp_d->id);
				break;
			}
		}
		sccp_device_unlock(temp_d);
		sccp_device_unlock(d);

		// removing temp_d
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Removing Cloned Device\n", temp_d->id);
		sccp_device_destroy(temp_d);
		temp_d = NULL;
	}
#endif										/* CS_DYNAMIC_CONFIG */

	d->realtime = isRealtime;

	res = pbx_db_get("SCCPM", d->id, message, sizeof(message));		//load save message from ast_db
	if (!res) {
		d->phonemessage = strdup(message);				//set message on device if we have a result
	}
#ifdef CS_DYNAMIC_CONFIG
	if (is_new) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Adding device to Globals\n", d->id);
		d->pendingUpdate = 0;
		sccp_device_addToGlobals(d);
	} else {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Removing pendingDelete\n", d->id);
		d->pendingDelete = 0;
	}
#else
	sccp_device_addToGlobals(d);
#endif

	return d;
}

/*!
 * \brief Build Line
 * \param variable Asterisk Variable
 * \param lineName Name of line as char
 * \param isRealtime is Realtime as Boolean
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- line
 * 	  - see sccp_line_changed()
 */
sccp_line_t *sccp_config_buildLine(struct ast_variable * variable, const char *lineName, boolean_t isRealtime)
{
	sccp_line_t *l = NULL;

	char *name = (char *)lineName;

	name = ast_strip(name);
	// Try to find out if we have the line already on file.
	// However, do not look into realtime, since
	// we might have been asked to create a device for realtime addition,
	// thus causing an infinite loop / recursion.
	l = sccp_line_find_byname_wo(name, FALSE);

	/* search for existing line */
	if (l
#ifdef CS_DYNAMIC_CONFIG
	    && !l->pendingDelete
#endif
	    ) {
		ast_log(LOG_WARNING, "SCCP: Line '%s' already exists\n", name);
		return l;
	}

	if (!l)
		l = sccp_line_create();

#ifdef CS_DYNAMIC_CONFIG
	/* clone l to temp_l */
	sccp_line_t *temp_l = NULL;

	if (l && l->pendingDelete == 1
#    ifdef CS_SCCP_REALTIME
	    && !l->realtime
#    endif
	    ) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: cloning line\n", name);
		temp_l = sccp_clone_line(l);
	}
#endif										/* CS_DYNAMIC_CONFIG */

	sccp_config_applyLineConfiguration(l, variable);			/* apply configuration using variable */

#ifdef CS_DYNAMIC_CONFIG
	/* compare temporairy temp_l to l */
	if (l->pendingDelete == 1
#    ifdef CS_SCCP_REALTIME
	    && !l->realtime
#    endif
	    && temp_l) {
		sccp_line_lock(l);
		sccp_line_lock(temp_l);
		switch (sccp_line_changed(temp_l, l)) {
		case CHANGES_NEED_RESET:{
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: major changes detected, reset required -> pendingUpdate=1\n", temp_l->name);
				l->pendingUpdate = 1;
				break;
			}
		case MINOR_CHANGES:{
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: minor changes detected, no reset required\n", temp_l->name);
				break;
			}
		case NO_CHANGES:{
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: no changes detected\n", temp_l->name);
				break;
			}
		}
		sccp_line_unlock(temp_l);
		sccp_line_unlock(l);

		// removing temp_d
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Remove Cloned Line\n", temp_l->name);
		sccp_line_destroy(temp_l);
		temp_l = NULL;
	}
#endif										/* CS_DYNAMIC_CONFIG */

	sccp_copy_string(l->name, name, sizeof(l->name));
#ifdef CS_SCCP_REALTIME
	l->realtime = isRealtime;
#endif

#ifdef CS_DYNAMIC_CONFIG
	if (!l->pendingDelete) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Adding line to Globals to 0\n", l->name);
		sccp_line_addToGlobals(l);
	} else {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Removing pendingDelete\n", l->name);
		l->pendingDelete = 0;
	}
#else
	sccp_line_addToGlobals(l);
#endif										/* CS_DYNAMIC_CONFIG */

	return l;
}

/*!
 * \brief Parse sccp.conf and Create General Configuration
 * \param readingtype SCCP Reading Type
 *
 * \callgraph
 * \callergraph
 */
boolean_t sccp_config_general(sccp_readingtype_t readingtype)
{
	struct ast_config *cfg;

	struct ast_variable *v;

	int firstdigittimeout = 0;

	int digittimeout = 0;

	int autoanswer_ring_time = 0;

	int autoanswer_tone = 0;

	int remotehangup_tone = 0;

	int transfer_tone = 0;

	int callwaiting_tone = 0;

	int amaflags = 0;

	int protocolversion = 0;

	char digittimeoutchar = '#';

	char *debug_arr[1];

	unsigned int sccp_tos = 0;

	unsigned int audio_tos = 0;

	unsigned int video_tos = 0;

	unsigned int sccp_cos = 0;

	unsigned int audio_cos = 0;

	unsigned int video_cos = 0;

	char pref_buf[128];

	struct ast_hostent ahp;

	struct hostent *hp;

	struct ast_ha *na;

	char config_value[256];

	char newcontexts[AST_MAX_CONTEXT];

	char oldcontexts[AST_MAX_CONTEXT];

	char *stringp, *context, *oldregcontext;

	/* Cleanup for reload */
	ast_free_ha(GLOB(ha));
	GLOB(ha) = NULL;
	ast_free_ha(GLOB(localaddr));
	GLOB(localaddr) = NULL;

	cfg = sccp_config_getConfig();
	if (!cfg) {
		ast_log(LOG_WARNING, "Unable to load config file sccp.conf, SCCP disabled\n");
		return FALSE;
	}

	/* read the general section */
	v = ast_variable_browse(cfg, "general");
	if (!v) {
		ast_log(LOG_WARNING, "Missing [general] section, SCCP disabled\n");
		ast_config_destroy(cfg);
		return FALSE;
	}

	for (; v; v = v->next) {
		sccp_copy_string(config_value, v->value, sizeof(config_value));

#if ASTERISK_VERSION_NUMBER >= 10400
		/* handle jb in configuration just let asterisk do that */
		if (!ast_jb_read_conf(&GLOB(global_jbconf), v->name, v->value)) {
			// Found a jb parameter
			continue;
		}
#endif

		if (!strcasecmp(v->name, "protocolversion")) {
			if (sscanf(v->value, "%i", &protocolversion) == 1) {
				if (protocolversion < SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW) {
					GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
				} else if (protocolversion > SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH) {
					GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
				} else {
					GLOB(protocolversion) = protocolversion;
				}
			} else {
				ast_log(LOG_WARNING, "Invalid protocolversion number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "servername")) {
			sccp_copy_string(GLOB(servername), v->value, sizeof(GLOB(servername)));
		} else if (!strcasecmp(v->name, "bindaddr")) {
			if (readingtype == SCCP_CONFIG_READINITIAL) {
				if (!(hp = pbx_gethostbyname(v->value, &ahp))) {
					ast_log(LOG_WARNING, "Invalid address: %s. SCCP disabled\n", v->value);
					return 0;
				} else {
					memcpy(&GLOB(bindaddr.sin_addr), hp->h_addr, sizeof(GLOB(bindaddr.sin_addr)));
				}
			}
		} else if (!strcasecmp(v->name, "permit") || !strcasecmp(v->name, "deny")) {
			GLOB(ha) = pbx_append_ha(v->name, v->value, GLOB(ha), NULL);
		} else if (!strcasecmp(v->name, "localnet")) {
			if (!(na = pbx_append_ha("d", v->value, GLOB(localaddr), NULL)))
				ast_log(LOG_WARNING, "Invalid localnet value: %s\n", v->value);
			else
				GLOB(localaddr) = na;
		} else if (!strcasecmp(v->name, "externip")) {
			if (!(hp = pbx_gethostbyname(v->value, &ahp)))
				ast_log(LOG_WARNING, "Invalid address for externip keyword: %s\n", v->value);
			else
				memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
			GLOB(externexpire) = 0;
		} else if (!strcasecmp(v->name, "externhost")) {
			sccp_copy_string(GLOB(externhost), v->value, sizeof(GLOB(externhost)));
			if (!(hp = pbx_gethostbyname(GLOB(externhost), &ahp)))
				ast_log(LOG_WARNING, "Invalid address resolution for externhost keyword: %s\n", GLOB(externhost));
			else
				memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
			time(&GLOB(externexpire));
		} else if (!strcasecmp(v->name, "externrefresh")) {
			if (sscanf(v->value, "%d", &GLOB(externrefresh)) != 1) {
				ast_log(LOG_WARNING, "Invalid externrefresh value '%s', must be an integer >0 at line %d\n", v->value, v->lineno);
				GLOB(externrefresh) = 10;
			}
		} else if (!strcasecmp(v->name, "keepalive")) {
			GLOB(keepalive) = atoi(v->value);
		} else if (!strcasecmp(v->name, "context")) {
			sccp_copy_string(GLOB(context), v->value, sizeof(GLOB(context)));
		} else if (!strcasecmp(v->name, "language")) {
			sccp_copy_string(GLOB(language), v->value, sizeof(GLOB(language)));
		} else if (!strcasecmp(v->name, "accountcode")) {
			sccp_copy_string(GLOB(accountcode), v->value, sizeof(GLOB(accountcode)));
		} else if (!strcasecmp(v->name, "amaflags")) {
			amaflags = ast_cdr_amaflags2int(v->value);
			if (amaflags < 0) {
				ast_log(LOG_WARNING, "Invalid AMA Flags: %s at line %d\n", v->value, v->lineno);
			} else {
				GLOB(amaflags) = amaflags;
			}
		} else if (!strcasecmp(v->name, "nat")) {
			GLOB(nat) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "directrtp")) {
			GLOB(directrtp) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "allowoverlap")) {
			GLOB(useoverlap) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "musicclass")) {
			sccp_copy_string(GLOB(musicclass), v->value, sizeof(GLOB(musicclass)));
		} else if (!strcasecmp(v->name, "callgroup")) {
			GLOB(callgroup) = ast_get_group(v->value);
#ifdef CS_SCCP_PICKUP
		} else if (!strcasecmp(v->name, "pickupgroup")) {
			GLOB(pickupgroup) = ast_get_group(v->value);
#endif
		} else if (!strcasecmp(v->name, "dateformat")) {
			sccp_copy_string(GLOB(date_format), v->value, sizeof(GLOB(date_format)));
		} else if (!strcasecmp(v->name, "port")) {
			if (readingtype == SCCP_CONFIG_READINITIAL) {
				if (sscanf(v->value, "%i", &GLOB(ourport)) == 1) {
					GLOB(bindaddr.sin_port) = htons(GLOB(ourport));
				} else {
					ast_log(LOG_WARNING, "Invalid port number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
				}
			}
		} else if (!strcasecmp(v->name, "firstdigittimeout")) {
			if (sscanf(v->value, "%i", &firstdigittimeout) == 1) {
				if (firstdigittimeout > 0 && firstdigittimeout < 255)
					GLOB(firstdigittimeout) = firstdigittimeout;
			} else {
				ast_log(LOG_WARNING, "Invalid firstdigittimeout number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "digittimeout")) {
			if (sscanf(v->value, "%i", &digittimeout) == 1) {
				if (digittimeout > 0 && digittimeout < 255)
					GLOB(digittimeout) = digittimeout;
			} else {
				ast_log(LOG_WARNING, "Invalid firstdigittimeout number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "digittimeoutchar")) {
			if (!sccp_strlen_zero(v->value))
				GLOB(digittimeoutchar) = v->value[0];
			else
				GLOB(digittimeoutchar) = digittimeoutchar;
		} else if (!strcasecmp(v->name, "recorddigittimeoutchar")) {
			GLOB(recorddigittimeoutchar) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "debug")) {
			if (readingtype == SCCP_CONFIG_READINITIAL) {
				GLOB(debug) = 0;
				debug_arr[0] = (char *)v->value;
				GLOB(debug) = sccp_parse_debugline(debug_arr, 0, 1, GLOB(debug));
			}
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&GLOB(global_codecs), &GLOB(global_capability), ast_strip(config_value), 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&GLOB(global_codecs), &GLOB(global_capability), ast_strip(config_value), 0);
		} else if (!strcasecmp(v->name, "dnd")) {
			if (!strcasecmp(v->value, "reject")) {
				GLOB(dndmode) = SCCP_DNDMODE_REJECT;
			} else if (!strcasecmp(v->value, "silent")) {
				GLOB(dndmode) = SCCP_DNDMODE_SILENT;
			} else {
				/* 0 is off and 1 (on) is reject */
				GLOB(dndmode) = sccp_true(v->value);
			}
		} else if (!strcasecmp(v->name, "cfwdall")) {
			GLOB(cfwdall) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "cfwdbusy")) {
			GLOB(cfwdbusy) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "cfwdnoanswer")) {
			GLOB(cfwdnoanswer) = sccp_true(v->value);
#ifdef CS_MANAGER_EVENTS
		} else if (!strcasecmp(v->name, "callevents")) {
			GLOB(callevents) = sccp_true(v->value);
#endif
		} else if (!strcasecmp(v->name, "echocancel")) {
			GLOB(echocancel) = sccp_true(v->value);
#ifdef CS_SCCP_PICKUP
		} else if (!strcasecmp(v->name, "pickupmodeanswer")) {
			GLOB(pickupmodeanswer) = sccp_true(v->value);
#endif
		} else if (!strcasecmp(v->name, "silencesuppression")) {
			GLOB(silencesuppression) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "trustphoneip")) {
			GLOB(trustphoneip) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "private")) {
			GLOB(privacy) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "earlyrtp")) {
			if (!strcasecmp(v->value, "none"))
				GLOB(earlyrtp) = 0;
			else if (!strcasecmp(v->value, "offhook"))
				GLOB(earlyrtp) = SCCP_CHANNELSTATE_OFFHOOK;
			else if (!strcasecmp(v->value, "dial"))
				GLOB(earlyrtp) = SCCP_CHANNELSTATE_DIALING;
			else if (!strcasecmp(v->value, "ringout"))
				GLOB(earlyrtp) = SCCP_CHANNELSTATE_RINGOUT;
			else if (!strcasecmp(v->value, "progress"))
				GLOB(earlyrtp) = SCCP_CHANNELSTATE_PROGRESS;
			else
				ast_log(LOG_WARNING, "Invalid earlyrtp state value at line %d, should be 'none', 'offhook', 'dial', 'ringout'\n", v->lineno);
		} else if (!strcasecmp(v->name, "sccp_tos")) {
			if (!pbx_str2tos(v->value, &sccp_tos))
				GLOB(sccp_tos) = sccp_tos;
			else if (sscanf(v->value, "%i", &sccp_tos) == 1)
				GLOB(sccp_tos) = sccp_tos & 0xff;
			else if (!strcasecmp(v->value, "lowdelay"))
				GLOB(sccp_tos) = IPTOS_LOWDELAY;
			else if (!strcasecmp(v->value, "throughput"))
				GLOB(sccp_tos) = IPTOS_THROUGHPUT;
			else if (!strcasecmp(v->value, "reliability"))
				GLOB(sccp_tos) = IPTOS_RELIABILITY;
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
			else if (!strcasecmp(v->value, "mincost"))
				GLOB(sccp_tos) = IPTOS_MINCOST;
#endif
			else if (!strcasecmp(v->value, "none"))
				GLOB(sccp_tos) = 0;
			else {
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
				ast_log(LOG_WARNING, "Invalid sccp_tos value at line %d, should be 'CS\?', 'AF\?\?', 'EF', '0x\?\?', 'lowdelay', 'throughput', 'reliability', 'mincost', or 'none'\n", v->lineno);
#else
				ast_log(LOG_WARNING, "Invalid sccp_tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', or 'none'\n", v->lineno);
#endif
				GLOB(sccp_tos) = 0x68 & 0xff;
			}
		} else if (!strcasecmp(v->name, "audio_tos")) {
			if (!pbx_str2tos(v->value, &audio_tos))
				GLOB(audio_tos) = audio_tos;
			else if (sscanf(v->value, "%i", &audio_tos) == 1)
				GLOB(audio_tos) = audio_tos & 0xff;
			else if (!strcasecmp(v->value, "lowdelay"))
				GLOB(audio_tos) = IPTOS_LOWDELAY;
			else if (!strcasecmp(v->value, "throughput"))
				GLOB(audio_tos) = IPTOS_THROUGHPUT;
			else if (!strcasecmp(v->value, "reliability"))
				GLOB(audio_tos) = IPTOS_RELIABILITY;
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
			else if (!strcasecmp(v->value, "mincost"))
				GLOB(audio_tos) = IPTOS_MINCOST;
#endif
			else if (!strcasecmp(v->value, "none"))
				GLOB(audio_tos) = 0;
			else {
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
				ast_log(LOG_WARNING, "Invalid audio_tos value at line %d, should be 'CS\?', 'AF\?\?', 'EF', '0x\?\?', 'lowdelay', 'throughput', 'reliability', 'mincost', or 'none'\n", v->lineno);
#else
				ast_log(LOG_WARNING, "Invalid audio_tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', or 'none'\n", v->lineno);
#endif
				GLOB(audio_tos) = 0xB8 & 0xff;
			}
		} else if (!strcasecmp(v->name, "video_tos")) {
			if (!pbx_str2tos(v->value, &video_tos))
				GLOB(video_tos) = video_tos;
			else if (sscanf(v->value, "%i", &video_tos) == 1)
				GLOB(video_tos) = video_tos & 0xff;
			else if (!strcasecmp(v->value, "lowdelay"))
				GLOB(video_tos) = IPTOS_LOWDELAY;
			else if (!strcasecmp(v->value, "throughput"))
				GLOB(video_tos) = IPTOS_THROUGHPUT;
			else if (!strcasecmp(v->value, "reliability"))
				GLOB(video_tos) = IPTOS_RELIABILITY;
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
			else if (!strcasecmp(v->value, "mincost"))
				GLOB(video_tos) = IPTOS_MINCOST;
#endif
			else if (!strcasecmp(v->value, "none"))
				GLOB(video_tos) = 0;
			else {
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
				ast_log(LOG_WARNING, "Invalid video_tos value at line %d, should be 'CS\?', 'AF\?\?', 'EF', '0x\?\?', 'lowdelay', 'throughput', 'reliability', 'mincost', or 'none'\n", v->lineno);
#else
				ast_log(LOG_WARNING, "Invalid video_tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', or 'none'\n", v->lineno);
#endif
				GLOB(video_tos) = 0x88 & 0xff;
			}
		} else if (!strcasecmp(v->name, "sccp_cos")) {
			if (sscanf(v->value, "%d", &sccp_cos) == 1) {
				if (sccp_cos > 7) {
					ast_log(LOG_WARNING, "Invalid sccp_cos value at line %d, refer to QoS documentation\n", v->lineno);
				}
				GLOB(sccp_cos) = sccp_cos;
			} else
				GLOB(sccp_cos) = 4;
		} else if (!strcasecmp(v->name, "audio_cos")) {
			if (sscanf(v->value, "%d", &audio_cos) == 1) {
				if (audio_cos > 7) {
					ast_log(LOG_WARNING, "Invalid audio_cos value at line %d, refer to QoS documentation\n", v->lineno);
				}
				GLOB(audio_cos) = audio_cos;
			} else
				GLOB(audio_cos) = 6;
		} else if (!strcasecmp(v->name, "video_cos")) {
			if (sscanf(v->value, "%d", &video_cos) == 1) {
				if (video_cos > 7) {
					ast_log(LOG_WARNING, "Invalid video_cos value at line %d, refer to QoS documentation\n", v->lineno);
				}
				GLOB(video_cos) = video_cos;
			} else
				GLOB(video_cos) = 5;
		} else if (!strcasecmp(v->name, "autoanswer_ring_time")) {
			if (sscanf(v->value, "%i", &autoanswer_ring_time) == 1) {
				if (autoanswer_ring_time >= 0 && autoanswer_ring_time <= 255)
					GLOB(autoanswer_ring_time) = autoanswer_ring_time;
			} else {
				ast_log(LOG_WARNING, "Invalid autoanswer_ring_time value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "autoanswer_tone")) {
			if (sscanf(v->value, "%i", &autoanswer_tone) == 1) {
				if (autoanswer_tone >= 0 && autoanswer_tone <= 255)
					GLOB(autoanswer_tone) = autoanswer_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid autoanswer_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "remotehangup_tone")) {
			if (sscanf(v->value, "%i", &remotehangup_tone) == 1) {
				if (remotehangup_tone >= 0 && remotehangup_tone <= 255)
					GLOB(remotehangup_tone) = remotehangup_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid remotehangup_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "transfer_tone")) {
			if (sscanf(v->value, "%i", &transfer_tone) == 1) {
				if (transfer_tone >= 0 && transfer_tone <= 255)
					GLOB(transfer_tone) = transfer_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid transfer_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "callwaiting_tone")) {
			if (sscanf(v->value, "%i", &callwaiting_tone) == 1) {
				if (callwaiting_tone >= 0 && callwaiting_tone <= 255)
					GLOB(callwaiting_tone) = callwaiting_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid callwaiting_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "mwilamp")) {
			if (!strcasecmp(v->value, "off"))
				GLOB(mwilamp) = SKINNY_LAMP_OFF;
			else if (!strcasecmp(v->value, "on"))
				GLOB(mwilamp) = SKINNY_LAMP_ON;
			else if (!strcasecmp(v->value, "wink"))
				GLOB(mwilamp) = SKINNY_LAMP_WINK;
			else if (!strcasecmp(v->value, "flash"))
				GLOB(mwilamp) = SKINNY_LAMP_FLASH;
			else if (!strcasecmp(v->value, "blink"))
				GLOB(mwilamp) = SKINNY_LAMP_BLINK;
			else
				ast_log(LOG_WARNING, "Invalid mwilamp value at line %d, should be 'off', 'on', 'wink', 'flash' or 'blink'\n", v->lineno);
		} else if (!strcasecmp(v->name, "callanswerorder")) {
			if (!strcasecmp(v->value, "oldestfirst"))
				GLOB(callAnswerOrder) = ANSWER_OLDEST_FIRST;
			else
				GLOB(callAnswerOrder) = ANSWER_LAST_FIRST;
		} else if (!strcasecmp(v->name, "mwioncall")) {
			GLOB(mwioncall) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "blindtransferindication")) {
			if (!strcasecmp(v->value, "moh"))
				GLOB(blindtransferindication) = SCCP_BLINDTRANSFER_MOH;
			else if (!strcasecmp(v->value, "ring"))
				GLOB(blindtransferindication) = SCCP_BLINDTRANSFER_RING;
			else
				ast_log(LOG_WARNING, "Invalid blindtransferindication value at line %d, should be 'moh' or 'ring'\n", v->lineno);
#ifdef CS_SCCP_REALTIME
		} else if (!strcasecmp(v->name, "devicetable")) {
			sccp_copy_string(GLOB(realtimedevicetable), v->value, sizeof(GLOB(realtimedevicetable)));
		} else if (!strcasecmp(v->name, "linetable")) {
			sccp_copy_string(GLOB(realtimelinetable), v->value, sizeof(GLOB(realtimelinetable)));
#endif
		} else if (!strcasecmp(v->name, "hotline_enabled")) {
			GLOB(allowAnonymus) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "hotline_context")) {
			sccp_copy_string(GLOB(hotline)->line->context, v->value, sizeof(GLOB(hotline)->line->context));
		} else if (!strcasecmp(v->name, "hotline_extension")) {
			sccp_copy_string(GLOB(hotline)->exten, v->value, sizeof(GLOB(hotline)->exten));
			if (GLOB(hotline)->line)
				sccp_copy_string(GLOB(hotline)->line->adhocNumber, v->value, sizeof(GLOB(hotline)->line->adhocNumber));

		} else if (!strcasecmp(v->name, "meetme")) {
			GLOB(meetme) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "meetmeopts")) {
			sccp_copy_string(GLOB(meetmeopts), v->value, sizeof(GLOB(meetmeopts)));

			/* begin regexten feature */
		} else if (!strcasecmp(v->name, "regcontext")) {
			sccp_copy_string(newcontexts, v->value, sizeof(newcontexts));
			stringp = newcontexts;
			/* Initialize copy of current global_regcontext for later use in removing stale contexts */
			sccp_copy_string(oldcontexts, GLOB(regcontext), sizeof(oldcontexts));
			oldregcontext = oldcontexts;
			/* Let's remove any contexts that are no longer defined in regcontext */
			cleanup_stale_contexts(stringp, oldregcontext);
			/* Create contexts if they don't exist already */
			while ((context = strsep(&stringp, "&"))) {
				sccp_copy_string(GLOB(used_context), context, sizeof(GLOB(used_context)));
				pbx_context_find_or_create(NULL, NULL, context, "SCCP");
			}
			sccp_copy_string(GLOB(regcontext), v->value, sizeof(GLOB(regcontext)));
			continue;
			/* end regexten feature */
		} else {
			ast_log(LOG_WARNING, "Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
		}
	}

	ast_codec_pref_string(&GLOB(global_codecs), pref_buf, sizeof(pref_buf) - 1);
	ast_verbose(VERBOSE_PREFIX_3 "GLOBAL: Preferred capability %s\n", pref_buf);

	if (!ntohs(GLOB(bindaddr.sin_port))) {
		GLOB(bindaddr.sin_port) = ntohs(DEFAULT_SCCP_PORT);
	}
	GLOB(bindaddr.sin_family) = AF_INET;
	ast_config_destroy(cfg);
	return TRUE;
}

/*!
 * \brief Cleanup Stale Contexts (regcontext)
 * \param new New Context as Character
 * \param old Old Context as Character
 */
void cleanup_stale_contexts(char *new, char *old)
{
	char *oldcontext, *newcontext, *stalecontext, *stringp, newlist[AST_MAX_CONTEXT];

	while ((oldcontext = strsep(&old, "&"))) {
		stalecontext = '\0';
		sccp_copy_string(newlist, new, sizeof(newlist));
		stringp = newlist;
		while ((newcontext = strsep(&stringp, "&"))) {
			if (strcmp(newcontext, oldcontext) == 0) {
				/* This is not the context you're looking for */
				stalecontext = '\0';
				break;
			} else if (strcmp(newcontext, oldcontext)) {
				stalecontext = oldcontext;
			}

		}
		if (stalecontext)
			ast_context_destroy(ast_context_find(stalecontext), "SCCP");
	}
}

/**
 * \brief Read Lines from the Config File
 *
 * \param readingtype as SCCP Reading Type
 * \since 10.01.2008 - branche V3
 * \author Marcello Ceschia
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - see sccp_config_applyLineConfiguration()
 */
void sccp_config_readDevicesLines(sccp_readingtype_t readingtype)
{
	struct ast_config *cfg = NULL;

	char *cat = NULL;

	struct ast_variable *v = NULL;

	uint8_t device_count = 0;

	uint8_t line_count = 0;

	ast_log(LOG_NOTICE, "Loading Devices and Lines from config\n");

#ifdef CS_DYNAMIC_CONFIG
	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Checking ReadingType\n");
	if (readingtype == SCCP_CONFIG_READRELOAD) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Device Pre Reload\n");
		sccp_device_pre_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Line Pre Reload\n");
		sccp_line_pre_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Softkey Pre Reload\n");
		sccp_softkey_pre_reload();
	}
#endif

	cfg = sccp_config_getConfig();
	if (!cfg) {
		ast_log(LOG_NOTICE, "Unable to load config file sccp.conf, SCCP disabled\n");
		return;
	}

	while ((cat = ast_category_browse(cfg, cat))) {

		const char *utype;

		if (!strcasecmp(cat, "general"))
			continue;

		utype = ast_variable_retrieve(cfg, cat, "type");

		if (!utype) {
			ast_log(LOG_WARNING, "Section '%s' is missing a type paramater\n", cat);
			continue;
		} else if (!strcasecmp(utype, "device")) {
			// check minimum requirements for a device
			if (sccp_strlen_zero(ast_variable_retrieve(cfg, cat, "devicetype"))) {
				ast_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sccp.conf");
				continue;
			} else {
				v = ast_variable_browse(cfg, cat);
				sccp_config_buildDevice(v, cat, FALSE);
				device_count++;
				ast_verbose(VERBOSE_PREFIX_3 "found device %d: %s\n", device_count, cat);
			}
		} else if (!strcasecmp(utype, "line")) {
			/* check minimum requirements for a line */
			if ((!(!sccp_strlen_zero(ast_variable_retrieve(cfg, cat, "label"))) && (!sccp_strlen_zero(ast_variable_retrieve(cfg, cat, "cid_name"))) && (!sccp_strlen_zero(ast_variable_retrieve(cfg, cat, "cid_num"))))) {
				ast_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sccp.conf");
				continue;
			}
			line_count++;

			v = ast_variable_browse(cfg, cat);
			sccp_config_buildLine(v, cat, FALSE);
			ast_verbose(VERBOSE_PREFIX_3 "found line %d: %s\n", line_count, cat);
		} else if (!strcasecmp(utype, "softkeyset")) {
			ast_verbose(VERBOSE_PREFIX_3 "read set %s\n", cat);
			v = ast_variable_browse(cfg, cat);
			sccp_config_softKeySet(v, cat);
		}
	}
	ast_config_destroy(cfg);

#ifdef CS_SCCP_REALTIME
	/* reload realtime lines */
	sccp_line_t *l;

	sccp_configurationchange_t res;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (l->realtime == TRUE && l != GLOB(hotline)->line) {
			sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "%s: reload realtime line\n", l->name);
			v = ast_load_realtime(GLOB(realtimelinetable), "name", l->name, NULL);
#    ifdef CS_DYNAMIC_CONFIG
			/* we did not find this line, mark it for deletion */
			if (!v) {
				sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "%s: realtime line not found - set pendingDelet=1\n", l->name);
				l->pendingDelete = 1;
				continue;
			}
#    endif

			res = sccp_config_applyLineConfiguration(l, v);
			/* check if we did some changes that needs a device update */
#    ifdef CS_DYNAMIC_CONFIG
			if (res == SCCP_CONFIG_NEEDDEVICERESET) {
				l->pendingUpdate = 1;
			}
#    endif
			ast_variables_destroy(v);
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	/* finished realtime line reload */
#endif

#ifdef CS_DYNAMIC_CONFIG
	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Checking ReadingType\n");
	if (readingtype == SCCP_CONFIG_READRELOAD) {
		/* IMPORTANT: The line_post_reload function may change the pendingUpdate field of
		 * devices, so it's really important to call it *before* calling device_post_real().
		 */
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Line Post Reload\n");
		sccp_line_post_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Device Post Reload\n");
		sccp_device_post_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Softkey Post Reload\n");
		sccp_softkey_post_reload();
	}
#endif
}

/**
 * \brief Get Configured Line from Asterisk Variable
 * \param l SCCP Line
 * \param v Asterisk Variable
 * \return Configured SCCP Line
 * \note also used by realtime functionality to line device from Asterisk Variable
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- line->mailboxes
 */
sccp_configurationchange_t sccp_config_applyLineConfiguration(sccp_line_t * l, struct ast_variable *v)
{
	int amaflags = 0;

	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;

	unsigned int audio_tos = 0;

	unsigned int video_tos = 0;

	unsigned int audio_cos = 0;

	unsigned int video_cos = 0;

	int secondary_dialtone_tone = 0;

#ifdef CS_DYNAMIC_CONFIG
	if (l->pendingDelete) {
		/* removing variables */
		if (l->variables) {
			ast_variables_destroy(l->variables);
			l->variables = NULL;
		}
	}
#endif

	while (v) {
		if (!(v->name)) {
			ast_log(LOG_WARNING, "Null variable name in configuration\n");
		} else if (!(v->value)) {
			ast_log(LOG_WARNING, "Null variable value in configuration\n");
		} else {
			//ast_verbose(VERBOSE_PREFIX_3, "Processing variable name \"%s\", value \"%s\"\n", v->name, v->value);
			if ((!strcasecmp(v->name, "line"))
#ifdef CS_SCCP_REALTIME
			    || (!strcasecmp(v->name, "name"))
#endif
			    ) {
				// do nothing
			} else if (!strcasecmp(v->name, "type")) {
				//skip;
			} else if (!strcasecmp(v->name, "id")) {
				sccp_copy_string(l->id, v->value, sizeof(l->id));
			} else if (!strcasecmp(v->name, "pin")) {
				sccp_copy_string(l->pin, v->value, sizeof(l->pin));
			} else if (!strcasecmp(v->name, "label")) {
				if (strcasecmp(l->label, v->value) != 0) {
					res = SCCP_CONFIG_NEEDDEVICERESET;
				}
				sccp_copy_string(l->label, v->value, sizeof(l->label));
			} else if (!strcasecmp(v->name, "description")) {
				sccp_copy_string(l->description, v->value, sizeof(l->description));
			} else if (!strcasecmp(v->name, "context")) {
				sccp_copy_string(l->context, v->value, sizeof(l->context));
			} else if (!strcasecmp(v->name, "cid_name")) {
				sccp_copy_string(l->cid_name, v->value, sizeof(l->cid_name));
			} else if (!strcasecmp(v->name, "cid_num")) {
				sccp_copy_string(l->cid_num, v->value, sizeof(l->cid_num));
			} else if (!strcasecmp(v->name, "defaultSubscriptionId_name")) {	// Subscription IDs
				sccp_copy_string(l->defaultSubscriptionId.name, v->value, sizeof(l->defaultSubscriptionId.name));
			} else if (!strcasecmp(v->name, "defaultSubscriptionId_number")) {
				sccp_copy_string(l->defaultSubscriptionId.number, v->value, sizeof(l->defaultSubscriptionId.number));
			} else if (!strcasecmp(v->name, "callerid")) {
				ast_log(LOG_WARNING, "obsolete callerid param. Use cid_num and cid_name\n");
			} else if (!strcasecmp(v->name, "mailbox")) {
				sccp_mailbox_t *mailbox = NULL;

				char *context, *mbox = NULL;

				mbox = context = sccp_strdupa(v->value);
				boolean_t mailbox_exists = FALSE;

				strsep(&context, "@");

				// Check mailboxes list
				SCCP_LIST_LOCK(&l->mailboxes);
				SCCP_LIST_TRAVERSE(&l->mailboxes, mailbox, list) {
					if ((!sccp_strlen_zero(mbox))) {
						if (!strcmp(mailbox->mailbox, mbox) || !strcmp(mailbox->mailbox, mbox)) {
							mailbox_exists = TRUE;
						}
					}
				}
				if ((!mailbox_exists) && (!sccp_strlen_zero(mbox))) {
					// Add mailbox entry
					mailbox = ast_calloc(1, sizeof(*mailbox));

					if (NULL != mailbox) {
						mailbox->mailbox = sccp_strdup(mbox);
						mailbox->context = sccp_strdup(context);

						SCCP_LIST_INSERT_TAIL(&l->mailboxes, mailbox, list);
						sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "%s: Added mailbox '%s@%s'\n", l->name, mailbox->mailbox, (mailbox->context) ? mailbox->context : "default");
					}
				}
				SCCP_LIST_UNLOCK(&l->mailboxes);
			} else if (!strcasecmp(v->name, "vmnum")) {
				sccp_copy_string(l->vmnum, v->value, sizeof(l->vmnum));
			} else if (!strcasecmp(v->name, "adhocNumber")) {
				sccp_copy_string(l->adhocNumber, v->value, sizeof(l->adhocNumber));
			} else if (!strcasecmp(v->name, "meetmenum")) {
				sccp_copy_string(l->meetmenum, v->value, sizeof(l->meetmenum));
			} else if (!strcasecmp(v->name, "meetme")) {
				l->meetme = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "meetmeopts")) {
				sccp_copy_string(l->meetmeopts, v->value, sizeof(l->meetmeopts));
			} else if (!strcasecmp(v->name, "transfer")) {
				l->transfer = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "incominglimit")) {
				l->incominglimit = atoi(v->value);
				if (l->incominglimit < 1)
					l->incominglimit = 1;
			} else if (!strcasecmp(v->name, "echocancel")) {
				l->echocancel = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "silencesuppression")) {
				l->silencesuppression = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "audio_tos")) {
				if (!pbx_str2tos(v->value, &audio_tos))
					l->audio_tos = audio_tos;
				else if (sscanf(v->value, "%i", &audio_tos) == 1)
					l->audio_tos = audio_tos & 0xff;
				else if (!strcasecmp(v->value, "lowdelay"))
					l->audio_tos = IPTOS_LOWDELAY;
				else if (!strcasecmp(v->value, "throughput"))
					l->audio_tos = IPTOS_THROUGHPUT;
				else if (!strcasecmp(v->value, "reliability"))
					l->audio_tos = IPTOS_RELIABILITY;
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
				else if (!strcasecmp(v->value, "mincost"))
					l->audio_tos = IPTOS_MINCOST;
#endif
				else if (!strcasecmp(v->value, "none"))
					l->audio_tos = 0;
				else {
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
					ast_log(LOG_WARNING, "Invalid audio_tos value at line %d, should be 'CS\?', 'AF\?\?', 'EF', '0x\?\?' ,'lowdelay', 'throughput', 'reliability', 'mincost', or 'none'\n", v->lineno);
#else
					ast_log(LOG_WARNING, "Invalid audio_tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', or 'none'\n", v->lineno);
#endif
					l->audio_tos = GLOB(audio_tos);
				}
			} else if (!strcasecmp(v->name, "video_tos")) {
				if (!pbx_str2tos(v->value, &video_tos))
					l->video_tos = video_tos;
				else if (sscanf(v->value, "%i", &video_tos) == 1)
					l->video_tos = video_tos & 0xff;
				else if (!strcasecmp(v->value, "lowdelay"))
					l->video_tos = IPTOS_LOWDELAY;
				else if (!strcasecmp(v->value, "throughput"))
					l->video_tos = IPTOS_THROUGHPUT;
				else if (!strcasecmp(v->value, "reliability"))
					l->video_tos = IPTOS_RELIABILITY;
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
				else if (!strcasecmp(v->value, "mincost"))
					l->video_tos = IPTOS_MINCOST;
#endif
				else if (!strcasecmp(v->value, "none"))
					l->video_tos = 0;
				else {
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
					ast_log(LOG_WARNING, "Invalid video_tos value at line %d, should be 'CS\?', 'AF\?\?', 'EF', '0x\?\?' ,'lowdelay', 'throughput', 'reliability', 'mincost', or 'none'\n", v->lineno);
#else
					ast_log(LOG_WARNING, "Invalid video_tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', or 'none'\n", v->lineno);
#endif
					l->video_tos = GLOB(video_tos);
				}
			} else if (!strcasecmp(v->name, "audio_cos")) {
				if (sscanf(v->value, "%d", &audio_cos) == 1) {
					if (audio_cos > 7) {
						ast_log(LOG_WARNING, "Invalid audio_cos value at line %d, refer to QoS documentation\n", v->lineno);
					}
					l->audio_cos = audio_cos;
				} else
					l->audio_cos = GLOB(audio_cos);
			} else if (!strcasecmp(v->name, "video_cos")) {
				if (sscanf(v->value, "%d", &video_cos) == 1) {
					if (video_cos > 7) {
						ast_log(LOG_WARNING, "Invalid video_cos value at line %d, refer to QoS documentation\n", v->lineno);
					}
					l->video_cos = video_cos;
				} else
					l->video_cos = GLOB(video_cos);
			} else if (!strcasecmp(v->name, "language")) {
				sccp_copy_string(l->language, v->value, sizeof(l->language));
			} else if (!strcasecmp(v->name, "musicclass")) {
				sccp_copy_string(l->musicclass, v->value, sizeof(l->musicclass));
			} else if (!strcasecmp(v->name, "accountcode")) {
				sccp_copy_string(l->accountcode, v->value, sizeof(l->accountcode));
			} else if (!strcasecmp(v->name, "amaflags")) {
				amaflags = ast_cdr_amaflags2int(v->value);

				if (amaflags < 0) {
					ast_log(LOG_WARNING, "Invalid AMA Flags: %s at line %d\n", v->value, v->lineno);
				} else {
					l->amaflags = amaflags;
				}
			} else if (!strcasecmp(v->name, "callgroup")) {
				l->callgroup = ast_get_group(v->value);
#ifdef CS_SCCP_PICKUP
			} else if (!strcasecmp(v->name, "pickupgroup")) {
				l->pickupgroup = ast_get_group(v->value);
#endif
			} else if (!strcasecmp(v->name, "trnsfvm")) {
				if (!sccp_strlen_zero(v->value)) {
					l->trnsfvm = strdup(v->value);
				}
			} else if (!strcasecmp(v->name, "secondary_dialtone_digits")) {
				if (strlen(v->value) > 9)
					ast_log(LOG_WARNING, "secondary_dialtone_digits value '%s' is too long at line %d of SCCP.CONF. Max 9 digits\n", v->value, v->lineno);

				sccp_copy_string(l->secondary_dialtone_digits, v->value, sizeof(l->secondary_dialtone_digits));
			} else if (!strcasecmp(v->name, "secondary_dialtone_tone")) {
				if (sscanf(v->value, "%i", &secondary_dialtone_tone) == 1) {
					if (secondary_dialtone_tone >= 0 && secondary_dialtone_tone <= 255)
						l->secondary_dialtone_tone = secondary_dialtone_tone;
					else
						l->secondary_dialtone_tone = SKINNY_TONE_OUTSIDEDIALTONE;
				} else {
					ast_log(LOG_WARNING, "Invalid secondary_dialtone_tone value '%s' at line %d of SCCP.CONF. Default is OutsideDialtone (0x22)\n", v->value, v->lineno);
				}
			} else if (!strcasecmp(v->name, "setvar")) {

				struct ast_variable *newvar = NULL;

				newvar = sccp_create_variable(v->value);

				if (newvar) {
					sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CONFIG | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "Add new channelvariable to line %s. Value is: %s \n", newvar->name, newvar->value);
					newvar->next = l->variables;
					l->variables = newvar;
				}
			} else if (!strcasecmp(v->name, "dnd")) {
				if (!strcasecmp(v->value, "reject")) {
					l->dndmode = SCCP_DNDMODE_REJECT;
				} else if (!strcasecmp(v->value, "silent")) {
					l->dndmode = SCCP_DNDMODE_SILENT;
				} else if (!strcasecmp(v->value, "user")) {
					l->dndmode = SCCP_DNDMODE_USERDEFINED;
				} else {
					/* 0 is off and 1 (on) is reject */
					l->dndmode = sccp_true(v->value);
				}

			} else if (!strcasecmp(v->name, "regexten")) {
				sccp_copy_string(l->regexten, v->value, sizeof(l->regexten));

			} else {
				if (v->name && v->value) {
					ast_log(LOG_WARNING, "Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
				} else {
					ast_log(LOG_WARNING, "Null param in config at line %d.\n", v->lineno);
				}
			}
		}
		v = v->next;
	}
	return res;
}

/**
 * \brief Apply Device Configuration from Asterisk Variable
 * \param d SCCP Device
 * \param v Asterisk Variable
 * \return Configured SCCP Device
 * \note also used by realtime functionality to line device from Asterisk Variable
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device->addons
 * 	- device->permithosts
 */
sccp_device_t *sccp_config_applyDeviceConfiguration(sccp_device_t * d, struct ast_variable * v)
{

	/* for button config */
	char *buttonType = NULL, *buttonName = NULL, *buttonOption = NULL, *buttonArgs = NULL;

	char k_button[256];

	uint16_t instance = 0;

	char *splitter;
	int digittimeout;

	char config_value[256];

#ifdef CS_DYNAMIC_CONFIG
	if (d->pendingDelete) {
		// remove all addons before adding them again (to find differences later on in sccp_device_change)
		sccp_addon_t *addon;

		SCCP_LIST_LOCK(&d->addons);
		while ((addon = SCCP_LIST_REMOVE_HEAD(&d->addons, list))) {
			ast_free(addon);
			addon = NULL;
		}
		SCCP_LIST_UNLOCK(&d->addons);
		SCCP_LIST_HEAD_DESTROY(&d->addons);
		SCCP_LIST_HEAD_INIT(&d->addons);

		/* removing variables */
		if (d->variables) {
			ast_variables_destroy(d->variables);
			d->variables = NULL;
		}

		/* removing permithosts */
		sccp_hostname_t *permithost;

		SCCP_LIST_LOCK(&d->permithosts);
		while ((permithost = SCCP_LIST_REMOVE_HEAD(&d->permithosts, list))) {
			ast_free(permithost);
			permithost = NULL;
		}
		SCCP_LIST_UNLOCK(&d->permithosts);
		SCCP_LIST_HEAD_DESTROY(&d->permithosts);
		SCCP_LIST_HEAD_INIT(&d->permithosts);
	}
#endif

	if (!v) {
		sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "no variable given\n");
		return d;
	}

	while (v) {
		sccp_copy_string(config_value, v->value, sizeof(config_value));
		sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s = %s\n", v->name, config_value);

		if ((!strcasecmp(v->name, "device"))
#ifdef CS_SCCP_REALTIME
		    || (!strcasecmp(v->name, "name"))
#endif
		    ) {
			// do nothing
		} else if (!strcasecmp(v->name, "keepalive")) {
			d->keepalive = atoi(v->value);
		} else if (!strcasecmp(v->name, "permit") || !strcasecmp(v->name, "deny")) {
			d->ha = pbx_append_ha(v->name, v->value, d->ha, NULL);
		} else if (!strcasecmp(v->name, "button")) {
#ifdef CS_DYNAMIC_CONFIG
			button_type_t type;

			unsigned i;
#endif										/* CS_DYNAMIC_CONFIG */

			sccp_log(0) (VERBOSE_PREFIX_3 "Found buttonconfig: %s\n", v->value);
			sccp_copy_string(k_button, v->value, sizeof(k_button));
			splitter = k_button;
			buttonType = strsep(&splitter, ",");
			buttonName = strsep(&splitter, ",");
			buttonOption = strsep(&splitter, ",");
			buttonArgs = splitter;

#ifdef CS_DYNAMIC_CONFIG
			for (i = 0; i < ARRAY_LEN(sccp_buttontypes) && strcasecmp(buttonType, sccp_buttontypes[i].text); ++i) ;

			if (i >= ARRAY_LEN(sccp_buttontypes)) {
				ast_log(LOG_WARNING, "Unknown button type '%s' at line %d.\n", buttonType, v->lineno);
				v = v->next;
				continue;
			}

			type = sccp_buttontypes[i].buttontype;
#endif

			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ButtonType: %s\n", buttonType);
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ButtonName: %s\n", buttonName);
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ButtonOption: %s\n", buttonOption);

#ifdef CS_DYNAMIC_CONFIG
			sccp_config_addButton(d, ++instance, type, buttonName ? ast_strip(buttonName) : buttonType, buttonOption ? ast_strip(buttonOption) : NULL, buttonArgs ? ast_strip(buttonArgs) : NULL);
#else
			if (!strcasecmp(buttonType, "line") && buttonName) {
				if (!buttonName)
					continue;
				sccp_config_addLine(d, (buttonName) ? ast_strip(buttonName) : NULL, buttonOption, ++instance);
			} else if (!strcasecmp(buttonType, "empty")) {
				sccp_config_addEmpty(d, ++instance);
			} else if (!strcasecmp(buttonType, "speeddial")) {
				sccp_config_addSpeeddial(d, (buttonName) ? ast_strip(buttonName) : "speeddial", (buttonOption) ? ast_strip(buttonOption) : NULL, (buttonArgs) ? ast_strip(buttonArgs) : NULL, ++instance);
			} else if (!strcasecmp(buttonType, "feature") && buttonName) {
				sccp_config_addFeature(d, (buttonName) ? ast_strip(buttonName) : "feature", (buttonOption) ? ast_strip(buttonOption) : NULL, buttonArgs, ++instance);
			} else if (!strcasecmp(buttonType, "service") && buttonName && !sccp_strlen_zero(buttonName)) {
				sccp_config_addService(d, (buttonName) ? ast_strip(buttonName) : "service", (buttonOption) ? ast_strip(buttonOption) : NULL, ++instance);
			}
#endif										/* CS_DYNAMIC_CONFIG */

		} else if (!strcasecmp(v->name, "permithost")) {
			sccp_permithost_addnew(d, v->value);
		} else if ((!strcasecmp(v->name, "type")) || !strcasecmp(v->name, "devicetype")) {
			if (strcasecmp(v->value, "device")) {
				sccp_copy_string(d->config_type, v->value, sizeof(d->config_type));
			}
		} else if (!strcasecmp(v->name, "digittimeout")) {
			if (sscanf(v->value, "%i", &digittimeout) == 1) {
				if (digittimeout > 0 && digittimeout < 255)
					d->digittimeout = digittimeout;
			}
		} else if (!strcasecmp(v->name, "addon")) {
			sccp_addon_addnew(d, v->value);
		} else if (!strcasecmp(v->name, "tzoffset")) {
			d->tz_offset = atoi(v->value);
		} else if (!strcasecmp(v->name, "description")) {
			sccp_copy_string(d->description, v->value, sizeof(d->description));
		} else if (!strcasecmp(v->name, "imageversion")) {
			sccp_copy_string(d->imageversion, v->value, sizeof(d->imageversion));
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&d->codecs, &d->capability, ast_strip(config_value), 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&d->codecs, &d->capability, ast_strip(config_value), 0);
		} else if (!strcasecmp(v->name, "transfer")) {
			d->transfer = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "cfwdall")) {
			d->cfwdall = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "cfwdbusy")) {
			d->cfwdbusy = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "cfwdnoanswer")) {
			d->cfwdnoanswer = sccp_true(v->value);
#ifdef CS_SCCP_PICKUP
		} else if (!strcasecmp(v->name, "pickupexten")) {
			d->pickupexten = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "pickupcontext")) {
			if (!sccp_strlen_zero(v->value)) {
				ast_free(d->pickupcontext);
				d->pickupcontext = strdup(v->value);
			}
		} else if (!strcasecmp(v->name, "pickupmodeanswer")) {
			d->pickupmodeanswer = sccp_true(v->value);
#endif
		} else if (!strcasecmp(v->name, "dnd")) {
			/* 0 is off and 1 (on) is reject */
			if (!strcasecmp(v->value, "off")) {
				d->dndFeature.enabled = FALSE;
			} else {
				d->dndFeature.enabled = TRUE;
			}
			sccp_copy_string(d->dndFeature.configOptions, v->value, sizeof(d->dndFeature.configOptions));
		} else if (!strcasecmp(v->name, "nat")) {
			d->nat = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "directrtp")) {
			d->directrtp = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "allowoverlap")) {
			d->overlapFeature.enabled = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "trustphoneip")) {
			d->trustphoneip = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "private")) {
			d->privacyFeature.enabled = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "softkeyset")) {
			sccp_copy_string(d->softkeyDefinition, v->value, sizeof(d->softkeyDefinition));
		} else if (!strcasecmp(v->name, "privacy")) {
			if (!strcasecmp(v->value, "full")) {
				d->privacyFeature.status = ~0;
			} else {
				d->privacyFeature.status = 0;
			}
		} else if (!strcasecmp(v->name, "earlyrtp")) {
			if (!strcasecmp(v->value, "none"))
				d->earlyrtp = 0;
			else if (!strcasecmp(v->value, "offhook"))
				d->earlyrtp = SCCP_CHANNELSTATE_OFFHOOK;
			else if (!strcasecmp(v->value, "dial"))
				d->earlyrtp = SCCP_CHANNELSTATE_DIALING;
			else if (!strcasecmp(v->value, "ringout"))
				d->earlyrtp = SCCP_CHANNELSTATE_RINGOUT;
			else if (!strcasecmp(v->value, "progress"))
				d->earlyrtp = SCCP_CHANNELSTATE_PROGRESS;
			else
				ast_log(LOG_WARNING, "Invalid earlyrtp state value at line %d, should be 'none', 'offhook', 'dial', 'ringout'\n", v->lineno);
		} else if (!strcasecmp(v->name, "dtmfmode")) {
			if (!strcasecmp(v->value, "outofband"))
				d->dtmfmode = SCCP_DTMFMODE_OUTOFBAND;

#ifdef CS_SCCP_PARK
		} else if (!strcasecmp(v->name, "park")) {
			d->park = sccp_true(v->value);
#endif
		} else if (!strcasecmp(v->name, "mwilamp")) {
			if (!strcasecmp(v->value, "off"))
				d->mwilamp = SKINNY_LAMP_OFF;
			else if (!strcasecmp(v->value, "on"))
				d->mwilamp = SKINNY_LAMP_ON;
			else if (!strcasecmp(v->value, "wink"))
				d->mwilamp = SKINNY_LAMP_WINK;
			else if (!strcasecmp(v->value, "flash"))
				d->mwilamp = SKINNY_LAMP_FLASH;
			else if (!strcasecmp(v->value, "blink"))
				d->mwilamp = SKINNY_LAMP_BLINK;
			else
				ast_log(LOG_WARNING, "Invalid mwilamp value at line %d, should be 'off', 'on', 'wink', 'flash' or 'blink'\n", v->lineno);
		} else if (!strcasecmp(v->name, "mwioncall")) {
			d->mwioncall = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "setvar")) {

			struct ast_variable *newvar = NULL;

			newvar = sccp_create_variable(v->value);

			if (newvar) {
				sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Add new channelvariable to line %s. Value is: %s \n", newvar->name, newvar->value);
				newvar->next = d->variables;
				d->variables = newvar;
			}
		} else if (!strcasecmp(v->name, "useRedialMenu")) {
#ifdef CS_ADV_FEATURES
			d->useRedialMenu = sccp_true(v->value);
#endif
		} else if (!strcasecmp(v->name, "meetme")) {
			d->meetme = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "meetmeopts")) {
			sccp_copy_string(d->meetmeopts, v->value, sizeof(d->meetmeopts));
		} else {
			ast_log(LOG_WARNING, "SCCP: Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
		}

		v = v->next;
	}

	/* load saved settings from ast db */
	sccp_config_restoreDeviceFeatureStatus(d);

	return d;
}

/**
 * \brief Find the Correct Config File
 * \return Asterisk Config Object as ast_config
 */
struct ast_config *sccp_config_getConfig()
{
	struct ast_config *cfg = NULL;

	struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS };
	cfg = pbx_config_load("sccp.conf", "chan_sccp", config_flags);
	if (!cfg)
		cfg = pbx_config_load("sccp_v3.conf", "chan_sccp", config_flags);

	/* Warn user when old entries exist in sccp.conf */
	if (cfg && ast_variable_browse(cfg, "devices")) {
		ast_log(LOG_WARNING, "\n\n --> You are using an old configuration format, pleas update your sccp.conf!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n");
		ast_config_destroy(cfg);
		return NULL;
	}

	return cfg;
}

/**
 * \brief Read a SoftKey configuration context
 * \param variable list of configuration variables
 * \param name name of this configuration (context)
 * 
 * \callgraph
 * \callergraph
 *
 * \lock
 *	- softKeySetConfig
 */
void sccp_config_softKeySet(struct ast_variable *variable, const char *name)
{
	int keySetSize;

	sccp_softKeySetConfiguration_t *softKeySetConfiguration = NULL;

	int keyMode = -1;

	unsigned int i = 0;

	sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "start reading softkeyset: %s\n", name);

	softKeySetConfiguration = ast_calloc(1, sizeof(sccp_softKeySetConfiguration_t));

	sccp_copy_string(softKeySetConfiguration->name, name, sizeof(softKeySetConfiguration->name));
	softKeySetConfiguration->numberOfSoftKeySets = 0;

	while (variable) {
		keyMode = -1;
		sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "softkeyset: %s \n", variable->name);
		if (!strcasecmp(variable->name, "type")) {

		} else if (!strcasecmp(variable->name, "onhook")) {
			keyMode = KEYMODE_ONHOOK;
		} else if (!strcasecmp(variable->name, "connected")) {
			keyMode = KEYMODE_CONNECTED;
		} else if (!strcasecmp(variable->name, "onhold")) {
			keyMode = KEYMODE_ONHOLD;
		} else if (!strcasecmp(variable->name, "ringin")) {
			keyMode = KEYMODE_RINGIN;
		} else if (!strcasecmp(variable->name, "offhook")) {
			keyMode = KEYMODE_OFFHOOK;
		} else if (!strcasecmp(variable->name, "conntrans")) {
			keyMode = KEYMODE_CONNTRANS;
		} else if (!strcasecmp(variable->name, "digitsfoll")) {
			keyMode = KEYMODE_DIGITSFOLL;
		} else if (!strcasecmp(variable->name, "connconf")) {
			keyMode = KEYMODE_CONNCONF;
		} else if (!strcasecmp(variable->name, "ringout")) {
			keyMode = KEYMODE_RINGOUT;
		} else if (!strcasecmp(variable->name, "offhookfeat")) {
			keyMode = KEYMODE_OFFHOOKFEAT;
		} else if (!strcasecmp(variable->name, "onhint")) {
			keyMode = KEYMODE_INUSEHINT;
		} else if (!strcasecmp(variable->name, "onstealable")) {
			keyMode = KEYMODE_ONHOOKSTEALABLE;
		} else {
			// do nothing
		}

		if (keyMode == -1) {
			variable = variable->next;
			continue;
		}

		if (softKeySetConfiguration->numberOfSoftKeySets < (keyMode + 1)) {
			softKeySetConfiguration->numberOfSoftKeySets = keyMode + 1;
		}

		for (i = 0; i < (sizeof(SoftKeyModes) / sizeof(softkey_modes)); i++) {
			if (SoftKeyModes[i].id == keyMode) {
				uint8_t *softkeyset = ast_calloc(StationMaxSoftKeySetDefinition, sizeof(uint8_t));

				keySetSize = sccp_config_readSoftSet(softkeyset, variable->value);

				if (keySetSize > 0) {
					softKeySetConfiguration->modes[i].id = keyMode;
					softKeySetConfiguration->modes[i].ptr = softkeyset;
					softKeySetConfiguration->modes[i].count = keySetSize;
				} else {
					ast_free(softkeyset);
				}
			}
		}

		variable = variable->next;
	}

	SCCP_LIST_LOCK(&softKeySetConfig);
	SCCP_LIST_INSERT_HEAD(&softKeySetConfig, softKeySetConfiguration, list);
	SCCP_LIST_UNLOCK(&softKeySetConfig);
}

/**
 * \brief Read a single SoftKeyMode (configuration values)
 * \param softkeyset SoftKeySet
 * \param data configuration values
 * \return number of parsed softkeys
 */
uint8_t sccp_config_readSoftSet(uint8_t * softkeyset, const char *data)
{
	int i = 0, j;

	char labels[256];

	char *splitter;

	char *label;

	if (!data)
		return 0;

	strcpy(labels, data);
	splitter = labels;
	while ((label = strsep(&splitter, ",")) != NULL && (i + 1) < StationMaxSoftKeySetDefinition) {
		softkeyset[i++] = sccp_config_getSoftkeyLbl(label);
	}

	for (j = i + 1; j < StationMaxSoftKeySetDefinition; j++) {
		softkeyset[j] = SKINNY_LBL_EMPTY;
	}
	return i;
}

/**
 * \brief Get softkey label as int
 * \param key configuration value
 * \return SoftKey label as int of SKINNY_LBL_*. returns an empty button if nothing matched
 */
int sccp_config_getSoftkeyLbl(char *key)
{
	int i = 0;

	int size = sizeof(softKeyTemplate) / sizeof(softkeyConfigurationTemplate);

	for (i = 0; i < size; i++) {
		if (!strcasecmp(softKeyTemplate[i].configVar, key)) {
			return softKeyTemplate[i].softkey;
		}
	}
	sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "softkeybutton: %s not defined", key);
	return SKINNY_LBL_EMPTY;
}

/**
 * \brief Restore feature status from ast-db
 * \param device device to be restored
 * \todo restore clfw feature
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device->devstateSpecifiers
 */
void sccp_config_restoreDeviceFeatureStatus(sccp_device_t * device)
{
	if (!device)
		return;

#ifdef CS_DEVSTATE_FEATURE
	char buf[256] = "";

	sccp_devstate_specifier_t *specifier;
#endif

#ifndef ASTDB_FAMILY_KEY_LEN
#    define ASTDB_FAMILY_KEY_LEN 256
#endif

#ifndef ASTDB_RESULT_LEN
#    define ASTDB_RESULT_LEN 256
#endif

	char buffer[ASTDB_RESULT_LEN];

	char family[ASTDB_FAMILY_KEY_LEN];

	int res;

	sprintf(family, "SCCP/%s", device->id);

	/* dndFeature */
	res = pbx_db_get(family, "dnd", buffer, sizeof(buffer));
	if (!res) {
		if (!strcasecmp(buffer, "silent"))
			device->dndFeature.status = SCCP_DNDMODE_SILENT;
		else
			device->dndFeature.status = SCCP_DNDMODE_REJECT;
	} else {
		device->dndFeature.status = SCCP_DNDMODE_OFF;
	}

	/* monitorFeature */
	res = pbx_db_get(family, "monitor", buffer, sizeof(buffer));
	if (!res) {
		device->monitorFeature.status = 1;
	} else {
		device->monitorFeature.status = 0;
	}

	/* privacyFeature */
	res = pbx_db_get(family, "privacy", buffer, sizeof(buffer));
	if (!res) {
		sscanf(buffer, "%u", (unsigned int *)&device->privacyFeature.status);
	} else {
		device->privacyFeature.status = 0;
	}

	/* Message */
	res = pbx_db_get("SCCPM", device->id, buffer, sizeof(buffer));		//load save message from ast_db
	if (!res)
		device->phonemessage = strdup(buffer);				//set message on device if we have a result

	/* lastDialedNumber */
	char lastNumber[AST_MAX_EXTENSION] = "";

	res = pbx_db_get(family, "lastDialedNumber", lastNumber, sizeof(lastNumber));
	if (!res) {
		sccp_copy_string(device->lastNumber, lastNumber, sizeof(device->lastNumber));
	}

	/* initialize so called priority feature */
	device->priFeature.status = 0x010101;
	device->priFeature.initialized = 0;

#ifdef CS_DEVSTATE_FEATURE
	/* Read and initialize custom devicestate entries */
	SCCP_LIST_LOCK(&device->devstateSpecifiers);
	SCCP_LIST_TRAVERSE(&device->devstateSpecifiers, specifier, list) {
		/* Check if there is already a devicestate entry */
		res = pbx_db_get(devstate_astdb_family, specifier->specifier, buf, sizeof(buf));
		if (!res) {
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "%s: Found Existing Custom Devicestate Entry: %s, state: %s\n", device->id, specifier->specifier, buf);
			/* If not present, add a new devicestate entry. Default: NOT_INUSE */
		} else {
			pbx_db_put(devstate_astdb_family, specifier->specifier, "NOT_INUSE");
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "%s: Initialized Devicestate Entry: %s\n", device->id, specifier->specifier);
		}
		/* Register as generic hint watcher */
		/*! \todo Add some filtering in order to reduce number of unneccessarily triggered events.
		   Have to work out whether filtering with AST_EVENT_IE_DEVICE matches extension or hint device name. */
		snprintf(buf, 254, "Custom:%s", specifier->specifier);

		/* When registering for devstate events we wish to know if a single asterisk box has contributed
		   a change even in a rig of multiple asterisk with distributed devstate. This is to enable toggling
		   even then when otherwise the aggregate devicestate would obscure the change.
		   However, we need to force distributed devstate even on single asterisk boxes so to get the desired events. (-DD) */
#ifdef CS_NEW_DEVICESTATE
		ast_enable_distributed_devstate();
		specifier->sub = ast_event_subscribe(AST_EVENT_DEVICE_STATE_CHANGE, sccp_devstateFeatureState_cb, device, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, strdup(buf), AST_EVENT_IE_END);
#endif             
	}
	SCCP_LIST_UNLOCK(&device->devstateSpecifiers);
#endif
}
