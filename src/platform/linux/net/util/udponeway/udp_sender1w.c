/*
 *	This is the original client code, but will only send data to 
 *	a receiver instead of receiving as well.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include "spin.h"

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

#define debug(fmt,...) 

unsigned long long max = 0, min = (unsigned long long)-1, tot = 0, cnt = 0;
int rcv = 0;
char *rcv_msg;

void construct_header(char *msg)
{
	static unsigned int seqno = 1;
	unsigned long long time;
	unsigned int *int_msg = (unsigned int *)msg;
	unsigned long long *ll_msg = (unsigned long long *)msg;
	
	rdtscll(time);
	int_msg[0] = seqno;
	ll_msg[1] = time;
	debug("sending message %d with timestamp %lld\n", seqno, time);
	seqno++;

	return;
}

#define RUN_TIME 120
unsigned int run_secs = 0;
unsigned int msg_sent = 0, msg_rcved;
void signal_handler(int signo)
{
	run_secs ++;
	printf("Messages sent/sec: %d", msg_sent);
	if (rcv) {
		debug(", avg response time: %lld, WC: %lld, min: %lld, received/sent:%d\n", 
		       cnt == 0 ? 0 : tot/cnt, 
		       max, min, 
		       msg_sent == 0 ? 0 : (unsigned int)(msg_rcved*100)/(unsigned int)msg_sent);
		tot = cnt = max = 0;
		min = (unsigned long long)-1;
		msg_rcved = 0;
	} else {
		printf("\n");
	}
	msg_sent = 0;
	if (run_secs > RUN_TIME) {
		printf("Done. Runtime:%u secs\n", run_secs);
		exit(1);
	}
}

int socket_nonblock(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
	{
		perror("retrieving flags for socket");
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		perror("setting socket's flags to nonblocking");
		return -1;
	}
	return 0;
}

void do_recv_proc(int fd, int msg_sz)
{
	int ret;

	/* Keep on reading while there are more packets */
	do {
		if ((ret = recv(fd, rcv_msg, msg_sz, MSG_DONTWAIT)) == msg_sz) {
			unsigned long long curr, amnt;
			unsigned int *seqno_ptr = (unsigned int *)rcv_msg, seqno;
			unsigned long long *prev_ptr = (unsigned long long *)rcv_msg, prev;
			
			rdtscll(curr);
			seqno = seqno_ptr[0];
			prev  = prev_ptr[1];
//			debug("Received message %d with stamp %lld @ %lld\n", seqno, prev, curr);
			amnt = curr - prev;
			tot += amnt;
			cnt++;
			min = min < amnt ? min : amnt;
			max = max > amnt ? max : amnt;
			msg_rcved++;
		}
		if (-1 == ret && EAGAIN != errno) {
			perror("Reading from recv socket");
		}
	} while (-1 != ret);
}

void start_timers()
{
	struct itimerval itv;
	struct sigaction sa;
	sigset_t mask;

	debug("Starting timer..\n");
	sa.sa_handler = signal_handler;
	sa.sa_flags = 0;
	//sigemptyset(&sa.sa_mask);
	sigfillset(&sa.sa_mask);

	if (sigaction(SIGALRM, &sa, NULL)) {
		perror("Setting up alarm handler");
		exit(-1);
	}
	sigprocmask(0, NULL, &mask);
	sigdelset(&mask, SIGALRM);

	memset(&itv, 0, sizeof(itv));
	itv.it_value.tv_sec = 1;
	itv.it_interval.tv_sec = 1;

	if (setitimer(ITIMER_REAL, &itv, NULL)) {
		perror("setitimer; setting up sigalarm");
		return;
	}

	return;
}

int foo = 0;

int main(int argc, char *argv[])
{
	unsigned long long start, end, cycs;
	int fd;
	struct sockaddr_in sa;
	int msg_size;
	char *msg;
	int sleep_val;

	if (argc != 5) {
		printf("Usage: %s <ip> <port> <msg size> <sleep_val>\n", argv[0]);
		return -1;
	}
	sleep_val = atoi(argv[4]);
//	sleep_val = sleep_val == 0 ? 1 : sleep_val;

	msg_size = atoi(argv[3]);
	msg_size = msg_size < (sizeof(unsigned long long)*2) ? (sizeof(unsigned long long)*2) : msg_size;
	msg     = malloc(msg_size);
	memset(msg, 0, msg_size);
	rcv_msg = malloc(msg_size);

	sa.sin_family      = PF_INET;
	sa.sin_port        = htons(atoi(argv[2]));
	sa.sin_addr.s_addr = inet_addr(argv[1]);
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}
	
	spin_calib();

//	rdtscll(start);
//	spin_usecs(1000);
//	rdtscll(end);
//	cycs = end - start;
//	printf("1000usec:%llu:%llu\n", end-start, (end-start)/CYCS_PER_USEC);
//
//	rdtscll(start);
//	spin_cycles(cycs);
//	rdtscll(end);
//	printf("1000usec:%llu:%llu\n", end-start, (end-start)/CYCS_PER_USEC);
//
	rdtscll(start);
	spin_usecs(100);
	rdtscll(end);
	printf("100usec:%llu:%llu\n", end-start, (end-start)/CYCS_PER_USEC);


	rdtscll(start);
	spin_usecs(50);
	rdtscll(end);
	printf("50usec:%llu:%llu\n", end-start, (end-start)/CYCS_PER_USEC);

	rdtscll(start);
	spin_usecs(40);
	rdtscll(end);
	printf("40usec:%llu:%llu\n", end-start, (end-start)/CYCS_PER_USEC);

	rdtscll(start);
	spin_usecs(10);
	rdtscll(end);
	printf("10usec:%llu:%llu\n", end-start, (end-start)/CYCS_PER_USEC);

	if (sleep_val) {
		rdtscll(start);
		spin_iters(sleep_val);
		rdtscll(end);
		cycs = end - start;
		printf("%diters:%llu:%lluusecs\n", sleep_val, end-start, (end-start)/CYCS_PER_USEC);
		rdtscll(start);
		spin_cycles(cycs);
		rdtscll(end);
		printf("%diters:%llu:%lluusecs\n", sleep_val, end-start, (end-start)/CYCS_PER_USEC);
//
//		rdtscll(start);
//		spin_usecs(sleep_val);
//		rdtscll(end);
//		printf("%dusec:%llu:%llu\n", sleep_val, end-start, (end-start)/CYCS_PER_USEC);
//		rdtscll(start);
//		spin_usecs(sleep_val);
//		rdtscll(end);
//		printf("%dusec:%llu:%llu\n", sleep_val, end-start, (end-start)/CYCS_PER_USEC);
//		rdtscll(start);
//		spin_usecs(sleep_val);
//		rdtscll(end);
//		printf("%dusec:%llu:%llu\n", sleep_val, end-start, (end-start)/CYCS_PER_USEC);

	} else {
		printf("skip sleep_val spin test\n");
	}
//	construct_header(msg);
//	printf("Msg_size=(%d)\n", msg_size);

	start_timers();

	while (1) {
//		int i;
		
//		construct_header(msg);
		
		//if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&	
		if (sendto(fd, NULL, 0, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&	
		    errno != EINTR) {
			perror("sendto");
			return -1;
		}
		msg_sent++;
		
		//if (sleep_val) spin_usecs(sleep_val); 
		if (sleep_val) spin_iters(sleep_val); 
		debug("Message sent! (%d) msg_size (%d)\n", msg_sent, msg_size);
	}
	return 0;
}
