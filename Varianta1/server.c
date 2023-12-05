#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_SIZE 2048
#define BUFFER_SIZE 104857600
#define MAX_QUEUE 10
#define THREAD_POOL_SIZE 5

typedef struct {
    int *queue;
    int front;
    int rear;
} ConnectionQueue;

typedef struct {
    int server_fd;
    ConnectionQueue *queue;
    sem_t *queue_sem;
} Server;

char *url_decode(const char *src) {
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    for (size_t i = 0; i < src_len; ++i) {
        if (src[i] == '%' && i + 2 < src_len) {
            char hex[3] = { src[i + 1], src[i + 2], '\0' };
            decoded[decoded_len++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            decoded[decoded_len++] = src[i];
        }
    }

    decoded[decoded_len] = '\0';

    return decoded;
}

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(file_ext, "png") == 0) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}

void build_http_response(const char *file_name,
                        const char *file_ext,
                        char *response,
                        size_t *response_len,
                        const char *post_data) {
    // if file doesn't exist
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "\r\n"
                 "404 Not Found");
        *response_len = strlen(response);
        return;
    }

    // get file size for Content-Length
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    const char *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n",
             mime_type);

    // If there's POST data, add it within the HTML content
    if (post_data) {
        strcat(header, "Content-Length: ");
        char post_length[20];  // Adjust the size if necessary
        snprintf(post_length, sizeof(post_length), "%ld", file_size + strlen(post_data));
        strcat(header, post_length);
        strcat(header, "\r\n");
        strcat(header, "\r\n");

        // Copy header to response buffer
        memcpy(response, header, strlen(header));
        *response_len = strlen(header);

        // Copy file to response buffer
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, response + *response_len, BUFFER_SIZE - *response_len)) > 0) {
            *response_len += bytes_read;
        }

        // Copy post data to response buffer after the file content
        memcpy(response + *response_len, post_data, strlen(post_data));
        *response_len += strlen(post_data);
    } else {
        // If there's no POST data, add Content-Length normally
        snprintf(header + strlen(header), BUFFER_SIZE - strlen(header),
                 "Content-Length: %ld\r\n\r\n", file_size);
        *response_len += strlen(header);

        // Copy header and file content to response buffer
        memcpy(response, header, *response_len);
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, response + *response_len, BUFFER_SIZE - *response_len)) > 0) {
            *response_len += bytes_read;
        }
    }

    free(header);
    close(file_fd);
}

void *handle_connection(void *server_void_ptr) {

    Server *server = (Server *)server_void_ptr;

    //char response[MAX_SIZE];

    while (1) {
        sem_wait(server->queue_sem);
        int client_fd = server->queue->queue[server->queue->front];
        server->queue->front = (server->queue->front + 1) % MAX_QUEUE;

        printf("Accepted connection, client_fd = %d\n", client_fd);

        while (1) { // Loop to handle multiple requests in a single connection
            char buffer[MAX_SIZE];
            ssize_t recv_size = recv(client_fd, buffer, MAX_SIZE, 0);

            if (recv_size <= 0) {
                break;
            }
            printf("Received request: %s\n", buffer);

            if (strncmp(buffer, "GET", 3) == 0) {
                char *get_token = "GET /";
                char *get_start = strstr(buffer, get_token);

                if (get_start != NULL) {
                // extract filename from request
                    char *file_name_start = get_start + strlen(get_token);
                    char *file_name_end = strchr(file_name_start, ' ');

                    if (file_name_end != NULL) {
                        // decode URL
                        *file_name_end = '\0';
                        char *url_encoded_file_name = file_name_start;

                        //printf("\n%s\n", file_name_start);
                        
                        char *file_name=url_decode(url_encoded_file_name);

                        printf("\n----------\n%s\n", file_name);
                        
                        // get file extension
                        char file_ext[32];
                        strcpy(file_ext, get_file_extension(file_name));

                        // build HTTP response
                        char *response = (char *)malloc(MAX_SIZE * 2 * sizeof(char));
                        size_t response_len;
                        build_http_response(file_name, file_ext, response, &response_len, NULL);

                        // send HTTP response to client
                        send(client_fd, response, response_len, 0);


                        free(response);
                        free(file_name);

                    }
                }
            

            } else if (strncmp(buffer, "POST", 4) == 0) {
                char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                char body[MAX_SIZE];
                strncpy(body, body_start, recv_size - (body_start - buffer));
                body[recv_size - (body_start - buffer)] = '\0';

                printf("Body: %s\n", body);

                // Build the HTTP response
                char response[MAX_SIZE * 2];
                size_t response_len;

                // Embed the body data into the HTML
                // char html[MAX_SIZE * 2];
                // snprintf(html, sizeof(html), "<!DOCTYPE html>\n<html>\n<head>\n<title>Data Display</title>\n</head>\n<body>\n<h1>Welcome to the Data Display Page</h1>\n<p id=\"data\">%s</p>\n</body>\n</html>", body);

                build_http_response("index2.html", "html", response, &response_len, body);

                printf("Response: %s\n", response);

                send(client_fd, response, response_len, 0);

                //printf("\n\n\n\nCombined: %s\n\n\n\n",response);

                close(client_fd);
            }



        }
    

            printf("Finished handling request, closing connection.\n");

            close(client_fd);

    }
}
        


int main() {
    int server_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    int reuse = 1; 
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Setarea opțiunii SO_REUSEADDR a eșuat");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(server_fd, (struct sockaddr *)&server_addr, addr_len) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, MAX_QUEUE) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port 8080...\n");

    ConnectionQueue queue = {.queue = malloc(sizeof(int) * MAX_QUEUE), .front = 0, .rear = 0};
    sem_t queue_sem;
    sem_init(&queue_sem, 0, 0);
    Server server = {.server_fd = server_fd, .queue = &queue, .queue_sem = &queue_sem};

    pthread_t thread_pool[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&thread_pool[i], NULL, handle_connection, &server);
    }

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        queue.queue[queue.rear] = client_fd;
        queue.rear = (queue.rear + 1) % MAX_QUEUE;
        sem_post(&queue_sem);
    }

    return 0;
}