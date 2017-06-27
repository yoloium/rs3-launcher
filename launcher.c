#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <unistd.h>

#define TITLE "RuneScape"
#define MIN_WIDTH 765
#define MIN_HEIGHT 540
#define MAX_WIDTH 3840
#define MAX_HEIGHT 2160
#define WIDTH 1366
#define HEIGHT 768

void print_message(const char *prefix, const char *msg, int length){
	printf("%s [ ", prefix);
	for (int i = 0; i < length; ++i){
		printf("%02x ", (unsigned char) msg[i]);
	}
	printf("] %iB\n", length);
}

/*
	launch parameters:
	0=executable name 1=game_folder
	2=in_fifo 3= out_fifo
*/ 
int main(int argc, char *argv[]){
	
	// Make FIFOs
	mkfifo(argv[2], 0600);
	mkfifo(argv[3], 0600);

	// Open FIFOs as files
	int fd_in = open(argv[2], O_RDONLY);
	int fd_out = open(argv[3], O_WRONLY);

	// store message size bytes	
	char buf[2];

	// Loop until unable to read a message
	while(read(fd_in, buf, sizeof buf)){
		// convert message size bytes to decimal
		uint16_t msg_len = le16toh(*(uint16_t *) buf);
		
		if(msg_len < 2){
			printf("ERROR: Message size recieved is invalid.\n");
			break;
		}
		
		char msg[msg_len];
		if(!read(fd_in, msg, msg_len)){
			printf("ERROR: Failed to read a message.\n");
			break;
		}

		// handle message
		char msg_id[2] = {msg[0], msg[1]};
		uint16_t id = be16toh(*(uint16_t *) msg_id);
		print_message("GOT", msg, msg_len);
		if(id == 0x0000) {
			int glen = strlen(argv[1]), tlen = strlen(TITLE), g2len = 2 * glen;
			// calculate reply size to make buffer
			uint16_t reply_size = htole16(g2len + tlen + 32);
			char reply[reply_size];

			uint16_t min_w = htobe16(MIN_WIDTH), min_h = htobe16(MIN_HEIGHT),
				max_w = htobe16(MAX_WIDTH), max_h = htobe16(MAX_HEIGHT),
				width = htobe16(WIDTH), height = htobe16(HEIGHT);

			// zero and fill reply. 
			memset(reply, 0, reply_size);
			memcpy(reply, "\0\1\0\4\0\0\0\0\0\0\0\0", 12);
			memcpy(reply+12, argv[1], glen);
			memcpy(reply+13+glen, argv[1], glen);
			memcpy(reply+14+g2len, "\0\4\0\0\7", 5);
			memcpy(reply+19+g2len, TITLE, tlen);
			memcpy(reply+20+g2len+tlen, &min_w, 2);
			memcpy(reply+22+g2len+tlen, &min_h, 2);
			memcpy(reply+24+g2len+tlen, &max_w, 2);
			memcpy(reply+26+g2len+tlen, &max_h, 2);
			memcpy(reply+28+g2len+tlen, &width, 2);
			memcpy(reply+30+g2len+tlen, &height, 2);

			// send reply
			write(fd_out, &reply_size, 2);
			write(fd_out, reply, reply_size);
			print_message("SENT", reply, reply_size);
		} else if (id == 0x0020){
			write(fd_out, "\4\0\0\v\0\1", 6);
		}
	}
	printf("Ended communication with client.\n");

	// Close access to FIFOs
	close(fd_in);
	close(fd_out);

	// Delete FIFOs
	unlink(argv[2]);
	unlink(argv[3]);
}