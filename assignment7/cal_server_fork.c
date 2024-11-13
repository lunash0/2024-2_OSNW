#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <signal.h>

#define PORT 3600

struct cal_data {
    int left_num;
    int right_num;
    char op;
    int result;
    int min;
    int max;
    short int error;
    char additional_info[30]; // course, id, name 중 하나 저장
};

int current_min = 2147483647; // 최댓값으로 초기화
int current_max = -2147483648; // 최솟값으로 초기화

void handle_client(int client_sockfd, int client_num) {
    struct cal_data rdata;
    int left_num, right_num, cal_result;
    short int cal_error;

    read(client_sockfd, (void *)&rdata, sizeof(rdata));
    cal_result = 0;
    cal_error = 0;

    left_num = ntohl(rdata.left_num);
    right_num = ntohl(rdata.right_num);

    switch (rdata.op) {
        case '+':
            cal_result = left_num + right_num;
            break;
        case '-':
            cal_result = left_num - right_num;
            break;
        case 'x':
            cal_result = left_num * right_num;
            break;
        case '/':
            if (right_num == 0) {
                cal_error = 2;
                break;
            }
            cal_result = left_num / right_num;
            break;
        default:
            cal_error = 1;
    }

    if (cal_error == 0) { // 오류가 없는 경우만 min과 max 갱신
        if (cal_result < current_min) current_min = cal_result;
        if (cal_result > current_max) current_max = cal_result;
    }

    rdata.result = htonl(cal_result);
    rdata.error = htons(cal_error);
    rdata.min = htonl(current_min);
    rdata.max = htonl(current_max);

    // 클라이언트 순서에 따라 course, id, name 중 하나를 선택
    if (client_num % 3 == 0) {
        strcpy(rdata.additional_info, "osnw2024");
    } else if (client_num % 3 == 1) {
        strcpy(rdata.additional_info, "32207316");
    } else if (client_num % 3 == 2) {
        strcpy(rdata.additional_info, "문승현");
    }

    // 수정된 출력 형식
    printf("Client %d: %d %c %d = %d (min: %d, max: %d), %s\n",
           client_num, left_num, rdata.op, right_num, cal_result, current_min, current_max,
           rdata.additional_info);

    write(client_sockfd, (void *)&rdata, sizeof(rdata));
    close(client_sockfd);
    exit(0); // 자식 프로세스 종료
}

int main(int argc, char **argv) {
    struct sockaddr_in server_addr, client_addr;
    int server_sockfd, client_sockfd;
    int addr_len;
    int client_num = 0;

    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error ");
        return 1;
    }

    memset((void *)&server_addr, 0x00, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error ");
        return 1;
    }

    if (listen(server_sockfd, 5) == -1) {
        perror("Error ");
        return 1;
    }

    signal(SIGCHLD, SIG_IGN); // 좀비 프로세스 방지

    printf("Server is running...\n");

    while (1) {
        addr_len = sizeof(client_addr);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sockfd == -1) {
            perror("Error ");
            continue;
        }

        printf("New Client Connect : %s\n", inet_ntoa(client_addr.sin_addr));
        client_num++;

        if (fork() == 0) { // 자식 프로세스
            close(server_sockfd); // 자식 프로세스는 서버 소켓 닫기
            handle_client(client_sockfd, client_num);
        } else { // 부모 프로세스
            close(client_sockfd); // 부모 프로세스는 클라이언트 소켓 닫기
        }
    }

    close(server_sockfd);
    return 0;
}
