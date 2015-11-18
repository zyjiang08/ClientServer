#include <stdio.h> 
#include <string.h>    
#include <sys/socket.h>   
#include <arpa/inet.h>
#include <crypt.h>
 
int main(int argc , char *argv[])
{
    int sock;
    struct sockaddr_in server;
    char message[1024] , server_reply[2048], password[1024];
    char *opCrypt;

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
    char user_data[2048], pass_data[2048], md5salt[2048];

    printf("Input ssh username: ");
    scanf("%s",user_data);
    send(sock, user_data, strlen(user_data), 0);
    printf("Input ssh password: ");
    scanf("%s",pass_data);
    
    //Шифруем пароль
    //Собираем salt в md5 формате: $1$ <password> $
    md5salt[0] = '\0';
    strcat(md5salt, "$1$");
    strcat(md5salt, user_data);
    strcat(md5salt, "$");

    //Используем логин как "Открытый ключ" для шифрования пароля алгоритмом md5
    opCrypt = crypt(pass_data, md5salt);  
    strcpy(pass_data, opCrypt);

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
        printf("ssh@%s >> ",user_data);
        scanf("%s" , message);
         
        //Запрос серверу
        if( send(sock , message , strlen(message) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
         
        //Ответ сервера
        if( recv(sock , server_reply , 2048 , 0) < 0)
        {
            puts("recv failed");
            break;
        }

        if(strstr(server_reply, "serverclose"))
        {
            printf("ssh@server << Server has been closed\n");
            goto errorpass;
        }
        else if(strstr(server_reply, "help"))
        {
            printf("ssh@server << Specific commands:\n< serverclose - for close server\n< help - for help\n< remindpass - for encrypt password remind\n< clientclose - close current client\n");
        }
        else if(strstr(server_reply, "clientclose"))
        {
            printf("ssh@server << Client will be closed\n");
            goto errorpass;
        }
        else
        {
                printf("ssh@server << %s\n", server_reply); 
        }
    }
errorpass:
    close(sock);
    return 0;
}
