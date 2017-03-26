#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <limits.h>

#define WINDOW_TITLE "RuneScape"
#define FIFO_PREFIX "/tmp/RS2LauncherConnection_"

typedef struct {
	char *pid;
	char *cache_f;
	char *user_f;
} ipc_args;

void print_message(const char *prefix, const char *msg, int length){
	printf("%smsg [ id=%02x%02x data=", prefix, msg[0], msg[1]);
	for (int i = 2; i < length; ++i){
		printf("%02x ", (unsigned char) msg[i]);
	}
	printf("] %iB\n", length-2);
}

int handle_message(const char *msg, int length, int fd_out, char *cache_folder, char *user_folder, char* pid){
	char msg_id[2] = {msg[0], msg[1]};
	uint16_t id = be16toh(*(uint16_t *) msg_id);
	print_message("GOT: ", msg, length);
	if(id == 0) {
		// calculate reply size to make buffer
		uint16_t reply_size = 17 + strlen(cache_folder) + strlen(user_folder);
		char reply[reply_size];
		
		// declare variables to write in reply
		uint16_t msg_id = htobe16(1);
		char *bytes1 = "\x00\x02\x00\x00\x00\x00";
		uint32_t parent_id = 0;
		char *bytes2 = "\x00\x03\x00";
		
		// zero and fill reply. 
                memset(reply, 0, reply_size); 
		memcpy(reply, &msg_id, 2);
		memcpy(reply+2, bytes1, 6);
		memcpy(reply+8, &parent_id, 4);
		int clen = strlen(cache_folder), ulen = strlen(user_folder);
		memcpy(reply+12, cache_folder, clen);
		memcpy(reply+13+clen, user_folder, ulen);
		memcpy(reply+14+clen+ulen, bytes2, 3);

		// send reply size
		write(fd_out, &reply_size, 2);
		// send reply
		write(fd_out, reply, reply_size);
		print_message("SENT: ", reply, reply_size);			

	} else if (id == 2 && length == 12){ 
		// connect to display
		xcb_connection_t *connection = xcb_connect(NULL, NULL);
		// only continue if successfully connected
		if(connection){
			// extract and convert window id
	                char wid[4] = {msg[8], msg[9], msg[10], msg[11]};
			uint32_t window_id = be32toh(* (uint32_t *) wid);
			// create window title
			char win_title[strlen(WINDOW_TITLE) + sizeof(pid) + 1];
			sprintf(win_title, "%s %s", WINDOW_TITLE, pid);
			// set window title
			xcb_change_property(connection, 
				XCB_PROP_MODE_REPLACE, 
				(xcb_window_t) window_id, 
				XCB_ATOM_WM_NAME, 
				XCB_ATOM_STRING, 
				8,
				strlen(win_title),
				(const unsigned char*) win_title
			);
			xcb_flush(connection);
			xcb_disconnect(connection);
		}
	} 
}

void *handle_ipc(void *args_ptr){

	ipc_args *args = args_ptr;
	
	// Size and assemble FIFO locations
	char in_fifo[sizeof(FIFO_PREFIX) + sizeof(args->pid) + sizeof("_i")];
	char out_fifo[sizeof(FIFO_PREFIX) + sizeof(args->pid) + sizeof("_o")];
	snprintf(in_fifo, sizeof in_fifo, "%s%s%s", FIFO_PREFIX, args->pid, "_i");
	snprintf(out_fifo, sizeof out_fifo, "%s%s%s", FIFO_PREFIX, args->pid, "_o");
	
	// Make FIFOs
	mkfifo(in_fifo, 0600);
	mkfifo(out_fifo, 0600);

	// Open FIFOs as files
	int fd_in = open(in_fifo, O_RDONLY);
	int fd_out = open(out_fifo, O_WRONLY);

	// store message size bytes	
	char buf[2];

	// Loop until unable to read a message
	while(read(fd_in, buf, sizeof buf)){
		// convert message size bytes to decimal
		uint16_t msg_len = le16toh(*(uint16_t *) buf);
		
		if(msg_len < 2){
			printf("Message size recieved is invalid.\n");
			break;
		}
		
		char msg[msg_len];
		if(!read(fd_in, msg, msg_len)){
			printf("Failed to read meaningful messaage.\n");
			break;
		}

		// handle message
		handle_message(msg, msg_len, fd_out, args->cache_f, args->user_f, args->pid);
	}
	printf("Ended communication with client.\n");

	// Close access to FIFOs
	close(fd_in);
	close(fd_out);

	// Delete FIFOs
	unlink(in_fifo);
	unlink(out_fifo);
}

/*
	launch parameters:
	0=executable name 1=process id
	2=cache folder  3=user folder
	4=args
*/ 
int main(int argc, char *argv[]){
	if(argc != 5){
		printf("Incorrect number of arguments recieved.\n");
		exit(1);
	}

	// load lib
	void *lib = dlopen("./librs2client.so", RTLD_NOW);
	if(!lib){
		printf("Error loading lib.\n");
		exit(2);
	}

	// create thread params
	ipc_args thread_args = { argv[1], argv[2], argv[3] };

	// create and run IPC thread.
	pthread_t thread;
	pthread_create(&thread, NULL, handle_ipc, &thread_args);
	
	// Get address of method, checking that it was successfully able to find function.
	// int RunMainBootstrap(const char* params, int width, int height);
	int (*rmbs)(const char*, int, int) = dlsym(lib, "RunMainBootstrap");

	// print error or run function
	if (dlerror() != NULL){
		printf("Unable to find bootstrap function.\n");
	} else {
		rmbs(argv[4], 1024, 768);
	}
	// cleanup
	pthread_join(thread, NULL);
	dlclose(lib);
}