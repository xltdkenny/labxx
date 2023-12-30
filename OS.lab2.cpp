#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

int server_sock = -1; // нормально не инициализированы
int client_socket = -1; // нормально не инициализированы
volatile sig_atomic_t wasSigHup = 1; //если бы был 0, то код мог бы завершиться без выполнения части кода

void handleTerminationSignal(int sig) //обработка сигнала
{
    wasSigHup = 0; //когда равно 0, цикл завершается
}

int main() 
{
    struct sockaddr_in server_addr; //предоставление адреса сервера в семействе IPv4. содержит информацию о порте и IP-адресе
    int new_socket; //хранение файлового дескриптора нового сокета при соединении с клиентом

    struct sigaction sa; //структура обработки сигналов (в нашем случае SIGHUP)
    memset(&sa, 0, sizeof(sa)); //заполнение нулевыми байтами
    sa.sa_handler = handleTerminationSignal; //установки обработчика сигнала, флаг wasSigHup устанавливает в 0 при получении сигнала SIGHUP
    sigaction(SIGHUP, &sa, nullptr); //регистрирует обработчик сигнала SIGHUP, при сигнале вызывается обработчик handleTerminationSignal
    //создание потокового сокета
    server_sock = socket(AF_INET, SOCK_STREAM, 0);//новый сокет,AF_INET - протокол IPv4, SOCK_STREAM - потоковый сокет (TCP), 0 - это протокол по умолчанию IPv4
    if (server_sock == -1) //проверка на создание
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET; //установка семейства прокотолок на IPv4. Использование адресации IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY - константа, означающая, что сервер будет слушать все сетевые интерфейсы на машине. принимать соединения, адресованные любому IP-адресу.
    server_addr.sin_port = htons(6696); //  Эта функция преобразует порт в сетевой порядок байт. Порт 6696 выбран произвольно, но здесь он должен быть преобразован в формат, который правильно интерпретируется сетью.

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) // связывает сокет с конкретным адресом (server_addr), если не удалось (-1), то программа выдаст ошибку и завершится
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    // Устанавливает сокет в режим прослушивания входящих соединений, Второй аргумент( у нас это 1), определяет максимальную длину очереди ожидающих соединений
    if (listen(server_sock, 1) == -1) 
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    sigset_t blockedMask; //хранение множество сигналов
    sigset_t origMask; //сохранение текущего состояния маски сигналов
    sigemptyset(&blockedMask); //очищение множества сигналов
    sigaddset(&blockedMask, SIGHUP); //добавление SIGHUP в множество. Это означает, что сигнал SIGHUP будет заблокирован при использовании маски сигналов
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask); //установка маски сигналов для процессов, блокируются сигналы в blockedMask, маска сигналов сохраняется в origmask

    while (wasSigHup) //пока wasSigHup = 1
    {
        fd_set read_fds; // создание множества файловых дескрипторов read_fds
        FD_ZERO(&read_fds);// и его инициализация
        FD_SET(server_sock, &read_fds);// добавление дескриптора server_sock в множество read_fds

        int max_fd = server_sock; //иницализация max_fd, используется для определния максимального значения дескрипторов среди всех дескрипторов
        //проверка, есть ли подключенный клиент, если есть, то добавляем в дескриптор read_fds
        if (client_socket != -1) 
        {
            FD_SET(client_socket, &read_fds);
            //если дескриптор client_socket больше текущего максимального reaf_fds, то обновляем значение
            if (client_socket > max_fd)
            {
                max_fd = client_socket;
            }   
                else
                {
                max_fd = max_fd;
                }
        }
        //функция для мультиплексирования ввода/вывода, обработкой сигнала sighup и принятием нового соединения
        if (pselect(max_fd + 1, &read_fds, nullptr, nullptr, nullptr, &origMask) < 0) 
        { //вызывает функцию, которая блокирует выполнение до тех пор, пока не произойдет событие из множества read_fds или не произойдет сигнал, который не заблокирован маской origmask
            if (errno == EINTR) //если pselect прервана сигналом, проверяется, был ли этот сигнал SIGHUP
            {
                if (!wasSigHup) // если false, то есть сигнал SIGHUP произошел, то выводится сообщение SIGHUP и программа продолжает цикл
                {
                printf("SIGHUP");
                }
                continue;
            } 
            
            else //если прерывание не связано с SIGHUP, выводится сообщение об ошибке perror и цикл завершается
            {
                perror("pselect"); 
                break;
            }
        }

        if (FD_ISSET(server_sock, &read_fds)) { //проверка, было ли установлено событие на серверном сокете
            if ((new_socket = accept(server_sock, nullptr, nullptr)) < 0) 
            { //если да, то происходит принятие нового соединения с испольнзованием accept
                perror("accept");
                exit(EXIT_FAILURE);
            }
            // проверка, есть ли уже активное соединение
            if (client_socket != -1) 
            { //если есть, то новый сокет закрывается, тк принимаются только одно соединение
                close(new_socket);
            } 

            else 
            { //если активного соединения нет, новое соединение устанавливается и выводится сообщение
                printf("New connection established\n");
                client_socket = new_socket;
            }
        }
        // этот блок отвечает за чтение данных из клиентского сокета и их обработку
        if (client_socket != -1 && FD_ISSET(client_socket, &read_fds)) // проверка на активное клиентское соединение и установлено ли событие на этом сокете
        {
            char buffer[1024]; //создается буфер размером 1024 байта для приема данных
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0); //recv читает данные из клиентского сокета в буфер.
            //bytes_received содержит количество фактически прочитанных байт
            if (bytes_received > 0) //если прочитаны данные, выводится сообщение с количеством принятых байт
            {
                printf("Data received from the client: %ld byte\n", bytes_received);
            } 

            else if (bytes_received == 0) 
            { //если = 0, то означает, что клиет отключился
                printf("Сlient disconnected\n");
                close(client_socket); // закрывается клиентский сокет
                client_socket = -1; // устанавливается значение 
            } 

            else 
            {
                perror("receive"); // если меньше 0, то это может быть ошиькой и выводится сообщение об ошибке
            }
        }
    }

    close(server_sock); //закрытие серверного сокета после выхода из цикла 
    close(client_socket); //закрытие клиентского сокета после завершения обработки

    return 0; //возврат из функции main
}
