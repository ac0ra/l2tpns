//vim: sw=8 ts=8 syn=on
/*

This file contains the code that allows us to calculate traffic that is considered to
be free traffic.

 */

#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "freetraffic.h"
#include "l2tpns.h"
#include "util.h"

// The following are the variables for holding the freetraffic zone.  We use a 256x256 array so we can
// directly map from the 2 chars of the pool-id to an array index.  Following the conventions 
// used in l2tpns on initialization we will populate freetraffic_zones with NULLs and the 
// freetraffic_pool_size will be populated with 0's.  A zone that is in use will have a non-zero freetraffic_zone_size
// and a pointer to a linked list.

free_networkt *freetraffic_zones[256][256];
static uint32_t freetraffic_zone_sizes[256][256];	// Amount used of freetraffic zones.

int free_traffic(char* zone, uint8_t *buf, int len) 
{
	in_addr_t src_ip;
	in_addr_t dst_ip;
	free_networkt *network;
	int i;
	int free_network_zone_size;

	if (len < 20) // up to end of destination address
		return 0;

	if ((*buf >> 4) != 4) // IPv4
		return 0;

	src_ip = *(in_addr_t *) (buf + 12);
	dst_ip = *(in_addr_t *) (buf + 16);

	free_network_zone_size = freetraffic_zone_sizes[zone[0]][zone[1]];
	for (i=0; i < free_network_zone_size; i++) {
		network = free_traffic_zones[zone[0]][zone[1]][i];
		if ((dst_ip & network->mask) == (network->host & network->mask))
			return 1;
	}
	return 0;
}

void add_free_network(char* zone, in_addr_t host, in_addr_t mask) 
{
	free_networkt new_network;

	if (!freetraffic_zone_sizes[zone[0]][zone[1]]) {
		free_traffic_zones[zone[0]][zone[1]] = shared_malloc(sizeof(free_networkt) * MAXFREENETWORKS);
	}

	// Do a bitwise AND here to provide a real starting address
	new_network.host = host & mask;
	new_network.mask = mask;

	free_traffic_zones[zone[0]][zone[1]][freetraffic_zone_sizes[zone[0]][zone[1]]++] = new_network; //post increment counter
}

int remove_free_network(char* zone, in_addr_t host, in_addr_t mask)
{

	free_network_zone_size = freetraffic_zone_sizes[zone[0]][zone[1]];
	for (i=0; i < free_network_zone_size; i++) {
		network = free_traffic_zones[zone[0]][zone[1]][i];
		if ((dst_ip & network->mask) == (network->host & network->mask))
			free_traffic_zones[zone[0]][zone[1]][freetraffic_zone_sizes[zone[0]][zone[1]]--] = NULL; //post-decrement counter
	}

	return 0;
}

void initfreenetworks() 
{
	int x,y;
	for (x = 0; x<256;x++) {
		for (y = 0; y<256;y++) {
			memset(freetraffic_zones[x][y], 0, sizeof(free_traffic_zones[x][y]));
			initfreenetwork(x,y);
		}
	}
}

void initfreenetwork(uint8_t x, uint8_t y) {

	FILE *f;
	char *p;
	char buf[4096];
	char filename[1024];
	char zone[MAXZONENAME];

	if (x && y) 
        {
        	snprintf(filename,sizeof(filename)-1,"%s.%c%c",FREETRAFFICFILE,x,y);
		snprintf(zone,sizeof(MAXZONENAME),"%c%c", x,y); 
        } else {
		return;
	}

	if (!(f = fopen(filename, "r")))
	{
		LOG(0, 0, 0, "Can't load free networks file " FREETRAFFICFILE ": %s\n", strerror(errno));
		exit(1);
	}

	freetraffic_zones[x][y] = ll_init();
	
	while (fgets(buf, 4096, f))
	{
		char *pool = buf;
		buf[4095] = 0;	// Force it to be zero terminated/

		if (*buf == '#' || *buf == '\n')
			continue; // Skip comments / blank lines
		if ((p = (char *)strrchr(buf, '\n'))) *p = 0;
		if ((p = (char *)strchr(buf, ':')))
		{
			in_addr_t src;
			*p = '\0';
			src = inet_addr(buf);
			if (src == INADDR_NONE)
			{
				LOG(0, 0, 0, "Invalid free traffic IP %s\n", buf);
				exit(1);
			}
			// This entry is for a specific IP only
			if (src != config->bind_address)
				continue;
			*p = ':';
			pool = p+1;
		}
		if ((p = (char *)strchr(pool, '/')))
		{
			// It's a range
			int numbits = 0;
			in_addr_t start = 0, mask = 0;

			LOG(2, 0, 0, "Adding IP address range %s\n", buf);
			*p++ = 0;
			if (!*p || !(numbits = atoi(p)))
			{
				LOG(0, 0, 0, "Invalid free traffic range %s\n", buf);
				continue;
			}
			start = ntohl(inet_addr(pool));
			mask = (in_addr_t) (pow(2, numbits) - 1) << (32 - numbits);

			// Add a static route for this pool
			LOG(5, 0, 0, "Adding IP Range as free traffic zone %s/%u\n",
				fmtaddr(htonl(start), 0), 32 + mask);

			add_free_network(zone, start, mask);
		}
		else
		{
			// It's a single ip address
			add_free_network(zone, ntohl(inet_addr(pool)), inet_addr("255.255.255.255"));
		}
	}
	fclose(f);
}
