/* This file handles the calculation of free traffic  */
#ifdef FREETRAFFIC

#ifndef __FREETRAFFIC_H__
#define __FREETRAFFIC_H__
#define MAXFREENETWORKS	50

typedef struct
{
	//char name[32];
	in_addr_t host;
	in_addr_t mask;
} free_networkt;


extern free_networkt *freetraffic_zones[256][256];	// Array of free traffic networks.
							// Configured same way as ip pools.

extern uint32_t freetraffic_zone_sizes[256][256];	// Amount used of freetraffic zones.


void add_free_network(char *zone, in_addr_t host, in_addr_t mask);
int free_traffic(char *zone, uint8_t *buf, int len);
int remove_free_network(char *zone, in_addr_t host, in_addr_t mask);

void initfreenetworks();
void initfreenetwork(uint8_t x, uint8_t y);

#endif // __FREETRAFFIC_H__
#endif // FREETRAFFIC
