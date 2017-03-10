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
#include <time.h>

#define WINDOW_TITLE "RuneScape"
#define FIFO_PREFIX "/tmp/RS2LauncherConnection_"

struct ipc_args {
	char *pid;
	char *cache_f;
	char *user_f;
};

void print_message(const char *prefix, const char *msg, int length){
	printf("%smsg [ id=%02x%02x data=", prefix, msg[0], msg[1]);
	int i = 2;
	while (i < length){
		unsigned char c = msg[i];
		printf("%02x ", c);
		++i;
	}
	printf("] %iB\n", length-2);
}

int handle_message(const char *msg, int length, int fd_out, char *cache_folder, char *user_folder){
	char msg_id[2] = {msg[0], msg[1]};
	uint16_t id = be16toh(*(uint16_t *) msg_id);
	print_message("GOT: ", msg, length);
	switch(id){
		case 0:
			{
				// calculate reply size to make buffer
				int reply_size = 17 + strlen(cache_folder) + strlen(user_folder);
				char reply[reply_size];

				// zero out memory.
				memset(reply, 0, reply_size);

				// create a pointer to pack data
				char *dest = reply;
				
				// add message id to reply
				uint16_t msg_id = htobe16(0x0001);
				memcpy(dest, &msg_id, 2);
				dest += 2;

				// add these 'nessesary' bytes to reply
				memcpy(dest, "\x00\x02\x00\x00\x00\x00", 6);
				dest += 6;

				// would encode parent window id aka 00 00 00 00 so just move pointer
				dest += 4;

				// add cache folder to reply
				memcpy(dest, cache_folder, strlen(cache_folder));
				dest += strlen(cache_folder);
				dest += 1;
				
				// add user folder to reply
				memcpy(dest, user_folder, strlen(user_folder));
				dest += strlen(user_folder);
				dest += 1;

				// add these nessesary bytes
				memcpy(dest, "\x00\x03\x00", 3);
				dest += 3;

				uint16_t enc_reply_size = htole16(reply_size);
				// send reply size
				write(fd_out, &enc_reply_size, 2);

				// send reply
				write(fd_out, reply, reply_size);
				print_message("SENT: ", reply, reply_size);			
			}
			break;
		case 2:
			{	// only continue if message is correct length	
				if(length == 12){
					// create buffer to extract window id
					char wid[4];
					// extract window id
					memcpy(wid, msg+8, 4);
					// convert raw window id to something usable
					uint32_t window_id = be32toh(* (uint32_t *) wid);
					// connect to display
					xcb_connection_t *connection = xcb_connect(NULL, NULL);
					// only continue if successfully connected
					if(connection){
						srand(time(NULL));
						int r_int = rand();
						char win_title[strlen(WINDOW_TITLE)+sizeof(r_int)+1];
						sprintf(win_title, "%s %i", WINDOW_TITLE, r_int);
						// set the window title
						xcb_change_property(connection, 
							XCB_PROP_MODE_REPLACE, 
							(xcb_window_t) window_id, 
							XCB_ATOM_WM_NAME, 
							XCB_ATOM_STRING, 
							8, strlen(win_title),
							(const unsigned char*) win_title
						);
						xcb_flush(connection);
						xcb_disconnect(connection);
					}
				} 
			}
			break;
		default:		
			break;
	}
}

void *handle_ipc(void *args_ptr){
	struct ipc_args *args = args_ptr;
	
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

	// Do FIFO read/write here 
	while(1){
		char buf[2];
		// read message size
		if(!read(fd_in, buf, sizeof buf)){
			perror("Communication ended with client.");
			break;
		}

		// create message buffer then read message
		uint16_t msg_len = le16toh(*(uint16_t *) buf);
		
		if(msg_len < 2){
			perror("Message size recieved was too small.");
			break;
		}
		
		char msg[msg_len];
		if(!read(fd_in, msg, msg_len)){
			break;
		}

		// handle message
		handle_message(msg, msg_len, fd_out, args->cache_f, args->user_f);
	}
	// Close access to FIFOs
	close(fd_in);
	close(fd_out);

	// Delete FIFOs
	unlink(in_fifo);
	unlink(out_fifo);
}

// launch parameters:
// 0=executable name 1=size of launch args
// 2=cache folder  3=user folder
int main(int argc, char *argv[]){
	if(argc != 4){
		perror("Not enough arguments recieved.");
		exit(1);
	}
	// get parameters from pipe (stdin)
	char buf[atoi(argv[1])];
	fgets(buf, sizeof buf, stdin);
	int i = strlen(buf) - 1;
	if(buf[i] == '\n'){
		buf[i] = '\0';
	}
	
	// load lib
	void *lib = dlopen("./librs2client.so", RTLD_NOW);
	if(!lib){
		perror("Error loading lib.");
		exit(1);
	}

	// get process ID and append to params
	char pid[4];
	snprintf(pid, 10, "%X", (int)getpid());
	char params[sizeof(pid)+sizeof(buf)];
	snprintf(params, sizeof params, "%s%s", buf, pid);	

	// create thread params
	struct ipc_args thread_args;
	thread_args.pid=pid;
	thread_args.cache_f=argv[2];
	thread_args.user_f=argv[3];

	// create and run IPC thread.
	pthread_t thread;
	pthread_create(&thread, NULL, handle_ipc, &thread_args);
	
	// int RunMainBootstrap(const char* params, int width, int height, int ?, int ?);
	int (*rmbs)(const char*, int, int, int, int);
	
	// Get address of method, check for errors
	char *error;
	rmbs = dlsym(lib, "RunMainBootstrap");
	if ((error = dlerror()) != NULL){
		perror(error);
		exit(1);
	}
	// run RunMainBootstrap
	int result = rmbs(params, 1024, 768, 1, 0);
	
	// cleanup
	pthread_join(thread, NULL);
	dlclose(lib);
}