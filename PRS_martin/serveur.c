#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#define RCVSIZE 508

void envoyer(int no_seq, int no_bytes, char* buffer_input, char* buffer_output, int server_socket, struct sockaddr_in* client_addr, socklen_t length);

int wait_ack(int no_seq, int no_seq_max, int no_bytes, char* buffer_input, char* buffer_output, int server_socket, struct sockaddr_in* client_addr, socklen_t* length);

int main (int argc, char *argv[]) {

    struct timeval timeout={5,0};
    struct sockaddr_in adresse, cliaddr, adresse2;
    memset(&cliaddr, 0, sizeof(cliaddr));
    memset(&adresse, 0, sizeof(adresse));
    memset(&adresse2, 0, sizeof(adresse));
    pid_t ppid = getpid();

    int port_syn,port_data=1000;

    char buffer[RCVSIZE];

    if(argc > 2){
        printf("Too many arguments\n");
        exit(0);
    }
    if(argc < 2){
        printf("Argument expected\n");
        exit(0);
    }
    if(argc == 2){
        port_syn = atoi(argv[1]);
        printf("%d\n",port_syn);
    }



    int server_desc_syn = socket(AF_INET, SOCK_DGRAM, 0);
    int server_desc_data = socket(AF_INET, SOCK_DGRAM, 0);


    if(server_desc_syn < 0){
        perror("Cannot create udp socket\n");
        return -1;
    }

    adresse.sin_family= AF_INET;
    adresse.sin_port= htons(port_syn);
    adresse.sin_addr.s_addr= INADDR_ANY;


    int newport = 1;
    port_data =  port_syn+newport;
    adresse2.sin_family= AF_INET;
    adresse2.sin_port= htons(port_data);
    adresse2.sin_addr.s_addr= INADDR_ANY;


    if (bind(server_desc_syn, (struct sockaddr*) &adresse, sizeof(adresse))<0) {
        perror("Bind failed\n");
        close(server_desc_syn);
        return -1;
    }


    socklen_t len = sizeof(cliaddr);
    int n;

    while(getpid() == ppid){


      printf("JE SUIS LE PERE - %d\n",getpid());
      printf("WAITING FOR SYN\n");
      n = recvfrom(server_desc_syn, (char *)buffer, RCVSIZE,MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
      buffer[n] = '\0';

      if(strcmp(buffer,"SYN")==0){

        char synack[13];
        char port_data_s[6];
        strcpy(synack, "SYN-ACK");
        printf("SYN COMING\n");
        snprintf((char *) port_data_s, 10 , "%d", port_data );
        strcat(synack,port_data_s);
        strcpy(buffer, synack);

        sendto(server_desc_syn, (char *)buffer, strlen(buffer),MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
        printf("Waiting for ACK...\n");
        n = recvfrom(server_desc_syn, (char *)buffer, RCVSIZE,MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
        buffer[n] = '\0';
        if(strcmp(buffer,"ACK")==0){
            printf("ACK received, connection\n");
        // Handshake reussi !
        }else{
            printf("Bad ack");
            exit(0);
        }
    }else{
        printf("bad SYN");
        exit(0);
    }

    if (bind(server_desc_data, (struct sockaddr*) &adresse2, sizeof(adresse2))<0) {
      perror("Bind failed\n");
      exit(0);
    }

    pid_t testf = fork();

    if(testf == 0){
      close(server_desc_syn);
    }else{
    close(server_desc_data);
    server_desc_data = socket(AF_INET, SOCK_DGRAM, 0);
    port_data ++;
    printf("port du prochain: %d\n",port_data);
    printf("\n");
    adresse2.sin_port= htons(port_data);
  }
}

    printf("JE SUIS LE FILS - %d DU PERE  %d\n\n",getpid(),getppid());



    FILE* fichier;

    timeout.tv_sec=0;
    timeout.tv_usec=500;
    setsockopt(server_desc_data,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));
    //printf("WAITING FOR MESSAGE\n");

    n = recvfrom(server_desc_data, (char *)buffer, RCVSIZE,MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
    buffer[n] = '\0';

    //printf("On a recu le msg : %s ...\n", buffer);
    // POSER UN ACK ICI

    fichier = NULL;

    if((fichier=fopen(buffer,"r"))==NULL){

        //printf("Something's wrong I can feel it (file)\n");
        exit(1);
    }

    size_t pos = ftell(fichier);    // Current position
    fseek(fichier, 0, SEEK_END);    // Go to end
    size_t length = ftell(fichier); // read the position which is the size
    fseek(fichier, pos, SEEK_SET);
    char buffer_lecture[length];

    if((fread(buffer_lecture,sizeof(char),length,fichier))!=length){
        //printf("Something's wrong I can feel it (read)....\n");
    }
    fclose(fichier);

    //printf("Le fichier lu a une taille de %ld octets\n",length);

    int nb_morceaux;
    int reste = length % (RCVSIZE-6);

    //printf("Le reste est egal a %d\n",reste);

    if((length % (RCVSIZE-6)) != 0){
        nb_morceaux = (length/(RCVSIZE-6))+1;
    } else {
        nb_morceaux = length/(RCVSIZE-6);
    }

    //printf("Le fichier lu est decoupe en %d envois\n",nb_morceaux);

    int num_seq=1;
    int num_seq_ack=0;
    int taille_window=4;
    int window[taille_window];

    clock_t t;
    t = clock();

    //printf("Initialisation\n");
    for(int i=0;i< taille_window;i++){
        //printf("-> %d\n",i+1);
        if(num_seq == nb_morceaux){
            envoyer(num_seq+i,reste,(char*)&buffer_lecture,(char *)&buffer,server_desc_data,&cliaddr,len);
        } else {
            envoyer(num_seq+i,RCVSIZE-6,(char*)&buffer_lecture,(char*)&buffer,server_desc_data,&cliaddr,len);
        }
        window[i]=i+1;
    }
    num_seq=taille_window;


    while(num_seq_ack <= nb_morceaux){
        int n = wait_ack(num_seq_ack,num_seq,RCVSIZE-6,(char*)&buffer_lecture,(char*)&buffer,server_desc_data,&cliaddr,&len);

        if(n == 0){
            //printf("Timeout recu, envoi de la sequence entiere\n");
            num_seq = num_seq_ack+1;
            for(int i=0;i< taille_window;i++){
                //printf("---> %d\n",num_seq+i);
                if(num_seq == nb_morceaux){
                    envoyer(num_seq+i,reste,(char*)&buffer_lecture,(char*)&buffer,server_desc_data,&cliaddr,len);
                } else {
                    envoyer(num_seq+i,RCVSIZE-6,(char*)&buffer_lecture,(char*)&buffer,server_desc_data,&cliaddr,len);
                }
                window[i]=num_seq+i;

            }
            num_seq += taille_window;
        } else {
            num_seq_ack = n;
            //printf("1----- Window: %d %d %d %d\n",window[0],window[1],window[2],window[3]);
            for(int i=0; i < taille_window;i++){

                if(window[i] < (num_seq_ack+1+i)){

                    window[i]=num_seq_ack+(i+1);

                    if(window[i] > num_seq){

                        num_seq=window[i];
                        if(num_seq == nb_morceaux){
                            envoyer(num_seq,reste,(char*)&buffer_lecture,(char*)&buffer,server_desc_data,&cliaddr,len);
                        } else {
                            //printf("numseq: %d, i: %d \n",num_seq,i);
                            //printf("----------------> %d\n",num_seq);
                            envoyer(num_seq,RCVSIZE-6,(char*)&buffer_lecture,(char*)&buffer,server_desc_data,&cliaddr,len);
                        }
                    }
                }
            }
            //printf("2----- Window: %d %d %d %d\n",window[0],window[1],window[2],window[3]);

            //envoyer(num_seq_ack+1,reste,&buffer_lecture,&buffer,server_desc_data,&cliaddr,len);
        }

    }


    // ToDo retransmission si pas ack
    sendto(server_desc_data,"FIN", strlen("FIN"),MSG_CONFIRM, (const struct sockaddr *) &cliaddr,len);
    //printf("WAITING for final ack\n");
    n = recvfrom(server_desc_data, (char *)buffer, RCVSIZE,MSG_WAITALL, (struct sockaddr *) &cliaddr,&len);
    buffer[n] = '\0';
    printf("Final ack done for %d .\n",getpid());
    t = clock() - t;
    double time_taken = (double)t/CLOCKS_PER_SEC;
    printf("Taille: %ld\n", length);
    printf("Temps: %f\n",time_taken);
    printf("DEBIT : %f Ko\n",((float)((float)length/time_taken))/1000);

    return 0;
}


void envoyer(int no_seq, int no_bytes, char* buffer_input, char* buffer_output, int server_socket, struct sockaddr_in* client_addr, socklen_t length){

    char num_seq_tot[7];
    char num_seq_s[7];

    memcpy(buffer_output+6, buffer_input+((RCVSIZE-6)*(no_seq-1)), no_bytes);
    strcpy(num_seq_tot, "000000");
    snprintf((char *) num_seq_s, 10 , "%d", no_seq );
    for(int i = strlen(num_seq_s);i>=0;i--){
        num_seq_tot[strlen(num_seq_tot)-i]=num_seq_s[strlen(num_seq_s)-i];
    }
    memcpy(buffer_output,num_seq_tot, 6);

    sendto(server_socket,(const char*)buffer_output, no_bytes+6 ,MSG_CONFIRM, (struct sockaddr*) client_addr,length);

    return;
}

int wait_ack(int no_seq, int no_seq_max, int no_bytes, char* buffer_input, char* buffer_output, int server_socket, struct sockaddr_in* client_addr, socklen_t* length){

    int n = recvfrom(server_socket, (char *)buffer_input, RCVSIZE,MSG_WAITALL, (struct sockaddr *) client_addr,length);
    buffer_input[n] = '\0';
    if(n != -1){

        int numack = atoi(strtok(buffer_input,"ACK_"));
        if(numack == no_seq){
            //printf("Ack == Numero de seq\n");
            return numack;

        }
        else if((numack > no_seq) && (numack <= no_seq_max)){
            //printf("Ack plus grand que le num de sequence : %d\n",numack);
            return numack;
        }
    }
    else {
        //printf("Timeout\n");
    }

    return 0;
}
