/* server.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Rampant Pixels is always available at
 *
 * https://github.com/rampantpixels/resource_lib
 *
 * The foundation library source code maintained by Rampant Pixels is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <foundation/foundation.h>
#include <resource/resource.h>
#include <network/network.h>

#include "server.h"

void
server_run(unsigned int port) {
	int slot;
	beacon_t beacon;
	socket_t* sock_ipv4 = nullptr;
	socket_t* sock_ipv6 = nullptr;
	bool terminate = false;

	beacon_initialize(&beacon);
	event_stream_set_beacon(system_event_stream(), &beacon);

	/*if (network_supports_ipv4())*/ {
		network_address_t* address = network_address_ipv4_any();
		network_address_ip_set_port(address, port);
		sock_ipv4 = tcp_socket_allocate();
		if (!socket_bind(sock_ipv4, address) ||
			!tcp_socket_listen(sock_ipv4)) {
			log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Unable to bind IPv4 socket"));
			socket_deallocate(sock_ipv4);
			sock_ipv4 = nullptr;
		}
		else {
			log_infof(HASH_RESOURCE, STRING_CONST("Listening to IPv4 port %u"),
				network_address_ip_port(socket_address_local(sock_ipv4)));
		}
		memory_deallocate(address);
	}
	if (network_supports_ipv6()) {
		network_address_t* address = network_address_ipv6_any();
		network_address_ip_set_port(address, port);
		sock_ipv6 = tcp_socket_allocate();
		if (!socket_bind(sock_ipv6, address) ||
			!tcp_socket_listen(sock_ipv6)) {
			log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Unable to bind IPv6 socket"));
			socket_deallocate(sock_ipv6);
			sock_ipv6 = nullptr;
		}
		else {
			log_infof(HASH_RESOURCE, STRING_CONST("Listening to IPv6 port %u"),
				network_address_ip_port(socket_address_local(sock_ipv6)));
		}
		memory_deallocate(address);
	}
	
	if (sock_ipv4)
		beacon_add(&beacon, socket_fd(sock_ipv4));
	if (sock_ipv6)
		beacon_add(&beacon, socket_fd(sock_ipv6));
	if (!sock_ipv4 && !sock_ipv6) {
		log_warn(HASH_RESOURCE, WARNING_UNSUPPORTED, STRING_CONST("No IPv4/IPv6 network connection"));
		terminate = true;
	}

	while (!terminate && ((slot = beacon_wait(&beacon)) >= 0)) {
		system_process_events();
		if (slot == 0) {
			event_t* event = nullptr;
			event_block_t* const block = event_stream_process(system_event_stream());
			while ((event = event_next(block, event))) {
				switch (event->id) {
				case FOUNDATIONEVENT_TERMINATE:
					terminate = true;
					break;

				default:
					break;
				}
			}
		}
		else if ((slot == 1) && sock_ipv4) {

		}
		else {

		}
	}

	//Requests handled (B = broadcast)
	
	// --> lookup <path>   
	// <-- <uuid>
	
	// --> reverse <uuid>
	// <-- <path>

	// --> import <path>|<uuid>
	// <-- <uuid> <flags>
	// <B- <notify-create> <uuid> (if new)
	// <B- <notify-change> <uuid> (if reimported)

	// file change and autoimport performed
	// <B- <notify-change> <uuid>

	// --> set <uuid> <key> <value>
	// <B- <notify-change> <uuid>

	// --> delete <uuid>
	// <-- <result>
	// <B- <notify-delete> <uuid>

	socket_deallocate(sock_ipv4);
	socket_deallocate(sock_ipv6);
	beacon_finalize(&beacon);
}
