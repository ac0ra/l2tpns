#include <string.h>
#include "l2tpns.h"
#include "plugin.h"

/* set up throttling based on RADIUS reply */

char const *cvs_id = "$Id: autothrottle.c,v 1.10 2004/11/29 02:17:17 bodea Exp $";

int plugin_api_version = PLUGIN_API_VERSION;
struct pluginfuncs *p;

#define THROTTLE_KEY "lcp:interface-config"

int plugin_radius_response(struct param_radius_response *data)
{
	char *t;
	int i = 0;
	int rate;

	if (strncmp(data->key, THROTTLE_KEY, strlen(THROTTLE_KEY)) == 0)
	{
		char *pt = strdup(data->value);
		while ((t = strsep(&pt, " ")) != NULL)
		{
			if (strcmp(t, "serv") == 0)
				i = 1;
			else if (strcmp(t, "o") && i == 1)
				i = 2;
			else if (strcmp(t, "i") && i == 1)
				i = 3;
			else if (i > 1 && (rate = atoi(t)) > 0)
			{
				switch (i)
				{
					case 2: // output
						data->s->throttle_out = rate;
						free(pt);
						p->log(3, p->get_id_by_session(data->s), data->s->tunnel,
							"      Set output throttle rate %dkb/s\n", rate);

						return PLUGIN_RET_OK;

					case 3: //input
						data->s->throttle_in = rate;
						free(pt);
						p->log(3, p->get_id_by_session(data->s), data->s->tunnel,
							"      Set input throttle rate %dkb/s\n", rate);

						return PLUGIN_RET_OK;

					default:
						p->log(1, p->get_id_by_session(data->s), data->s->tunnel,
							"Syntax error in rate limit AV pair: %s=%s\n", data->key, data->value);

						free(pt);
						return PLUGIN_RET_OK;
				}
			}
			else
			{
				free(pt);
				p->log(1, p->get_id_by_session(data->s), data->s->tunnel,
					"Syntax error in rate limit AV pair: %s=%s\n",
					data->key, data->value);

				return PLUGIN_RET_OK;
			}
		}
		free(pt);
	}
	else if (strcmp(data->key, "throttle") == 0)
	{
		if (strcmp(data->value, "yes") == 0)
		{
		    	unsigned long *rate = p->getconfig("throttle_speed", UNSIGNED_LONG);
			if (rate)
			{
				if (*rate)
					p->log(3, p->get_id_by_session(data->s), data->s->tunnel,
						"         Throttling user to %dkb/s\n", *rate);
				else
					p->log(3, p->get_id_by_session(data->s), data->s->tunnel,
						"         Not throttling user (throttle_speed=0)\n");

				data->s->throttle_in = data->s->throttle_out = *rate;
			}
			else
				p->log(1, p->get_id_by_session(data->s), data->s->tunnel,
					"Not throttling user (can't get throttle_speed)\n");
		}
		else if (strcmp(data->value, "no") == 0)
		{
			p->log(3, p->get_id_by_session(data->s), data->s->tunnel,
				"         Not throttling user\n");

			data->s->throttle_in = data->s->throttle_out = 0;
		}
	}

	p->log(4, p->get_id_by_session(data->s), data->s->tunnel,
		"autothrottle module ignoring AV pair %s=%s\n",
		data->key, data->value);

	return PLUGIN_RET_OK;
}

int plugin_init(struct pluginfuncs *funcs)
{
	return ((p = funcs)) ? 1 : 0;
}
