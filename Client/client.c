#include<stdio.h> 
#include<string.h>    
#include<sys/socket.h>   
#include<arpa/inet.h>
 
int main(int argc , char *argv[])
{
    int sock;
    struct sockaddr_in server;
    char message[1000] , server_reply[2000], password[1000];

    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( 8888 );
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("Connect failed. Error");
        return 1;
    }
    
    printf("Enter password : ");
    scanf("%s" , password);
    printf("Checking password. Wait...\n");
    if( send(sock , password , strlen(password) , 0) < 0)
    {
            puts("Send password failed");
            return 1;
    }
    
    sleep(5);
    if( recv(sock , server_reply , 2000 , 0) < 0)
    {
            puts("recv failed");
    }
    printf("%s\n", server_reply);
    if(strstr(server_reply, "adenied"))
    {
        printf("Error password. Access denied\n");
        goto errorpass;
    }
    else
    {
        printf("Correct password. Access granted\n");
    }
    
    //keep communicating with server
    while(1)
    {
        printf("cmd >> ");
        scanf("%s" , message);
         
        //Send some data
        if( send(sock , message , strlen(message) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
         
        //Receive a reply from the server
        if( recv(sock , server_reply , 2000 , 0) < 0)
        {
            puts("recv failed");
            break;
        }

        if(strstr(server_reply, "unkncomm"))
        {
            printf("out << Unknown command\n");
        }
        else if(strstr(server_reply, "serverclose"))
        {
            printf("out << Server has been closed\n");
            goto errorpass;
        }
        else if(strstr(server_reply, "help"))
        {
            printf("out << Commands:\n    << serverclose - for close server\n    << help - for help\n    << remindpass - for password remind\n");
        }
        else
            printf("out << %s\n", server_reply); 
    }
errorpass:
    close(sock);
    return 0;
}
