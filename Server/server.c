#include <stdio.h>      // for printf() and fprintf()
#include <sys/socket.h> // for socket(), bind(), and connect()
#include <arpa/inet.h>  // for sockaddr_in and inet_ntoa()
#include <stdlib.h>     // exit()
#include <string.h>     // memset()
#include <unistd.h>     // close() и write()
#include <pthread.h>

#define N_THREADS 2 //Число рабочих потоков
#define MAXPENDING 3 // Выдаётся времени на запрос соединения
pthread_mutex_t mymutex = PTHREAD_MUTEX_INITIALIZER;
char loopstart = 0; // Старт бесконечного цикла


//Блокирующая очередь
//Структура для очереди
struct message
{
    int sock;
    struct message *next;
};

//Очередь
struct queue
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct message *first;
    struct message *last;
    unsigned int length;
};

struct queue thread_q;

//Инициализация очереди
void queue_init(struct queue *q)
{
    q -> length = 0;
    q -> first = NULL;
    q -> last = NULL;
    pthread_cond_init(&q -> cond,NULL);
    pthread_mutex_init(&q -> mutex,NULL);
}

//Удаление очереди
void queue_destroy(struct queue *q)
{
    struct message * temp;
    pthread_mutex_lock(&q->mutex);
    while(q -> first)
    {
        temp = q -> first -> next;
        free(q -> first);
        q -> first = temp;
    }  
    free(q -> last);
    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q -> mutex);
    pthread_cond_destroy(&q-> cond);
}

//Добавление в очередь
void queue_put(struct queue *q, int sock)
{
    struct message *msg;
    msg = (struct message *)malloc(sizeof(msg));
    msg -> sock = sock;
    msg -> next = NULL;
    pthread_mutex_lock(&q -> mutex);
    if(q -> length == 0)
    {
        q -> last = msg;
        q -> first = msg;
        q -> length ++;
    }
    else
    {
        q->last->next = msg; 
        q -> last = msg;
        q -> length ++;
    }
    pthread_cond_signal(&q -> cond);
    pthread_mutex_unlock(&q -> mutex);
}

//Получение из очереди
int queue_get(struct queue *q)
{
    int sock;
    struct message *temp;
    pthread_mutex_lock(&q -> mutex);
 
    while(q -> length == 0)
    {
        pthread_cond_wait(&q -> cond, &q -> mutex);
    }    
    sock = q -> first -> sock;
    temp = q -> first;
    q -> first = q -> first -> next;
    q -> length--;   
 
    if(q -> length == 0)
    {
        q -> last == NULL;
    }
    free(temp);
    pthread_mutex_unlock(&q -> mutex);
    return sock;
}

//Структура с параметрами, которую передаём в каждый поток
struct thread_param
{
    char authPass[2000];
    int mutexUnlocker;
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

    //Адрес клиента
    struct sockaddr_in clntAddr;
    char client_message[2000];
    int read_size;

    pthread_mutex_lock( &mymutex );

    if( loopstart == 0 )
        pthread_mutex_unlock( &mymutex );
    

    for (;;) 
    {
        //Ожидание коннекта с клиентом

        int clntSock = queue_get(&thread_q);

        if (clntSock < 0)
            die("accept() failed");

        recv(clntSock , client_message, 2000 , 0);
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
            puts("Client connected");
            //Получаем сообщение от клиента
	    while( (read_size = recv(clntSock , client_message , 2000 , 0)) > 0 )
	    {
                //Завершение работы сервера командой клиента
                if(strstr(client_message, "serverclose"))
                {
                    write(clntSock , "serverclose", 11);
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
                //Команда напоминания команд
                else if(strstr(client_message, "help"))
                {
                    //Отправляем обратно клиенту
		    write(clntSock , "help", 4);
                    //Зачистка от мусора прошлой присланной строки от этого клиента
                    memset(&client_message, ' ', 100);
                }
                //Команда закрытия клиента
                else if(strstr(client_message, "clientclose"))
                {
                    //Отправляем обратно клиенту
		    write(clntSock , "clientclose", 11);
                    //Зачистка от мусора прошлой присланной строки от этого клиента
                    memset(&client_message, ' ', 100);
                }
		else
                {   
                    //Команда неизвестна
		    write(clntSock , "unkncomm" , 8);
                    //Зачистка от мусора прошлой присланной строки от этого клиента
                    memset(&client_message, ' ', 100); 
                }
	     }
             if(read_size == 0 || read_size == -1)
	     {
		puts("Client disconnected");
                loopstart = 0;
                close(clntSock);
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

    struct sockaddr_in clntAddr;
    unsigned int clntLen = sizeof(clntAddr);

    //Создали сокет
    int servSock = createServerSocket(servPort);
    int i;
    //Инициализировали очередь
    queue_init(&thread_q);
    //Задали параметры
    struct thread_param param;
    strcpy(param.authPass,"Pass10");
    param.mutexUnlocker = 0; 

    //Содаём N_THREADS потоков
    for(i = 0; i < N_THREADS; i++)
    {
        pthread_create(&thread_pool[i], NULL, &threadMain, (void*)&param);
    }

    int clntSock;
    //Ждём запроса клиента. Если есть, то добавляем в очередь
    for(;;)
    {
        
        if( clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntLen))
        {
            queue_put(&thread_q, clntSock);
            loopstart = 1;
        }
    }

    //Ожидаем завершения всех N_THREADS потоков
    for(i = 0;i < N_THREADS; i++)
    {
        pthread_join(thread_pool[i], NULL);
    }
    return 0;
}