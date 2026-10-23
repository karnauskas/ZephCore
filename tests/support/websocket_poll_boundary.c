#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
enum { ZSOCK_POLLIN = 1, ZSOCK_POLLOUT = 2, ZSOCK_POLLHUP = 4 };
enum { ZFD_IOCTL_POLL_PREPARE, ZFD_IOCTL_POLL_UPDATE, ZFD_IOCTL_POLL_OFFLOAD,
	ZFD_IOCTL_SET_LOCK };
#define CONFIG_ZVFS_POLL_MAX 4
struct zsock_pollfd { int fd; short events, revents; };
struct websocket_context { int real_sock; struct { size_t count; } recv_buf; };
struct fd_op_vtable { int (*ioctl)(void *, unsigned, va_list); };
static struct websocket_context ws;
static int result, timeout_seen, bookkeeping;
static short underlying_events;
static int underlying_ioctl(void *, unsigned request, va_list args)
{
	if (request == ZFD_IOCTL_POLL_OFFLOAD) {
		struct zsock_pollfd *fds = va_arg(args, struct zsock_pollfd *);
		int count = va_arg(args, int);
		timeout_seen = va_arg(args, int);
		for (int i = 0; i < count; ++i) {
			assert(fds[i].fd == 1);
			fds[i].revents = underlying_events;
		}
	} else {
		struct zsock_pollfd *pfd = va_arg(args, struct zsock_pollfd *);
		pfd->revents = underlying_events;
		++bookkeeping;
	}
	return result;
}
static const struct fd_op_vtable websocket_fd_op_vtable = { NULL };
static const struct fd_op_vtable underlying_vtable = { underlying_ioctl };
static void *zvfs_get_fd_obj(int fd, const struct fd_op_vtable *, int)
{
	return fd == 2 ? &ws : NULL;
}
static void *zvfs_get_fd_obj_and_vtable(int fd, const struct fd_op_vtable **vtable, void *)
{
	assert(fd == 1);
	*vtable = &underlying_vtable;
	return &ws;
}
static int zvfs_fdtable_call_ioctl(const struct fd_op_vtable *vtable, void *ctx,
				 unsigned request, ...)
{
	va_list args;
	va_start(args, request);
	int ret = vtable->ioctl(ctx, request, args);
	va_end(args);
	return ret;
}
#include "websocket_poll.inc"
static int native_ioctl(unsigned request, ...)
{
	va_list args;
	va_start(args, request);
	int ret = websocket_ioctl_vmeth(&ws, request, args);
	va_end(args);
	return ret;
}
static void reset()
{
	ws = (struct websocket_context){ 1, { 9 } };
	result = 0; underlying_events = 0; timeout_seen = -1; bookkeeping = 0;
}

int test_websocket_native_poll(void) {
	reset();
	struct zsock_pollfd fd = { 2, ZSOCK_POLLIN, 0 };
	// Three one-byte frames remain after the first CONNACK byte was read.
	for (int i = 0; i < 3; ++i) {
		assert(native_ioctl(ZFD_IOCTL_POLL_PREPARE, &fd) == -EALREADY);
		result = -EAGAIN;
		assert(native_ioctl(ZFD_IOCTL_POLL_UPDATE, &fd) == 0);
		assert(fd.revents & ZSOCK_POLLIN);
		ws.recv_buf.count -= 3;
		result = 0;
	}
	assert(bookkeeping == 6);
	assert(native_ioctl(ZFD_IOCTL_POLL_PREPARE, &fd) == 0);
	assert(native_ioctl(ZFD_IOCTL_POLL_UPDATE, &fd) == 0);
	assert(fd.revents == 0);
	ws.recv_buf.count = 3;
	fd.events = ZSOCK_POLLOUT;
	assert(native_ioctl(ZFD_IOCTL_POLL_PREPARE, &fd) == 0);
	fd.events = ZSOCK_POLLIN;
	result = -EBADF;
	assert(native_ioctl(ZFD_IOCTL_POLL_UPDATE, &fd) == -EBADF);
	assert(native_ioctl(ZFD_IOCTL_POLL_PREPARE, &fd) == -EBADF);
	return 1;
}

int test_websocket_offload_poll(void) {
	reset();
	struct zsock_pollfd fd = { 2, ZSOCK_POLLIN, 0 };
	assert(websocket_poll_offload(&fd, 1, 5000) == 1);
	assert(timeout_seen == 0 && fd.fd == 2 && fd.revents == ZSOCK_POLLIN);
	underlying_events = ZSOCK_POLLHUP; result = 1;
	assert(websocket_poll_offload(&fd, 1, 5000) == 1);
	assert(fd.revents == (ZSOCK_POLLHUP | ZSOCK_POLLIN));
	result = -1;
	assert(websocket_poll_offload(&fd, 1, 5000) == -1);
	assert(fd.fd == 2);
	result = 0; underlying_events = 0; ws.recv_buf.count = 0;
	assert(websocket_poll_offload(&fd, 1, 5000) == 0);
	assert(timeout_seen == 5000 && fd.revents == 0 && fd.fd == 2);
	ws.recv_buf.count = 3; fd.events = ZSOCK_POLLOUT;
	assert(websocket_poll_offload(&fd, 1, 5000) == 0);
	assert(timeout_seen == 5000 && fd.revents == 0);
	return 1;
}
