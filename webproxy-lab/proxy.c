#include <stdio.h>
#include "csapp.h"
#include "cache.h"
#include <time.h>

clock_t start,end;
double elapsed;

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#if IS_LOCAL_TEST
int is_local_test = 1;
#else
int is_local_test = 0;
#endif

void *thread(void *vargp);
void format_http_header(rio_t *client_rio, char *path, char *hostname, char *other_header);
void read_requesthdrs(rio_t *rp, char *host_header, char *other_header);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void handle_client(int clientfd);
void sigint_handler(int sig);


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


void flush_gprof() {
  // gprof용 gmon.out flush 보장
  printf("[INFO] Flushing gprof output...\n");
  _exit(0); // _exit으로 명확한 종료
}

int main(int argc, char **argv)
{
  atexit(flush_gprof);
  start=clock();
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); //듣기 소켓 오픈!
  init_cache();
  signal(SIGINT, sigint_handler); // 시그널 핸들러는 가능한 빨리
  while (1) //무한 서버 루프 실행 
  {
    int* connfd_p = Malloc(sizeof(int));
    *connfd_p = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    pthread_t tid;
    pthread_create(&tid, NULL, thread, connfd_p);
  }

}



void handle_client(int clientfd){
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host_header[MAXLINE], other_header[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  char request_buf[MAXLINE], response_buf[MAXLINE];
  rio_t client_rio, server_rio;
  int flag;
  int serverfd;

  //1. 요청 라인 읽기
  Rio_readinitb(&client_rio, clientfd);
  Rio_readlineb(&client_rio, buf, MAXLINE);
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);


  if(strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") != 0){ //strcasecmp는 두 함수가 동일하면 리턴 0, 다르면 1이다.
    clienterror(clientfd, method, "501", "Not implemented",
    "Tiny dose not implement this method");
    close(clientfd);
    return;
  }

  //2. URI 파싱
  parse_uri(uri, hostname, port, path);

  //3. 헤더 읽고
  read_requesthdrs(&client_rio, host_header, other_header); // read HTTP request headers
  // HTTP 1.1->HTTP 1.0으로 변경
  format_http_header(request_buf, path, hostname, other_header);

  // NEW! : 캐시 조회
  char cache_buf[MAX_OBJECT_SIZE];
  int cache_size;
  if (find_cache(uri, cache_buf, &cache_size)) {
    Rio_writen(clientfd, cache_buf, cache_size);
    Close(clientfd);
    return;
  }


  //4. 서버 연결
  serverfd = Open_clientfd(hostname, port);
  if(serverfd<0) return;

  //5. 서버로 요청 전송
  Rio_writen(serverfd, request_buf, strlen(request_buf));

  //6. 응답 수신+ 클라이언트 전달 + 캐시 누적
  char data_buf[MAX_OBJECT_SIZE];
  int total_size = 0;
  ssize_t n;
  Rio_readinitb(&server_rio, serverfd);

  while ((n = Rio_readnb(&server_rio, response_buf, MAXBUF)) > 0) {
    if (total_size + n <= MAX_OBJECT_SIZE)
      memcpy(data_buf + total_size, response_buf, n);
      total_size += n;
      Rio_writen(clientfd, response_buf, n);
    }
  Close(serverfd);

  //캐시 저장
  write_cache(uri, data_buf, total_size );
}

void read_requesthdrs(rio_t *rp, char *host_header, char *other_header){
  char buf[MAXLINE];

  host_header[0]='\0';
  other_header[0]='\0';

  while(Rio_readlineb(rp, buf, MAXLINE) > 0 && strcmp(buf, "\r\n")){
    //buf에 남은 자리가 있고, buf가 "\r\n"으로 마지막에 도달하지 않았으면 계속 읽음
    if(!strncasecmp(buf, "Host:",5)){
      //Host만 따로 저장해주면 되서? 
      strcpy(host_header, buf);
    }
    else if(!strncasecmp(buf, "User-Agent:", 11)||!strncasecmp(buf, "Connection:", 11)||!strncasecmp(buf, "Proxy-Connection:", 17)){
      //얘들은 버림
      continue;;
    }else{
      //그 외의 헤더는 다른곳에 저장해둠 
      strcat(other_header, buf);
    }
  }
}

void parse_uri(char *uri, char *hostname, char *port, char *path) {
  char *hostbegin, *hostend, *portbegin, *pathbegin;
  int hostlen, portlen, pathlen;

  // 1. "http://" 스킴 건너뛰기
  hostbegin = strstr(uri, "//");
  if (hostbegin != NULL)
      hostbegin += 2;
  else
      hostbegin = (char *)uri;

  // 2. path 위치 찾기
  pathbegin = strchr(hostbegin, '/');
  if (pathbegin != NULL) {
      hostend = pathbegin;
      pathlen = strlen(pathbegin);
      strncpy(path, pathbegin, pathlen);
      path[pathlen] = '\0';
  } else {
      hostend = hostbegin + strlen(hostbegin);
      path[0] = '\0'; // path 없음
  }

  // 3. port 위치 찾기 (hostend 범위 내에서)
  portbegin = strchr(hostbegin, ':');
  if (portbegin != NULL && portbegin < hostend) {
      hostlen = portbegin - hostbegin;
      strncpy(hostname, hostbegin, hostlen);
      hostname[hostlen] = '\0';

      portbegin++; // ':' 다음부터
      portlen = hostend - portbegin;
      strncpy(port, portbegin, portlen);
      port[portlen] = '\0';
  } else {
      hostlen = hostend - hostbegin;
      strncpy(hostname, hostbegin, hostlen);
      hostname[hostlen] = '\0';
      port[0] = '\0'; // 포트 없음
  }

  if (strlen(port) == 0) {
    strcpy(port, "80");
  }
}
//format_http_header(&client_rio, path, host_header, other_header)
void format_http_header(rio_t *client_rio, char *path, char *hostname, char *other_header){
  sprintf( client_rio,
  "GET %s HTTP/1.0\r\n"
  "Host: %s\r\n"
  "%s"
  "Connection: close\r\n"
  "Proxy-Connection: close\r\n"
  "%s"
  "\r\n",
  path, hostname, user_agent_hdr, other_header);
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
  char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));

}

void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Free(vargp);
  pthread_detach(pthread_self()); // join 필요 없음
  handle_client(connfd);
  Close(connfd);
  debug_print_cache();
  return NULL;
}

void sigint_handler(int sig) {
  deinit_cache();
  end = clock();
  elapsed = (double)(end - start) / CLOCKS_PER_SEC;
  printf("전체 실행 시간: %f 초\n", elapsed);
  fflush(stdout);  // <- 추가!
  exit(0);
}