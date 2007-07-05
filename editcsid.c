#include <string.h>
#include "l2tpns.h"
#include "plugin.h"

/* change 61x into 0x for Australian Phone numbers */

char const *cvs_id = "Id: clitousername.c,v 1.0 2007/06/12 010:50:53 rmcleay Exp $";

int plugin_api_version = PLUGIN_API_VERSION;
static struct pluginfuncs *f = 0;

int plugin_pre_auth(struct param_pre_auth *data)
{
	char *p, *tmp;
	char tmp_calling[MAXTEL];
	tmp = &tmp_calling[0];

	if (!data->continue_auth) return PLUGIN_RET_STOP;

	//if (!(data->s->calling[0] == '6' && data->s->calling[1] == '1')) {
	if (!(data->s->calling[0] == '6' && data->s->calling[1] == '1')) {
		f->log(3, 0, 0, "Not altering Calling Station ID from \"%s\"\n", data->s->calling);
		return PLUGIN_RET_OK;
	}

	// Add the 0
	tmp_calling[0] = '0';
	//Miss the 61 (first two digits)
	p = &data->s->calling[2];
	//Copy in the remaining part of the username and null terminate
	strncpy(tmp + 1, p, strlen(data->s->calling) - 2);
	tmp[strlen(data->s->calling)] = 0;

	//Assign this to both the username and the calling station id
	strncpy(data->s->calling, tmp, MAXTEL);

	// It's highly unlikely that the following lines will
	// be required, thanks to MatW's patch.

	// strncpy(data->s->user, tmp, MAXTEL);
	// strncpy(data->username, tmp, MAXTEL);

	f->log(3, 0, 0, "Altering Calling Station ID to \"%s\"\n", tmp);

	return PLUGIN_RET_OK;
}

int plugin_init(struct pluginfuncs *funcs)
{
    return ((f = funcs)) ? 1 : 0;
}
