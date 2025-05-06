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
void serve_static(int fd, char *filename, int filesize, char *version);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *version);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// 참고 : MAXLINE은 8192입니다. 2^13승!

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

  // 소켓을 위한 디스크립터 생성을 시도합니다.
  listenfd = Open_listenfd(argv[1]);
  // 이 코드는 서버위의 작동을 전제하기 때문에 무한루프 구문이 필요합니다.
  while (1)
  {
    // 클라이언트 정보 저장을 위한 공간을 초기화합니다.
    clientlen = sizeof(clientaddr);
    // 리스닝 소켓으로부터 연결 요청을 수락하여, 클라이언트와 통신할 새로운 소켓 디스크립터(connfd)를 할당합니다.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}



void doit(int fd)
{
  // 
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiagrgs[MAXLINE];
  rio_t rio;

  // 요청 라인을 읽은 다음 파싱합니다.
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request Headers\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  // GET 함수 여부를 확인합니다. strcasecmp는 대소문자를 가리지 않고 두 매개변수를 비교합니다.
  // 같은 내용인 경우에만 0이 됩니다. 즉, False인 경우 같은 글자이고, True이면 내용이 다릅니다.
  if(strcasecmp(method, "GET"))
  {   // method의 내용이 GET이 아닌 경우 이 코드를 실행합니다 :
      // 501, GET이 아닌 요청에 대해서 준비되지 않았습니다!
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method!");
    return;
  }
  read_requesthdrs(&rio);
  // request headers를 rio를 통해서 읽어들입니다. 

  // 3개의 매개변수를 던져서 정적 컨텐츠 여부를 확인합니다.
  // parse_uri의 반환 값은 1 또는 0 입니다.
  is_static = parse_uri(uri, filename, cgiagrgs);

  // 파일이 존재하지 않는 경우를 확인합니다.
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not Found", "Tiny couldn't find this file.");
    return;
  } 

  if(is_static) // 정적 컨텐츠에 대해 준비합니다.
  {
    // 파일이 일반 파일인지, 그리고 읽기 권한을 가졌는지를 검증합니다.
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 조건이 만족되면 정적 컨텐츠를 클라이언트에 전송합니다.
    serve_static(fd, filename, sbuf.st_size, version);
  }
  else // serve dynamic content
  {
    printf("gogo dynamic\n");
    // 파일이 실행가능한지, 정적 컨텐츠처럼 읽기 권한을 가졌는지를 검증합니다.
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program..");
      return;
    }

    // 조건이 만족되면 동적 컨텐츠를 클라이언트에 전송합니다. 
    serve_dynamic(fd, filename, cgiagrgs, version);
  }

}

// clienterror는 오류를 명확하게 처리하기 위한 페이지를 뱉는 함수이다.
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// Tiny 서버에서는 요청 헤더를 읽어오긴 하지만, 별 달리 무언가를 하진 않습니다.
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// Tiny 서버를 작성하면서 두 가지를 가정합니다.
// 1. 홈 디렉토리는 현재 디렉토리입니다.
// 2. 실행 파일의 홈 디렉토리는 ./cgi-bin으로 가정합니다.
// URI 내용에 cgi-bin이 포함되어 있다면 동적 컨텐츠 요청으로 간주합니다.
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // uri 내용에 cgi-bin 문자열이 없다면
  if (!strstr(uri, "cgi-bin")) // 정적 컨텐츠쪽의 분기를 실행합니다.
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);

    if(uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
      return 1;
  }
  else // cgi-bin 문자열을 확인한 분기점입니다. 동적 컨텐츠쪽의 분기를 실행합니다.
  {
    printf("detected!!\n");
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *version)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF], *mallocCh;

  // 파일 이름의 접미사를 검사하여 파일 타입을 정한다.
  get_filetype(filename, filetype); 

  // 응답 라인과 응답 헤더를 클라이언트에 전송한다.
  sprintf(buf, "%s 200 OK\r\n", version);
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close \r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  // 빈 줄이 헤더의 끝을 나타낸다는 점에 주목하기

  
  printf("Response headers:\n");
  printf("%s", buf);

  // filename을 읽기 모드로 열어서 파일 디스크립터를 확인한다.
  srcfd = Open(filename, O_RDONLY, 0);
  // 요청된 파일을 가상 메모리 영역에 매핑한다.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  

  // 더 이상 파일 디스크립터를 쓸 필요가 없으니 Close한다.
  // 이를 안 닫으면 메모리 누수가 난다.
  Close(srcfd);
  
  // srcp에서 시작하는 filesize 바이트가 클라이언트의 연결 디스크립터로 복사되도록 한다.
  Rio_writen(fd, srcp, filesize);

  // 매핑된 가상 메모리 영역을 해제한다.
  Free(srcp);
  //Munmap(srcp, filesize);
}

// 파일 타입을 정하는 함수!!
void get_filetype(char *filename, char *filetype)
{

  if(strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *version)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  // Return first part of HTTP response
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Serveraasdasd\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0)
  {
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
    
  }
  Wait(NULL);
}