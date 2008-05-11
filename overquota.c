#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "l2tpns.h"
#include "plugin.h"
#include "control.h"

/* walled overquota */

int plugin_api_version = PLUGIN_API_VERSION;
static struct pluginfuncs *f = 0;

static int iam_master = 0;	// We're all slaves! Slaves I tell you!

char *up_commands[] = {
    "iptables -t nat -N overquota >/dev/null 2>&1",		// Create a chain that all overquotaed users will go through
    "iptables -t nat -F overquota",
    ". " PLUGINCONF "/build-overquota",				// Populate with site-specific DNAT rules
    "iptables -t nat -N overquota_users >/dev/null 2>&1",		// Empty chain, users added/removed by overquota_session
    "iptables -t nat -F overquota_users",
    "iptables -t nat -A PREROUTING -j overquota_users",		// DNAT any users on the overquota_users chain
    "sysctl -w net.ipv4.netfilter.ip_conntrack_max=51200000"	// lots of entries
    	     " net.ipv4.netfilter.ip_conntrack_tcp_timeout_established=18000 >/dev/null", // 5hrs
    NULL,
};

char *down_commands[] = {
    "iptables -t nat -F PREROUTING",
    "iptables -t nat -F overquota_users",
    "iptables -t nat -X overquota_users",
    "iptables -t nat -F overquota",
    "iptables -t nat -X overquota",
    "rmmod iptable_nat",	// Should also remove ip_conntrack, but
				// doing so can take hours...  literally.
				// If a master is re-started as a slave,
				// either rmmod manually, or reboot.
    NULL,
};

#define F_UNOVERQUOTA	0
#define F_OVERQUOTA	1
#define F_CLEANUP	2

int overquota_session(sessiont *s, int flag, char *newuser);

int plugin_post_auth(struct param_post_auth *data)
{
    // Ignore if user authentication was successful
    // In the case of the radius Wall-Authenticated-User, vendor-supplied
    // attribute, auth_allowed will be 1 anyway, as will be overquota_garden.
    if (data->auth_allowed)
	return PLUGIN_RET_OK;

    f->log(1, f->get_id_by_session(data->s), data->s->tunnel,
	"OverQuota user allowing login\n");

    data->auth_allowed = 1;
    data->s->overquota_garden = 1;
    return PLUGIN_RET_OK;
}

int plugin_new_session(struct param_new_session *data)
{
    if (!iam_master)
	return PLUGIN_RET_OK;	// Slaves don't do walled overquota processing.

    if (data->s->overquota_garden)
	overquota_session(data->s, F_OVERQUOTA, 0);

    return PLUGIN_RET_OK;
}

int plugin_kill_session(struct param_new_session *data)
{
    if (!iam_master)
	return PLUGIN_RET_OK;	// Slaves don't do walled overquota processing.

    if (data->s->overquota_garden)
	overquota_session(data->s, F_CLEANUP, 0);

    return PLUGIN_RET_OK;
}

char *plugin_control_help[] = {
    "  overquota USER|SID                             Put user into the overquota garden",
    "  unoverquota SID [USER]                         Release session from overquota garden",
    0
};

int plugin_control(struct param_control *data)
{
    sessionidt session;
    sessiont *s = 0;
    int flag;
    char *end;

    if (data->argc < 1)
	return PLUGIN_RET_OK;

    if (strcmp(data->argv[0], "overquota") && strcmp(data->argv[0], "unoverquota"))
	return PLUGIN_RET_OK; // not for us

    if (!iam_master)
	return PLUGIN_RET_NOTMASTER;

    flag = data->argv[0][0] == 'g' ? F_OVERQUOTA : F_UNOVERQUOTA;

    if (data->argc < 2 || data->argc > 3 || (data->argc > 2 && flag == F_OVERQUOTA))
    {
	data->response = NSCTL_RES_ERR;
	data->additional = flag == F_OVERQUOTA
	    ? "requires username or session id"
	    : "requires session id and optional username";

	return PLUGIN_RET_STOP;
    }

    if (!(session = strtol(data->argv[1], &end, 10)) || *end)
    {
	if (flag)
	    session = f->get_session_by_username(data->argv[1]);
	else
	    session = 0; // can't unoverquota by username
    }

    if (session)
	s = f->get_session_by_id(session);

    if (!s || !s->ip)
    {
	data->response = NSCTL_RES_ERR;
	data->additional = "session not found";
	return PLUGIN_RET_STOP;
    }

    if (s->overquota_garden == flag)
    {
	data->response = NSCTL_RES_ERR;
	data->additional = flag ? "already in overquota garden" : "not in overquota garden";
	return PLUGIN_RET_STOP;
    }

    overquota_session(s, flag, data->argc > 2 ? data->argv[2] : 0);
    f->session_changed(session);

    data->response = NSCTL_RES_OK;
    data->additional = 0;

    return PLUGIN_RET_STOP;
}

int plugin_become_master(void)
{
    int i;
    iam_master = 1;	// We just became the master. Wow!

    for (i = 0; up_commands[i] && *up_commands[i]; i++)
    {
	f->log(3, 0, 0, "Running %s\n", up_commands[i]);
	system(up_commands[i]);
    }

    return PLUGIN_RET_OK;
}

// Called for each active session after becoming master
int plugin_new_session_master(sessiont *s)
{	
    if (s->overquota_garden)
	overquota_session(s, F_OVERQUOTA, 0);

    return PLUGIN_RET_OK;
}

int overquota_session(sessiont *s, int flag, char *newuser)
{
    char cmd[2048];
    sessionidt sess;

    if (!s) return 0;
    if (!s->opened) return 0;

    sess = f->get_id_by_session(s);
    if (flag == F_OVERQUOTA)
    {
	f->log(2, sess, s->tunnel, "OverQuota user %s (%s)\n", s->user,
	    f->fmtaddr(htonl(s->ip), 0));

	snprintf(cmd, sizeof(cmd),
	    "iptables -t nat -A overquota_users -s %s -j overquota",
	    f->fmtaddr(htonl(s->ip), 0));

	f->log(3, sess, s->tunnel, "%s\n", cmd);
	system(cmd);
	s->overquota_garden = 1;
    }
    else
    {
	sessionidt other;
	int count = 40;

	// Normal User
	f->log(2, sess, s->tunnel, "Remove Overquota for user %s (%s)\n", s->user, f->fmtaddr(htonl(s->ip), 0));
	if (newuser)
	{
	    snprintf(s->user, MAXUSER, "%s", newuser);
	    f->log(2, sess, s->tunnel, "  Setting username to %s\n", s->user);
	}

	// Kick off any duplicate usernames
	// but make sure not to kick off ourself
	if (s->ip && !s->die && (other = f->get_session_by_username(s->user)) &&
	    s != f->get_session_by_id(other))
	{
	    f->sessionkill(other,
		"Duplicate session when user released from overquota");
	}

	/* Clean up counters */
	s->pin = s->pout = 0;
	s->cin = s->cout = 0;
	s->cin_delta = s->cout_delta = 0;
	s->cin_wrap = s->cout_wrap = 0;

	snprintf(cmd, sizeof(cmd),
	    "iptables -t nat -D overquota_users -s %s -j overquota",
	    f->fmtaddr(htonl(s->ip), 0));

	f->log(3, sess, s->tunnel, "%s\n", cmd);
	while (--count)
	{
	    int status = system(cmd);
	    if (WEXITSTATUS(status) != 0) break;
	}

	s->overquota_garden = 0;

	if (flag != F_CLEANUP)
	{
	    /* OK, we're up! */
	    uint16_t r = f->radiusnew(f->get_id_by_session(s));
	    if (r) f->radiussend(r, RADIUSSTART);
	}
    }

    return 1;
}

int plugin_init(struct pluginfuncs *funcs)
{
    FILE *tables;
    int found_nat = 0;

    if (!funcs)
	return 0;

    f = funcs;

    if ((tables = fopen("/proc/net/ip_tables_names", "r")))
    {
	char buf[1024];
	while (fgets(buf, sizeof(buf), tables) && !found_nat)
	    found_nat = !strcmp(buf, "nat\n");

	fclose(tables);
    }

    /* master killed/crashed? */
    if (found_nat)
    {
	int i;
	for (i = 0; down_commands[i] && *down_commands[i]; i++)
	{
	    f->log(3, 0, 0, "Running %s\n", down_commands[i]);
	    system(down_commands[i]);
	}
    }

    return 1;
}

void plugin_done()
{
    int i;

    if (!iam_master)	// Never became master. nothing to do.
	return;

    for (i = 0; down_commands[i] && *down_commands[i]; i++)
    {
	f->log(3, 0, 0, "Running %s\n", down_commands[i]);
	system(down_commands[i]);
    }
}
