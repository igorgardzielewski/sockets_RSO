#define CONFIG_FILE "client_config.txt"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
//te definy sa zle popraw je bo sa takie same
#define SQRT_REQUEST 0x01
#define TIME_REQUEST 0x02
#define SQRT_RESPONSE 0x01
#define TIME_RESPONSE 0x02
#define BUFFER_SIZE 1024
typedef struct
{
    int socket;
    char server_ip[16];
    long int server_port;
}ConnectionInfo;
void empty3(uint8_t buff[])
{
    buff[0] = 0x00;
    buff[1] = 0x00;
    buff[2] = 0x00;
}
void sqrt_req(ConnectionInfo *conn, uint8_t rq_id, double num)
{
    if(num<0)
    {
        printf("5. Invalid request\n");
        return;
    }
    uint64_t network_num;
    memcpy(&network_num, &num, sizeof(uint64_t));
    //host to big endian z little endian lub big endian do host
    network_num = htobe64(network_num);
    uint8_t request[13];
    empty3(request);
    request[3] = SQRT_REQUEST;
    request[4] = rq_id;
    memcpy(&request[5], &network_num, sizeof(uint64_t));
    write(conn->socket, request, 13);
}
void time_req(ConnectionInfo *conn, uint8_t rq_id)
{
    uint8_t request[5];
    empty3(request);
    request[3] = TIME_REQUEST;
    request[4] = rq_id;
    write(conn->socket, request, 5);
}
void response(ConnectionInfo *conn)
{
    //musi byc buffer bo na 2 redy podzielone nie dziala
    uint8_t buff[BUFFER_SIZE];
    ssize_t bytes_read = read(conn->socket, buff, BUFFER_SIZE);
    if(bytes_read < 6)
    {
        printf("5. Invalid response\n");
        return;
    }

    size_t offset = 0;
    while(offset < bytes_read)
    {
        uint8_t isResponse = buff[offset];
        if(isResponse != 0x01)
        {
            printf("5. Invalid response\n");
            return;
        }

        uint8_t response_Type = buff[offset + 3];
        uint8_t rq_id = buff[offset + 4];

        switch(response_Type)
        {
            case SQRT_RESPONSE:
            {
                uint64_t result;
                memcpy(&result, &buff[offset + 5], sizeof(uint64_t));
                result = be64toh(result);
                double res;
                memcpy(&res, &result, sizeof(double));
                printf("--------\n");
                printf("Response type: SQRT_RESPONSE\n");
                printf("Request ID: %d\n", rq_id);
                printf("Result: %lf\n", res);
                printf("--------\n");
                offset += 13;
                break;
            }
            case TIME_RESPONSE:
            {
                uint16_t time_len;
                memcpy(&time_len, &buff[offset + 5], sizeof(uint8_t));
                time_len = time_len;
                char time_str[32];
                memcpy(time_str, &buff[offset + 6], time_len);
                time_str[time_len] = '\0';
                printf("--------\n");
                printf("Response type: TIME_RESPONSE\n");
                printf("Request ID: %d\n", rq_id);
                printf("Time: %s\n", time_str);
                printf("--------\n");
                offset += 6 + time_len;
                break;
            }
            default:
            {
                printf("5. Invalid response\n");
                return;
            }
        }
    }
}
//read config file
//ATTRIBUTE:VALUE
//ex SERVER_IP:127.0.0.1
//ex SERVER_PORT:9090
void parameters_read(ConnectionInfo *conn)
{
    FILE *config_file = fopen(CONFIG_FILE, "r");
    if(config_file == NULL)
    {
        printf("1. Config file not found\n");
        exit(1);
    }
    char line[256];
    while(fgets(line, sizeof(line), config_file))
    {
        char *key = strtok(line, ":");
        char *value = strtok(NULL, ":");
        if(strcmp(key, "SERVER_IP") == 0)
        {
            value[strcspn(value, "\n")] = 0;
            strcpy(conn->server_ip, value);
        }
        else if(strcmp(key, "SERVER_PORT") == 0)
        {
            conn->server_port = atol(value);
        }
    }
    printf("1. Config file read successfully\n");
    fclose(config_file);
}
int set_up_connection(ConnectionInfo *conn)
{
    parameters_read(conn);
    //IPV4, TCP, 0 -- default tcp
    conn->socket = socket(AF_INET, SOCK_STREAM, 0);
    if(conn->socket == -1)
    {
        printf("2. Socket creation failed\n");
        return -1;
    }
    else
    {
        printf("2. Socket created successfully\n");
    }
    struct sockaddr_in server_address;
        //struct ipv4
    server_address.sin_family = AF_INET;
        //htons do big endian

    server_address.sin_port = htons(conn->server_port);
        
    //konwertuje adres z postaci string na binarna
    if(inet_pton(AF_INET, conn->server_ip, &server_address.sin_addr)<=0)
    {
        printf("3. Invalid address\n");
        return -1;
    }
    printf("3. Address is valid\n");
    if(connect(conn->socket, (struct sockaddr *)&server_address, sizeof(server_address))<0)
    {
        printf("4. Connection failed\n");
        return -1;
    }
    else
    {
        printf("4. Connection established\n");
    }
    return 0;
}
int main()
{
    ConnectionInfo conn;
    if(set_up_connection(&conn) == -1)
    {
        return -1;
    }
    //requst
    double num;
    int choice;
    while(1)
    {
        printf("Choose option:\n");
        printf("1. Sqrt request\n");
        printf("2. Time request\n");
        printf("0. Exit\n");
        if(1!=scanf("%d", &choice))
        {
            printf("5. Invalid input\n");
            while(getchar() != '\n');
            continue;
        }
        switch(choice)
        {
            case 1:
                printf("Enter number: ");
                if(1!=scanf("%lf", &num))
                {
                    printf("5. Invalid input\n");
                    while(getchar() != '\n');
                    continue;
                }
                sqrt_req(&conn, 1, num);
                response(&conn);
                break;
            case 2:
                time_req(&conn, 2);
                response(&conn);
                break;
            case 0:
                close(conn.socket);
                return 0;
            default:
                printf("Invalid choice\n");
                continue;
        }
    }
    // sqrt_req(&conn,1, 16.0);
    // response(&conn);
    // time_req(&conn, 2);
    // response(&conn);
    // sqrt_req(&conn, 3, 25.0);
    // response(&conn);
    // close(conn.socket);
    return 0;
}