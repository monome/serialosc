/**
 * Copyright (c) 2011 William Light <wrl@illest.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <winreg.h>
#include <dbt.h>
#include <io.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>

#define FTDI_REG_PATH "SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS"

struct detector_state {
	HANDLE to_supervisor;

	ATOM window_class;
	HANDLE msg_window;

	HANDLE ftdi_notify;
};

static void
send_connect(const struct detector_state *state, char *port)
{
	uint8_t buf[64];
	DWORD written;
	size_t bufsiz;

	sosc_ipc_msg_t m = {
		.type = SOSC_DEVICE_CONNECTION,
		.connection.devnode = port
	};

	if (!state->to_supervisor) {
		fprintf(stderr, " [>] %s connected\n", port);
		return;
	}

	bufsiz = sosc_ipc_msg_to_buf(buf, sizeof(buf), &m);

	if (bufsiz < 0) {
		fprintf(stderr, "[-] couldn't serialize msg\n");
		return;
	}

	WriteFile(state->to_supervisor, buf, bufsiz, &written, NULL);
}

static int
scan_connected_devices(const struct detector_state *state)
{
	HKEY key, subkey;
	char subkey_name[MAX_PATH], *subkey_path;
	unsigned char port_name[64];
	DWORD klen, plen, ptype;
	int i = 0;

	switch (RegOpenKeyEx(HKEY_LOCAL_MACHINE, FTDI_REG_PATH,
			0, KEY_READ, &key)) {
	case ERROR_SUCCESS:
		/* ERROR: request was (unexpectedly) successful */
		break;

	case ERROR_FILE_NOT_FOUND:
		/* print message about needing the FTDI driver maybe? */
		/* fall through also */
	default:
		return 1;
	}

	for (;;) {
		klen = sizeof(subkey_name) / sizeof(char);

		switch (RegEnumKeyEx(key, i++, subkey_name, &klen,
					NULL, NULL, NULL, NULL)) {
		case ERROR_MORE_DATA:
		case ERROR_SUCCESS:
			break;

		default:
			goto done;
		}

		subkey_path = s_asprintf("%s\\%s\\0000\\Device Parameters",
				FTDI_REG_PATH, subkey_name);

		switch (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey_path,
					0, KEY_READ, &subkey)) {
		case ERROR_SUCCESS:
			break;

		default:
			free(subkey_path);
			continue;
		}

		free(subkey_path);

		plen = sizeof(port_name) / sizeof(char);
		ptype = REG_SZ;
		switch (RegQueryValueEx(subkey, "PortName", 0, &ptype,
					port_name, &plen)) {
		case ERROR_SUCCESS:
			port_name[plen] = '\0';
			break;

		default:
			goto next;
		}

		send_connect(state, (char *) port_name);

next:
		RegCloseKey(subkey);
	}

done:
	RegCloseKey(key);
	return 0;
}

static int
ftdishit_to_port(char *dst, size_t dst_size, const char *bullshit)
{
	char *subkey_path, *port;
	DWORD plen, ptype;
	HKEY subkey;

	if (!(bullshit = strchr(bullshit, '#')))
		return -1;

	if (!(port = strchr(++bullshit, '#')))
		return -1;

	*port = '\0';

	subkey_path = s_asprintf(FTDI_REG_PATH "\\%s\\0000\\Device Parameters",
	                         bullshit);

	switch (RegOpenKeyEx(
	        HKEY_LOCAL_MACHINE, subkey_path,
	        0, KEY_READ, &subkey)) {
	case ERROR_SUCCESS:
		break;

	default:
		free(subkey_path);
		return -1;
	}

	free(subkey_path);

	plen = dst_size;
	ptype = REG_SZ;
	switch (RegQueryValueEx(subkey, "PortName", 0, &ptype,
	                        (unsigned char *) dst, &plen)) {
	case ERROR_SUCCESS:
		dst[plen] = '\0';
		break;

	default:
		RegCloseKey(subkey);
		return -1;
	}

	RegCloseKey(subkey);
	return 0;
}

static int
init_device_notification(struct detector_state *state)
{
	DEV_BROADCAST_DEVICEINTERFACE filter;
	GUID vcp_guid = {0x86e0d1e0L, 0x8089, 0x11d0,
		{0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73}};

	filter.dbcc_size = sizeof(filter);
	filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	filter.dbcc_reserved = 0;
	filter.dbcc_classguid = vcp_guid;
	filter.dbcc_name[0] = '\0';

	state->ftdi_notify = RegisterDeviceNotification(state->msg_window,
			&filter, DEVICE_NOTIFY_WINDOW_HANDLE);

	if (!state->ftdi_notify)
		return 1;
	return 0;
}

static void
fini_device_notification(struct detector_state *state)
{
	UnregisterDeviceNotification(state->ftdi_notify);
}

static void
handle_device_arrival(struct detector_state *state,
		DEV_BROADCAST_DEVICEINTERFACE *dev)
{
	char devname[256], port[64];

	/* XXX: we could theoretically rip out all this FTDI-specific nonsense
	 *      and just get port notifications, but win32 libmonome only knows
	 *      how to get serial numbers from FTDI devices anyway so there's
	 *      no real win for that change. */
	if (dev->dbcc_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
		return;

	wcstombs(devname, (void *) dev->dbcc_name, sizeof(devname));
	if (ftdishit_to_port(port, sizeof(port), devname))
		return;

	send_connect(state, port);
}

static LRESULT CALLBACK
wndproc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	struct detector_state *state = GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message) {
	case WM_CREATE:
		init_device_notification(state);
		return 1;

	case WM_DESTROY:
		fini_device_notification(state);
		return 1;

	case WM_DEVICECHANGE:
		if (wparam == DBT_DEVICEARRIVAL)
			handle_device_arrival(state, (void *) lparam);

		return 1;

	default:
		return DefWindowProcW(hwnd, message, wparam, lparam);
	}
}

static ATOM
register_window_class(void)
{
	WNDCLASSW wc = {
		.style         = CS_OWNDC,
		.lpfnWndProc   = wndproc,

		.cbClsExtra    = 0,
		.cbWndExtra    = 0,
		.hInstance     = 0,

		.hIcon         = LoadIcon(NULL, IDI_APPLICATION),
		.hCursor       = LoadCursor(NULL, IDC_ARROW),
		.hbrBackground = NULL,

		.lpszMenuName  = NULL,
		.lpszClassName = L"serialosc_detector"
	};

	return RegisterClassW(&wc);
}

static void
unregister_window_class(ATOM cls)
{
	UnregisterClassW((void *) MAKEINTATOM(cls), NULL);
}

#ifndef WM_DISABLED
#define WM_DISABLED 0x08000000L
#endif

static HANDLE
open_msg_window(struct detector_state *state)
{
	HANDLE wnd;

	wnd = CreateWindowExW(0, (void *) MAKEINTATOM(state->window_class),
			L"<serialosc detector message receiver>", WM_DISABLED,
			0, 0, 0, 0, NULL, NULL, NULL, (void *) state);

	if (!wnd)
		return NULL;

	SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR) state);
	return wnd;
}

static void
event_loop(struct detector_state *state)
{
	int status;
	MSG msg;

	while ((status = GetMessage(&msg, state->msg_window, 0, 0)) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static HANDLE
get_handle_to_supervisor(void)
{
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetFileType(out) == FILE_TYPE_PIPE)
		return out;
	return NULL;
}

int
main(int argc, char **argv)
{
	struct detector_state state;

	state.to_supervisor = get_handle_to_supervisor();
	state.window_class  = register_window_class();
	state.msg_window    = open_msg_window(&state);

	if (!state.to_supervisor)
		fprintf(stderr, " [-] serialosc-detector running in debug mode\n");

	scan_connected_devices(&state);
	event_loop(&state);

	DestroyWindow(state.msg_window);
	unregister_window_class(state.window_class);

	return 0;
}
