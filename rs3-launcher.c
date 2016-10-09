// #define ENABLE_X11 true

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

/*
#ifdef ENABLE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
*/
#define FIFO_PREFIX "/tmp/RS2LauncherConnection_"

struct ipc_args {
	char* pid;
	char* cache_f;
	char* user_f;
};

// decodes recieved message ids
unsigned int decode_short_be(const char* src){
	const unsigned char* src_unsigned_char = (const unsigned char*) src;
	return (unsigned int) src_unsigned_char[0] | (unsigned int) src_unsigned_char[1] << 8;
}

// encodes reply size to send
char* encode_short_be(unsigned int src){
	char *buf = malloc(sizeof(char) * 2);
	buf[0] = (char) src & 0xFF;
	buf[1] = (char) (src >> 8) & 0xFF;
	return buf;
}

// encodes message id to send
char* encode_short_le(unsigned int i){
	char *buf = malloc(sizeof(i) * 2);
        buf[0] = (char) (i >> 8) & 0xFF;
        buf[1] = (char) i & 0xFF;
        return buf;
}

int handle_messages(const char*msg, int length, int fd_out ,char* cache_folder, char* user_folder){
	unsigned int id = decode_short_be(msg);
	switch(id){
		case 0:
			{
				int reply_size = 17 + strlen(cache_folder) + strlen(user_folder);
				char reply[reply_size];
				memset(reply, 0, reply_size);
				char *dest = reply;
				
				char* msg_id = encode_short_le((uint16_t) 0x0001);
				memcpy(dest, msg_id, 2);
				free(msg_id);
				dest += 2;

				memcpy(dest, "\x00\x002\x00\x00\x00\x000", 6);
				dest += 6;

				// would encode parent window id .. but it's just 00 00 00 00 so do NOTHING
				dest += 4;

				memcpy(dest, cache_folder, strlen(cache_folder));
				dest += strlen(cache_folder);
				dest += 1;
				
				memcpy(dest, user_folder, strlen(user_folder));
				dest += strlen(user_folder);
				dest += 1;

				memcpy(dest, "\x00\x03\x00", 3);
				dest += 3;

				char *enc_reply_size = encode_short_be(reply_size);
				write(fd_out, enc_reply_size, 2);
				free(enc_reply_size);	
				
				write(fd_out, reply, reply_size);			
			}
			break;
		case 512: // recieved notification to set window title
			/* DOESNT WORK FOR NOW WILL TRY AGAIN LATER
			{
				if(length != 10){
					printf("Not recieved enough info to change window title.\n");
					break;
				}
				char *msg_ptr = strdup(msg);
				strcpy(msg_ptr, msg);
				msg_ptr += 6;
				unsigned int window_id = decode_int_le(msg_ptr);
				printf("window_id = %i\n", window_id);
				Display *display = XOpenDisplay(NULL);
				if(!display){
					printf("Couldn't open display .. w/e LUL");
					break;
				}
				XChangeProperty(display, (Window) window_id, XA_WM_NAME, XA_STRING, 8, PropModeReplace, (const unsigned char*) "Roonscaep", strlen("Roonscaep"));
				XCloseDisplay(display);				
			}
			*/
			break;
		case 3072: // recieved closing message .. break and clean up
			return 0;
		default:
			//printf("Recieved a useless message! %02x%02x aka %i\n", msg[0], msg[1], id);
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

	// Do FIFO logic here
	while(1){
		char buf[2];
		if(!read(fd_in, buf, sizeof buf)){
			perror("Communication ended with client.");
			break;
		}
		int length = decode_short_be(buf);
		char msg[length];
		if(!read(fd_in, msg, length)){
			break;
		}

		if(length < 2){
			perror("Message recieved was too short to be valid");
			break;
		}
		if(!handle_messages(msg, length, fd_out, args->cache_f, args->user_f)){
			break;
		}
	}
	// Close FIFOs as files
	close(fd_in);
	close(fd_out);

	// remove FIFOs
	unlink(in_fifo);
	unlink(out_fifo);
	
	return;
}

// launch parameters:
// 0=executable name
// 1=executable lib
// 2=size of launch args
// 3=client width
// 4=client height
// 5=cache folder
// 6=user folder
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
	// run method
	int result = rmbs(params, atoi(argv[3]), atoi(argv[4]), 1, 0);
	
	// cleanup
	pthread_join(thread, NULL);
	dlclose(lib);
}
