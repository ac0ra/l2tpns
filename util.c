/* Misc util functions */

char const *cvs_id_util = "$Id: util.c,v 1.9 2004/12/20 07:23:53 bodea Exp $";

#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/mman.h>

#include "l2tpns.h"
#ifdef BGP
#include "bgp.h"
#endif

// format ipv4 addr as a dotted-quad; n chooses one of 4 static buffers
// to use
char *fmtaddr(in_addr_t addr, int n)
{
	static char addrs[4][16];
	struct in_addr in;

	if (n < 0 || n >= 4) return "";
	in.s_addr = addr;
	return strcpy(addrs[n], inet_ntoa(in));
}

void *shared_malloc(unsigned int size)
{
	void * p;
	p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

	if (p == MAP_FAILED)
		p = NULL;

	return p;
}

extern int forked;
extern int udpfd, controlfd, tunfd, snoopfd, ifrfd, cluster_sockfd;
extern int *radfds;

pid_t fork_and_close()
{
	pid_t pid = fork();
	int i;

	if (pid)
		return pid;

	forked++;
	if (config->scheduler_fifo)
	{
		struct sched_param params = {0};
		params.sched_priority = 0;
		if (sched_setscheduler(0, SCHED_OTHER, &params))
		{
			LOG(0, 0, 0, "Error setting scheduler to OTHER after fork: %s\n", strerror(errno));
			LOG(0, 0, 0, "This is probably really really bad.\n");
		}
	}

	signal(SIGPIPE, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGKILL, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	// Close sockets
	if (tunfd != -1)          close(tunfd);
	if (udpfd != -1)          close(udpfd);
	if (controlfd != -1)      close(controlfd);
	if (snoopfd != -1)        close(snoopfd);
	if (ifrfd != -1)          close(ifrfd);
	if (cluster_sockfd != -1) close(cluster_sockfd);
	if (clifd != -1)          close(clifd);

	for (i = 0; radfds && i < config->num_radfds; i++)
		close(radfds[i]);
#ifdef BGP
	for (i = 0; i < BGP_NUM_PEERS; i++)
		if (bgp_peers[i].sock != -1)
			close(bgp_peers[i].sock);
#endif /* BGP */

	return pid;
}
