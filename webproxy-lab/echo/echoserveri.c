#include "csapp.h"

void echo(int connfd);
int Open_listenfd(char *port);

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("server received %d byted\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2)
    {
        // 실행 시도 시 갯수가 안맞으면 안내 후 종료.
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // 포트번호를 매개변수로 Open_listenfd를 호출한다.
    listenfd = Open_listenfd(argv[1]);

    // 서버는 항상 클라이언트의 연결 요청을 대기하게끔한다.
    while (1)
    {
        // 클라이언트의 신상정보가 저장될 내용을 초기화한다.
        clientlen = sizeof(struct sockaddr_storage);
        // Accept 함수는 리스닝 소켓(listenfd)에 도착한 연결 요청을 수락합니다.
        // 수락된 연결은 새 소켓 디스크립터 connfd에 할당되며,
        // 이 디스크립터를 통해 클라이언트와 통신할 수 있습니다.

        // 동시에, 클라이언트의 네트워크 주소 정보가 clientaddr에 저장되고,
        // clientlen은 실제 주소 길이를 업데이트합니다.
        // (SA *)는 일반적인 소켓 주소 자료형(struct sockaddr)으로 캐스팅하기 위한 관례적인 방법입니다.

        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        echo(connfd);
        Close(connfd);
        // echo 함수는 클라이언트와의 통신을 담당하는 부분으로, 클라이언트로부터 받은 데이터를 그대로 다시 돌려보내는 간단한 에코(echo) 서비스를 구현합니다.
        // 이 함수 내부에서는 수신 및 전송 I/O 함수들이 호출되어, 클라이언트의 요청에 대해 응답을 제공할 것입니다.
        // 에코 서비스는 네트워크 프로그래밍의 기초적인 예제로 많이 사용됩니다.

        // 클라이언트와의 에코 통신이 끝난 후, 해당 연결 소켓을 닫아 리소스를 해제합니다.
        // 서버는 하나의 클라이언트와의 연결 처리를 완료하면, 다시 다음 연결 요청을 기다리기 위해 루프의 처음으로 돌아갑니다.
    }
}

int Open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, optval = 1;

    // memset 함수로 hints 구조체를 완전히 초기화한다.
    memset(&hints, 0, sizeof(struct addrinfo));

    // hints에 대한 상세 내용을 정의한다.
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_flags |= AI_NUMERICSERV;

    Getaddrinfo(NULL, port, &hints, &listp);
    // hints를 필터로, port가 반영 된 내용을 listp에 가져온다.



    
    // listp 내의 모든 아이템에 대해 시도한다.
    for (p = listp; p; p->ai_next)
    {
        // 소켓 디스크립터를 생성합니다.
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; // 음수가 나오면 소켓 생성 실패로, 다음 주소에 대해 진행한다.

        // Setsockopt를 사용해 SO_REUSEADDR을 설정함으로써, 서버 프로그램을 재실행할 때 이전 연결에서 사용된 주소와 포트를 바로 재사용할 수 있도록 합니다.
        // 만약 이전 연결이 TIME_WAIT 상태에 있어도 주소 재바인딩이 가능하게 됩니다.
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

        // 디스크립터가 생성
        if(bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; // 성공
        Close(listenfd); // 바인드 실패, 다음꺼 시도
    }

    // listp를 해제해서 가용 메모리로 복귀
    Freeaddrinfo(listp);
    if (!p) // 작동 되는게 없었다면 NULL로 추정됨
        return -1;
    
    // Make it a listening socket ready to accept connection requests
    if (listen(listenfd, LISTENQ) < 0)
    {
        // 마지막 시도때의 listenfd가 제대로 종료되지 않으면 Close를 해준다.
        // 그리고 제대로 된 연결이 이뤄지지 않았다는 것이기도 하니 -1을 반환시킨다.
        Close(listenfd);
        return -1;
    }
    return listenfd;
}