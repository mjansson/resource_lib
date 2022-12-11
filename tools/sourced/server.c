/* server.c  -  Resource library  -  Public Domain  -  2016 Mattias Jansson
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/resource_lib
 *
 * The foundation library source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <foundation/foundation.h>
#include <resource/resource.h>
#include <resource/sourced.h>
#include <network/network.h>
#include <blake3/blake3.h>

#include "server.h"

#define SERVER_MESSAGE_TERMINATE 0
#define SERVER_MESSAGE_CONNECTION 1
#define SERVER_MESSAGE_BROADCAST_NOTIFY 2

struct server_message_t {
	int message;
	void* data;
	unsigned int id;
	uuid_t uuid;
	uint64_t platform;
	hash_t token;
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

static int
server_handle_read_blob(socket_t* sock, size_t msgsize);

static int
server_broadcast_notify(socket_t** sockets, unsigned int msg, uuid_t uuid, uint64_t platform, hash_t token);

void
server_run(unsigned int port) {
	int slot;
	beacon_t beacon;
	socket_t* sock[2] = {nullptr, nullptr};
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

	thread_initialize(&network_thread, server_serve, local_socket, STRING_CONST("serve"), THREAD_PRIORITY_NORMAL, 0);

	/*if (network_supports_ipv4())*/ {
		network_address_ipv4_t ipv4_addr;
		network_address_t* address = network_address_ipv4_initialize(&ipv4_addr);
		network_address_ip_set_port(address, port);
		sock[sockets] = tcp_socket_allocate();
		socket_set_beacon(sock[sockets], &beacon);
		if (!socket_bind(sock[sockets], address) || !tcp_socket_listen(sock[sockets])) {
			log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Unable to bind IPv4 socket"));
			socket_deallocate(sock[sockets]);
			sock[sockets] = nullptr;
		} else {
			log_infof(HASH_RESOURCE, STRING_CONST("Listening to IPv4 port %u"),
			          network_address_ip_port(socket_address_local(sock[sockets])));
			++sockets;
		}
	}
	if (network_supports_ipv6()) {
		network_address_ipv6_t ipv6_addr;
		network_address_t* address = network_address_ipv6_initialize(&ipv6_addr);
		network_address_ip_set_port(address, port);
		sock[sockets] = tcp_socket_allocate();
		socket_set_beacon(sock[sockets], &beacon);
		if (!socket_bind(sock[sockets], address) || !tcp_socket_listen(sock[sockets])) {
			log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Unable to bind IPv6 socket"));
			socket_deallocate(sock[sockets]);
			sock[sockets] = nullptr;
		} else {
			log_infof(HASH_RESOURCE, STRING_CONST("Listening to IPv6 port %u"),
			          network_address_ip_port(socket_address_local(sock[sockets])));
			++sockets;
		}
	}

	if (!sockets) {
		log_warn(HASH_RESOURCE, WARNING_UNSUPPORTED, STRING_CONST("No IPv4/IPv6 network connection"));
		terminate = true;
	} else {
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
					case RESOURCEEVENT_DEPENDS:
					case RESOURCEEVENT_DELETE:
						message.message = SERVER_MESSAGE_BROADCAST_NOTIFY;
						if (event->id == RESOURCEEVENT_CREATE)
							message.id = SOURCED_NOTIFY_CREATE;
						else if (event->id == RESOURCEEVENT_MODIFY)
							message.id = SOURCED_NOTIFY_MODIFY;
						else if (event->id == RESOURCEEVENT_DEPENDS)
							message.id = SOURCED_NOTIFY_DEPENDS;
						else if (event->id == RESOURCEEVENT_DELETE)
							message.id = SOURCED_NOTIFY_DELETE;
						message.uuid = resource_event_uuid(event);
						message.platform = resource_event_platform(event);
						message.token = resource_event_token(event);
						udp_socket_sendto(&local_socket[0], &message, sizeof(message),
						                  socket_address_local(&local_socket[1]));
						break;

					default:
						break;
				}
			}
		} else {
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
	socket_t** clients = nullptr;

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
				const network_address_t* addr;
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
						server_broadcast_notify(clients, message.id, message.uuid, message.platform, message.token);
						break;
				}
			} else {
				socket_t* sock = events[ievt].socket;
				bool disconnect = false;
				if (events[ievt].event == NETWORKEVENT_DATAIN) {
					if (server_handle(sock) < 0)
						disconnect = true;
				} else if (events[ievt].event == NETWORKEVENT_ERROR) {
					log_info(HASH_RESOURCE, STRING_CONST("Socket error, closing connection"));
					disconnect = true;
				} else if (events[ievt].event == NETWORKEVENT_HANGUP) {
					log_info(HASH_RESOURCE, STRING_CONST("Socket disconnected"));
					disconnect = true;
				}
				if (disconnect) {
					unsigned int client = sock->id;
					array_erase(clients, client);  // Swap with last, patch up swapped client
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
	sourced_message_t msg = {(uint32_t)sock->data.header.id, (uint32_t)sock->data.header.size};

	sock->data.header.id = 0;

	if (!msg.id) {
		size_t read = socket_read(sock, &msg, sizeof(msg));
		if (!read)
			return -1;
		if (read != sizeof(msg)) {
			log_infof(HASH_RESOURCE, STRING_CONST("Read partial message header: %" PRIsize " of %" PRIsize), read,
			          sizeof(msg));
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

		case SOURCED_READ_BLOB:
			return server_handle_read_blob(sock, msg.size);

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
		case SOURCED_READ_BLOB_RESULT:
		case SOURCED_NOTIFY_CREATE:
		case SOURCED_NOTIFY_MODIFY:
		case SOURCED_NOTIFY_DEPENDS:
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
		log_infof(HASH_RESOURCE, STRING_CONST("Perform lookup of resource: %.*s"), STRING_FORMAT(path));
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
		log_infof(HASH_RESOURCE, STRING_CONST("Perform read of resource: %.*s"), STRING_FORMAT(uuidstr));
		if (resource_autoimport_need_update(readmsg.uuid, 0)) {
			uuidstr = string_from_uuid_static(readmsg.uuid);
			log_debugf(HASH_RESOURCE, STRING_CONST("Reimporting resource %.*s (read)"), STRING_FORMAT(uuidstr));
			resource_autoimport(readmsg.uuid);
		}
		if (resource_source_read(&source, readmsg.uuid)) {
			ret = sourced_write_read_reply(sock, &source, resource_source_hash(readmsg.uuid, 0));
			log_infof(HASH_RESOURCE, STRING_CONST("  read resource successfully, wrote reply"));
		} else {
			ret = sourced_write_read_reply(sock, nullptr, blake3_hash_null());
			log_infof(HASH_RESOURCE, STRING_CONST("  failed reading resource, wrote reply"));
		}
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
			log_debugf(HASH_RESOURCE, STRING_CONST("Reimporting resource %.*s (read hash)"), STRING_FORMAT(uuidstr));
			resource_autoimport(hashmsg.uuid);
		}
		blake3_hash_t hash = resource_source_hash(hashmsg.uuid, hashmsg.platform);
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
		resource_dependency_t localdeps[8];
		resource_dependency_t* deps = localdeps;
		size_t capacity = sizeof(localdeps) / sizeof(localdeps[0]);
		size_t numdeps = resource_source_dependencies(depmsg.uuid, depmsg.platform, localdeps, capacity);
		if (numdeps > capacity) {
			capacity = numdeps;
			deps = memory_allocate(HASH_RESOURCE, capacity * sizeof(resource_dependency_t), 0, MEMORY_PERSISTENT);
			numdeps = resource_source_dependencies(depmsg.uuid, depmsg.platform, deps, capacity);
		}
		int ret = sourced_write_dependencies_reply(sock, deps, numdeps);
		if (deps != localdeps)
			memory_deallocate(deps);
		return ret;
	}
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial dependencies message: %" PRIsize " of %" PRIsize), read,
		          msgsize);
		return -1;
	}

	sock->data.header.id = SOURCED_DEPENDENCIES;
	sock->data.header.size = msgsize;
	return 0;
}

static int
server_handle_read_blob(socket_t* sock, size_t msgsize) {
	size_t expected_size = sizeof(uuid_t) + sizeof(uint64_t) * 2;
	if (msgsize != expected_size)
		return -1;

	sourced_read_blob_t readmsg;
	size_t read = socket_read(sock, &readmsg.uuid, expected_size);
	if (read == expected_size) {
		int ret = -1;
		resource_source_t source;
		resource_source_initialize(&source);
		string_const_t uuidstr = string_from_uuid_static(readmsg.uuid);
		log_infof(HASH_RESOURCE, STRING_CONST("Perform read of resource blob: %.*s %" PRIx64), STRING_FORMAT(uuidstr),
		          readmsg.key);
		if (resource_autoimport_need_update(readmsg.uuid, readmsg.platform)) {
			uuidstr = string_from_uuid_static(readmsg.uuid);
			log_debugf(HASH_RESOURCE, STRING_CONST("Reimporting resource %.*s (read blob)"), STRING_FORMAT(uuidstr));
			resource_autoimport(readmsg.uuid);
		}
		if (resource_source_read(&source, readmsg.uuid)) {
			resource_change_t* blobchange = resource_source_get(&source, readmsg.key, readmsg.platform);
			if (blobchange && (blobchange->flags & RESOURCE_SOURCEFLAG_BLOB)) {
				size_t size = blobchange->value.blob.size;
				void* blob = memory_allocate(HASH_RESOURCE, size, 0, MEMORY_PERSISTENT);
				if (resource_source_read_blob(readmsg.uuid, readmsg.key, readmsg.platform,
				                              blobchange->value.blob.checksum, blob, size))
					ret = sourced_write_read_blob_reply(sock, blobchange->value.blob.checksum, blob, size);
				else
					ret = sourced_write_read_blob_reply(sock, 0, nullptr, 0);
				memory_deallocate(blob);
			}
		}
		resource_source_finalize(&source);
		return ret;
	}
	if (read != 0) {
		log_infof(HASH_RESOURCE, STRING_CONST("Read partial read blob message: %" PRIsize " of %" PRIsize), read,
		          msgsize);
		return -1;
	}

	sock->data.header.id = SOURCED_READ_BLOB;
	sock->data.header.size = msgsize;
	return 0;
}

static int
server_broadcast_notify(socket_t** sockets, unsigned int msg, uuid_t uuid, uint64_t platform, hash_t token) {
	for (size_t isock = 0, send = array_size(sockets); isock < send; ++isock)
		sourced_write_notify(sockets[isock], msg, uuid, platform, token);
	return 0;
}
