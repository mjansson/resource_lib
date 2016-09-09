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

static int
server_handle_read(socket_t* sock, size_t msgsize);

static int
server_handle_hash(socket_t* sock, size_t msgsize);

static int
server_handle_dependencies(socket_t* sock, size_t msgsize);

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
	event_stream_set_beacon(resource_event_stream(), &beacon);

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
			const event_block_t* block = event_stream_process(system_event_stream());
			while ((event = event_next(block, event))) {
				switch (event->id) {
				case FOUNDATIONEVENT_TERMINATE:
					terminate = true;
					break;

				default:
					break;
				}

				resource_event_handle(event);
			}

			event = nullptr;
			block = event_stream_process(resource_event_stream());
			while ((event = event_next(block, event))) {
				switch (event->id) {
				case RESOURCEEVENT_CREATE:
					//SOURCED_NOTIFY_CREATE
				case RESOURCEEVENT_MODIFY:
					//SOURCED_NOTIFY_CHANGE
				case RESOURCEEVENT_DELETE:
					//SOURCED_NOTIFY_DELETE
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
				bool disconnect = false;
				if (events[ievt].event == NETWORKEVENT_DATAIN) {
					if (server_handle(sock) < 0)
						disconnect = true;
				}
				else if (events[ievt].event == NETWORKEVENT_ERROR) {
					log_info(HASH_RESOURCE, STRING_CONST("Socket error, closing connection"));
					disconnect = true;
				}
				else if (events[ievt].event == NETWORKEVENT_HANGUP) {
					log_info(HASH_RESOURCE, STRING_CONST("Socket disconnected"));
					disconnect = true;
				}
				if (disconnect) {
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
		if (!read)
			return -1;
		if (read != sizeof(msg)) {
			log_infof(HASH_RESOURCE, STRING_CONST("Read partial message header: %" PRIsize " of %" PRIsize), read, sizeof(msg));
			return -1;
		}
	}

	switch (msg.id) {
		case SOURCED_LOOKUP:
			return server_handle_lookup(sock, msg.size);

		case SOURCED_READ:
			return server_handle_read(sock, msg.size);

		case SOURCED_HASH:
			return server_handle_hash(sock, msg.size);

		case SOURCED_DEPENDENCIES:
			return server_handle_dependencies(sock, msg.size);

		case SOURCED_REVERSE_LOOKUP:

		case SOURCED_IMPORT:

		case SOURCED_GET:
		case SOURCED_SET:

		case SOURCED_UNSET:

		case SOURCED_DELETE:

		case SOURCED_LOOKUP_RESULT:
		case SOURCED_REVERSE_LOOKUP_RESULT:
		case SOURCED_IMPORT_RESULT:
		case SOURCED_READ_RESULT:
		case SOURCED_GET_RESULT:
		case SOURCED_SET_RESULT:
		case SOURCED_UNSET_RESULT:
		case SOURCED_DELETE_RESULT:
		case SOURCED_HASH_RESULT:
		case SOURCED_DEPENDENCIES_RESULT:
		case SOURCED_NOTIFY_CREATE:
		case SOURCED_NOTIFY_CHANGE:
		case SOURCED_NOTIFY_DELETE:
		default:
			break;
	}

	return -1;
}

static int
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
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial lookup message: %" PRIsize " of %" PRIsize), read, msgsize);
		return -1;
	}

	sock->data.header.id = SOURCED_LOOKUP;
	sock->data.header.size = msgsize;
	return 0;
}

static int
server_handle_read(socket_t* sock, size_t msgsize) {
	size_t expected_size = sizeof(uuid_t);
	if (msgsize != expected_size)
		return -1;

	sourced_read_t readmsg;
	size_t read = socket_read(sock, &readmsg.uuid, expected_size);
	if (read == expected_size) {
		int ret;
		resource_source_t source;
		string_const_t uuidstr = string_from_uuid_static(readmsg.uuid);
		resource_source_initialize(&source);
		log_infof(HASH_RESOURCE, STRING_CONST("Perform read of resource: %.*s"),
		          STRING_FORMAT(uuidstr));
		if (resource_source_read(&source, readmsg.uuid))
			ret = sourced_write_read_reply(sock, &source, resource_source_read_hash(readmsg.uuid, 0));
		else
			ret = sourced_write_read_reply(sock, nullptr, uint256_null());
		resource_source_finalize(&source);
		return ret;
	}
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial read message: %" PRIsize " of %" PRIsize), read, msgsize);
		return -1;
	}

	sock->data.header.id = SOURCED_READ;
	sock->data.header.size = msgsize;
	return 0;
}

static int
server_handle_hash(socket_t* sock, size_t msgsize) {
	size_t expected_size = sizeof(uuid_t) + sizeof(uint64_t);
	if (msgsize != expected_size)
		return -1;

	sourced_hash_t hashmsg;
	size_t read = socket_read(sock, &hashmsg.uuid, expected_size);
	if (read == expected_size) {
		if (resource_autoimport_need_update(hashmsg.uuid, hashmsg.platform)) {
			string_const_t uuidstr = string_from_uuid_static(hashmsg.uuid);
			log_debugf(HASH_RESOURCE, STRING_CONST("Reimporting resource %.*s (read hash)"),
			           STRING_FORMAT(uuidstr));
			resource_autoimport(hashmsg.uuid);
		}
		uint256_t hash = resource_source_read_hash(hashmsg.uuid, hashmsg.platform);
		return sourced_write_hash_reply(sock, hash);
	}
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial hash message: %" PRIsize " of %" PRIsize), read, msgsize);
		return -1;
	}

	sock->data.header.id = SOURCED_HASH;
	sock->data.header.size = msgsize;
	return 0;
}

static int
server_handle_dependencies(socket_t* sock, size_t msgsize) {
	size_t expected_size = sizeof(uuid_t) + sizeof(uint64_t);
	if (msgsize != expected_size)
		return -1;

	sourced_dependencies_t depmsg;
	size_t read = socket_read(sock, &depmsg.uuid, expected_size);
	if (read == expected_size) {
		uuid_t localdeps[16];
		uuid_t* deps = localdeps;
		size_t capacity = sizeof(localdeps) / sizeof(localdeps[0]);
		size_t numdeps = resource_source_dependencies(depmsg.uuid, depmsg.platform, localdeps, capacity);
		if (numdeps > capacity) {
			capacity = numdeps;
			deps = memory_allocate(HASH_RESOURCE, capacity * sizeof(uuid_t), 0, MEMORY_PERSISTENT);
			numdeps = resource_source_dependencies(depmsg.uuid, depmsg.platform, deps, capacity);
		}
		int ret = sourced_write_dependencies_reply(sock, deps, numdeps);
		if (deps != localdeps)
			memory_deallocate(deps);
		return ret;
	}
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial dependencies message: %" PRIsize " of %" PRIsize), read, msgsize);
		return -1;
	}

	sock->data.header.id = SOURCED_DEPENDENCIES;
	sock->data.header.size = msgsize;
	return 0;
}


