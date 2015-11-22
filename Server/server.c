#include <stdio.h>      // for printf() and fprintf()
#include <sys/socket.h> // for socket(), bind(), and connect()
#include <arpa/inet.h>  // for sockaddr_in and inet_ntoa()
#include <stdlib.h>     // exit()
#include <string.h>     // memset()
#include <unistd.h>     // close() и write()
#include <pthread.h>    // threads
#include <crypt.h>

#define N_THREADS 4 //Число рабочих потоков
#define MAXPENDING 3 // Выдаётся времени на запрос соединения

char buf[2048], md5salt[2048];

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
    pthread_mutex_lock(&q -> mutex);
    while(q -> first)
    {
        temp = q -> first -> next;
        free(q -> first);
        q -> first = temp;
    }  
    free(q -> last);
    pthread_mutex_unlock(&q -> mutex);
    pthread_mutex_destroy(&q -> mutex);
    pthread_cond_destroy(&q -> cond);
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
        q -> last -> next = msg; 
        q -> last = msg;
        q -> length++;
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

//Все наши потоки
pthread_t thread_pool[N_THREADS];

//На сервере возникла ошибка и он прекращает работу, выводя её
static void serverMustBeClosed(const char *message)
{
    printf("%s\n", message);
    exit(1); 
}

//Создание сокет сервера
static int createServerSocket(unsigned short port)
{
    int servSock;
    struct sockaddr_in servAddr;

    //Создание сокета для входящих подключений
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        serverMustBeClosed("socket() failed");
      
   //Задаём начальные параметры
    memset(&servAddr, 0, sizeof(servAddr));       // Заполняем структуру нулями
    servAddr.sin_family = AF_INET;                // Семейство адресов
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // IP-адрес
    servAddr.sin_port = htons(port);              // Порт

    //Назначаем номер порта клиентскому сокету
    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        serverMustBeClosed("bind() failed");

    //Отмечаем сколько сокет ждет запрос со стороны клиента
    if (listen(servSock, MAXPENDING) < 0)
        serverMustBeClosed("listen() failed");

    return servSock;
}

void* threadMain(void *tparam)
{
    pthread_mutex_t mymutex = PTHREAD_MUTEX_INITIALIZER;
    char loopstart = 0; // Старт бесконечного цикла сначала выключен
    //Приняли параметры в поток
    struct thread_param *param = (struct thread_param *)tparam;
    //Адрес клиента
    struct sockaddr_in clntAddr;
    char client_message[2048];
    int clntSock;
    //Основной цикл, в котором обрабатывается текущий клиент
    for(;;)
    {
        pthread_mutex_lock( &mymutex ); //лочим мьютекс

        if(clntSock = queue_get(&thread_q))
        {
            if (clntSock < 0)
                serverMustBeClosed("accept() failed");
            else
                loopstart = 1; //включаем бесконечный цикл
        }

        pthread_mutex_unlock( &mymutex ); //анлочим мьютекс
    
        if(loopstart == 1)
        {   
            //Проверка присланного клиентом пароля
            int bytes_r;
            char user_name[2048], pass_name[2048], fname[50];
            bytes_r = recv(clntSock, user_name, 2048, 0);
            user_name[bytes_r] = '\0';
            bytes_r = recv(clntSock, pass_name, 2048, 0);
            pass_name[bytes_r] = '\0';
            char chk[100],fi[100],fpass[100];
            int y = 0;
            char *opCrypt;
            //Считаем пароли для сверки из файла и проверяем
            FILE *fp = fopen("pass.txt","r");
            while(!feof(fp))
            {	
                fscanf(fp, "%s", fname);
                fscanf(fp, "%s", fpass);
                
                //Шифрование
                //Зануляем буферную переменную, куда всё будем копировать
                md5salt[0] = '\0';
                //salt в md5 формате: $1$ <password> $
                strcat(md5salt, "$1$");
                strcat(md5salt, fname);
                strcat(md5salt, "$");
     
                opCrypt = crypt(fpass, md5salt);
                strcpy(fi, opCrypt);

                //Сверяем присланное зашифрованное с прочитанным и зашифрованным                
	        if((strcmp(fi, pass_name) == 0) && (strcmp(fname, user_name) == 0))
	        {	
                    y++;
	        }
            }
            fclose(fp);
            if(y < 0 || y == 0)
            {
                write(clntSock , "adenied" , 7);
                puts("Incorrect client disconnected");
                loopstart = 0;
                close(clntSock);
            }
            else
            {
                write(clntSock , "agranted" , 8);
                puts("Client connected");
                chdir(user_name);
                int read_size;
                //Цикл, в котором получаем данные от клиента
                for(;;)
                {
                    //Получаем сообщение от клиента
                    read_size = recv(clntSock , client_message , 2048 , 0);
               
                    //Если что-то пришло от клиента и он не отключился, то пытаемся разобраться, что же с этим делать
                    if(read_size > 0)
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
         	            write(clntSock , pass_name, 50);
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
                            //Зануляем буферную переменную, куда всё будем копировать
                            buf[0] = '\0';
                            //Копируем в буферную переменную сначала команду от клиента, а потом команду логгирования в файл
                            strcat(buf, client_message);
                            strcat(buf, " | tee logfile.txt");
                            //Выполняем команду, образовавшуюся в буферной переменной, на сервере
                            system(buf);

                            //Зануляем буферную переменную, куда всё будем копировать. Старое уже не надо
                            buf[0] = '\0';
                            //Соединяем файл в строку построчно
                            FILE *fsend;
                            char line[256];
                            fsend = fopen("logfile.txt", "r");
                            int first = 0;
                            while(fgets(line, sizeof(line), fsend) != NULL)
                            {  
                                if(first == 0)
                                {
                                    strcat(buf, "\n");
                                    strcat(buf, line);
                                    first = 1;
                                }
                                else
  		                    strcat(buf, line);
                            }
                            fclose(fsend);   
                            //Отправляем весь файл
                            write(clntSock, buf , 2048);                 
    
                            //Зачистка от мусора прошлой присланной строки от этого клиента
                            memset(&client_message, ' ', 100);
                            client_message[0] = '\0';
                            //Удаляем файл, чтобы в следующий раз начать лог сначала
                            system("rm logfile.txt");
                        }
                    }   
                    else //А если клиент был отключен
                    {
                        puts("Client disconnected");
                        loopstart = 0;
                        close(clntSock);
                        break;
                    }
                }   
            }   
        }   
    }   
    endServer:
        serverMustBeClosed("Server closed");
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

    //Содаём N_THREADS потоков
    for(i = 0; i < N_THREADS; i++)
    {
        pthread_create(&thread_pool[i], NULL, &threadMain, NULL);
    }

    int clntSock;
    puts("Server started. Waiting for connections...");
    //Ждём запроса клиента. Если есть, то добавляем в очередь
    for(;;)
    {
        if( clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntLen))
        {
            queue_put(&thread_q, clntSock);
        }
    }

    //Ожидаем завершения всех N_THREADS потоков
    for(i = 0; i < N_THREADS; i++)
    {
        pthread_join(thread_pool[i], NULL);
    }
    return 0;
}