// macros provided by Lewis Van Winkle
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>

#endif


#if defined(_WIN32)
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

#else
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#endif
// macros provided by Lewis Van Winkle

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "usernamekeytable.h"
#include "pollsockhandling.h"

int main() {    

    // Hash table initialization & declaration 
    table_t *user_table = (table_t*) calloc(1, sizeof(table_t));
    table_elem *table = (table_elem*) calloc(1, sizeof(table_elem));
    user_table -> table = table; 
    user_table -> table_size = 1;
    user_table -> num_of_elems = 0;

    // Pollfd array initialization & declaration 
    pfdhandler_t *pfthandler = calloc(1, sizeof(pfdhandler_t));
    struct pollfd *pollsocket_ptr = calloc(1, sizeof(struct pollfd));
    pfthandler->pollfd_ptr = pollsocket_ptr;
    pfthandler->arr_size = 1;
    pfthandler->pollfd_num = 0;
    
    // getaddrinfo hints
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 

    struct addrinfo *hostaddr;
    int addrinfocheck = getaddrinfo("localhost", "8080", &hints, &hostaddr);
    // getaddrinfo returns not int on failure
    if (addrinfocheck < 0) {
        printf("getaddrinfo() failed.\n");
        return 1;
    }

    SOCKET socket_listen;
    socket_listen = socket(hostaddr->ai_family, hostaddr->ai_socktype,
                        hostaddr->ai_protocol);
    if(!ISVALIDSOCKET(socket_listen)) {
        printf("socket() failed. %d\n", GETSOCKETERRNO());
        return 1;
    }
    
   
    if(bind(socket_listen, hostaddr->ai_addr, hostaddr->ai_addrlen)) {
        printf("bind() failed. %d\n", GETSOCKETERRNO());
        return 1;
    }
    // double freeaddr was in while loop
    freeaddrinfo(hostaddr);

    int listenerr = listen(socket_listen, 5);
    if (listenerr < 0) {
        printf("listen() failed. %d\n", GETSOCKETERRNO());
        return 1;
    }

    // Poll() for windows might be WSAPoll() ? 
    // last arg for poll is timeout (-1 means forever)
    // if timeout is made active poll will return 0 when it fully times out

    struct sockaddr client_sock_addr;
    socklen_t addrlen = sizeof(client_sock_addr);

    // Create listener poll fd for appending 
    struct pollfd *listenerfd = (struct pollfd *) calloc(1, sizeof(struct pollfd *)); 
    listenerfd->fd = socket_listen; 
    listenerfd->events = POLLIN;
    append_pollfd(pfthandler, listenerfd);

    printf("Waiting for connections.\n");

    // Perhaps using mmap for shared processes? (fork)

    while(1) {
        int poll_event = poll((pfthandler->pollfd_ptr), (pfthandler->pollfd_num), -1);

        if(poll_event == -1) {
            printf("error with poll(). (%d)\n", GETSOCKETERRNO());                            
            exit(1);
        } 

        for(int i = 0; i < pfthandler->arr_size; i++) {
            if(pfthandler->pollfd_ptr[i].revents & POLLIN) {
                // if active poll is socket_listen
                if(pfthandler->pollfd_ptr[i].fd == socket_listen) {
                    // accept socket, add to fds list in pfthandler
                    SOCKET client_socket = accept(socket_listen, &client_sock_addr, &addrlen);
                    struct pollfd *new_fd = (struct pollfd*) calloc(1, sizeof(struct pollfd));
                    new_fd->events = POLLIN;
                    new_fd->fd = client_socket;
                    append_pollfd(pfthandler, new_fd);
                    printf("%d\n", new_fd->fd);
                    char msg[100] = {"Server Connection confirmed"};
                    send(client_socket, msg, strlen(msg), 0);

                    // getting socket info
                    struct sockaddr_in peer_addr;
                    socklen_t addr_len;
                    int peer_err = getpeername(client_socket,(struct sockaddr *) &peer_addr, &addr_len);
                    if(peer_err == -1) {
                        printf("error on getpeername(). %d\n", peer_err);
                    }
                    char char_ip_addr[16];
                    memset(char_ip_addr, 0, sizeof(char_ip_addr));
                    printf("Peer IP address: %s\n", inet_ntoa(peer_addr.sin_addr));
                    // getting socket info

                    // Username send & add
                    char username_recv[48];
                    memset(username_recv, 0, sizeof(username_recv));
                    recv(client_socket, username_recv, sizeof(username_recv), 0);
                    printf("%s\n", username_recv);
                    table_elem *new_elem = (table_elem *) calloc(1, sizeof(table_elem));
                    new_elem->key = &client_socket;
                    new_elem->username = username_recv; 
                    append_element(user_table, new_elem);
                    // TODO work on adding username by key
                    // Username send & add


                } else {
                    char msgbuff[2048];
                    memset(msgbuff, 0, sizeof(msgbuff));
                    int recverr = recv(pfthandler->pollfd_ptr[i].fd, msgbuff, sizeof(msgbuff), 0);

                    if (recverr <= 0) {
                        if (recverr == 0) {
                            // Connection closed
                            printf("Socket %d hung up.\n", pfthandler->pollfd_ptr[i].fd);
                        } else {
                            printf("error with recv(). (%d)\n", GETSOCKETERRNO());                            
                        }
                        CLOSESOCKET(pfthandler->pollfd_ptr[i].fd);

                        remove_pollfd(pfthandler, &(pfthandler->pollfd_ptr[i]));
                        remove_element(user_table, table_search(user_table, user_table->table_size, &pfthandler->pollfd_ptr[i].fd));
                    // added this else after recverr and stopped program from exiting out after closing from client
                    } else {
                        SOCKET sender_soc = pfthandler->pollfd_ptr[i].fd;
                        // TODO: error handling for recv call
                        for(int l = 0; l < pfthandler->arr_size; l++) {
                            // if socket can recieve messages
                            if((pfthandler->pollfd_ptr[l].fd != socket_listen) | (pfthandler->pollfd_ptr[l].fd != sender_soc))
                            {
                                int send_err = send(pfthandler->pollfd_ptr[l].fd, msgbuff, 2048, 0);
                                if (send_err <= 0) {
                                    printf("error with send(). (%d)\n", GETSOCKETERRNO());
                                }
                            }
                        }
                    }
                    
                }
            } // pfthandler->pollfd_ptr[i].revents == (POLLIN)
        } // for (int i = 0; i < pfthandler->pollfd_num; i++)
    }


    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client = accept(socket_listen,
            (struct sockaddr*) &client_address, &client_len);
    if (!ISVALIDSOCKET(socket_client)) {
        printf("accept() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    // getting socket info
    // TODO getting client socket info to log
    
    // closes accepted socket

    // recv ?       

    // Try Ncurses
    free(user_table);
    free(table);
    free(pfthandler);
    free(pollsocket_ptr);
    free(listenerfd);
    return 0;
}