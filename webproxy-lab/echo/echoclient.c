#include "csapp.h"

int Open_clientfd(char *hostname, char *port);

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if(argc != 3)
    {
        // arg 갯수가 정확히 주어지지 못하면..
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        // 실행이 되지만 즉시 종료된다.
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    // Open_clientfd로 연결을 시도한다.
    // 성공하면 리턴값이 소켓 디스크립터가 된다.
    clientfd = Open_clientfd(host, port);

    // robust I/O 라이브러리의 버퍼를 초기화한다.
    // 이후의 네트워크 데이터 읽기 작업 시, 
    // 오류나 부분적(read partial)인 데이터를 대비해 내부 버퍼를 활용하여 보다 안정적으로 데이터를 처리합니다.
    Rio_readinitb(&rio, clientfd);

    /*
    사용자 입력 읽기:
    Fgets();로 표준 입력에서 한 줄씩 데이터를 읽는다.
        사용자가 EOF(예: Ctrl-D)를 입력할 때까지 지속적으로 실행된다.

    서버로 데이터 전송:
        Rio_writen();는 읽어온 문자열을 서버로 전송한다.
    이 함수는 모든 바이트가 전송될 때까지 내부적으로 반복 호출할 수 있어,
    네트워크 특성상 한 번에 데이터가 다 전송되지 않는 문제(짧은 쓰기 문제)를 방지한다.

    서버로부터 응답 읽기:
        Rio_readlineb();는 서버가 응답하는 한 줄의 데이터를 읽어온다.
    내부 버퍼를 활용하여 줄 단위로 데이터를 안전하게 받아오며,
    줄 바꿈 문자까지 완전히 읽으면 반환한다.

    응답 출력:
    Fputs(buf, stdout);로 읽은 데이터를 표준 출력에 출력한다.
    */

    while (Fgets(buf,MAXLINE,stdin) != NULL)
    {
        Rio_writen(clientfd, buf, strlen(buf));
        Rio_readlineb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);
    }
    Close(clientfd);
    exit(0);
}



int Open_clientfd(char *hostname, char *port)
{
    int clientfd;
    struct addrinfo hints, *listp, *p;

    memset(&hints, 0, sizeof(struct addrinfo));
    // memset을 사용하여 hints 구조체를 0으로 초기화한다.
    // 이는 사용하지 않는 필드를 명확히 0으로 설정하여 예상치 못한 동작을 방지한다.
    hints.ai_socktype = SOCK_STREAM;
    // TCP와 같이 신뢰성 있는 연결 지향형 소켓을 생성하겠다는 의미한다.
    hints.ai_flags = AI_NUMERICSERV;
    // port 인자가 문자열로 된 숫자임을 보장하여, 서비스 이름을 호스트가 알아서 변환해 주는 과정을 생략합니다.
    // Numeric Serv!
    hints.ai_flags |= AI_ADDRCONFIG;
    // 시스템에 실제로 구성된 주소 체계(예, IPv4 또는 IPv6)에 맞는 결과만 반환하도록 합니다.
    Getaddrinfo(hostname, port, &hints, &listp);
   // hostname과 port에 대응하는 주소 정보를 linked list 형태로 받아온다.
   // hostname, port를 hints에 던져서 필터링 된 걸 listp에 주르륵 받아옴.


    // listp에 아이템을 받아왔으니 하나씩 시도한다.
    for( p = listp; p; p = p->ai_next)
    {
        // 소켓 디스크립터를 생성한다.
        // 소켓 함수에 구조체 포인터 p를 담아서 생성을 시도하는데, 실패 시 음수를 반환한다.
        if((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; // 실패하면 다른 아이템에 대해서 시도한다.

        // 서버에 연결을 시도한다. 만들어진 소켓 디스크립터와 남은 내용들
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break; // 성공!

        // 연결 실패, 다른 서버에 시도한다.
        // 소켓을 수동으로 닫아주는 습관이 좋다.
        Close(clientfd); 

    }

    // 다 했으면 정리를 해야한다. listp에 대한 내용을 Free List화 한다.
    Freeaddrinfo(listp);

    // 모든 접속 시도 실패. for문을 다 돌았다면 접속을 단 한번도 성공하지 못한 것이다. 그리고 p가 NULL이 되어 끝나겠지..
    if (!p)
        return -1;
    else // 그렇진 않은경우
        return clientfd;
}