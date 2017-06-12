#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <unistd.h>
#include <limits.h>

#define FIFO_PREFIX "/tmp/RS2LauncherConnection_"
#define TITLE "RuneScape"

void print_message(const char *prefix, const char *msg, int length){
	printf("%smsg [ id=%02x%02x data=", prefix, msg[0], msg[1]);
	for (int i = 2; i < length; ++i){
		printf("%02x ", (unsigned char) msg[i]);
	}
	printf("] %iB\n", length-2);
}

/*
	launch parameters:
	0=executable name 
	1=cache folder  2=user folder
	3=pid
*/ 
int main(int argc, char *argv[]){
	
	// Size and assemble FIFO locations
	char in_fifo[sizeof(FIFO_PREFIX) + sizeof(argv[3]) + sizeof("_i")];
	char out_fifo[sizeof(FIFO_PREFIX) + sizeof(argv[3]) + sizeof("_o")];
	snprintf(in_fifo, sizeof in_fifo, "%s%s%s", FIFO_PREFIX, argv[3], "_i");
	snprintf(out_fifo, sizeof out_fifo, "%s%s%s", FIFO_PREFIX, argv[3], "_o");
	
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
		char msg_id[2] = {msg[0], msg[1]};
		uint16_t id = be16toh(*(uint16_t *) msg_id);
		print_message("GOT: ", msg, msg_len);
		if(id == 0) {
			int clen = strlen(argv[1]), ulen = strlen(argv[2]), tlen = strlen(TITLE);
			// calculate reply size to make buffer
			uint16_t reply_size = htole16(clen + ulen + tlen + 52);
			char reply[reply_size];
	
			// declare variables to write in reply
			uint16_t msg_id = htobe16(0x0001);
			uint16_t msg_size = htole16(reply_size);
			uint16_t min_w = htobe16(765), min_h = htobe16(540);
			uint16_t max_w = htobe16(3840), max_h = htobe16(2160);
			uint16_t width = htobe16(1024), height = htobe16(768); 
			uint16_t max = htobe16(18); // 16+ = maximised, default = 18 ??			
			uint16_t maximised_w_offset = htobe16(0), maximised_h_offset = htobe16(28);
			uint16_t maximised_w = htobe16(1920), maximised_h = htobe16(1021);
	
			char *bytes1 = "\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00";
			char *bytes2 = "\x00\x04\x00\x00\x07";
			// zero and fill reply. 
			memset(reply, 0, reply_size);
			memcpy(reply, &msg_id, 2);
			memcpy(reply+2, bytes1, 10);
			memcpy(reply+12, argv[1], clen);
			memcpy(reply+13+clen, argv[2], ulen);
			memcpy(reply+14+clen+ulen, bytes2, 5);
			memcpy(reply+19+clen+ulen, (const char*) TITLE, tlen);
			memcpy(reply+20+clen+ulen+tlen, &min_w, 2);
			memcpy(reply+22+clen+ulen+tlen, &min_h, 2);
			memcpy(reply+24+clen+ulen+tlen, &max_w, 2);
			memcpy(reply+26+clen+ulen+tlen, &max_h, 2);
			memcpy(reply+28+clen+ulen+tlen, &width, 2);
			memcpy(reply+30+clen+ulen+tlen, &height, 2);
			memcpy(reply+32+clen+ulen+tlen, &max, 2);
			memcpy(reply+36+clen+ulen+tlen, &maximised_w_offset, 2);
			memcpy(reply+40+clen+ulen+tlen, &maximised_h_offset, 2);
			memcpy(reply+44+clen+ulen+tlen, &maximised_w, 2);
			memcpy(reply+48+clen+ulen+tlen, &maximised_h, 2);
		
			// send reply
			write(fd_out, &reply_size, 2);
			write(fd_out, reply, reply_size);
			print_message("SENT: ", reply, reply_size);
		} else if (id == 0x0020){
			write(fd_out, "\4\0\0\v\0\1" ,6);
		}
	}
	printf("Ended communication with client.\n");

	// Close access to FIFOs
	close(fd_in);
	close(fd_out);

	// Delete FIFOs
	unlink(in_fifo);
	unlink(out_fifo);
}