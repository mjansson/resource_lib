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
#include <resource/compiled.h>
#include <network/network.h>

#include "server.h"

#define SERVER_MESSAGE_TERMINATE 0
#define SERVER_MESSAGE_CONNECTION 1
#define SERVER_MESSAGE_BROADCAST_NOTIFY 2

struct server_message_t {
	int message;
	void* data;
	unsigned int id;
	uuid_t uuid;
	hash_t token;
};

typedef struct server_message_t server_message_t;

static void*
server_serve(void* arg);

static int
server_handle(socket_t* sock);

static int
server_handle_open_static(socket_t* sock, size_t msgsize);

static int
server_handle_open_dynamic(socket_t* sock, size_t msgsize);

static int
server_write_stream_to_socket(stream_t* stream, socket_t* sock);

static int
server_broadcast_notify(socket_t** sockets, unsigned int msg, uuid_t uuid, hash_t token);

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
	event_stream_set_beacon(fs_event_stream(), &beacon);
	event_stream_set_beacon(resource_event_stream(), &beacon);

	network_address_t** localaddr = network_address_local();
	udp_socket_initialize(&local_socket[0]);
	udp_socket_initialize(&local_socket[1]);
	socket_bind(&local_socket[0], localaddr[0]);
	socket_bind(&local_socket[1], localaddr[0]);

	thread_initialize(&network_thread, server_serve, local_socket, STRING_CONST("serve"),
	                  THREAD_PRIORITY_NORMAL, 0);

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
			}

			event = nullptr;
			block = event_stream_process(fs_event_stream());
			while ((event = event_next(block, event))) {
				resource_event_handle(event);
			}

			event = nullptr;
			block = event_stream_process(resource_event_stream());
			while ((event = event_next(block, event))) {
				switch (event->id) {
				case RESOURCEEVENT_CREATE:
				case RESOURCEEVENT_MODIFY:
				case RESOURCEEVENT_DELETE:
					message.message = SERVER_MESSAGE_BROADCAST_NOTIFY;
					if (event->id == RESOURCEEVENT_CREATE)
						message.id = COMPILED_NOTIFY_CREATE;
					else if (event->id == RESOURCEEVENT_MODIFY)
						message.id = COMPILED_NOTIFY_MODIFY;
					else if (event->id == RESOURCEEVENT_DELETE)
						message.id = COMPILED_NOTIFY_DELETE;
					message.uuid = resource_event_uuid(event);
					message.token = resource_event_token(event);
					udp_socket_sendto(&local_socket[0], &message, sizeof(message),
					                  socket_address_local(&local_socket[1]));
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
				udp_socket_sendto(&local_socket[0], &message, sizeof(message),
				                  socket_address_local(&local_socket[1]));
			}
		}
	}

	message.message = SERVER_MESSAGE_TERMINATE;
	udp_socket_sendto(&local_socket[0], &message, sizeof(message),
	                  socket_address_local(&local_socket[1]));

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
	socket_t** clients = nullptr;

	if (socket_fd(control_socket) == NETWORK_SOCKET_INVALID)
		return nullptr;

	network_poll_t* poll = network_poll_allocate(512);

	local_addr = socket_address_local(control_source);
	network_poll_add_socket(poll, control_socket);

	while (!terminate) {
		size_t ievt;
		size_t count = network_poll(poll, events, sizeof(events) / sizeof(events[0]),
		                            NETWORK_TIMEOUT_INFINITE);
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
				socket_t* sock;
				switch (message.message) {
				case SERVER_MESSAGE_CONNECTION:
					sock = message.data;
					sock->id = array_size(clients);
					socket_set_blocking(sock, false);
					network_poll_add_socket(poll, sock);
					array_push(clients, sock);
					break;

				case SERVER_MESSAGE_BROADCAST_NOTIFY:
					server_broadcast_notify(clients, message.id, message.uuid, message.token);
					break;
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
					unsigned int client = sock->id;
					array_erase(clients, client); //Swap with last, patch up swapped client
					if (array_size(clients) > client)
						clients[client]->id = client;
					network_poll_remove_socket(poll, sock);
					socket_deallocate(sock);
				}
			}
		}
	}

	network_poll_deallocate(poll);

	return nullptr;
}

static int
server_handle(socket_t* sock) {
	compiled_message_t msg = {
		(uint32_t)sock->data.header.id,
		(uint32_t)sock->data.header.size
	};

	sock->data.header.id = 0;

	if (!msg.id) {
		size_t read = socket_read(sock, &msg, sizeof(msg));
		if (!read)
			return -1;
		if (read != sizeof(msg)) {
			log_infof(HASH_RESOURCE, STRING_CONST("Read partial message header: %" PRIsize " of %" PRIsize),
			          read, sizeof(msg));
			return -1;
		}
	}

	switch (msg.id) {
	case COMPILED_OPEN_STATIC:
		return server_handle_open_static(sock, msg.size);
	case COMPILED_OPEN_DYNAMIC:
		return server_handle_open_dynamic(sock, msg.size);

	case COMPILED_OPEN_STATIC_RESULT:
	case COMPILED_OPEN_DYNAMIC_RESULT:
	default:
		break;
	}

	return -1;
}

static int
server_handle_open_static(socket_t* sock, size_t msgsize) {
	size_t expected_size = sizeof(uuid_t) + sizeof(uint64_t);
	if (msgsize != expected_size)
		return -1;

	compiled_open_static_t readmsg;
	size_t read = socket_read(sock, &readmsg.uuid, expected_size);
	if (read == expected_size) {
		int ret = -1;
		string_const_t uuidstr = string_from_uuid_static(readmsg.uuid);
		log_infof(HASH_RESOURCE, STRING_CONST("Perform read of static resource: %.*s"),
		          STRING_FORMAT(uuidstr));
		stream_t* stream = resource_stream_open_static(readmsg.uuid, readmsg.platform);
		if (stream) {
			compiled_write_open_static_reply(sock, true, stream_size(stream));
			ret = server_write_stream_to_socket(stream, sock);
		}
		else {
			compiled_write_open_static_reply(sock, false, 0);
		}
		return ret;
	}
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial open static message: %" PRIsize " of %"
		                                      PRIsize), read, msgsize);
		return -1;
	}

	sock->data.header.id = COMPILED_OPEN_STATIC;
	sock->data.header.size = msgsize;
	return 0;
}

static int
server_handle_open_dynamic(socket_t* sock, size_t msgsize) {
	size_t expected_size = sizeof(uuid_t) + sizeof(uint64_t);
	if (msgsize != expected_size)
		return -1;

	compiled_open_static_t readmsg;
	size_t read = socket_read(sock, &readmsg.uuid, expected_size);
	if (read == expected_size) {
		int ret = -1;
		string_const_t uuidstr = string_from_uuid_static(readmsg.uuid);
		log_infof(HASH_RESOURCE, STRING_CONST("Perform read of dynamic resource: %.*s"),
		          STRING_FORMAT(uuidstr));
		stream_t* stream = resource_stream_open_dynamic(readmsg.uuid, readmsg.platform);
		if (stream) {
			compiled_write_open_dynamic_reply(sock, true, stream_size(stream));
			ret = server_write_stream_to_socket(stream, sock);
		}
		else {
			compiled_write_open_dynamic_reply(sock, false, 0);
		}
		return ret;
	}
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial open dynamic message: %" PRIsize " of %"
		                                      PRIsize), read, msgsize);
		return -1;
	}

	sock->data.header.id = COMPILED_OPEN_DYNAMIC;
	sock->data.header.size = msgsize;
	return 0;
}

static int
server_write_stream_to_socket(stream_t* stream, socket_t* sock) {
	int ret = 0;
	size_t written = 0;
	const size_t capacity = 4096;
	char* buffer = memory_allocate(HASH_RESOURCE, capacity, 0, MEMORY_PERSISTENT);
	while (!stream_eos(stream)) {
		size_t read = stream_read(stream, buffer, capacity);
		if (read) {
			size_t total = 0;
			do {
				size_t want_write = read - total;
				size_t wrote = socket_write(sock, pointer_offset(buffer, total), want_write);
				if (wrote != want_write) {
					if (socket_state(sock) != SOCKETSTATE_CONNECTED) {
						ret = -1;
						break;
					}
					thread_yield();
				}
				total += wrote;
			}
			while (total != read);

			if (total == read)
				ret = 0;
			written += total;
		}
	}
	memory_deallocate(buffer);
	stream_deallocate(stream);

	log_infof(HASH_RESOURCE, STRING_CONST("Wrote resource stream data: %" PRIsize " (%d)"), written,
	          ret);

	return ret;
}

static int
server_broadcast_notify(socket_t** sockets, unsigned int msg, uuid_t uuid, hash_t token) {
	for (size_t isock = 0, send = array_size(sockets); isock < send; ++isock)
		compiled_write_notify(sockets[isock], msg, uuid, token);
	return 0;
}
