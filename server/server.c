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
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define SQRT_REQUEST 0x01
#define TIME_REQUEST 0x02
#define SQRT_RESPONSE 0x01
#define TIME_RESPONSE 0x02
#define BUFFER_SIZE 1024
#define MAX_CLIENT_THREADS 16
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
int thread_count = 0;
void parameters_read(long int *SERVER_PORT,char *net_if,size_t size)
{
    FILE *config_file = fopen("server_config.txt", "r");
    if(config_file == NULL)
    {
        printf("1. Config file not found\n");
        exit(1);
    }
    //jakkis zeby byl domyslnie
    *SERVER_PORT = 8080;
    //domyslnie pusty interfejs
    strcpy(net_if, "");
    char line[256];
    while (fgets(line, sizeof(line), config_file) != NULL) {
        line[strcspn(line, "\n")] = 0;
        char *key = strtok(line, ":");
        if (key == NULL) continue;
        char *value = strtok(NULL, ":");
        if (value == NULL) continue;
        while (*value == ' ') value++;
        if(strcmp(key, "SERVER_PORT") == 0)
        {
            *SERVER_PORT = atol(value);
        }
        else if(strcmp(key, "NETWORK_INTERFACE") == 0)
        {
            strncpy(net_if, value, size - 1);
            net_if[size - 1] = '\0';
        }
    }
    printf("1. Config file read successfully\n");
    fclose(config_file);
}
typedef struct
{
    int socket_fd;
    struct sockaddr_in client_address;
}Client_t;
void setArrayToResponse(uint8_t *res, uint8_t rq_id, uint8_t response_type)
{
    res[0] = 0x01;
    res[1] = 0x00;
    res[2] = 0x00;
    res[3] = response_type;
    res[4] = rq_id;
}
void sqrt_request(int client_socket, uint8_t rq_id, const uint8_t *buff)
{
    uint64_t network_num;
    memcpy(&network_num, buff, sizeof(uint64_t));
    network_num = be64toh(network_num);
    double num;
    memcpy(&num, &network_num, sizeof(double));
    printf("%lf\n -> ",num);
    double result = sqrt(num);
    printf("%lf\n", result);
    uint64_t network_result;
    memcpy(&network_result, &result, sizeof(uint64_t));
    network_result = htobe64(network_result);
    uint8_t res[13];
    setArrayToResponse(res, rq_id, SQRT_RESPONSE);
    memcpy(&res[5], &network_result, sizeof(uint64_t));
    write(client_socket, res, 13);
}
void time_request(int client_socket, uint8_t rq_id)
{
    time_t curr = time(NULL);
    struct tm *time_info = localtime(&curr);
    char time_str[32];
    size_t tim_len=strftime(time_str, 32, "%Y-%m-%d %H:%M:%S", time_info);
    printf("%s\n", time_str);
    uint16_t network_tim_len = htons(tim_len);
    //chlopaki nie maja czasu na bigendian 
    uint8_t res[6 + tim_len];
    // uint8_t res[8 + tim_len];
    setArrayToResponse(res, rq_id, TIME_RESPONSE);
    res[5] = (uint8_t)tim_len;
    // memcpy(&res[5], &network_tim_len, sizeof(uint16_t));
    // memcpy(&res[7], time_str, tim_len);
    // write(client_socket, res, 7 + tim_len);
    memcpy(&res[6], time_str, tim_len);
    write(client_socket, res, 6 + tim_len);
}

void *client_thread(void *arg)
{
    Client_t *client = (Client_t *) arg;
    int client_socket = client->socket_fd;
    uint8_t buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while((bytes_read = read(client_socket, buffer, BUFFER_SIZE)) > 0)
    {
        size_t offset = 0;
        //kilka zapytan na raz
        while(offset < bytes_read)
        {
            uint8_t rq_id = buffer[offset + 4];
            switch(buffer[offset + 3])
            {
                case SQRT_REQUEST:
                    fprintf(stdout,"SQRT_REQUEST\n");
                    sqrt_request(client_socket, rq_id, &buffer[offset + 5]);
                    offset += 13;
                    break;
                case TIME_REQUEST:
                    fprintf(stdout,"TIME_REQUEST\n");
                    time_request(client_socket, rq_id);
                    offset += 5;
                    break;
                default:
                    printf("Invalid request\n");
                    offset++;
                    break;
            }
        }
    }
    close(client_socket);
    free(client);
    pthread_mutex_lock(&thread_count_mutex);
    thread_count--;
    pthread_mutex_unlock(&thread_count_mutex);
    return NULL;
}
//dodac wybor karty sieciowe
//timeout dla serwerow
int main()
{
    setbuf(stdout, NULL);
    setvbuf(stdout, NULL, _IONBF, 0);
    int server_socket;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    pthread_t client_threads[MAX_CLIENT_THREADS];
    //socket create
    long int PORT;
    char interface[16];
    parameters_read(&PORT,&interface,16);    
    //ipv4, tcp, 0 -- default tcp
    if((server_socket = socket(AF_INET, SOCK_STREAM, 0))==-1)
    {
        fprintf(stdout,"2. Socket creation failed\n");
        return -1;
    }
    else
    {
        fprintf(stdout,"2. Socket created successfully\n");
    }
    int socket_option = 1;
    //restart serwera zapobiega bleda so_reuseaddr zapobiega already in use
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,&socket_option, sizeof(socket_option)) < 0) {
        perror("3. Setsockopt failed\n");
        close(server_socket);
        return -1;
    }
    //ipv4
    server_address.sin_family = AF_INET;
    //dowolny adr
    server_address.sin_port = htons(PORT);
    if(strlen(interface) > 0)
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));

        strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
        //pobieranie adresu z interfejsu
        if (ioctl(server_socket, SIOCGIFADDR, &ifr) == -1) {
            fprintf(stdout,"3. ioctl failed for: %s\n",interface);
            fprintf(stdout,"Address is set to INADDR_ANY\n");
            server_address.sin_addr.s_addr = INADDR_ANY;
        }
        struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
        server_address.sin_addr = addr->sin_addr;
        fprintf(stdout,"3. Address is set to %s\n", inet_ntoa(server_address.sin_addr));
    }
    else
    {
        fprintf(stdout,"3. Address is set to INADDR_ANY\n");
        server_address.sin_addr.s_addr = INADDR_ANY;

    }
    //bind - przypisanie adresu do socketa
    if(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address))==-1)
    {
        fprintf(stdout,"4. Binding failed\n");
        close(server_socket);
        return -1;
    }
    else
    {
        fprintf(stdout,"4. Binding successful\n");
    }
    //listener na gniezdzie
    if(listen(server_socket, MAX_CLIENT_THREADS)==-1)
    {
        fprintf(stdout,"5. Listening failed\n");
        close(server_socket);
        return -1;
    }
    else
    {
        fprintf(stdout,"5. Listening successful\n");
    }
    fprintf(stdout,"6. all set up\n");
    fprintf(stdout,"--------Server is running--------\n");
    fprintf(stdout,"Server is listening on port %d\n", PORT);
    while(1)
    {
        fprintf(stdout,"Waiting for client connection...\n");
        socklen_t client_address_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if(client_socket == -1)
        {
            fprintf(stdout,"Failed to accept client connection\n");
            continue;
        }
        fprintf(stdout,"Client connected\n");
        Client_t *client = (Client_t *)malloc(sizeof(Client_t));
        client->socket_fd = client_socket;
        client->client_address = client_address;
        if(pthread_create(&client_threads[thread_count], NULL, client_thread, client) != 0)
        {
            fprintf(stdout,"Failed to create client thread\n");
            close(client_socket);
            free(client);
        }
        else
        {
            //odejmowanie wątkw.
            pthread_mutex_lock(&thread_count_mutex);
            thread_count++;
            pthread_mutex_unlock(&thread_count_mutex);
        }
    }
    close(server_socket);
    return 0;
}