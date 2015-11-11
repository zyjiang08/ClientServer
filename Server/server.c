#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     // exit()
#include <string.h>     // memset()
#include <unistd.h>     // close() и write()
#include <pthread.h>

#define N_THREADS 2 //Число рабочих потоков
#define MAXPENDING 2 // Выдаётся времени на запрос соединения
//Структура с параметрами, которую передаём в каждый поток
struct thread_param
{
    int serverSocket;
    char authPass[2000];
};
//Все наши потоки
pthread_t thread_pool[N_THREADS];

//На сервере возникла ошибка и он прекращает работу, выводя её
static void die(const char *message)
{
    perror(message);
    exit(1); 
}

//Создание сокет сервера
static int createServerSocket(unsigned short port)
{
    int servSock;
    struct sockaddr_in servAddr;

    //Создание сокета для входящих подключений
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        die("socket() failed");
      
   //Задаём начальные параметры
    memset(&servAddr, 0, sizeof(servAddr));       // Заполняем структуру нулями
    servAddr.sin_family = AF_INET;                // Семейство адресов
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // IP-адрес
    servAddr.sin_port = htons(port);              // Порт

    //Назначаем номер порта клиентскому сокету
    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        die("bind() failed");

    //Отмечаем сколько сокет ждет запрос со стороны клиента
    if (listen(servSock, MAXPENDING) < 0)
        die("listen() failed");

    return servSock;
}

void* threadMain(void *tparam)
{
    //Приняли параметры в поток
    struct thread_param *param = (struct thread_param *)tparam;
    int servSock = param -> serverSocket;

    //Адрес клиента
    struct sockaddr_in clntAddr;
    char client_message[2000];
    int read_size;

    for (;;) 
    {
        //Ожидание коннекта с клиентом

        unsigned int clntLen = sizeof(clntAddr); 
        int clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntLen);
        if (clntSock < 0)
            die("accept() failed");

        recv(clntSock , client_message , 2000 , 0);
        if(strcmp(client_message, param -> authPass) != 0)
        {  
            printf("%s\n", client_message);
            printf("%s\n", param -> authPass);
            write(clntSock , "adenied" , 7);
        }
        else
        {
            memset(&client_message, ' ', 100);
            write(clntSock , "agranted" , 8);

            //Получаем сообщение от клиента
	    while( (read_size = recv(clntSock , client_message , 2000 , 0)) > 0 )
	    {
                //Завершение работы сервера командой клиента
                if(strstr(client_message, "serverclose"))
                {
                    goto endServer;
                }
                //Команда напоминания пароля пользователю
                else if(strstr(client_message, "remindpass"))
                {
                    //Отправляем обратно клиенту
		    write(clntSock , param -> authPass, strlen(client_message));
                    //Зачистка от мусора прошлой присланной строки от этого клиента
                    memset(&client_message, ' ', 100);
                }
		else
                {   
                     //Отправляем обратно клиенту
		    write(clntSock , client_message , strlen(client_message));
                    //Зачистка от мусора прошлой присланной строки от этого клиента
                    memset(&client_message, ' ', 100);
                    
                }
	     }
         }
   }
   pthread_exit(NULL);
endServer:
   exit(1); 
}

int main(int argc, char *argv[])
{
    //Задали порт
    unsigned short servPort = 8888;
    //Создали сокет
    int servSock = createServerSocket(servPort);
    int i;
    struct thread_param param;
    param.serverSocket = servSock;
    strcpy(param.authPass,"Pass10");
    //Содаём N_THREADS потоков
    for(i=0;i<N_THREADS;i++)
    {
        pthread_create(&thread_pool[i],NULL,&threadMain,(void*)&param);
    }
    //Ожидаем завершения всех N_THREADS потоков
    for(i=0;i<N_THREADS;i++)
    {
        pthread_join(thread_pool[i],NULL);
    }
    return 0;
}