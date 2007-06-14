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
	size_t userlen = 0;
	char *at = "@";
	ptmp = &tmp[0];

   	if (!data->continue_auth) return PLUGIN_RET_STOP;

	f->log(3, 0, 0, "Seeking to append realm: \"%s\"\n", f->getconfig("append_realm", STRING));
	
	//Get the size to copy
	if ((p = strchr(data->username, '@'))) {
		userlen = p - data->username;
	} else {
		userlen = strlen(data->username);
	}
	
	//Copy in the pre-realm part of the username, or all of it
	//if there is no realm
	strncpy(ptmp, data->username, userlen);

	//Add the @ symbol
	strcat(ptmp, at);

	//Copy in the realm
	strncat(ptmp, f->getconfig("append_realm", STRING), MAXUSER - (userlen + 1));
		
	//Let's recreate memory with the correct size
	free(data->username);
	data->username = malloc(MAXUSER);
	
	//Assign this to both the username and the calling station id
	strncpy(data->s->user, ptmp, MAXUSER);
	strncpy(data->username, ptmp, MAXUSER);

	
	f->log(3, 0, 0, "Appended or replaced realm. Username: \"%s\"\n", data->s->user);

	return PLUGIN_RET_OK;
}

int plugin_init(struct pluginfuncs *funcs)
{
    return ((f = funcs)) ? 1 : 0;
}
