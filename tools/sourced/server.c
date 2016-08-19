/* server.c  -  Resource library  -  Public Domain  -  2016 Mattias Jansson / Rampant Pixels
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
#include <resource/sourced.h>
#include <network/network.h>

#include "server.h"

#define SERVER_MESSAGE_TERMINATE 0
#define SERVER_MESSAGE_CONNECTION 1

struct server_message_t {
	int message;
	void* data;
};

typedef struct server_message_t server_message_t;

static void*
server_serve(void* arg);

static int
server_handle(socket_t* sock);

static int
server_handle_lookup(socket_t* sock, size_t msgsize);

void
server_run(unsigned int port) {
	int slot;
	beacon_t beacon;
	socket_t* sock[2] = { nullptr, nullptr };
	size_t sockets = 0;
	bool terminate = false;
	server_message_t message;
	socket_t local_socket[2];
	thread_t network_thread;

	beacon_initialize(&beacon);
	event_stream_set_beacon(system_event_stream(), &beacon);

	network_address_t** localaddr = network_address_local();
	udp_socket_initialize(&local_socket[0]);
	udp_socket_initialize(&local_socket[1]);
	socket_bind(&local_socket[0], localaddr[0]);
	socket_bind(&local_socket[1], localaddr[0]);

	thread_initialize(&network_thread, server_serve, local_socket, STRING_CONST("serve"), THREAD_PRIORITY_NORMAL, 0);

	/*if (network_supports_ipv4())*/ {
		network_address_t* address = network_address_ipv4_any();
		network_address_ip_set_port(address, port);
		sock[sockets] = tcp_socket_allocate();
		socket_set_beacon(sock[sockets], &beacon);
		if (!socket_bind(sock[sockets], address) ||
			!tcp_socket_listen(sock[sockets])) {
			log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Unable to bind IPv4 socket"));
			socket_deallocate(sock[sockets]);
			sock[sockets] = nullptr;
		}
		else {
			log_infof(HASH_RESOURCE, STRING_CONST("Listening to IPv4 port %u"),
				network_address_ip_port(socket_address_local(sock[sockets])));
			++sockets;
		}
		memory_deallocate(address);
	}
	if (network_supports_ipv6()) {
		network_address_t* address = network_address_ipv6_any();
		network_address_ip_set_port(address, port);
		sock[sockets] = tcp_socket_allocate();
		socket_set_beacon(sock[sockets], &beacon);
		if (!socket_bind(sock[sockets], address) ||
			!tcp_socket_listen(sock[sockets])) {
			log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Unable to bind IPv6 socket"));
			socket_deallocate(sock[sockets]);
			sock[sockets] = nullptr;
		}
		else {
			log_infof(HASH_RESOURCE, STRING_CONST("Listening to IPv6 port %u"),
				network_address_ip_port(socket_address_local(sock[sockets])));
			++sockets;
		}
		memory_deallocate(address);
	}
	
	if (!sockets) {
		log_warn(HASH_RESOURCE, WARNING_UNSUPPORTED, STRING_CONST("No IPv4/IPv6 network connection"));
		terminate = true;
	}
	else {
		thread_start(&network_thread);
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
		else {
			socket_t* listener = sock[slot - 1];
			socket_t* accepted = tcp_socket_accept(listener, 0);
			if (accepted) {
				message.message = SERVER_MESSAGE_CONNECTION;
				message.data = accepted;
				udp_socket_sendto(&local_socket[0], &message, sizeof(message), socket_address_local(&local_socket[1]));
			}
		}
	}

	message.message = SERVER_MESSAGE_TERMINATE;
	udp_socket_sendto(&local_socket[0], &message, sizeof(message), socket_address_local(&local_socket[1]));

	thread_finalize(&network_thread);
	socket_finalize(&local_socket[0]);
	socket_finalize(&local_socket[1]);

	if (sock[0])
		socket_deallocate(sock[0]);
	if (sock[1])
		socket_deallocate(sock[1]);

	beacon_finalize(&beacon);
}

void*
server_serve(void* arg) {
	bool terminate = false;
	server_message_t message;
	socket_t* local_sockets = (socket_t*)arg;
	socket_t* control_source = local_sockets;
	socket_t* control_socket = local_sockets + 1;
	const network_address_t* local_addr;
	network_poll_event_t events[64];

	if (socket_fd(control_socket) == NETWORK_SOCKET_INVALID)
		return nullptr;

	network_poll_t* poll = network_poll_allocate(512);

	local_addr = socket_address_local(control_source);
	network_poll_add_socket(poll, control_socket);

	while (!terminate) {
		size_t ievt;
		size_t count = network_poll(poll, events, sizeof(events) / sizeof(events[0]), NETWORK_TIMEOUT_INFINITE);
		if (!count)
			continue;

		for (ievt = 0; ievt < count; ++ievt) {
			if (events[ievt].socket == control_socket) {
				network_address_t* addr;
				if (udp_socket_recvfrom(control_socket, &message, sizeof(message), &addr) != sizeof(message))
					continue;
				if (!network_address_equal(addr, local_addr))
					continue;
				if (message.message == SERVER_MESSAGE_TERMINATE) {
					terminate = true;
					break;
				}
				if (message.message == SERVER_MESSAGE_CONNECTION) {
					socket_t* sock = message.data;
					socket_set_blocking(sock, false);
					network_poll_add_socket(poll, sock);
				}
			}
			else {
				socket_t* sock = events[ievt].socket;
				if (server_handle(sock) < 0) {
					network_poll_remove_socket(poll, sock);
					socket_deallocate(sock);
				}
			}
		}
	}

	network_poll_deallocate(poll);

	return nullptr;
}

int
server_handle(socket_t* sock) {
	sourced_message_t msg = {
		(uint32_t)sock->data.header.id,
		(uint32_t)sock->data.header.size
	};

	sock->data.header.id = 0;

	if (!msg.id) {
		size_t read = socket_read(sock, &msg, sizeof(msg));
		if (read != sizeof(msg))
			return -1;
	}

	switch (msg.id) {
		case SOURCED_LOOKUP:
			return server_handle_lookup(sock, msg.size);

		case SOURCED_REVERSE_LOOKUP:

		case SOURCED_IMPORT:

		case SOURCED_SET:

		case SOURCED_UNSET:

		case SOURCED_DELETE:

		case SOURCED_LOOKUP_RESULT:
		case SOURCED_REVERSE_LOOKUP_RESULT:
		case SOURCED_IMPORT_RESULT:
		case SOURCED_SET_RESULT:
		case SOURCED_UNSET_RESULT:
		case SOURCED_DELETE_RESULT:
		case SOURCED_NOTIFY_CREATE:
		case SOURCED_NOTIFY_CHANGE:
		case SOURCED_NOTIFY_DELETE:
		default:
			break;
	}

	return -1;
}

int
server_handle_lookup(socket_t* sock, size_t msgsize) {

	if (msgsize > BUILD_MAX_PATHLEN)
		return -1;

	char buffer[BUILD_MAX_PATHLEN];
	size_t read = socket_read(sock, buffer, msgsize);
	if (read == msgsize) {
		string_t path = path_clean(buffer, msgsize, BUILD_MAX_PATHLEN);
		if (!path_is_absolute(buffer, msgsize)) {
			string_const_t base_path = resource_import_base_path();
			path = path_prepend(STRING_ARGS(path), BUILD_MAX_PATHLEN, STRING_ARGS(base_path));
			path = path_absolute(STRING_ARGS(path), BUILD_MAX_PATHLEN);
		}
		log_infof(HASH_RESOURCE, STRING_CONST("Perform lookup of resource: %.*s"),
		          STRING_FORMAT(path));
		resource_signature_t sig = resource_import_lookup(STRING_ARGS(path));
		return sourced_write_lookup_reply(sock, sig.uuid, sig.hash);
	}

	if (read != 0)
		return -1;

	sock->data.header.id = SOURCED_LOOKUP;
	sock->data.header.size = msgsize;
	return 0;
}
