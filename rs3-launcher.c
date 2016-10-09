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

#define FIFO_PREFIX "/tmp/RS2LauncherConnection_"

struct ipc_args {
	char* pid;
	char* cache_f;
	char* user_f;
};

void print_message(const char *prefix, const char *msg, int length){
	printf("%smsg[id=%02x%02x, data=", prefix, msg[0], msg[1]);
	int i = 2;
	while (i < length){
		unsigned char c = msg[i];
		printf("%02x ", c);
		++i;
	}
	printf(" len=%i]\n", length-2);
}

int handle_messages(const char*msg, int length, int fd_out ,char* cache_folder, char* user_folder){
	char msg_id[2];
	strncpy(msg_id, msg, 2);
	uint16_t id = le16toh(*(uint16_t*) msg_id);
	print_message("REC: ", msg, length);
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
				memcpy(dest, "\x00\x002\x00\x00\x00\x000", 6);
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
			}
			break;
		case 3072: // recieved closing message .. break and clean up
			return 0;
		default:		
			break;
		return 1;
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
		if(!handle_messages(msg, msg_len, fd_out, args->cache_f, args->user_f)){
			break;
		}
	}
	// Close access to FIFOs
	close(fd_in);
	close(fd_out);

	// Delete FIFOs
	unlink(in_fifo);
	unlink(out_fifo);
	
	return;
}

// launch parameters:
// 0=executable name  1=executable lib  2=size of launch args
// 3=client width 4=client height  5=cache folder  6=user folder
int main(int argc, char *argv[]){
	if(argc != 7){
		perror("Not enough arguments recieved.");
		exit(1);
	}
	// get parameters from pipe (stdin)
	char buf[atoi(argv[2])];
	fgets(buf, sizeof buf, stdin);
	int i = strlen(buf) - 1;
	if(buf[i] == '\n'){
		buf[i] = '\0';
	}
	
	// load lib
	void *lib = dlopen(argv[1],  RTLD_LAZY);
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
	thread_args.cache_f=argv[5];
	thread_args.user_f=argv[6];

	// create and run IPC thread.
	pthread_t thread;
	pthread_create(&thread, NULL, handle_ipc, &thread_args);
	
	// void RunMainBootstrap(const char* params, int width, int height, int ?, int ?);
	int (*rmbs)(const char*, int, int, int, int);
	
	// Get address of method, check for errors
	char *error;
	rmbs = dlsym(lib, "RunMainBootstrap");
	if ((error = dlerror()) != NULL){
		perror(error);
		exit(1);
	}
	// run RunMainBootstrap
	int result = rmbs(params, atoi(argv[3]), atoi(argv[4]), 1, 0);
	
	// cleanup
	pthread_join(thread, NULL);
	dlclose(lib);
}
