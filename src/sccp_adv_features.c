
/*!
 * \file 	sccp_adv_features.c
 * \brief 	SCCP Advanced Features Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \author	Federico Santulli <fsantulli [at] users.sourceforge.net>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
//#include "sccp_adv_features.h"

/*!
 * \brief Dummy Procedure 1
 */
void dummyproc1(void)
{
	/* this is a dummy proc */
	if (0)
		dummyproc2();
}

/*!
 * \brief Dummy Procedure 2
 */
void dummyproc2(void)
{
	/* this is a dummy proc */
	if (0)
		dummyproc1();
}

/*
 * New Provision Section (sccp_provision.c)
 */
 
/*!
 * \brief HTTP Callback Function
 */
/*
static struct ast_str *sccp_prov_http_callback(struct ast_tcptls_session_instance *ser, const struct ast_http_uri *urih, const char *uri, enum ast_http_method method, struct ast_variable *vars, struct ast_variable *headers, int *status, char **title, int *contentlength) {
        // parse uri
        // parse variables
        // match uri to functions:
        //  - welcome
        //  - ask_pin
        //  - ask_line
        //  - ask_button
        //  - ask_confirm
out404:
	*status = 404;
	*title = strdup("Not Found");  
	*contentlength = 0;
	return ast_http_error(404, "Not Found", NULL, "The requested URL was not found on this server.");

out500:
	route = unref_route(route);
	*status = 500;
	*title = strdup("Internal Server Error");
	*contentlength = 0;
        return ast_http_error(500, "Internal Error", NULL, "An internal error has occured.");
}
*/

/*
 *
 */
// ast_http_uri_link(&sccp_prov_uri);
// ast_http_uri_link(&sccp_prov_uri);
/*
static struct ast_http_uri sccp_prov_uri = {
	.callback = sccp_prov_http_callback,
 	.description = "SCCP Phone Provisioning Tool",   
	.uri = "sccp_prov",
	.has_subtree = 1,
	.supports_get = 1,
	.data = NULL,
	.key = __FILE__,
};
*/

/*
config entries:
 - provision=yes		(realtime requirement to save entries)
 - provision_pin=1234
 - provision_tftpdir=/data/tftpboot
 
realtime functions: ast_update_realtime, ast_update2_realtime, ast_store_realtime, ast_destroy_realtime
*/
/*
sccp_prov_ask_pin();
sccp_prov_verify_pin(char *pin);
sccp_prov_ask_line_info();
sccp_prov_handle_line_info(char *username, char *fullname, char *extension);
sccp_prov_ask_button_info();
sccp_prov_handle_button_info(char *action, char *buttonname, char *type, char *options);
sccp_prov_store_device();
sccp_prov_store_line(char *username, char *fullname, char *password, char *extension);
sccp_prov_store_button(char *buttonname, char *type, char *options);
*/