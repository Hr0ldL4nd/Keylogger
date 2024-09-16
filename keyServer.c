#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 65535
#define ENCRYPTEDFILE "/home/lacampelo/Documents/SO/logs/recebido.log"
#define LOGFILE "/home/lacampelo/Documents/SO/logs/descripto.log"
#define BUFF_SIZE 1024

//Função auxiliar para descriptografar os dados do log antes de armazenar
void decrypt() {
    FILE *fin, *fout;
    int i,n;
    char buffer[BUFF_SIZE];
    fin = fopen(ENCRYPTEDFILE,"r");
    if(fin == NULL){
        printf("%s file does not exist\n", ENCRYPTEDFILE);
        exit(EXIT_FAILURE);
    }
    fout = fopen(LOGFILE,"w");
    if(fout == NULL){
        perror("Cannot create the file\n");
        exit(EXIT_FAILURE);
    }
    //printf("\n%s\n", fgets(buffer,sizeof(buffer),fin));
    while(fgets(buffer,sizeof(buffer),fin)!=NULL){
        n = strlen(buffer);
        for(i=0; i<n; i++){
            buffer[i] = buffer[i] + 45;
        }
        fputs(buffer,fout);
    }
    fclose(fin);
    fclose(fout);
}


int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    FILE *output_file;

    //Criando o socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Erro: falha na criação do socket\n");
        return 1;
    }

    //Configurando o socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //Associa o socket a um endereço e porta
    if (bind(server_fd, (struct sockaddr *)&address, addrlen) < 0) {
        printf("Erro: falha ao associar o socket a uma porta\n");
        close(server_fd);
        return 1;
    }

    // Escuta por conexões
    if (listen(server_fd, 3) < 0) {
        printf("Erro: falha ao configurar o socket para aceitar conexões\n");
        close(server_fd);
        return 1;
    }

    printf("Servidor aguardando conexões na porta %d...\n", PORT);

    //Aceita uma nova conexão
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (new_socket < 0) {
        printf("Erro: falha ao aceitar a conexão\n");
        close(server_fd);
        return 1;
    }

    //Abre o arquivo para salvar o log criptografado recebido
    output_file = fopen(ENCRYPTEDFILE, "a");
    if (output_file == NULL) {
        perror("Erro ao abrir o arquivo de saída");
        close(new_socket);
        close(server_fd);
        return 1;
    }

    //Recebe os dados e grava no arquivo
    while (recv(new_socket, buffer, sizeof(buffer), 0) > 0) {
        fprintf(output_file, "%s", buffer);
    }

    //Fecha o arquivo
    fclose(output_file);

    //Descriptografa os dados recebidos
    decrypt();

    // Fecha o socket    
    close(new_socket);
    close(server_fd);

    printf("Log recebido e salvo em %s\n", LOGFILE);

    return 0;
}
