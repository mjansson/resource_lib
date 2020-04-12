/* remote.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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
 * This library is put in the public domain; you can redistribute it and/or modify it without any
 * restrictions.
 *
 */

#include <resource/resource.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

#if RESOURCE_ENABLE_REMOTE_SOURCED || RESOURCE_ENABLE_REMOTE_COMPILED

#include <network/network.h>

#if FOUNDATION_PLATFORM_LINUX || FOUNDATION_PLATFORM_ANDROID
#include <sys/epoll.h>
#elif FOUNDATION_PLATFORM_MACOS || FOUNDATION_PLATFORM_IOS
#include <sys/poll.h>
#endif

#define REMOTE_CONNECT_BACKOFF_MIN (2 * 1000)
#define REMOTE_CONNECT_BACKOFF_MAX (60 * 1000)

#define REMOTE_MESSAGE_NONE 0
#define REMOTE_MESSAGE_TERMINATE 1
#define REMOTE_MESSAGE_WAKEUP 2

#if RESOURCE_ENABLE_REMOTE_SOURCED
#define REMOTE_MESSAGE_LOOKUP 3
#define REMOTE_MESSAGE_READ 4
#define REMOTE_MESSAGE_HASH 5
#define REMOTE_MESSAGE_DEPENDENCIES 6
#define REMOTE_MESSAGE_REVERSE_DEPENDENCIES 7
#endif

#if RESOURCE_ENABLE_REMOTE_COMPILED
#define REMOTE_MESSAGE_OPEN_STATIC 8
#define REMOTE_MESSAGE_OPEN_DYNAMIC 9
#endif

#if RESOURCE_ENABLE_REMOTE_SOURCED
#define REMOTE_MESSAGE_READ_BLOB 10
#endif

typedef struct remote_header_t remote_header_t;
typedef struct remote_message_t remote_message_t;
typedef struct remote_poll_t remote_poll_t;
typedef struct remote_context_t remote_context_t;

struct remote_header_t {
	uint32_t id;
	uint32_t size;
};

struct remote_message_t {
	int message;
	const void* data;
	size_t size;
	uuid_t uuid;
	uint64_t platform;
	hash_t key;
	hash_t checksum;
	void* store;
	size_t capacity;
};

struct remote_poll_t {
	NETWORK_DECLARE_FIXEDSIZE_POLL(2);
};

struct remote_context_t {
	string_const_t url;
	network_poll_t* poll;
	socket_t* remote;
	socket_t* control;
	socket_t* client;
	int (*read)(remote_context_t*, remote_header_t, remote_message_t);
	int (*write)(remote_context_t*, remote_message_t);
};

static void*
resource_remote_comm(void* arg) {
	remote_context_t* context = (remote_context_t*)arg;
	char addrbuf[NETWORK_ADDRESS_NUMERIC_MAX_LENGTH];

	if (socket_fd(context->control) == NETWORK_SOCKET_INVALID)
		return nullptr;
	if (!context->url.length)
		return nullptr;

	network_address_t** address = network_address_resolve(STRING_ARGS(context->url));
	if (!address) {
		log_warnf(HASH_RESOURCE, WARNING_INVALID_VALUE, STRING_CONST("Unable to resolve remote URL: %.*s"),
		          STRING_FORMAT(context->url));
		return nullptr;
	}

	socket_t remote;
	tcp_socket_initialize(&remote);
	socket_set_blocking(&remote, true);
	context->remote = &remote;

	remote_poll_t fixedpoll;
	network_poll_t* poll = (network_poll_t*)&fixedpoll;
	network_poll_initialize(poll, 2);
	network_poll_add_socket(poll, context->control);
	network_poll_add_socket(poll, context->remote);
	context->poll = poll;

	bool terminate = false;
	bool connected = false;
	bool reconnect = true;
	unsigned int backoff = 0;
	unsigned int wait = 0;
	tick_t next_reconnect = 0;
	size_t iaddr = 0;
	size_t lastaddr = 0;

	remote_message_t pending;
	remote_message_t waiting;
	memset(&pending, 0, sizeof(pending));
	memset(&waiting, 0, sizeof(waiting));

	while (!terminate) {
		size_t ievt;
		network_poll_event_t events[64];
		size_t count = network_poll(poll, events, sizeof(events) / sizeof(events[0]), wait);

		for (ievt = 0; ievt < count; ++ievt) {
			if (events[ievt].socket == context->control) {
				const network_address_t* addr;
				remote_message_t message;
				if (udp_socket_recvfrom(context->control, &message, sizeof(message), &addr) != sizeof(message))
					continue;
				if (!network_address_equal(addr, socket_address_local(context->client)))
					continue;
				if ((message.message == REMOTE_MESSAGE_NONE) || (message.message == REMOTE_MESSAGE_WAKEUP)) {
					break;
				}
				if (message.message == REMOTE_MESSAGE_TERMINATE) {
					terminate = true;
					break;
				}
				pending = message;
			} else {
				socket_t* sock = events[ievt].socket;
				if (sock != &remote)
					continue;

				if (events[ievt].event == NETWORKEVENT_CONNECTED) {
					connected = true;
					reconnect = false;
					backoff = 0;

					string_t addrstr =
					    network_address_to_string(addrbuf, sizeof(addrbuf), socket_address_remote(&remote), true);
					log_infof(HASH_RESOURCE, STRING_CONST("Connection completed to remote address: %.*s"),
					          STRING_FORMAT(addrstr));
				} else if ((events[ievt].event == NETWORKEVENT_ERROR) || (events[ievt].event == NETWORKEVENT_HANGUP)) {
					if (connected) {
						string_t addrstr =
						    network_address_to_string(addrbuf, sizeof(addrbuf), socket_address_remote(&remote), true);
						log_warnf(HASH_RESOURCE, WARNING_SUSPICIOUS, STRING_CONST("Disconnected from remote: %.*s"),
						          STRING_FORMAT(addrstr));
					} else {
						string_t addrstr = network_address_to_string(addrbuf, sizeof(addrbuf), address[lastaddr], true);
						log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL,
						          STRING_CONST("Unable to connect to remote: %.*s"), STRING_FORMAT(addrstr));
					}

					connected = false;
					reconnect = true;
				} else if (events[ievt].event == NETWORKEVENT_DATAIN) {
					remote_header_t msg = {(uint32_t)remote.data.header.id, (uint32_t)remote.data.header.size};
					bool had_header = (msg.id != 0);

					remote.data.header.id = 0;

					if (!had_header) {
						size_t read = socket_read(&remote, &msg, sizeof(msg));
						if (read != sizeof(msg)) {
							log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL,
							         STRING_CONST("Failed to read remote message header"));
							socket_close(&remote);
							network_poll_update_socket(poll, &remote);
							connected = false;
							reconnect = true;
						}
					}

					int result = context->read(context, msg, waiting);
					if (result < 0) {
						if (had_header) {
							log_warn(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL,
							         STRING_CONST("Failed to read remote message"));
							socket_close(&remote);
							network_poll_update_socket(poll, &remote);
							connected = false;
							reconnect = true;
						} else {
							remote.data.header.id = msg.id;
							remote.data.header.size = msg.size;
						}
					} else if (result == 0) {
						waiting.message = REMOTE_MESSAGE_NONE;
					}
				}
			}
		}

		if (connected && !waiting.message && pending.message) {
			waiting = pending;
			pending.message = REMOTE_MESSAGE_NONE;

			context->write(context, waiting);
		}

		if (reconnect) {
			if (waiting.message)
				pending = waiting;
			waiting.message = REMOTE_MESSAGE_NONE;

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
				log_infof(HASH_RESOURCE, STRING_CONST("Connecting to remote address: %.*s"), STRING_FORMAT(addrstr));

				if (!socket_connect(&remote, address[iaddr], 0)) {
					log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL,
					          STRING_CONST("Unable to connect to remote: %.*s"), STRING_FORMAT(addrstr));
				} else {
					reconnect = false;
					connected = (socket_state(&remote) == SOCKETSTATE_CONNECTED);
					if (connected) {
						addrstr =
						    network_address_to_string(addrbuf, sizeof(addrbuf), socket_address_remote(&remote), true);
						log_infof(HASH_RESOURCE, STRING_CONST("Connected to remote address: %.*s"),
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
		} else {
			wait = NETWORK_TIMEOUT_INFINITE;
		}
	}

	uint32_t dummy = 0;
	if (waiting.message)
		udp_socket_sendto(context->control, &dummy, sizeof(dummy), socket_address_local(context->client));
	if (pending.message)
		udp_socket_sendto(context->control, &dummy, sizeof(dummy), socket_address_local(context->client));

	socket_finalize(&remote);
	network_poll_finalize(poll);
	network_address_array_deallocate(address);

	return nullptr;
}

#endif

#if RESOURCE_ENABLE_REMOTE_SOURCED

#include <resource/sourced.h>

static bool _sourced_initialized;
static string_t _sourced_url;
static thread_t _sourced_thread;
static socket_t _sourced_client;
static socket_t _sourced_proxy;
static remote_context_t _sourced_context;

static int
resource_sourced_read_lookup_result(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	sourced_lookup_result_t reply;
	log_info(HASH_RESOURCE, STRING_CONST("Read lookup result from remote sourced service"));
	int ret = sourced_read_lookup_reply(context->remote, msg.size, &reply);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_LOOKUP)) {
		resource_signature_t sig = {reply.uuid, reply.hash};
		udp_socket_sendto(context->control, &sig, sizeof(sig), socket_address_local(context->client));
	}
	return ret;
}

static int
resource_sourced_read_read_result(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	sourced_read_result_t* reply = memory_allocate(HASH_RESOURCE, msg.size, 0, MEMORY_PERSISTENT);
	log_info(HASH_RESOURCE, STRING_CONST("Read read result from remote sourced service"));
	int ret = sourced_read_read_reply(context->remote, msg.size, reply);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_READ)) {
		uint32_t status = 0;
		if ((reply->result == SOURCED_OK) && (msg.size >= sizeof(sourced_read_result_t))) {
			sourced_change_t* change = reply->payload;
			resource_source_t* source = waiting.store;
			for (uint32_t ich = 0; ich < reply->changes_count; ++ich, ++change) {
				if (change->flags & RESOURCE_SOURCEFLAG_BLOB)
					resource_source_set_blob(source, change->timestamp, change->hash, change->platform,
					                         change->value.blob.checksum, change->value.blob.size);
				else if (change->flags & RESOURCE_SOURCEFLAG_VALUE)
					resource_source_set(source, change->timestamp, change->hash, change->platform,
					                    pointer_offset(reply->payload, change->value.value.offset),
					                    change->value.value.length);
				else
					resource_source_unset(source, change->timestamp, change->hash, change->platform);
			}
			status = 1;
		}
		udp_socket_sendto(context->control, &status, sizeof(status), socket_address_local(context->client));
	}
	memory_deallocate(reply);

	return ret;
}

static int
resource_sourced_read_hash_result(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	sourced_hash_result_t result;
	log_info(HASH_RESOURCE, STRING_CONST("Read hash result from remote sourced service"));
	int ret = sourced_read_hash_reply(context->remote, msg.size, &result);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_HASH))
		udp_socket_sendto(context->control, &result.hash, sizeof(result.hash), socket_address_local(context->client));
	return ret;
}

static int
resource_sourced_read_dependencies_result(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	uint64_t count = 0;
	log_info(HASH_RESOURCE, STRING_CONST("Read dependencies result from remote sourced service"));
	int ret = sourced_read_dependencies_reply(context->remote, msg.size, waiting.store, waiting.capacity, &count);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_DEPENDENCIES))
		udp_socket_sendto(context->control, &count, sizeof(count), socket_address_local(context->client));
	return ret;
}

static int
resource_sourced_read_reverse_dependencies_result(remote_context_t* context, remote_header_t msg,
                                                  remote_message_t waiting) {
	uint64_t count = 0;
	log_info(HASH_RESOURCE, STRING_CONST("Read reverse dependencies result from remote sourced service"));
	int ret =
	    sourced_read_reverse_dependencies_reply(context->remote, msg.size, waiting.store, waiting.capacity, &count);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_REVERSE_DEPENDENCIES))
		udp_socket_sendto(context->control, &count, sizeof(count), socket_address_local(context->client));
	return ret;
}

static int
resource_sourced_read_read_blob_result(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	log_info(HASH_RESOURCE, STRING_CONST("Read read blob result from remote sourced service"));
	sourced_read_blob_reply_t reply;
	int ret = sourced_read_read_blob_reply(context->remote, msg.size, &reply, waiting.store, waiting.capacity);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_READ_BLOB)) {
		uint32_t status = ((waiting.checksum == reply.checksum) && (waiting.capacity >= reply.size)) ? 1 : 0;
		udp_socket_sendto(context->control, &status, sizeof(status), socket_address_local(context->client));
	}
	return ret;
}

static int
resource_sourced_read_notify(remote_context_t* context, remote_header_t msg) {
	log_info(HASH_RESOURCE, STRING_CONST("Read notify from remote sourced service"));
	sourced_notify_t notify;
	int ret = sourced_read_notify(context->remote, msg.size, &notify);
	if (ret >= 0) {
		switch (msg.id) {
			case SOURCED_NOTIFY_CREATE:
				resource_event_post(RESOURCEEVENT_CREATE, notify.uuid, notify.platform, notify.token);
				break;
			case SOURCED_NOTIFY_MODIFY:
				resource_event_post(RESOURCEEVENT_MODIFY, notify.uuid, notify.platform, notify.token);
				break;
			case SOURCED_NOTIFY_DEPENDS:
				resource_event_post(RESOURCEEVENT_DEPENDS, notify.uuid, notify.platform, notify.token);
				break;
			case SOURCED_NOTIFY_DELETE:
				resource_event_post(RESOURCEEVENT_DELETE, notify.uuid, notify.platform, notify.token);
				break;
		}
		return 1;  // Don't clear waiting message on notifies
	}
	return -1;
}

static int
resource_sourced_read(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	switch (msg.id) {
		case SOURCED_LOOKUP_RESULT:
			return resource_sourced_read_lookup_result(context, msg, waiting);

		case SOURCED_READ_RESULT:
			return resource_sourced_read_read_result(context, msg, waiting);

		case SOURCED_HASH_RESULT:
			return resource_sourced_read_hash_result(context, msg, waiting);

		case SOURCED_DEPENDENCIES_RESULT:
			return resource_sourced_read_dependencies_result(context, msg, waiting);

		case SOURCED_REVERSE_DEPENDENCIES_RESULT:
			return resource_sourced_read_reverse_dependencies_result(context, msg, waiting);

		case SOURCED_READ_BLOB_RESULT:
			return resource_sourced_read_read_blob_result(context, msg, waiting);

		case SOURCED_NOTIFY_CREATE:
		case SOURCED_NOTIFY_MODIFY:
		case SOURCED_NOTIFY_DEPENDS:
		case SOURCED_NOTIFY_DELETE:
			return resource_sourced_read_notify(context, msg);

		default:
			break;
	}
	return -1;
}

static int
resource_sourced_write(remote_context_t* context, remote_message_t waiting) {
	switch (waiting.message) {
		case REMOTE_MESSAGE_LOOKUP:
			log_info(HASH_RESOURCE, STRING_CONST("Write lookup message to remote sourced service"));
			if (sourced_write_lookup(context->remote, waiting.data, waiting.size) < 0) {
				resource_signature_t sig = {uuid_null(), uint256_null()};
				udp_socket_sendto(context->control, &sig, sizeof(sig), socket_address_local(context->client));
			}
			break;

		case REMOTE_MESSAGE_READ:
			log_info(HASH_RESOURCE, STRING_CONST("Write read message to remote sourced service"));
			if (sourced_write_read(context->remote, waiting.uuid) < 0) {
				uint32_t status = 0;
				udp_socket_sendto(context->control, &status, sizeof(status), socket_address_local(context->client));
			}
			break;

		case REMOTE_MESSAGE_HASH:
			log_info(HASH_RESOURCE, STRING_CONST("Write hash message to remote sourced service"));
			if (sourced_write_hash(context->remote, waiting.uuid, waiting.platform) < 0) {
				uint256_t result = uint256_null();
				udp_socket_sendto(context->control, &result, sizeof(result), socket_address_local(context->client));
			}
			break;

		case REMOTE_MESSAGE_DEPENDENCIES:
			log_info(HASH_RESOURCE, STRING_CONST("Write dependencies message to remote sourced service"));
			if (sourced_write_dependencies(context->remote, waiting.uuid, waiting.platform) < 0) {
				uint64_t count = 0;
				udp_socket_sendto(context->control, &count, sizeof(count), socket_address_local(context->client));
			}
			break;

		case REMOTE_MESSAGE_REVERSE_DEPENDENCIES:
			log_info(HASH_RESOURCE, STRING_CONST("Write reverse dependencies message to remote sourced service"));
			if (sourced_write_reverse_dependencies(context->remote, waiting.uuid, waiting.platform) < 0) {
				uint64_t count = 0;
				udp_socket_sendto(context->control, &count, sizeof(count), socket_address_local(context->client));
			}
			break;

		case REMOTE_MESSAGE_READ_BLOB:
			log_info(HASH_RESOURCE, STRING_CONST("Write read blob message to remote sourced service"));
			if (sourced_write_read_blob(context->remote, waiting.uuid, waiting.platform, waiting.key) < 0) {
				uint32_t status = 0;
				udp_socket_sendto(context->control, &status, sizeof(status), socket_address_local(context->client));
			}
			break;

		default:
			break;
	}

	return 0;
}

static void
resource_remote_sourced_initialize(const char* url, size_t length) {
	if (_sourced_initialized)
		return;
	if (!resource_module_config().enable_remote_sourced)
		return;

	network_address_t** localaddr = network_address_local();
	udp_socket_initialize(&_sourced_client);
	udp_socket_initialize(&_sourced_proxy);
	socket_bind(&_sourced_client, localaddr[0]);
	socket_bind(&_sourced_proxy, localaddr[0]);
	socket_set_blocking(&_sourced_client, true);
	network_address_array_deallocate(localaddr);

	_sourced_url = string_clone(url, length);
	_sourced_initialized = true;

	_sourced_context.url = string_to_const(_sourced_url);
	_sourced_context.client = &_sourced_client;
	_sourced_context.control = &_sourced_proxy;
	_sourced_context.read = resource_sourced_read;
	_sourced_context.write = resource_sourced_write;

	thread_initialize(&_sourced_thread, resource_remote_comm, &_sourced_context, STRING_CONST("sourced-client"),
	                  THREAD_PRIORITY_NORMAL, 0);
	thread_start(&_sourced_thread);
}

static void
resource_remote_sourced_finalize(void) {
	if (!_sourced_initialized)
		return;

	_sourced_initialized = false;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_TERMINATE;
	udp_socket_sendto(&_sourced_client, &message, sizeof(message), socket_address_local(&_sourced_proxy));

	thread_finalize(&_sourced_thread);
	string_deallocate(_sourced_url.str);

	socket_finalize(&_sourced_client);
	socket_finalize(&_sourced_proxy);
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
	resource_signature_t nullsig = {uuid_null(), uint256_null()};
	resource_signature_t sig = nullsig;
	if (!_sourced_initialized)
		return sig;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_LOOKUP;
	message.data = path;
	message.size = length;
	if (udp_socket_sendto(&_sourced_client, &message, sizeof(message), socket_address_local(&_sourced_proxy)) !=
	    sizeof(message))
		return sig;

	const network_address_t* addr;
	if (udp_socket_recvfrom(&_sourced_client, &sig, sizeof(sig), &addr) == sizeof(sig))
		return sig;

	return nullsig;
}

uint256_t
resource_remote_sourced_hash(uuid_t uuid, uint64_t platform) {
	uint256_t ret = uint256_null();
	if (!_sourced_initialized)
		return ret;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_HASH;
	message.uuid = uuid;
	message.platform = platform;
	if (udp_socket_sendto(&_sourced_client, &message, sizeof(message), socket_address_local(&_sourced_proxy)) !=
	    sizeof(message))
		return ret;

	const network_address_t* addr;
	if (udp_socket_recvfrom(&_sourced_client, &ret, sizeof(ret), &addr) == sizeof(ret))
		return ret;

	return uint256_null();
}

size_t
resource_remote_sourced_dependencies(uuid_t uuid, uint64_t platform, resource_dependency_t* deps, size_t capacity) {
	if (!_sourced_initialized)
		return 0;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_DEPENDENCIES;
	message.uuid = uuid;
	message.platform = platform;
	message.store = deps;
	message.capacity = capacity;
	if (udp_socket_sendto(&_sourced_client, &message, sizeof(message), socket_address_local(&_sourced_proxy)) !=
	    sizeof(message))
		return 0;

	uint64_t ret;
	const network_address_t* addr;
	if (udp_socket_recvfrom(&_sourced_client, &ret, sizeof(ret), &addr) == sizeof(ret))
		return (size_t)ret;

	return 0;
}

size_t
resource_remote_sourced_reverse_dependencies(uuid_t uuid, uint64_t platform, resource_dependency_t* deps,
                                             size_t capacity) {
	if (!_sourced_initialized)
		return 0;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_REVERSE_DEPENDENCIES;
	message.uuid = uuid;
	message.platform = platform;
	message.store = deps;
	message.capacity = capacity;
	if (udp_socket_sendto(&_sourced_client, &message, sizeof(message), socket_address_local(&_sourced_proxy)) !=
	    sizeof(message))
		return 0;

	uint64_t ret;
	const network_address_t* addr;
	if (udp_socket_recvfrom(&_sourced_client, &ret, sizeof(ret), &addr) == sizeof(ret))
		return (size_t)ret;

	return 0;
}

bool
resource_remote_sourced_read(resource_source_t* source, uuid_t uuid) {
	if (!_sourced_initialized)
		return false;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_READ;
	message.store = source;
	message.uuid = uuid;
	if (udp_socket_sendto(&_sourced_client, &message, sizeof(message), socket_address_local(&_sourced_proxy)) !=
	    sizeof(message))
		return false;

	uint32_t status = 0;
	const network_address_t* addr;
	if (udp_socket_recvfrom(&_sourced_client, &status, sizeof(status), &addr) == sizeof(status))
		return status > 0;

	return false;
}

bool
resource_remote_sourced_read_blob(const uuid_t uuid, hash_t key, uint64_t platform, hash_t checksum, void* data,
                                  size_t capacity) {
	if (!_sourced_initialized)
		return false;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_READ_BLOB;
	message.uuid = uuid;
	message.platform = platform;
	message.key = key;
	message.checksum = checksum;
	message.store = data;
	message.capacity = capacity;
	if (udp_socket_sendto(&_sourced_client, &message, sizeof(message), socket_address_local(&_sourced_proxy)) !=
	    sizeof(message))
		return false;

	uint32_t status = 0;
	const network_address_t* addr;
	if (udp_socket_recvfrom(&_sourced_client, &status, sizeof(status), &addr) == sizeof(status))
		return status > 0;

	return false;
}

#else

string_const_t
resource_remote_sourced(void) {
	return string_empty();
}

void
resource_remote_sourced_connect(const char* url, size_t length) {
	FOUNDATION_UNUSED(url);
	FOUNDATION_UNUSED(length);
}

void
resource_remote_sourced_disconnect(void) {
}

bool
resource_remote_sourced_is_connected(void) {
	return false;
}

resource_signature_t
resource_remote_sourced_lookup(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	resource_signature_t sig = {uuid_null(), uint256_null()};
	return sig;
}

uint256_t
resource_remote_sourced_hash(uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return uint256_null();
}

size_t
resource_remote_sourced_dependencies(uuid_t uuid, uint64_t platform, resource_dependency_t* deps, size_t capacity) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(deps);
	FOUNDATION_UNUSED(capacity);
	return 0;
}

size_t
resource_remote_sourced_reverse_dependencies(uuid_t uuid, uint64_t platform, resource_dependency_t* deps,
                                             size_t capacity) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(deps);
	FOUNDATION_UNUSED(capacity);
	return 0;
}

bool
resource_remote_sourced_read(resource_source_t* source, uuid_t uuid) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(uuid);
	return false;
}

bool
resource_remote_sourced_read_blob(const uuid_t uuid, hash_t key, uint64_t platform, hash_t checksum, void* data,
                                  size_t capacity) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(key);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(checksum);
	FOUNDATION_UNUSED(data);
	FOUNDATION_UNUSED(capacity);
	return false;
}

#endif

#if RESOURCE_ENABLE_REMOTE_COMPILED

#include <resource/compiled.h>

static bool _compiled_initialized;
static string_t _compiled_url;
static thread_t _compiled_thread;
static socket_t _compiled_client;
static socket_t _compiled_proxy;
static remote_context_t _compiled_context;

static stream_vtable_t _compiled_stream_vtable;

typedef struct compiled_stream_t compiled_stream_t;

FOUNDATION_ALIGNED_STRUCT(compiled_stream_t, 8) {
	FOUNDATION_DECLARE_STREAM;
	socket_t* sock;

	size_t total_read;
	size_t stream_size;
};

static void
resource_compiled_stream_finish(compiled_stream_t* stream) {
	if (!stream->sock)
		return;
	network_poll_add_socket(_compiled_context.poll, stream->sock);
	remote_message_t wakeup;
	wakeup.message = REMOTE_MESSAGE_WAKEUP;
	udp_socket_sendto(&_compiled_client, &wakeup, sizeof(wakeup), socket_address_local(&_compiled_proxy));
	stream->sock = 0;
}

static size_t
resource_compiled_stream_read(stream_t* rawstream, void* buffer, size_t size) {
	compiled_stream_t* stream = (compiled_stream_t*)rawstream;
	size_t total = 0;
	if (!stream->sock)
		return total;
	do {
		size_t read = socket_read(stream->sock, pointer_offset(buffer, total), size - total);
		total += read;
		if (total != size) {
			if (socket_state(stream->sock) != SOCKETSTATE_CONNECTED)
				break;
			thread_yield();
		}
	} while (total < size);
	stream->total_read += total;
	if (stream->total_read >= stream->stream_size)
		resource_compiled_stream_finish(stream);
	return total;
}

static bool
resource_compiled_stream_eos(stream_t* rawstream) {
	compiled_stream_t* stream = (compiled_stream_t*)rawstream;
	if (!stream->sock || (stream->total_read >= stream->stream_size))
		return true;
	return (socket_state(stream->sock) != SOCKETSTATE_CONNECTED) && (socket_available_read(stream->sock) == 0);
}

static size_t
resource_compiled_stream_available_read(stream_t* stream) {
	return ((compiled_stream_t*)stream)->stream_size - ((compiled_stream_t*)stream)->total_read;
}

static size_t
resource_compiled_stream_size(stream_t* stream) {
	return ((compiled_stream_t*)stream)->stream_size;
}

static size_t
resource_compiled_stream_tell(stream_t* stream) {
	return ((compiled_stream_t*)stream)->total_read;
}

static tick_t
resource_compiled_stream_last_modified(const stream_t* stream) {
	FOUNDATION_ASSERT(stream);
	FOUNDATION_ASSERT(stream->type == STREAMTYPE_SOCKET);
	return time_current();
}

static stream_t*
resource_compiled_stream_allocate(socket_t* sock, size_t size) {
	compiled_stream_t* stream =
	    memory_allocate(HASH_NETWORK, sizeof(compiled_stream_t), 8, MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);

	// Network streams are always little endian by default
	stream_initialize((stream_t*)stream, BYTEORDER_LITTLEENDIAN);

	stream->type = STREAMTYPE_CUSTOM;
	stream->sequential = 1;
	stream->mode = STREAM_IN | STREAM_BINARY;
	stream->vtable = &_compiled_stream_vtable;
	stream->sock = sock;
	stream->total_read = 0;
	stream->stream_size = size;

	return (stream_t*)stream;
}

static void
resource_compiled_stream_finalize(stream_t* rawstream) {
	compiled_stream_t* stream = (compiled_stream_t*)rawstream;
	resource_compiled_stream_finish(stream);
}

static int
resource_compiled_streams_initialize(void) {
	memset(&_compiled_stream_vtable, 0, sizeof(_compiled_stream_vtable));
	_compiled_stream_vtable.read = resource_compiled_stream_read;
	_compiled_stream_vtable.eos = resource_compiled_stream_eos;
	_compiled_stream_vtable.size = resource_compiled_stream_size;
	_compiled_stream_vtable.tell = resource_compiled_stream_tell;
	_compiled_stream_vtable.lastmod = resource_compiled_stream_last_modified;
	_compiled_stream_vtable.available_read = resource_compiled_stream_available_read;
	_compiled_stream_vtable.finalize = resource_compiled_stream_finalize;
	return 0;
}

static int
resource_compiled_read_open_static_result(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	compiled_open_result_t result;
	log_info(HASH_RESOURCE, STRING_CONST("Read open static result from remote compiled service"));
	int ret = compiled_read_open_static_reply(context->remote, msg.size, &result);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_OPEN_STATIC)) {
		uint64_t size = (result.result == COMPILED_OK) ? result.stream_size : 0;
		if (size > 0)
			network_poll_remove_socket(context->poll, context->remote);
		udp_socket_sendto(context->control, &size, sizeof(uint64_t), socket_address_local(context->client));
	}
	return ret;
}

static int
resource_compiled_read_open_dynamic_result(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	compiled_open_result_t result;
	log_info(HASH_RESOURCE, STRING_CONST("Read open dynamic result from remote compiled service"));
	int ret = compiled_read_open_dynamic_reply(context->remote, msg.size, &result);
	if ((ret >= 0) && (waiting.message == REMOTE_MESSAGE_OPEN_DYNAMIC)) {
		uint64_t size = (result.result == COMPILED_OK) ? result.stream_size : 0;
		if (size > 0)
			network_poll_remove_socket(context->poll, context->remote);
		udp_socket_sendto(context->control, &size, sizeof(uint64_t), socket_address_local(context->client));
	}
	return ret;
}

static int
resource_compiled_read_notify(remote_context_t* context, remote_header_t msg) {
	log_info(HASH_RESOURCE, STRING_CONST("Read notify from remote compiled service"));
	compiled_notify_t notify;
	int ret = compiled_read_notify(context->remote, msg.size, &notify);
	if (ret >= 0) {
		switch (msg.id) {
			case COMPILED_NOTIFY_CREATE:
				resource_event_post(RESOURCEEVENT_CREATE, notify.uuid, notify.platform, notify.token);
				break;
			case COMPILED_NOTIFY_MODIFY:
				resource_event_post(RESOURCEEVENT_MODIFY, notify.uuid, notify.platform, notify.token);
				break;
			case COMPILED_NOTIFY_DEPENDS:
				resource_event_post(RESOURCEEVENT_DEPENDS, notify.uuid, notify.platform, notify.token);
				break;
			case COMPILED_NOTIFY_DELETE:
				resource_event_post(RESOURCEEVENT_DELETE, notify.uuid, notify.platform, notify.token);
				break;
		}
		return 1;  // Don't clear waiting message on notifies
	}
	return -1;
}

static int
resource_compiled_read(remote_context_t* context, remote_header_t msg, remote_message_t waiting) {
	switch (msg.id) {
		case COMPILED_OPEN_STATIC_RESULT:
			return resource_compiled_read_open_static_result(context, msg, waiting);

		case COMPILED_OPEN_DYNAMIC_RESULT:
			return resource_compiled_read_open_dynamic_result(context, msg, waiting);

		case COMPILED_NOTIFY_CREATE:
		case COMPILED_NOTIFY_MODIFY:
		case COMPILED_NOTIFY_DEPENDS:
		case COMPILED_NOTIFY_DELETE:
			return resource_compiled_read_notify(context, msg);

		default:
			break;
	}
	return -1;
}

static int
resource_compiled_write(remote_context_t* context, remote_message_t waiting) {
	switch (waiting.message) {
		case REMOTE_MESSAGE_OPEN_STATIC:
			log_info(HASH_RESOURCE, STRING_CONST("Write open static message to remote compiled service"));
			if (compiled_write_open_static(context->remote, waiting.uuid, waiting.platform) < 0) {
				log_warn(HASH_RESOURCE, WARNING_SUSPICIOUS,
				         STRING_CONST("Failed writing open static message to remote compiled service"));
				size_t size = 0;
				udp_socket_sendto(context->control, &size, sizeof(size), socket_address_local(context->client));
			}
			break;

		case REMOTE_MESSAGE_OPEN_DYNAMIC:
			log_info(HASH_RESOURCE, STRING_CONST("Write open dynamic message to remote compiled service"));
			if (compiled_write_open_dynamic(context->remote, waiting.uuid, waiting.platform) < 0) {
				log_warn(HASH_RESOURCE, WARNING_SUSPICIOUS,
				         STRING_CONST("Failed writing open dynamic message to remote compiled service"));
				size_t size = 0;
				udp_socket_sendto(context->control, &size, sizeof(size), socket_address_local(context->client));
			}
			break;

		default:
			break;
	}

	return 0;
}

static void
resource_remote_compiled_initialize(const char* url, size_t length) {
	if (_compiled_initialized)
		return;
	if (!resource_module_config().enable_remote_compiled)
		return;

	network_address_t** localaddr = network_address_local();
	udp_socket_initialize(&_compiled_client);
	udp_socket_initialize(&_compiled_proxy);
	socket_bind(&_compiled_client, localaddr[0]);
	socket_bind(&_compiled_proxy, localaddr[0]);
	socket_set_blocking(&_compiled_client, true);
	network_address_array_deallocate(localaddr);

	_compiled_url = string_clone(url, length);
	_compiled_initialized = true;

	_compiled_context.url = string_to_const(_compiled_url);
	_compiled_context.client = &_compiled_client;
	_compiled_context.control = &_compiled_proxy;
	_compiled_context.read = resource_compiled_read;
	_compiled_context.write = resource_compiled_write;

	thread_initialize(&_compiled_thread, resource_remote_comm, &_compiled_context, STRING_CONST("compiled-client"),
	                  THREAD_PRIORITY_NORMAL, 0);
	thread_start(&_compiled_thread);
}

static void
resource_remote_compiled_finalize(void) {
	if (!_compiled_initialized)
		return;

	_compiled_initialized = false;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_TERMINATE;
	udp_socket_sendto(&_compiled_client, &message, sizeof(message), socket_address_local(&_compiled_proxy));

	thread_finalize(&_compiled_thread);
	string_deallocate(_compiled_url.str);

	socket_finalize(&_compiled_client);
	socket_finalize(&_compiled_proxy);
}

string_const_t
resource_remote_compiled(void) {
	return string_const(STRING_ARGS(_compiled_url));
}

void
resource_remote_compiled_connect(const char* url, size_t length) {
	resource_remote_compiled_finalize();
	resource_remote_compiled_initialize(url, length);
}

void
resource_remote_compiled_disconnect(void) {
	resource_remote_compiled_finalize();
}

bool
resource_remote_compiled_is_connected(void) {
	return _compiled_url.length > 0;
}

stream_t*
resource_remote_open_static(const uuid_t uuid, uint64_t platform) {
	if (!_compiled_initialized)
		return nullptr;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_OPEN_STATIC;
	message.uuid = uuid;
	message.platform = platform;
	if (udp_socket_sendto(&_compiled_client, &message, sizeof(message), socket_address_local(&_compiled_proxy)) !=
	    sizeof(message))
		return nullptr;

	size_t size = 0;
	const network_address_t* addr;
	if (udp_socket_recvfrom(&_compiled_client, &size, sizeof(size), &addr) == sizeof(size)) {
		if (size > 0)
			return resource_compiled_stream_allocate(_compiled_context.remote, size);
	}

	return nullptr;
}

stream_t*
resource_remote_open_dynamic(const uuid_t uuid, uint64_t platform) {
	if (!_compiled_initialized)
		return nullptr;

	remote_message_t message;
	message.message = REMOTE_MESSAGE_OPEN_DYNAMIC;
	message.uuid = uuid;
	message.platform = platform;
	if (udp_socket_sendto(&_compiled_client, &message, sizeof(message), socket_address_local(&_compiled_proxy)) !=
	    sizeof(message))
		return nullptr;

	size_t size = 0;
	const network_address_t* addr;
	if (udp_socket_recvfrom(&_compiled_client, &size, sizeof(size), &addr) == sizeof(size)) {
		if (size > 0)
			return resource_compiled_stream_allocate(_compiled_context.remote, size);
	}

	return nullptr;
}

#else

string_const_t
resource_remote_compiled(void) {
	return string_empty();
}

void
resource_remote_compiled_connect(const char* url, size_t length) {
	FOUNDATION_UNUSED(url);
	FOUNDATION_UNUSED(length);
}

void
resource_remote_compiled_disconnect(void) {
}

stream_t*
resource_remote_open_static(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return nullptr;
}

stream_t*
resource_remote_open_dynamic(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return nullptr;
}

#endif

int
resource_remote_initialize(void) {
#if RESOURCE_ENABLE_REMOTE_COMPILED
	if (resource_compiled_streams_initialize() < 0)
		return -1;
#endif
	return 0;
}

void
resource_remote_finalize(void) {
#if RESOURCE_ENABLE_REMOTE_SOURCED
	resource_remote_sourced_finalize();
#endif
#if RESOURCE_ENABLE_REMOTE_COMPILED
	resource_remote_compiled_finalize();
#endif
}
