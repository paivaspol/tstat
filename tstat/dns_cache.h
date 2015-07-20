/*
 *
 * Copyright (c) 2001
 *	Politecnico di Torino.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For bug report and other information please visit Tstat site:
 * http://tstat.polito.it
 *
 * Tstat is deeply based on TCPTRACE. The authors would like to thank
 * Shawn Ostermann for the development of TCPTRACE.
 *
*/

#include "dnscache/DNSCache.h"
#include "dnscache/DNSEntry.h"

struct DNS_data* get_dns_entry(
		unsigned long int client_ip,
		unsigned long int server_ip);

unsigned char* reverse_lookup(unsigned long int client_ip, unsigned long int server_ip);

/*
unsigned long int dns_server(unsigned long int client_ip, unsigned long int server_ip);
*/
