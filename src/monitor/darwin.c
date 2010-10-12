#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <glob.h>

#include <monome.h>

static void disable_subproc_waiting() {
	struct sigaction s;

	memset(&s, 0, sizeof(struct sigaction));
	s.sa_flags = SA_NOCLDWAIT;
	s.sa_handler = SIG_IGN;

	if( sigaction(SIGCHLD, &s, NULL) < 0 ) {
		perror("disable_subproc_waiting");
		exit(EXIT_FAILURE);
	}
}

monome_t *scan_connected_devices() {
	monome_t *device = NULL;
	glob_t gb;
	int i;

	gb.gl_offs = 0;
	if( glob("/dev/tty.usbserial*", GLOB_NOSORT, NULL, &gb) )
		return NULL;
	
	for( i = 0; i < gb.gl_pathc; i++ )
		if( (device = monome_open(gb.gl_pathv[i])) )
			break;
	
	globfree(&gb);
	return device;
}

monome_t *next_device() {
	monome_t *device = NULL;

	disable_subproc_waiting();
	if( (device = scan_connected_devices()) )
		return device;

	return NULL;
}
