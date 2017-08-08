#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <event.h>
#include "filter.c"

#define VERSION "0.0.5"

#define ERR -1
#define OK 1

#define ERR_BUF_SIZE 1024

typedef struct conn {
	int fd;
	int state;
	struct event event;
	short which;
	char *rbuff;
	int result;
} conn;

enum {LISTENING, READ, WRITE, CLOSING};

static char err[ERR_BUF_SIZE];
static tree *stree;

int new_socket();
int new_server_socket(int port);
conn *new_conn(int fd, int init_state, int event_flags);
void event_handler(int fd, short which, void *arg);
void drive_machine(conn *c);

void show_err(char *err) {
	printf("err:%s\n", err);
}

void set_err(char *err, char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vsnprintf(err, ERR_BUF_SIZE, fmt, va);
	va_end(va);
}

int new_socket() {
	int fd;
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		set_err(err, "create new socket failed:%s\n", strerror(errno));
		return ERR;
	}
	//set socket nonblock
	int flags;
	if ((flags = fcntl(fd, F_GETFL, 0)) < 0 ||
		fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		set_err(err, "setting O_NONBLOCK\n");
		close(fd);
		return ERR;
	}
	return fd;
}

int new_server_socket(int port) {
	int fd;
	struct sockaddr_in addr;
	if((fd = new_socket()) == ERR) {
		return ERR;
	}
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	addr.sin_port = htons(port);
	if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		set_err(err, "bind server socket failed\n");
		close(fd);
		return ERR;
	}
	if(listen(fd, 1024) == -1) {
		set_err(err, "listen failed\n");
		close(fd);
		return ERR;
	}
	return fd;
}

conn *new_conn(int fd, int init_state, int event_flags) {
	conn *c = malloc(sizeof(conn));
	c->fd = fd;
	c->state = init_state;
    event_set(&c->event, fd, event_flags, event_handler, (void *)c);
	if(event_add(&c->event, 0) == -1) {
		set_err(err, "add event failed\n");
		free(c);
		return NULL;
	}
	return c;
}

void conn_close(conn *c) {
	event_del(&c->event);
	close(c->fd);
	free(c->rbuff);
	free(c);
}

void event_handler(int fd, short which, void *arg) {
	conn *c = (conn *)arg;
	c->which = which;
	drive_machine(c);
}

void conn_listening(conn *c) {
	int fd;
	struct sockaddr addr;
	conn *client;
	socklen_t addrlen = sizeof(addr);
	if((fd = accept(c->fd, &addr, &addrlen)) == -1) {
		show_err("accept failed\n");
	}
	if((client = new_conn(fd, READ, EV_READ | EV_PERSIST)) == NULL) {
		show_err("create new client conn failed\n");
	}
}

char *rtrim(char *str, char *mask) {
	int slen = strlen(str);
	int mlen = strlen(mask);
	if(slen < mlen) return str;
	char *tail = str + (slen - mlen);
	if(strcmp(tail, mask) == 0) {
		str[slen - mlen] = '\0';
	}
	return str;
}

int get_result(char *input) {
	input = rtrim(input, "\r\n");
	return hit(stree, input);
}

void conn_read(conn *c) {
	//bad smell
	int size = 1025;
	int expected = 512;
	int len = 0;
	c->rbuff = malloc(size);
	while(1) {
		if(len + expected > size) {
			size = len + (expected * 2);
			c->rbuff -= len;
			c->rbuff = realloc(c->rbuff, size);
			c->rbuff += len;
		}
		int nread = read(c->fd, c->rbuff, expected);
		if(nread <= 0) break;
		len += nread;
		c->rbuff += nread;
		if(nread < expected) break;
	}
	c->rbuff[len] = '\0';
	c->rbuff -= len;
	c->result = get_result(c->rbuff);
	c->state = WRITE;
}

void conn_write(conn *c) {
	char ret[10];
	sprintf(ret, "%d\n", c->result);
	write(c->fd, ret, strlen(ret));
	c->state = READ;
}

void conn_closing(conn *c) {
	conn_close(c);
}

void drive_machine(conn *c) {
	int stop = 0;
	while(!stop) {
		switch(c->state) {
			case LISTENING:
				conn_listening(c);
				stop = 1;
				break;
			case READ:
				conn_read(c);
				break;
			case WRITE:
				if(strstr(c->rbuff, "quit") > 0) {
					c->state = CLOSING;
				}else {
					conn_write(c);
					stop = 1;
				}
				break;
			case CLOSING:
				conn_closing(c);
				stop = 1;
				break;
			default:
				stop = 1;
				break;
		}
	}
}

void int_handler(int dummy) {
	printf("quit\n");
	event_loopexit(NULL);
}

int main(int argc, char **argv) {
	printf("sharpye, version %s\n", VERSION);
	signal(SIGINT, int_handler);
	stree = t_new();
	t_add(stree, "123");
	event_init();
	int fd = new_server_socket(2222);
	if(fd == ERR) { show_err(err); return 1; }
	conn *server_conn = new_conn(fd, LISTENING, EV_READ | EV_PERSIST);
	if(!server_conn) { show_err(err); return 1; }
	printf("ok, listening at 2222\n");
	event_loop(0);
	conn_close(server_conn);
	t_flush(stree);
	return 0;
}
