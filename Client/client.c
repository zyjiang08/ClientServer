#include<stdio.h> 
#include<string.h>    
#include<sys/socket.h>   
#include<arpa/inet.h>
 
int main(int argc , char *argv[])
{
    int sock;
    struct sockaddr_in server;
    char message[1000] , server_reply[2000], password[1000];

    //Создаём сокет
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( 8888 );
 
    //Коннект к удалённому серверу
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("Connect failed. Error");
        return 1;
    }
    
    //Ввод пароля и логина
    char user_data[2048], pass_data[2048];

    printf("Input ssh username: ");
    scanf("%s",user_data);
    send(sock, user_data, strlen(user_data), 0);
    printf("Input ssh password: ");
    scanf("%s",pass_data);
    send(sock, pass_data, strlen(pass_data), 0);

    int bytes_r = recv(sock,server_reply,2048,0);
    server_reply[bytes_r] = '\0';
    if(strstr(server_reply, "adenied"))
    {
         printf("Error password. Access denied\n");
         goto errorpass;
    }
    else
    {
         printf("Correct password. Access granted\n");
    }
    
    //Если данные верны, то работаем
    while(1)
    {
        printf("cmd >> ");
        scanf("%s" , message);
         
        //Запрос серверу
        if( send(sock , message , strlen(message) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
         
        //Ответ сервера
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
            printf("out << Commands:\n    << serverclose - for close server\n    << help - for help\n    << remindpass - for password remind\n    << clientclose - close current client\n");
        }
        else if(strstr(server_reply, "clientclose"))
        {
            printf("out << Client will be closed\n");
            goto errorpass;
        }
        else
            printf("out << %s\n", server_reply); 
    }
errorpass:
    close(sock);
    return 0;
}
