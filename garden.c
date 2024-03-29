#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <netinet/ip6.h>
#include "dhcp6.h"
#include "l2tpns.h"
#include "plugin.h"
#include "control.h"

/* walled garden */

int plugin_api_version = PLUGIN_API_VERSION;
static struct pluginfuncs *f = 0;

static int iam_master = 0;	// We're all slaves! Slaves I tell you!

char *up_commands[] = {
    "/sbin/iptables -t nat -N %s >/dev/null 2>&1",		// Create a chain that all gardened users will go through
    "/sbin/iptables -t nat -F %s",
    ". " PLUGINCONF "/build-%s",				// Populate with site-specific DNAT rules
    "/sbin/iptables -t nat -N %s_users >/dev/null 2>&1",		// Empty chain, users added/removed by garden_session
    "/sbin/iptables -t nat -F %s_users",
    "/sbin/iptables -t nat -A PREROUTING -j %s_users",		// DNAT any users on the garden_users chain
    NULL,
};

char *down_commands[] = {
    "/sbin/iptables -t nat -F PREROUTING",
    "/sbin/iptables -t nat -F %s_users",
    "/sbin/iptables -t nat -X %s_users",
    "/sbin/iptables -t nat -F %s",
    "/sbin/iptables -t nat -X %s",
    // Not sure if this is valid anymore.  Commenting this out.
    // "rmmod iptable_nat",	// Should also remove ip_conntrack, but
				// doing so can take hours...  literally.
				// If a master is re-started as a slave,
				// either rmmod manually, or reboot.
    NULL,
};

// Hardened garden security commands
char *up_garden_security_commands[] = {
    "/sbin/iptables -N garden_security >/dev/null 2>&1", // garden_security will be populate if set garden_hardened_security is set to 1
    "/sbin/iptables -F garden_security",                 // Flush in case chain existed (copy'n'paste from what has been done in the past)
    "/sbin/iptables -A FORWARD -j garden_security",	 // Add garden_security as a subchain of FORWARD
    NULL,
};

char *down_garden_security_commands[] = {
    "/sbin/iptables -F garden_security",
    "/sbin/iptables -D FORWARD -j garden_security",
    "/sbin/iptables -X garden_security",
    NULL,
};

#define F_UNGARDEN	0
#define F_GARDEN	1
#define F_CLEANUP	2

int garden_session(sessiont *s, int flag, char *newuser);

int plugin_post_auth(struct param_post_auth *data)
{
	// If he doesn't have credentials (wrong login/password)
	// let's see what can still do for this user...
    if (!data->auth_allowed)
	{
		int *defaultgarden = f->getconfig("default_garden", INT);
		/* if default garden is enabled then let's accept this user
		 * and throw him in the default garden otherwise... see ya
		 * Note: Backward compatibility/behaviour is broken /!\
		 *       default garden is now disabled by default. To enable
		 *       it you must explicitly set in the main configuration:
		 *         set default_garden 1
		 */
		if (*defaultgarden != 0) 
		{
			f->log(3, f->get_id_by_session(data->s), data->s->tunnel,
			"Walled Garden allowing login (default garden)\n");

			data->auth_allowed = 1;
			data->s->walled_garden = 1;
      		strncpy(data->s->walled_garden_name, "garden",7);
		} 
	}
    return PLUGIN_RET_OK;
}

int plugin_new_session(struct param_new_session *data)
{
    if (!iam_master)
	return PLUGIN_RET_OK;	// Slaves don't do walled garden processing.

    if (data->s->walled_garden)
	garden_session(data->s, F_GARDEN, 0);

    return PLUGIN_RET_OK;
}

int plugin_kill_session(struct param_new_session *data)
{
    if (!iam_master)
	return PLUGIN_RET_OK;	// Slaves don't do walled garden processing.

    if (data->s->walled_garden)
	garden_session(data->s, F_CLEANUP, 0);

    return PLUGIN_RET_OK;
}

char *plugin_control_help[] = {
    "  garden USER|SID [gardenname]                Put user into the walled garden",
    "  ungarden SID [USER]                         Release session from garden",
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

    if (strcmp(data->argv[0], "garden") && strcmp(data->argv[0], "ungarden"))
	return PLUGIN_RET_OK; // not for us

    if (!iam_master)
	return PLUGIN_RET_NOTMASTER;

    flag = data->argv[0][0] == 'g' ? F_GARDEN : F_UNGARDEN;

    if (data->argc < 2 || data->argc > 4 || (data->argc > 3 && flag == F_GARDEN))
    {
	data->response = NSCTL_RES_ERR;
	data->additional = flag == F_GARDEN
	    ? "requires username or session id"
	    : "requires session id and optional username";

	return PLUGIN_RET_STOP;
    }

    if (!(session = strtol(data->argv[1], &end, 10)) || *end)
    {
	if (flag)
	    session = f->get_session_by_username(data->argv[1]);
	else
	    session = 0; // can't ungarden by username
    }

    if (session)
	s = f->get_session_by_id(session);

    if (!s || !s->ip)
    {
	data->response = NSCTL_RES_ERR;
	data->additional = "session not found";
	return PLUGIN_RET_STOP;
    }

    if (s->walled_garden == flag)
    {
	data->response = NSCTL_RES_ERR;
	data->additional = flag ? "already in walled garden" : "not in walled garden";
	return PLUGIN_RET_STOP;
    }

    if(data->argc > 2) { 
      strncpy(s->walled_garden_name, data->argv[2],MAXGARDEN);
      f->log(5, session, s->tunnel, "Using garden of %s", s->walled_garden_name);
    } else if ((data->argc > 1) && flag == F_GARDEN) {
      strncpy(s->walled_garden_name, "garden",7);
      f->log(5, session, s->tunnel, "Using garden of %s (default)", s->walled_garden_name);
    }

    garden_session(s, flag, data->argc > 2 ? data->argv[1] : 0);
    f->session_changed(session);

    data->response = NSCTL_RES_OK;
    data->additional = 0;

    return PLUGIN_RET_STOP;
}

int plugin_become_master(void)
{
    int i;
    char gardens[MAXGARDEN * MAXGARDENCOUNT];
    char *tok;
    char temporary[MAXGARDEN + 200]; 

    strncpy(gardens,f->getconfig("gardens", STRING),MAXGARDEN * MAXGARDENCOUNT);

    iam_master = 1;	// We just became the master. Wow!

    for (i = 0; up_commands[i] && *up_commands[i]; i++)
    {
	f->log(3, 0, 0, "Running %s\n", up_commands[i]);
	if (-1 == system(up_commands[i])) f->log(0, 0, 0, "error command %s\n", up_commands[i]);
    }

   // Should we create new chains to harden security in the gardens ?
   int *garden_hardened_security = f->getconfig("garden_hardened_security", INT);
   if ( *garden_hardened_security != 0 ) {
      for (i = 0; up_garden_security_commands[i] && *up_garden_security_commands[i]; i++)
      {
         f->log(3, 0, 0, "Running %s\n", up_garden_security_commands[i]);
         system(up_garden_security_commands[i]);
      }
   }

    return PLUGIN_RET_OK;
}

// Called for each active session after becoming master
int plugin_new_session_master(sessiont *s)
{	
    if (s->walled_garden)
	garden_session(s, F_GARDEN, 0);

    return PLUGIN_RET_OK;
}

int garden_session(sessiont *s, int flag, char *newuser)
{
    char cmd[2048];
    sessionidt sess;
	int status;

    if (!s) return 0;
    if (!s->opened) return 0;

    sess = f->get_id_by_session(s);

    // Required to block inbound traffic towards garden if garden_hardened_security = 1 in startup_config
    int *garden_hardened_security = f->getconfig("garden_hardened_security", INT);
    char cmd2[256];

    if (flag == F_GARDEN)
    {

	f->log(2, sess, s->tunnel, "Garden user %s (%s) with name of %s \n", s->user,
	    f->fmtaddr(htonl(s->ip), 0), s->walled_garden_name);
	#ifdef ISEEK_CONTROL_MESSAGE
	f->log(1, sess, s->tunnel, "iseek-control-message garden %s %d/%d %s %s\n", s->user, s->rx_connect_speed, s->tx_connect_speed, f->fmtaddr(htonl(s->ip), 0), s->walled_garden_name);
	#endif

	snprintf(cmd, sizeof(cmd),
                 "/sbin/iptables -t nat -A %s_users -s %s -j %s",
                 s->walled_garden_name,
                 f->fmtaddr(htonl(s->ip), 0),
                 s->walled_garden_name);

	f->log(3, sess, s->tunnel, "%s\n", cmd);
	system(cmd);
    status = system(cmd);

	if ( *garden_hardened_security != 0 )
	{
		// HARDENING our gardens so no inbound traffic can reach a user in a garden.
		// We want to block any incoming traffic that is not established by 10.13.10.1
		//iptables -A garden_security -o tun0 -d 10.13.10.1/32 -m state --state ESTABLISHED -j ACCEPT
		//iptables -A garden_security -o tun0 -d 10.13.10.1/32 -j DROP
		// Tom - 31/7/13
		snprintf(cmd2, sizeof(cmd2),
			 "/sbin/iptables -A garden_security -o tun0 -d %s/32 -m state --state ESTABLISHED -j ACCEPT && /sbin/iptables -A garden_security -o tun0 -d %s/32 -j DROP",
			 f->fmtaddr(htonl(s->ip), 0),
			 f->fmtaddr(htonl(s->ip), 0));
		f->log(3, sess, s->tunnel, "%s\n", cmd2);
		system(cmd2);
	}

	s->walled_garden = 1;
    }
    else
    {
	sessionidt other;
	int count = 40;

	// Normal User
	f->log(2, sess, s->tunnel, "Un-Garden user %s (%s)\n", s->user, f->fmtaddr(htonl(s->ip), 0));
	#ifdef ISEEK_CONTROL_MESSAGE
	f->log(1, sess, s->tunnel, "iseek-control-message ungarden %s %d/%d %s %s\n", s->user, s->rx_connect_speed, s->tx_connect_speed, f->fmtaddr(htonl(s->ip), 0), s->walled_garden_name);
	#endif
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
	    f->sessionkill(other, "Duplicate session when user released from walled garden", TERM_ADMIN_RESET);
	}

	/* Clean up counters */
#ifdef FREETRAFFIC
	s->fcin = s->fcout = 0;
	s->fcin_delta = s->fcout_delta = 0;
	s->fcin_wrap = s->fcout_wrap = 0;
#endif //freetraffic
	s->pin = s->pout = 0;
	s->cin = s->cout = 0;
	s->cin_delta = s->cout_delta = 0;
	s->cin_wrap = s->cout_wrap = 0;

	snprintf(cmd, sizeof(cmd),
	    "iptables -t nat -D %s_users -s %s -j %s",
             s->walled_garden_name,
             f->fmtaddr(htonl(s->ip), 0),
             s->walled_garden_name);

	f->log(3, sess, s->tunnel, "%s\n", cmd);

	if ( *garden_hardened_security != 0 )
	{
		// Removing rules
		//iptables -D garden_security -o tun0 -d 10.13.10.1/32 -m state --state ESTABLISHED -j ACCEPT
		//iptables -D garden_security -o tun0 -d 10.13.10.1/32 -j DROP
		// Tom - 31/7/13
		snprintf(cmd2, sizeof(cmd2),
			 "/sbin/iptables -D garden_security -o tun0 -d %s/32 -m state --state ESTABLISHED -j ACCEPT && /sbin/iptables -D garden_security -o tun0 -d %s/32 -j DROP",
			 f->fmtaddr(htonl(s->ip), 0),
			 f->fmtaddr(htonl(s->ip), 0));
		f->log(3, sess, s->tunnel, "%s\n", cmd2);
		system(cmd2);
	}
	while (--count)
	{
	    status = system(cmd);
	    if (WEXITSTATUS(status) != 0) break;

            // Note: This while loop is ugly if I understand it correctly. It will
            // try 40 times to remove the rule until iptables whinges because the
            // rule does not exist (which mean it will return exit code != 0)
            // Why 40 times (int count=40) ? Could have been 100 times or 10...
            // I reckon it is maybe designed that way to remove multiple identical
            // iptables rules that may have been created twice.
            // Anyway, I will do the same for the moment for my extra rules.
            // Tom - 5/4/13
	    int status2 = system(cmd2);
	    if (WEXITSTATUS(status2) != 0) break;
	}

	s->walled_garden = 0;
	memset(s->walled_garden_name,0,sizeof(s->walled_garden_name));

	if (flag != F_CLEANUP)
	{
	    /* OK, we're up! */
	    uint16_t r = f->radiusnew(f->get_id_by_session(s));
	    if (r) f->radiussend(r, RADIUSSTART);
	}
    }

    return 1;
}

// Take down the ip tables firewall rules for the natting.

void go_down(void)
{
    int i;
    char gardens[MAXGARDEN * MAXGARDENCOUNT];
    char *tok;
    char temporary[MAXGARDEN + 200]; 

    strncpy(gardens,f->getconfig("gardens", STRING),MAXGARDEN * MAXGARDENCOUNT);

    if (gardens[0] == 0)
      strncpy(gardens,"garden",7);
    
    tok = strtok(gardens,",");

    while (tok != NULL) {
      for (i = 0; down_commands[i] && *down_commands[i]; i++)
      {
	sprintf(temporary,down_commands[i],tok);
        f->log(3, 0, 0, "Running %s\n", temporary);
        system(temporary);
      }
      tok = strtok(NULL,",");
    }

   // Did we hardened security in the gardens ?
   int *garden_hardened_security = f->getconfig("garden_hardened_security", INT);
   if ( *garden_hardened_security != 0 ) {
      for (i = 0; down_garden_security_commands[i] && *down_garden_security_commands[i]; i++)
      {
         f->log(3, 0, 0, "Running %s\n", temporary,down_garden_security_commands[i]);
         system(down_garden_security_commands[i]);
      }
   }
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
	    if (-1 == system(down_commands[i])) f->log(0, 0, 0, "error command %s\n", down_commands[i]);
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
	if (-1 == system(down_commands[i])) f->log(0, 0, 0, "error command %s\n", down_commands[i]);
    }
}
