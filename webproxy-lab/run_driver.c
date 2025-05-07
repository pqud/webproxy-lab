#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int main() {
    struct timeval start, end;
    gettimeofday(&start, NULL);

    int ret = system("./driver.sh");

    gettimeofday(&end, NULL);

    double elapsed = (end.tv_sec - start.tv_sec)
                   + (end.tv_usec - start.tv_usec) / 1000000.0;

    printf("driver.sh 전체 실행 시간: %.3f 초 (종료 코드: %d)\n", elapsed, ret);

    return 0;
}
