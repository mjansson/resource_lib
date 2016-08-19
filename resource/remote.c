/* remote.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/resource.h>
#include <resource/sourced.h>

#include <foundation/foundation.h>

#if RESOURCE_ENABLE_REMOTE_SOURCED

#include <network/network.h>

static bool _sourced_initialized;
static bool _sourced_connected;
static string_t _sourced_url;
static thread_t _sourced_thread;
static socket_t _sourced_control[2];

#define REMOTE_CONNECT_BACKOFF_MIN (2 * 1000)
#define REMOTE_CONNECT_BACKOFF_MAX (60 * 1000)

#define REMOTE_MESSAGE_NONE 0
#define REMOTE_MESSAGE_TERMINATE 1
#define REMOTE_MESSAGE_LOOKUP 2

#define REMOTE_REPLY_LOOKUP 2

struct remote_message_t {
	int message;
	const void* data;
	size_t size;
};

struct remote_poll_t {
	NETWORK_DECLARE_POLL(2);
};

typedef struct remote_message_t remote_message_t;
typedef struct remote_poll_t remote_poll_t;

static void*
resource_remote_comm(void* arg) {
	socket_t* local_sockets = (socket_t*)arg;
	socket_t* control_source = local_sockets;
	socket_t* control_socket = local_sockets + 1;
	char addrbuf[NETWORK_ADDRESS_NUMERIC_MAX_LENGTH];

	if (socket_fd(control_socket) == NETWORK_SOCKET_INVALID)
		return nullptr;
	if (!_sourced_url.length)
		return nullptr;

	network_address_t** address = network_address_resolve(STRING_ARGS(_sourced_url));
	if (!address) {
		log_warnf(HASH_RESOURCE, WARNING_INVALID_VALUE,
		          STRING_CONST("Unable to resolve remote sourced URL: %.*s"),
		          STRING_FORMAT(_sourced_url));
		return nullptr;
	}

	socket_t remote;
	tcp_socket_initialize(&remote);
	socket_set_blocking(&remote, false);

	remote_poll_t fixedpoll;
	network_poll_t* poll = (network_poll_t*)&fixedpoll;
	network_poll_initialize(poll, 2);
	network_poll_add_socket(poll, control_socket);
	network_poll_add_socket(poll, &remote);

	bool terminate = false;
	bool connected = false;
	bool reconnect = true;
	unsigned int backoff = 0;
	unsigned int wait = 0;
	tick_t next_reconnect = 0;
	size_t iaddr = 0;
	size_t lastaddr = 0;

	remote_message_t pending = {REMOTE_MESSAGE_NONE, 0, 0};
	remote_message_t waiting = {REMOTE_MESSAGE_NONE, 0, 0};

	while (!terminate) {
		size_t ievt;
		network_poll_event_t events[64];
		size_t count = network_poll(poll, events, sizeof(events) / sizeof(events[0]), wait);

		for (ievt = 0; ievt < count; ++ievt) {
			if (events[ievt].socket == control_socket) {
				network_address_t* addr;
				remote_message_t message;
				if (udp_socket_recvfrom(control_socket, &message, sizeof(message), &addr) != sizeof(message))
					continue;
				if (!network_address_equal(addr, socket_address_local(control_source)))
					continue;
				if (message.message == REMOTE_MESSAGE_TERMINATE) {
					terminate = true;
					break;
				}
				else if (message.message == REMOTE_MESSAGE_LOOKUP) {
					pending = message;
				}
			}
			else {
				socket_t* sock = events[ievt].socket;
				if (sock != &remote)
					continue;

				if (events[ievt].event == NETWORKEVENT_CONNECTED) {
					connected = true;
					reconnect = false;
					backoff = 0;

					string_t addrstr = network_address_to_string(addrbuf, sizeof(addrbuf),
					                                             socket_address_remote(&remote), true);
					log_infof(HASH_RESOURCE, STRING_CONST("Connected to remote sourced address: %.*s"),
					          STRING_FORMAT(addrstr));
				}
				else if ((events[ievt].event == NETWORKEVENT_ERROR) ||
				         (events[ievt].event == NETWORKEVENT_HANGUP)) {
					if (connected) {
						string_t addrstr = network_address_to_string(addrbuf, sizeof(addrbuf),
						                                             socket_address_remote(&remote), true);
						log_warnf(HASH_RESOURCE, WARNING_SUSPICIOUS,
						          STRING_CONST("Disconnected from remote sourced: %.*s"),
						          STRING_FORMAT(addrstr));
					}
					else {
						string_t addrstr = network_address_to_string(addrbuf, sizeof(addrbuf),
						                                             address[lastaddr], true);
						log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL,
						          STRING_CONST("Unable to connect to remote sourced: %.*s"),
						          STRING_FORMAT(addrstr));
					}

					if (waiting.message)
						pending = waiting;
					connected = false;
					reconnect = true;
				}
				else if (events[ievt].event == NETWORKEVENT_DATAIN) {
					sourced_message_t msg = {
						(uint32_t)remote.data.header.id,
						(uint32_t)remote.data.header.size
					};

					remote.data.header.id = 0;

					if (!msg.id) {
						size_t read = socket_read(&remote, &msg, sizeof(msg));
						if (read != sizeof(msg)) {
							socket_close(&remote);
							network_poll_update_socket(poll, &remote);
							connected = false;
							reconnect = true;
						}
					}
					
					switch (msg.id) {
						case SOURCED_LOOKUP_RESULT:
							{
								sourced_lookup_result_t reply;
								log_info(HASH_RESOURCE, STRING_CONST("Read lookup result from remote sourced service"));
								if (sourced_read_lookup_reply(&remote, &reply) < 0) {
									socket_close(&remote);
									network_poll_update_socket(poll, &remote);
									connected = false;
									reconnect = true;
								}
								else if (waiting.message == REMOTE_MESSAGE_LOOKUP) {
									resource_signature_t sig = {reply.uuid, reply.hash};
									udp_socket_sendto(control_socket, &sig, sizeof(sig), socket_address_local(control_source));
									waiting.message = REMOTE_MESSAGE_NONE;
								}
							}
							break;

						default:
							break;
					}
				}
			}
		}

		if (connected && !waiting.message && pending.message) {
			waiting = pending;
			pending.message = REMOTE_MESSAGE_NONE;

			switch (waiting.message) {
			case REMOTE_MESSAGE_LOOKUP:
				log_info(HASH_RESOURCE, STRING_CONST("Write lookup message to remote sourced service"));
				if (sourced_write_lookup(&remote, waiting.data, waiting.size) < 0) {
					resource_signature_t sig = {uuid_null(), uint256_null()};
					udp_socket_sendto(control_socket, &sig, sizeof(sig), socket_address_local(control_source));
					waiting.message = REMOTE_MESSAGE_NONE;
				}
				break;

			default:
				break;
			}
		}

		if (reconnect) {
			if (time_system() > next_reconnect) {
				if (!backoff)
					backoff = REMOTE_CONNECT_BACKOFF_MIN + random32_range(0, 1000);
				else if (backoff < REMOTE_CONNECT_BACKOFF_MAX)
					backoff *= 2;
				if (backoff > REMOTE_CONNECT_BACKOFF_MAX)
					backoff = REMOTE_CONNECT_BACKOFF_MAX;

				connected = false;
				next_reconnect = time_system() + backoff;

				string_t addrstr = network_address_to_string(addrbuf, sizeof(addrbuf), address[iaddr], true);
				log_infof(HASH_RESOURCE, STRING_CONST("Connecting to remote sourced address: %.*s"),
				          STRING_FORMAT(addrstr));

				if (!socket_connect(&remote, address[iaddr], 0)) {
					log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL,
					          STRING_CONST("Unable to connect to remote sourced: %.*s"),
					          STRING_FORMAT(addrstr));
				}
				else {
					reconnect = false;
					connected = (socket_state(&remote) == SOCKETSTATE_CONNECTED);
					if (connected) {
						addrstr = network_address_to_string(addrbuf, sizeof(addrbuf), socket_address_remote(&remote), true);
						log_infof(HASH_RESOURCE, STRING_CONST("Connected to remote sourced address: %.*s"),
						          STRING_FORMAT(addrstr));
						backoff = 0;
					}
				}

				network_poll_update_socket(poll, &remote);

				lastaddr = iaddr;
				iaddr = (iaddr + 1) % array_size(address);
			}

			tick_t nextwait = next_reconnect - time_system();
			if (nextwait < 0)
				nextwait = 0;
			wait = (unsigned int)nextwait;
		}
		else {
			wait = NETWORK_TIMEOUT_INFINITE;
		}
	}

	if (pending.message) {
		switch (pending.message) {
		case REMOTE_MESSAGE_LOOKUP:
			{
				resource_signature_t sig = {uuid_null(), uint256_null()};
				udp_socket_sendto(control_socket, &sig, sizeof(sig), socket_address_local(control_source));
			}
			break;
		}
	}
	if (waiting.message) {
		switch (waiting.message) {
		case REMOTE_MESSAGE_LOOKUP:
			{
				resource_signature_t sig = {uuid_null(), uint256_null()};
				udp_socket_sendto(control_socket, &sig, sizeof(sig), socket_address_local(control_source));
			}
			break;
		}
	}

	socket_finalize(&remote);
	network_poll_finalize(poll);
	network_address_array_deallocate(address);

	return nullptr;
}

static void
resource_remote_sourced_initialize(const char* url, size_t length) {
	if (_sourced_initialized)
		return;

	network_address_t** localaddr = network_address_local();
	udp_socket_initialize(&_sourced_control[0]);
	udp_socket_initialize(&_sourced_control[1]);
	socket_bind(&_sourced_control[0], localaddr[0]);
	socket_bind(&_sourced_control[1], localaddr[0]);
	socket_set_blocking(&_sourced_control[0], true);

	_sourced_url = string_clone(url, length);
	_sourced_initialized = true;

	thread_initialize(&_sourced_thread, resource_remote_comm, _sourced_control,
	                  STRING_CONST("sourced-client"), THREAD_PRIORITY_NORMAL, 0);
	thread_start(&_sourced_thread);
}

static void
resource_remote_sourced_finalize(void) {
	if (_sourced_initialized) {
		_sourced_initialized = false;

		remote_message_t message;
		message.message = REMOTE_MESSAGE_TERMINATE;
		udp_socket_sendto(&_sourced_control[0], &message, sizeof(message),
		                  socket_address_local(&_sourced_control[1]));

		thread_finalize(&_sourced_thread);
		string_deallocate(_sourced_url.str);

		socket_finalize(&_sourced_control[0]);
		socket_finalize(&_sourced_control[1]);
	}
}

string_const_t
resource_remote_sourced(void) {
	return string_const(STRING_ARGS(_sourced_url));
}

void
resource_remote_sourced_connect(const char* url, size_t length) {
	resource_remote_sourced_finalize();
	resource_remote_sourced_initialize(url, length);
}

void
resource_remote_sourced_disconnect(void) {
	resource_remote_sourced_finalize();
}

bool
resource_remote_sourced_is_connected(void) {
	return _sourced_url.length > 0;
}

resource_signature_t
resource_remote_sourced_lookup(const char* path, size_t length) {
	resource_signature_t sig = {uuid_null(), uint256_null()};
	if (!_sourced_initialized)
		return sig;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_LOOKUP;
	message.data = path;
	message.size = length;
	if ((udp_socket_sendto(&_sourced_control[0], &message, sizeof(message),
	                       socket_address_local(&_sourced_control[1])) != sizeof(message)) ||
	        !_sourced_initialized)
		return sig;

	network_address_t* addr;
	udp_socket_recvfrom(&_sourced_control[0], &sig, sizeof(sig), &addr);

	return sig;
}

#else

resource_signature_t
resource_remote_sourced_lookup(const char* path, size_t length) {
	resource_signature_t sig = {uuid_null(), uint256_null()};
	return sig;
}

#endif

#if RESOURCE_ENABLE_REMOTE_COMPILED

static string_t _remote_compiled;

string_const_t
resource_remote_compiled(void) {
	return string_const(STRING_ARGS(_remote_compiled));
}

void
resource_remote_compiled_connect(const char* url, size_t length) {
	string_deallocate(_remote_compiled.str);
	_remote_compiled = string_clone(url, length);
}

void
resource_remote_compiled_disconnect(void) {
	string_deallocate(_remote_compiled.str);
	_remote_compiled = (string_t){0, 0};
}

stream_t*
resource_remote_open_static(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return 0;
}

stream_t*
resource_remote_open_dynamic(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return 0;
}

#endif

int
resource_remote_initialize(void) {
	return 0;
}

void
resource_remote_finalize(void) {
#if RESOURCE_ENABLE_REMOTE_SOURCED
	resource_remote_sourced_finalize();
#endif
}
