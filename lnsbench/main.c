/*
 * This program will rapidly setup tunnels and then as many connections as possible
 * using these tunnels.
 *
 * It is not multi-threaded, as it is expected that jMeter will take care of that.
 */

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	int tunnels;
	int connections;
	char* prefix;
} args_t;

args_t args;

int main (int argc, char **argv) {
	int c;
	int i;

	while ((c = getopt (argc, argv, "t:c:p:h")) != -1) {
		switch (c) {
			case 't':
				args.tunnels = atoi(optarg);
				break;
			case 'c':
				args.connections = atoi(optarg);
				break;
			case 'p':
				args.prefix = optarg;
				break;
			case 'h':
				print_usage();
				return 0;
				break;
			case '?':
				print_usage();
				return 1;
		}
	}

	// OK, now we create the tunnels:
	for (i=0;i<args.tunnels;i++) {
		l2tp_tunnel_create_1(struct l2tp_api_tunnel_msg_data params, int *clnt_res,  CLIENT *clnt);


	}

	// OK, now we create the connections:
	for (i=0;i<args.connections;i++) {
	}
}
