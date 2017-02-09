#include "unpthread.h"

static void *echoServer(void *); /* each thread executes this function */
static void *timeServer(void *);
static void timeRequest(int);
static void echoRequest(int);

void sig_pipe(int signo) {
  printf("SIGPIPE received\n");
  return;
}

int main(int argc, char **argv) {
  char sp1[] = "21696";
  char sp2[] = "21697";
  int port1 = 21696;
  int port2 = 21697;

  int maxfd;
  int nready, clientEcho[FD_SETSIZE], clientTime[FD_SETSIZE];
  ssize_t nEcho, nTime;
  fd_set rset, allset;

  int listenfdEcho, *iptrEcho;
  pthread_t tid;
  socklen_t addrlenEcho, lenEcho;
  struct sockaddr *cliaddrEcho;

  int listenfdTime, *iptrTime;
  //	pthread_t tid;
  socklen_t addrlenTime, lenTime;
  struct sockaddr *cliaddrTime;

  struct sockaddr_in echoservaddr, timeservaddr;

  Signal(SIGPIPE, sig_pipe);

  if (argc == 1) {
    // listenfdEcho = Tcp_listen(NULL, sp1, &addrlenEcho);
    // listenfdTime = Tcp_listen(NULL, sp2, &addrlenTime);

    listenfdEcho = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&echoservaddr, sizeof(echoservaddr));
    echoservaddr.sin_family = AF_INET;
    echoservaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoservaddr.sin_port = htons(port1);

    listenfdTime = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&timeservaddr, sizeof(timeservaddr));
    timeservaddr.sin_family = AF_INET;
    timeservaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    timeservaddr.sin_port = htons(port2);

    int oneEcho = 1;
    setsockopt(listenfdEcho, SOL_SOCKET, SO_REUSEADDR, &oneEcho,
               sizeof(oneEcho));

    int oneTime = 1;
    setsockopt(listenfdTime, SOL_SOCKET, SO_REUSEADDR, &oneTime,
               sizeof(oneTime));

    int flags1_;
    /* Set a socket as nonblocking */
    if ((flags1_ = fcntl(listenfdEcho, F_GETFL, 0)) < 0)
      err_sys("F_GETFL error");

    flags1_ |= O_NONBLOCK;
    if (fcntl(listenfdEcho, F_SETFL, flags1_) < 0)
      err_sys("F_SETFL error");

    int flags2_;
    /* Set a socket as nonblocking */
    if ((flags2_ = fcntl(listenfdTime, F_GETFL, 0)) < 0)
      err_sys("F_GETFL error");

    flags2_ |= O_NONBLOCK;
    if (fcntl(listenfdTime, F_SETFL, flags2_) < 0)
      err_sys("F_SETFL error");

    Bind(listenfdEcho, (SA *)&echoservaddr, sizeof(echoservaddr));
    Listen(listenfdEcho, LISTENQ);

    Bind(listenfdTime, (SA *)&timeservaddr, sizeof(timeservaddr));
    Listen(listenfdTime, LISTENQ);

  } else
    err_quit("usage: tcpserv01 [ <host> ] <service or port>");

  cliaddrTime = Malloc(addrlenTime);
  cliaddrEcho = Malloc(addrlenEcho);

  FD_ZERO(&allset);
  FD_SET(listenfdEcho, &allset);
  FD_SET(listenfdTime, &allset);

  if (listenfdEcho > listenfdTime)
    maxfd = listenfdEcho;
  else
    maxfd = listenfdTime;

  printf("%s\n\n", "waiting for connection");

  // use select to monitor echo and time service
  for (;;) {

    rset = allset; /* structure assignment */
    nready = Select(maxfd + 1, &rset, NULL, NULL, NULL);

    if (FD_ISSET(listenfdEcho, &rset)) { /* new client connection */
      int *iptrEcho2;
      lenEcho = addrlenEcho;
      iptrEcho2 = Malloc(sizeof(int));
      // *iptrEcho = Accept(listenfdEcho, cliaddrEcho, &lenEcho);

      *iptrEcho2 = accept(listenfdEcho, cliaddrEcho, &lenEcho);
      // if ( n  < 0 || errno == EPROTO || errno == ECONNABORTED || errno  ==
      // EWOULDBLOCK ||  errno == EINTR)
      // 	continue;
      if (*iptrEcho2 >= 0) {

        int flags1;
        /* Set a socket as blocking */
        if ((flags1 = fcntl(*iptrEcho2, F_GETFL, 0)) < 0)
          err_sys("F_GETFL error");
        flags1 &= ~O_NONBLOCK;
        if (fcntl(*iptrEcho2, F_SETFL, flags1) < 0)
          err_sys("F_SETFL error");

        Pthread_create(&tid, NULL, &echoServer, iptrEcho2);
      } else
        continue;
      // printf("echo server thread started, thread id: %d \n\n",tid);
    }

    if (FD_ISSET(listenfdTime, &rset)) { /* new client connection */
      int *iptrTime2;
      lenTime = addrlenTime;
      iptrTime2 = Malloc(sizeof(int));
      //	*iptrTime = Accept(listenfdTime, cliaddrTime, &lenTime);

      *iptrTime2 = accept(listenfdTime, cliaddrTime, &lenTime);

      // if ( n  < 0 || errno == EPROTO || errno == ECONNABORTED || errno  ==
      // EWOULDBLOCK ||  errno == EINTR)
      // 	continue;
      if (*iptrTime2 >= 0) {

        int flags2;
        /* Set a socket as blocking */
        if ((flags2 = fcntl(*iptrTime2, F_GETFL, 0)) < 0)
          err_sys("F_GETFL error");
        flags2 &= ~O_NONBLOCK;
        if (fcntl(*iptrTime2, F_SETFL, flags2) < 0)
          err_sys("F_SETFL error");

        Pthread_create(&tid, NULL, &timeServer, iptrTime2);
        // printf("time server thread started, thread id: %d \n\n",tid);
      } else
        continue;
    }
  }
}

static void *echoServer(void *arg) {
  int connfd;

  connfd = *((int *)arg);
  free(arg);

  Pthread_detach(pthread_self());

  printf("%s\n\n", "echo server thread started");

  echoRequest(connfd);
  Close(connfd); /* done with connected socket */
  return (NULL);
}

static void echoRequest(int sockfd) {
  ssize_t n;
  char buf[MAXLINE];

  //	printf("before read:   %s\n", buf);

  while (1) {
    memset(buf, 0, strlen(buf));
    n = read(sockfd, buf, MAXLINE);

    if (n > 0) {
      printf("message from client child process:   %s\n", buf);
      Writen(sockfd, buf, n);
    } else if (n == 0) {
      printf("echo client child process termination:   socket read returned "
             "with value 0\n\n");
      pthread_t tid;
      pthread_exit(&tid);
    } else if (n < 0 && errno == EINTR) {
      printf("error EINTR\n\n");
      continue;
    } else if (n < 0)
      err_sys("str_echo: read error");
  }
}

static void *timeServer(void *arg) {
  int connfd;

  connfd = *((int *)arg);
  free(arg);

  Pthread_detach(pthread_self());

  printf("%s\n\n", "time server thread started");

  timeRequest(connfd);
  Close(connfd); /* done with connected socket */
  return (NULL);
}

static void timeRequest(int sockfd) {
  ssize_t n;
  static char buf[MAXLINE];
  time_t ticks;
  static char buff[MAXLINE];

  //	Write(connfd, buff, strlen(buff));

  while (1) {

    int tmp = Readable_timeo(sockfd, 5);
    if (tmp == 0) {

      // n = Recvfrom(sockfd, buf, MAXLINE, 0, NULL, NULL);
      ticks = time(NULL);
      snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
      printf("current time   %s\n", buff);
      // Writen(sockfd, buff, strlen(buff));

      if (writen(sockfd, buff, strlen(buff)) < 0) {
        if (errno == EPIPE) {
          printf("%s\n\n", "epipe error");
        }
        pthread_t tid;
        pthread_exit(&tid);
      }

      continue;
    } else {
      //			n = Recvfrom(sockfd, buf, MAXLINE, 0, NULL,
      //NULL);
      n = recvfrom(sockfd, buf, MAXLINE, 0, NULL, NULL);

      if (n == 0) {
        printf("time client child process termination :    socket read "
               "returned with value 0\n\n");

        pthread_t tid;
        pthread_exit(&tid);
      } else if (n < 0) {
        if (errno == EWOULDBLOCK)
          fprintf(stderr, "socket timeout\n");
      }
    }
  }
}
