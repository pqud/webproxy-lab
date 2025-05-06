/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *version, int flag);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *version);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

typedef struct _parsed_url{
  char *scheme;
  char *host;
  char *port;
  char *path;
  char *query;
  char *fragment;
  char *username;
  char *password;
}parsed_url;


int main(int argc, char **argv)
{
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
  while (1) //무한 서버 루프 실행 
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, //반복적으로 연결 요청 접수 
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit 트랜잭션 처리
    Close(connfd); // line:netp:tiny:close 자신 쪽의 연결 끝을 닫기.
  }
}


void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  int flag;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);



  /* Tiny는 GET 메소드만 지원한다. 따라서 클라이언트가 GET 외의 다른 메소드를 요텅하면, 에러메시지를 보내고 main으로 돌아간다.*/
  if(strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") != 0){ //strcasecmp는 두 함수가 동일하면 리턴 0, 다르면 1이다.
    clienterror(fd, method, "501", "Not implemented",
    "Tiny dose not implement this method");
    return;
  }


  if(strcasecmp(method, "GET")) flag=1;
  else flag=0;

  read_requesthdrs(&rio); // read HTTP request headers

  is_static=parse_uri(uri, filename, cgiargs); //동적 컨텐츠인지 정적 컨텐츠인지 확인
  if(stat(filename, &sbuf)<0){
    /* 지정한 파일의 다양한 속성정보(메타데이터)를 구조체에 채워준다.
    stat은 첫번째 인자 __file에 파일 경로를 받고, 두번째 인자 __buf에 파일 정보를 저장할 struct stat 구조체 주소를 받아 
    함수가 성공하면 0을 반환하고, 실패하면 -1을 반환한다.*/
    clienterror(fd, filename, "404", "Not found",
    "Tiny couldn't find this file.");
    return;
  }

  if(is_static){ //정적 컨텐츠라면
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      /* 파일이 일반 파일이 아니거나, 소유자가 읽기 권한이 없으면*/
      clienterror(fd, filename, "403", "Forbidden",
      "Tiny couldn't read the file.");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, version, flag);
  }
  else{ //동적 컨텐츠라면
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      /* 파일이 일반 파일이 아니거나, 소유자가 실행행 권한이 없으면*/
      clienterror(fd, filename, "403", "Forbidden",
      "Tiny couldn't run the CGI program.");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, version);
  }
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

void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

char *substr(const char *start, const char *end){
  size_t len=end-start;
  char *buf = (char *)malloc(len+1);
  strncpy(buf, start, len);
  buf[len]='\0';
  return buf;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  if(!strstr(uri, "cgi-bin")){ //uri에 cgi-bin이 없으면 정적 컨텐츠임
    strcpy(cgiargs,"");
    strcpy(filename, ".");
    strcat(filename, uri); //uri를 filename에 연결하고 널 문자로 결과 스트링 종료
    if(uri[strlen(uri)-1]=='/') strcat(filename, "home.html");
    //uri의 마지막 글자가 /면 filename에 home.html을 연결함. 
    return 1;
  }
  else{ //동적 콘텐츠
    ptr=index(uri, '?');
    if(ptr){
      strcpy(cgiargs, ptr+1);
      *ptr='\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *version, int flag){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXLINE];

  get_filetype(filename, filetype);
  sprintf(buf, "%s 200 OK\r\n", version);
  sprintf(buf, "%sServer:: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close \r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s",buf);

  srcfd=Open(filename, O_RDONLY, 0);
  // srcp=Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd,0);
  srcp=(char *)malloc(filesize);
  rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  if(!flag)
    Rio_writen(fd, srcp,filesize);
  // Munmap(srcp, filesize);
  free(srcp);
}

void get_filetype(char *filename, char *filetype){
  if(strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if(strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpeg");
  else if(strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char* version){
  char buf[MAXLINE], *emptylist[]={NULL};

  sprintf(buf, "%s 200 OK\r\n", version);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork()==0){
    setenv("QUERY_STRING",cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    execve(filename, emptylist, environ);
  }
  Wait(NULL);

}