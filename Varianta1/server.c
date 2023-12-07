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
#include <ctype.h>
#include <sys/wait.h>

#define MAX_SIZE 4096
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

typedef struct {
    char *name;
    int *phone;
    char *color;
    char *qualities;
    char *income;
    char *sociability;
    char *data_from_cgi;
} date_formular;

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
    char lowercase_extension[strlen(file_ext) + 1];
    for (size_t i = 0; i < strlen(file_ext); i++) {
        lowercase_extension[i] = tolower(file_ext[i]);
    }
    lowercase_extension[strlen(file_ext)] = '\0';

    if (strcmp(lowercase_extension, "html") == 0 || strcmp(lowercase_extension, "htm") == 0) {
        return "text/html";
    } else if (strcmp(lowercase_extension, "txt") == 0) {
        return "text/plain";
    } else if (strcmp(lowercase_extension, "css") == 0) {
        return "text/css";
    } else if (strcmp(lowercase_extension, "js") == 0) {
        return "application/javascript";
    } else if (strcmp(lowercase_extension, "jpg") == 0 || strcmp(lowercase_extension, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(lowercase_extension, "png") == 0) {
        return "image/png";
    } else if (strcmp(lowercase_extension, "gif") == 0) {
        return "image/gif";
    } else if (strcmp(lowercase_extension, "pdf") == 0) {
        return "application/pdf";
    } else {
        return "application/octet-stream";
    }
}

void build_http_response(const char *file_name, const char *file_ext, char *response, size_t *response_len, const char *post_data) {

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
        size_t header_len = strlen(header);

        // Copy header to response buffer
        memcpy(response, header, header_len);
        *response_len = header_len;

        // Copy file content to response buffer
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, response + *response_len, BUFFER_SIZE - *response_len)) > 0) {
            *response_len += bytes_read;
        }
    }

    free(header);
    //close(file_fd);
}

void replace_plus_with_space(char *str) {
    for(int i = 0; str[i]; i++) {
        if(str[i] == '+') {
            str[i] = ' ';
        }
    }
}

void parse_input_string(const char *input, date_formular *data) {
    // Initialize structure members with default or blank values
    data->name = NULL;
    data->phone = NULL;
    data->color = NULL;
    data->qualities = NULL;
    data->income = NULL;
    data->sociability = NULL;
    data->data_from_cgi=NULL;

    char *token, *saveptr;
    char *input_copy = strdup(input); // Duplicate the input string for tokenization

    // Tokenize the input string using '&' and '=' as delimiters
    token = strtok_r(input_copy, "&=", &saveptr);

    while (token != NULL) {
        char *value = strtok_r(NULL, "&=", &saveptr);

        // Check if the value is not NULL and not empty or contains non-space characters before assigning it
        if (value != NULL) {
            replace_plus_with_space(value);

            if (strcmp(token, "nume") == 0) {
                data->name = strdup(value);
                printf("\n\n\nnume: %s\n\n\n", data->name);
            } else if (strcmp(token, "Telefon") == 0) {
                data->phone = malloc(sizeof(int));
                *data->phone = atoi(value);
            } else if (strcmp(token, "zona") == 0) {
                data->color = strdup(value);
            } else if (strcmp(token, "calitati") == 0) {
                data->qualities = strdup(value);
            } else if (strcmp(token, "venituri") == 0) {
                data->income = strdup(value);
            } else if (strcmp(token, "sociabil") == 0) {
                data->sociability = strdup(value);
            }
        }

        token = strtok_r(NULL, "&=", &saveptr);
    }

    free(input_copy);
}

char *print_parsed_data(const date_formular *data) {
    // Allocate memory for the output string
    char *output = (char *)malloc(MAX_SIZE); // Adjust the size accordingly

    // Format the output string
    snprintf(output, MAX_SIZE, "Name: %s\nPhone: %d\nColor: %s\nQualities: %s\nIncome: %s\nSociability: %s\nData from CGI: %s\n",
             data->name, (data->phone != NULL) ? *data->phone : 0, data->color,
             data->qualities, data->income, data->sociability, data->data_from_cgi);

    return output;
}

void* execute_script() {
    const char *cgi_script = "cgi-bin/ceva.sh";

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        fprintf(stderr, "Error creating pipe\n");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
        fprintf(stderr, "Error forking process\n");
    } else if (child_pid == 0) {

        close(pipefd[0]); // Close read end of the pipe

        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe

        int result = execl("/bin/bash", "bash", cgi_script, (char*)NULL);

        if (result == -1) {
            // Handle error
            fprintf(stderr, "Error executing CGI script\n");
            exit(EXIT_FAILURE);
        }
    } else {
        
        close(pipefd[1]); // Close write end of the pipe

        char *output = malloc(MAX_SIZE);
        if (output == NULL) {
            fprintf(stderr, "Error allocating memory\n");
            exit(EXIT_FAILURE);
        }

        read(pipefd[0], output, MAX_SIZE);

        int status;
        waitpid(child_pid, &status, 0);

        printf("\n----------\nWe've executed the script, this is the ouput: %s\n----------\n",output);
        return output;
    }
}

void *handle_connection(void *server_void_ptr) {

    Server *server = (Server *)server_void_ptr;

    while (1) {
        sem_wait(server->queue_sem);
        int client_fd = server->queue->queue[server->queue->front];
        server->queue->front = (server->queue->front + 1) % MAX_QUEUE;

        printf("\n----------\nAccepted connection, client_fd = %d\n----------\n", client_fd);

        while (1) { // Loop to handle multiple requests in a single connection
            char buffer[MAX_SIZE];
            ssize_t recv_size = recv(client_fd, buffer, MAX_SIZE, 0);

            if (recv_size <= 0) {
                break;
            }

            printf("\n----------\nReceived request: %s\n----------\n",  buffer);

            if (strncmp(buffer, "GET", 3) == 0) {
                char *get_token = "GET /";
                char *get_start = strstr(buffer, get_token);

                if (get_start != NULL) {
                // extract filename from request
                    char *file_name_start = get_start + strlen(get_token);
                    char *file_name_end = strchr(file_name_start, ' ');

                    if (file_name_end != NULL) {

                        printf("\n----------\nMethod: GET\n----------\n");

                        // decode URL
                        *file_name_end = '\0';
                        char *url_encoded_file_name = file_name_start;

                        //printf("\n%s\n", file_name_start);
                        
                        char *file_name=url_decode(url_encoded_file_name);

                        printf("\n----------\nFile name: %s\n----------\n", file_name);
                        
                        // get file extension
                        char file_ext[32];
                        strcpy(file_ext, get_file_extension(file_name));

                        printf("\n----------\nFile extension: %s\n----------\n", file_ext);

                        // build HTTP response
                        char *response = (char *)malloc(MAX_SIZE * 10 * sizeof(char));
                        size_t response_len;

                        // Reset the response buffer
                        memset(response, 0, MAX_SIZE * 10 * sizeof(char));

                        printf("\n----------\nTrying to build the response.\n----------\n");

                        build_http_response(file_name, file_ext, response, &response_len, NULL);

                        printf("\n----------\nThis is the reponse: \n %s\n----------\n",response);

                        printf("\n----------\nWe've built the response.\nTrying to send the response.\n----------\n");

                        // send HTTP response to client
                        send(client_fd, response, response_len, 0);

                        printf("\n----------\nThe response has been sent.\n----------\n");

                        free(response);
                        free(file_name);

                        //close(client_fd);

                    }
               }
            

            } else if (strncmp(buffer, "POST", 4) == 0) {

                char *cgi_output;

            if (strstr(buffer, "cgi-bin/") != NULL) {
                printf("\n----------\nAm intrat in if-ul de executare script\n----------\n");
                // Execute CGI script in a new thread
                pthread_t cgi_thread_id;
                pthread_create(&cgi_thread_id, NULL, execute_script, NULL);
                pthread_join(cgi_thread_id, (void**)&cgi_output); // Wait for the script to finish and capture its output

                //ceva = execute_script();
            }

                printf("\n----------\nDin script: %s\n----------\n", cgi_output);

                printf("\n----------\nMethod: POST\n----------\n");

                char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                char body[MAX_SIZE];

                strncpy(body, body_start, recv_size - (body_start - buffer));
                body[recv_size - (body_start - buffer)] = '\0';

                printf("\n----------\nBody: %s\n----------\n",body);

                // Build the HTTP response
                char response[MAX_SIZE * 10];
                size_t response_len;

                // Parsing the data
                date_formular data;

                parse_input_string(body, &data);

                data.data_from_cgi = cgi_output;

                build_http_response("index2.html", "html", response, &response_len, print_parsed_data(&data));

                printf("\n----------\nResponse: %s\n----------\n", response);

                send(client_fd, response, response_len, 0);

            }



        }
    

        printf("\n----------\nFinished handling request, closing connection.\n----------\n");

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