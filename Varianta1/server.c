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
#include <sys/epoll.h>

#define MAX_SIZE 4096
#define BUFFER_SIZE 104857600
#define MAX_QUEUE 10
#define THREAD_POOL_SIZE 5

char *fileName;

//pthread_mutex_t mutex;

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

date_formular data_formular;

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
    const char *question_mark = strchr(file_name, '?');

    // Verificare dacă există un punct și este înaintea semnului întrebare
    if (!dot || (question_mark && dot > question_mark)) {
        return "";
    }

    // Determină lungimea extensiei
    size_t extension_length = (question_mark) ? (size_t)(question_mark - dot - 1) : strlen(dot + 1);

    // Creează un șir nou pentru a stoca extensia
    char *extension = malloc(extension_length + 1);
    if (!extension) {
        // Tratare eroare la alocare de memorie
        return "";
    }

    // Copiază extensia în noul șir
    strncpy(extension, dot + 1, extension_length);
    extension[extension_length] = '\0';  // Adaugă terminatorul de șir

    return extension;
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
        char post_length[MAX_SIZE];
        snprintf(post_length, sizeof(post_length), "%ld", file_size + strlen(post_data));
        strcat(header, post_length);
        strcat(header, "\r\n");
        strcat(header, "\r\n");

        strcat(response, "?");
        strcat(response, post_data);

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

        response[*response_len] = '\0';
        *response_len += 1;
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

        response[*response_len] = '\0';
        *response_len += 1;
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
                //printf("\n\n\nnume: %s\n\n\n", data->name);
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
    printf("\n----------\nSuntem in print parsed data, inainte e a face raspunsul\n----------\n");
    // Allocate memory for the output string
    char *output = (char *)malloc(MAX_SIZE); // Adjust the size accordingly

    // Format the output string
    snprintf(output, MAX_SIZE, "Name: %s\nPhone: %d\nColor: %s\nQualities: %s\nIncome: %s\nSociability: %s\nData from CGI: %s\n",
             data->name, (data->phone != NULL) ? *data->phone : 0, data->color,
             data->qualities, data->income, data->sociability, (data->data_from_cgi != NULL) ? data->data_from_cgi : "Nu am executat un script.");

    return output;
}

void* execute_script() {
    char *token = fileName;
    if (strncmp(token, "cgi-bin/", 8) != 0)
    {
        fprintf(stderr, "Error extracting the cgi name. Token=%s, Filename=%s\n", token, fileName);
        exit(EXIT_FAILURE);
    }
            printf("Token=%s, Filename=%s\n", token, fileName);

    const char *cgi_script = token;

    char *question = strchr(cgi_script, '?');
    if (question != NULL)
    {
        *question = '\0';
        question++;
    }

    //pthread_mutex_lock(&mutex);

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

        if (question != NULL)
        {
            setenv("QUERY_STRING", question, 1);
        }

        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe

        int result = execl("/bin/bash", "bash", cgi_script, (char*)NULL);
        //execve

        if (result == -1) {
            fprintf(stderr, "Error executing CGI script\n");
            exit(EXIT_FAILURE);
        }
    } else {
        
        close(pipefd[1]); // Close write end of the pipe

        char *output = malloc(MAX_SIZE*sizeof(char));
        if (output == NULL) {
            fprintf(stderr, "Error allocating memory\n");
            exit(EXIT_FAILURE);
        }

        wait(NULL);

        ssize_t bytesRead = read(pipefd[0], output, MAX_SIZE - 1);
        if (bytesRead == -1) {
            fprintf(stderr, "Error reading from pipe\n");
            free(output);
            exit(EXIT_FAILURE);
        }


        
        
        output[bytesRead] = '\0'; // Null-terminate the output

        printf("\n----------\nWe've executed the script, this is the ouput: %s\n----------\n",output);


        //pthread_mutex_unlock(&mutex);

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
            char bufferCopy[MAX_SIZE];
            ssize_t recv_size = recv(client_fd, buffer, MAX_SIZE, 0);
            strcpy(bufferCopy,buffer);

            if (recv_size <= 0) {
                break;
            }

            printf("\n----------\nReceived request: %s\n----------\n",  buffer);

            if (strncmp(buffer, "GET", 3) == 0) {
                char *get_token = "GET /";
                char *get_start = strstr(bufferCopy, get_token);

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
                        
                        fileName=url_decode(url_encoded_file_name);

                        printf("\n----------\nFile name: %s\n----------\n", fileName);
                        
                        // get file extension
                        char file_ext[32];
                        strcpy(file_ext, get_file_extension(fileName));

                        printf("\n----------\nFile extension: %s\n----------\n", file_ext);


                        // build HTTP response
                        char *responseGet = (char *)malloc(MAX_SIZE * 10 * sizeof(char));
                        size_t response_len;

                        // Reset the response buffer
                        memset(responseGet, 0, MAX_SIZE * 10 * sizeof(char));


                        char *cgi_output = malloc(MAX_SIZE);
                        cgi_output = NULL;


                        if (strstr(bufferCopy, "cgi-bin/") != NULL) {

                            //pthread_mutex_init(&mutex, NULL);

                            //char *queryString = getenv("QUERY_STRING");
                            
                            //printf("\n----------\nAm extras query string-ul acesta: %s\n----------\n", queryString);

                            printf("\n----------\nAm intrat in if-ul de executare script, pentru pagina %s\n----------\n", fileName);
                            // Execute CGI script in a new thread
                            pthread_t cgi_thread_id;
                            pthread_create(&cgi_thread_id, NULL, execute_script, NULL);
                            pthread_join(cgi_thread_id, (void**)&cgi_output); // Wait for the script to finish and capture its output

                            //pthread_mutex_destroy(&mutex);


                            printf("\n----------\nDin script: %s\n----------\n", cgi_output);

                            printf("\n----------\nBuffer: %s\n----------\n",buffer);

                            char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                            char body[MAX_SIZE];

                            strncpy(body, body_start, recv_size - (body_start - buffer));
                            body[recv_size - (body_start - buffer)] = '\0';

                            printf("\n----------\nBody: %s\n----------\n",body);

                            //parse_input_string(body, &data_formular);

                            data_formular.data_from_cgi = cgi_output;

                            build_http_response("index3.html", "html", responseGet, &response_len, data_formular.data_from_cgi);
                                                        
                            send(client_fd, responseGet, response_len, 0);

                            printf("\n----------\nThe response has been sent.\n----------\n");

                        }
                        else
                        {

                            printf("\n----------\nTrying to build the response.\n----------\n");

                            build_http_response(fileName, file_ext, responseGet, &response_len, NULL);

                            printf("\n----------\nThis is the reponse: \n %s\n----------\n",responseGet);

                            printf("\n----------\nWe've built the response.\nTrying to send the response.\n----------\n");

                            // send HTTP response to client
                            send(client_fd, responseGet, response_len, 0);

                            printf("\n----------\nThe response has been sent.\n----------\n");

                        }


                        free(responseGet);
                        free(fileName);
                        free(cgi_output);
                        data_formular.data_from_cgi=NULL;

                        //close(client_fd);

                    }
               }
            

            } else if (strncmp(buffer, "POST", 4) == 0) {

                    char *cgi_output = malloc(MAX_SIZE);
                    cgi_output = NULL;

                    char *get_token = "POST /";
                    char *get_start = strstr(bufferCopy, get_token);

                    char *file_name_start = get_start + strlen(get_token);
                    char *file_name_end = strchr(file_name_start, ' ');

                    printf("\n----------\nMethod: POST\n----------\n");

                    // decode URL
                    *file_name_end = '\0';
                    char *url_encoded_file_name = file_name_start;

                    //printf("\n%s\n", file_name_start);
                    
                    fileName=url_decode(url_encoded_file_name);

                    printf("\n----------\nFile name: %s\n----------\n", fileName);
                    
                    // get file extension
                    char file_ext[32];
                    strcpy(file_ext, get_file_extension(fileName));

                    printf("\n----------\nFile extension: %s\n----------\n", file_ext);


                    if (strstr(bufferCopy, "cgi-bin/") != NULL) {

                    //pthread_mutex_init(&mutex, NULL);

                    //char *queryString = getenv("QUERY_STRING");
                    
                    //printf("\n----------\nAm extras query string-ul acesta: %s\n----------\n", queryString);

                    printf("\n----------\nAm intrat in if-ul de executare script, pentru pagina %s\n----------\n", fileName);
                    // Execute CGI script in a new thread
                    pthread_t cgi_thread_id;
                    pthread_create(&cgi_thread_id, NULL, execute_script, NULL);
                    pthread_join(cgi_thread_id, (void**)&cgi_output); // Wait for the script to finish and capture its output

                    //pthread_mutex_destroy(&mutex);

                    }

                    printf("\n----------\nDin script: %s\n----------\n", cgi_output);

                    printf("\n----------\nBuffer: %s\n----------\n",buffer);

                    char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                    char body[MAX_SIZE];

                    strncpy(body, body_start, recv_size - (body_start - buffer));
                    body[recv_size - (body_start - buffer)] = '\0';

                    printf("\n----------\nBody: %s\n----------\n",body);

                    // Build the HTTP response
                    char *responsePost=malloc(MAX_SIZE * 10*sizeof(char));
                    size_t response_len;

                    //responsePost=NULL;

                    parse_input_string(body, &data_formular);

                    data_formular.data_from_cgi = cgi_output;

                    build_http_response("index2.html", "html", responsePost, &response_len, print_parsed_data(&data_formular));

                    printf("\n----------\nResponse in post: %s\n----------\n", responsePost);

                    send(client_fd, responsePost, response_len, 0);
                    

                    free(fileName);
                    free(cgi_output);
                    free(responsePost);
                    data_formular.data_from_cgi=NULL;
                    
            } else if (strncmp(buffer, "PUT", 3) == 0) {

                char *get_token = "PUT";
                char *get_start = strstr(bufferCopy, get_token);

                char *file_name_start = get_start + strlen(get_token);
                char *file_name_end = strchr(file_name_start, ' ');

                printf("\n----------\nMethod: PUT\n----------\n");

                // decode URL
                *file_name_end = '\0';
                char *url_encoded_file_name = file_name_start;

                //printf("\n%s\n", file_name_start);
                
                fileName=url_decode(url_encoded_file_name);

                printf("\n----------\nFile name: %s\n----------\n", fileName);
                
                // get file extension
                char file_ext[32];
                strcpy(file_ext, get_file_extension(fileName));

                printf("\n----------\nFile extension: %s\n----------\n", file_ext);

                char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                char body[MAX_SIZE];

                strncpy(body, body_start, recv_size - (body_start - buffer));
                body[recv_size - (body_start - buffer)] = '\0';

                printf("\n----------\nBody: %s\n----------\n",body);

                // Build the HTTP response
                char *responsePut=malloc(MAX_SIZE * 10*sizeof(char));
                size_t response_len;

                //responsePost=NULL;

                parse_input_string(body, &data_formular);

                build_http_response("index2.html", "html", responsePut, &response_len, print_parsed_data(&data_formular));

                printf("\n----------\nResponse in PUT: %s\n----------\n", responsePut);

                send(client_fd, responsePut, response_len, 0);
                

                free(fileName);
                free(responsePut);

            }

        }
    
        printf("\n----------\nFinished handling request, closing connection.\n----------\n");

        free(data_formular.name);
        free(data_formular.color);
        free(data_formular.data_from_cgi);
        free(data_formular.income);
        free(data_formular.phone);
        free(data_formular.qualities);
        free(data_formular.sociability);

        close(client_fd);

    }

    free(server);
}


void handle_client_epoll(int client_fd)
{
      char buffer[MAX_SIZE];

        while (1) 
        { // Loop to handle multiple requests in a single connection
            char buffer[MAX_SIZE];
            char bufferCopy[MAX_SIZE];
            ssize_t recv_size = recv(client_fd, buffer, MAX_SIZE, 0);
            strcpy(bufferCopy,buffer);

            if (recv_size <= 0) {
                break;
            }

            printf("\n----------\nReceived request: %s\n----------\n",  buffer);

            if (strncmp(buffer, "GET", 3) == 0) {
                char *get_token = "GET /";
                char *get_start = strstr(bufferCopy, get_token);

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
                        
                        fileName=url_decode(url_encoded_file_name);

                        printf("\n----------\nFile name: %s\n----------\n", fileName);
                        
                        // get file extension
                        char file_ext[32];
                        strcpy(file_ext, get_file_extension(fileName));

                        printf("\n----------\nFile extension: %s\n----------\n", file_ext);


                        // build HTTP response
                        char *responseGet = (char *)malloc(MAX_SIZE * 10 * sizeof(char));
                        size_t response_len;

                        // Reset the response buffer
                        memset(responseGet, 0, MAX_SIZE * 10 * sizeof(char));


                        char *cgi_output = malloc(MAX_SIZE);
                        cgi_output = NULL;


                        if (strstr(bufferCopy, "cgi-bin/") != NULL) {

                            //pthread_mutex_init(&mutex, NULL);

                            //char *queryString = getenv("QUERY_STRING");
                            
                            //printf("\n----------\nAm extras query string-ul acesta: %s\n----------\n", queryString);

                            printf("\n----------\nAm intrat in if-ul de executare script, pentru pagina %s\n----------\n", fileName);
                            // Execute CGI script in a new thread
                            pthread_t cgi_thread_id;
                            pthread_create(&cgi_thread_id, NULL, execute_script, NULL);
                            pthread_join(cgi_thread_id, (void**)&cgi_output); // Wait for the script to finish and capture its output

                            //pthread_mutex_destroy(&mutex);


                            printf("\n----------\nDin script: %s\n----------\n", cgi_output);

                            printf("\n----------\nBuffer: %s\n----------\n",buffer);

                            char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                            char body[MAX_SIZE];

                            strncpy(body, body_start, recv_size - (body_start - buffer));
                            body[recv_size - (body_start - buffer)] = '\0';

                            printf("\n----------\nBody: %s\n----------\n",body);

                            //parse_input_string(body, &data_formular);

                            data_formular.data_from_cgi = cgi_output;

                            build_http_response("index3.html", "html", responseGet, &response_len, data_formular.data_from_cgi);
                                                        
                            send(client_fd, responseGet, response_len, 0);

                            printf("\n----------\nThe response has been sent.\n----------\n");

                        }
                        else
                        {

                            printf("\n----------\nTrying to build the response.\n----------\n");

                            build_http_response(fileName, file_ext, responseGet, &response_len, NULL);

                            printf("\n----------\nThis is the reponse: \n %s\n----------\n",responseGet);

                            printf("\n----------\nWe've built the response.\nTrying to send the response.\n----------\n");

                            // send HTTP response to client
                            send(client_fd, responseGet, response_len, 0);

                            printf("\n----------\nThe response has been sent.\n----------\n");

                        }


                        free(responseGet);
                        free(fileName);
                        free(cgi_output);
                        data_formular.data_from_cgi=NULL;

                        //close(client_fd);

                    }
               }
            

            } else if (strncmp(buffer, "POST", 4) == 0) {

                    char *cgi_output = malloc(MAX_SIZE);
                    cgi_output = NULL;

                    char *get_token = "POST /";
                    char *get_start = strstr(bufferCopy, get_token);

                    char *file_name_start = get_start + strlen(get_token);
                    char *file_name_end = strchr(file_name_start, ' ');

                    printf("\n----------\nMethod: POST\n----------\n");

                    // decode URL
                    *file_name_end = '\0';
                    char *url_encoded_file_name = file_name_start;

                    //printf("\n%s\n", file_name_start);
                    
                    fileName=url_decode(url_encoded_file_name);

                    printf("\n----------\nFile name: %s\n----------\n", fileName);
                    
                    // get file extension
                    char file_ext[32];
                    strcpy(file_ext, get_file_extension(fileName));

                    printf("\n----------\nFile extension: %s\n----------\n", file_ext);


                    if (strstr(bufferCopy, "cgi-bin/") != NULL) {

                    //pthread_mutex_init(&mutex, NULL);

                    //char *queryString = getenv("QUERY_STRING");
                    
                    //printf("\n----------\nAm extras query string-ul acesta: %s\n----------\n", queryString);

                    printf("\n----------\nAm intrat in if-ul de executare script, pentru pagina %s\n----------\n", fileName);
                    // Execute CGI script in a new thread
                    pthread_t cgi_thread_id;
                    pthread_create(&cgi_thread_id, NULL, execute_script, NULL);
                    pthread_join(cgi_thread_id, (void**)&cgi_output); // Wait for the script to finish and capture its output

                    //pthread_mutex_destroy(&mutex);

                    }

                    printf("\n----------\nDin script: %s\n----------\n", cgi_output);

                    printf("\n----------\nBuffer: %s\n----------\n",buffer);

                    char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                    char body[MAX_SIZE];

                    strncpy(body, body_start, recv_size - (body_start - buffer));
                    body[recv_size - (body_start - buffer)] = '\0';

                    printf("\n----------\nBody: %s\n----------\n",body);

                    // Build the HTTP response
                    char *responsePost=malloc(MAX_SIZE * 10*sizeof(char));
                    size_t response_len;

                    //responsePost=NULL;

                    parse_input_string(body, &data_formular);

                    data_formular.data_from_cgi = cgi_output;

                    build_http_response("index2.html", "html", responsePost, &response_len, print_parsed_data(&data_formular));

                    printf("\n----------\nResponse in post: %s\n----------\n", responsePost);

                    send(client_fd, responsePost, response_len, 0);
                    

                    free(fileName);
                    free(cgi_output);
                    free(responsePost);
                    data_formular.data_from_cgi=NULL;
                    
            } else if (strncmp(buffer, "PUT", 3) == 0) {

                char *get_token = "PUT";
                char *get_start = strstr(bufferCopy, get_token);

                char *file_name_start = get_start + strlen(get_token);
                char *file_name_end = strchr(file_name_start, ' ');

                printf("\n----------\nMethod: PUT\n----------\n");

                // decode URL
                *file_name_end = '\0';
                char *url_encoded_file_name = file_name_start;

                //printf("\n%s\n", file_name_start);
                
                fileName=url_decode(url_encoded_file_name);

                printf("\n----------\nFile name: %s\n----------\n", fileName);
                
                // get file extension
                char file_ext[32];
                strcpy(file_ext, get_file_extension(fileName));

                printf("\n----------\nFile extension: %s\n----------\n", file_ext);

                char *body_start = strstr(buffer, "\r\n\r\n") + 4;
                char body[MAX_SIZE];

                strncpy(body, body_start, recv_size - (body_start - buffer));
                body[recv_size - (body_start - buffer)] = '\0';

                printf("\n----------\nBody: %s\n----------\n",body);

                // Build the HTTP response
                char *responsePut=malloc(MAX_SIZE * 10*sizeof(char));
                size_t response_len;

                //responsePost=NULL;

                parse_input_string(body, &data_formular);

                build_http_response("index2.html", "html", responsePut, &response_len, print_parsed_data(&data_formular));

                printf("\n----------\nResponse in PUT: %s\n----------\n", responsePut);

                send(client_fd, responsePut, response_len, 0);
                

                free(fileName);
                free(responsePut);

            }

        }

        printf("\n----------\nFinished handling request, closing connection.\n----------\n");

        free(data_formular.name);
        free(data_formular.color);
        free(data_formular.data_from_cgi);
        free(data_formular.income);
        free(data_formular.phone);
        free(data_formular.qualities);
        free(data_formular.sociability);

        close(client_fd);
}


void handle_new_connection(int epollfd, int listenfd) {
    int clientfd = accept(listenfd, NULL, NULL);
    if (clientfd == -1) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    printf("New connection accepted. Client socket fd: %d\n", clientfd);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
    ev.data.fd = clientfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);
}

int main(int argc, char **argv) {
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




    if(strcmp(argv[1],"-t")==0){
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
    }

    else if(strcmp(argv[1],"-p")==0)
    { int i=0;
        pthread_t tids[100];
        printf("sunt in proces\n");
    while (1) {
            // client info
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd;

    
            // accept client connection
            if ((client_fd = accept(server_fd, 
                                    (struct sockaddr *)&client_addr, 
                                    &client_addr_len)) < 0) {
                perror("accept failed");
                continue;
            }


            // create a new process to handle client request
            pid_t pid = fork();

            if (pid == 0) {
                // This is the child process
        
                close(server_fd); // Close the server socket in the child process


            handle_client_epoll(client_fd);

                close(client_fd);
                exit(0);
            } else if (pid > 0) {
                // This is the parent process
    //  pthread_join(tids[i], NULL);
                close(client_fd); // Close the client socket in the parent process
            } else {
                perror("fork failed");
            }
            
            
        }

    }else if(strcmp(argv[1],"-e")==0)
    {
        struct epoll_event ev, events[10];
    int  clientfd, epollfd;
    epollfd = epoll_create1(0);
        if (epollfd == -1) {
            perror("Epoll creation failed");
            exit(EXIT_FAILURE);
        }

        // Add listening socket to epoll
        ev.events = EPOLLIN;
        ev.data.fd = server_fd;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, server_fd, &ev);



        while (1) {
            int nfds = epoll_wait(epollfd, events, 10, -1);
            if (nfds == -1) {
                perror("Epoll wait failed");
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == server_fd) {
                
                    handle_new_connection(epollfd, server_fd);
                } else {
                    handle_client_epoll(events[i].data.fd);
                }
            }
        }

        close(server_fd);
        close(epollfd);



    }
    else 
    {
        printf("optiune negasita\n");
        
        }




    return 0;
}