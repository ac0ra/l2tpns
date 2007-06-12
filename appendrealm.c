#include <string.h>
#include "l2tpns.h"
#include "plugin.h"

/* Append a realm to the username */

char const *cvs_id = "Id: appendrealm.c,v 1.0 2007/06/12 010:50:53 rmcleay Exp $";

int plugin_api_version = PLUGIN_API_VERSION;
static struct pluginfuncs *f = 0;

int plugin_pre_auth(struct param_pre_auth *data)
{
    	char *p, *ptmp;
	char tmp[MAXUSER];
	ptmp = &tmp[0];
	size_t userlen = 0;

   	if (!data->continue_auth) return PLUGIN_RET_STOP;


	
	//Get the size to copy
	if ((p = strchr(data->username, '@'))) {
		userlen = p - data->username;
	} else {
		userlen = strlen(data->username);
	}
	
	//Copy in the pre-realm part of the username, or all of it
	//if there is no realm
	strncpy(ptmp, data->username, userlen);

	//Copy in the realm
	//ptmp is beginning of tmp var, p is at the null 0 after pre-realm
	p = ptmp + strlen(ptmp);
	strncpy(p, config->append_realm, strlen(config->append_realm));
	
	//Assign this to both the username and the calling station id
	strncpy(data->username, ptmp, MAXUSER);
	
	f->log(3, 0, 0, "Appended or replaced realm. Username: \"%s\"\n", data->username);

	return PLUGIN_RET_OK;
}

int plugin_init(struct pluginfuncs *funcs)
{
    return ((f = funcs)) ? 1 : 0;
}
