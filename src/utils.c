#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

#define DATA 0
#define SYN 1
#define FIN 2
#define RST 4
#define ACK 16
#define STOP_AND_WAIT 1
#define GO_BACK_N 2


#define MAX_SEQ 5535
#define MAX_ESSAI 5
#define TIMEOUT_SEC 1
#define NB_PAQUETS 1000

struct struct_paquet{
    unsigned char id_flux;
    unsigned char type;
    unsigned short int num_sequence;
    unsigned short int num_acquittement;
    unsigned char ecn;
    unsigned char fenetre;
    char data[44];
};
typedef struct struct_paquet Paquet;

//Envoyer un paquet quelque part
void send_paquet(int sockfd, struct sockaddr_in * destination_addr, int seq, int ack, int type, int fenetre, int ecn){

    Paquet p_send;
    p_send.num_sequence = seq; 
    p_send.num_acquittement = ack; 
    p_send.type = type;
    p_send.fenetre = fenetre;
    p_send.ecn = ecn;

    if(sendto(sockfd, &p_send, sizeof(p_send), 0, (struct sockaddr*)destination_addr, sizeof(*destination_addr))<0){
        perror("Erreur dans l'envoi d'un paquet");
        exit(EXIT_FAILURE);
    }
}

int rcv_paquet(int sockfd, struct sockaddr_in * destination_addr, Paquet * paquet){

    int sizeOfPaquet = recvfrom(sockfd, paquet, sizeof(*paquet), 0,(struct sockaddr *) &destination_addr, NULL);
    if(sizeOfPaquet < 0){
        perror("Erreur dans la réception d'un paquet");
        exit(EXIT_FAILURE);
    }

    return sizeOfPaquet;
}

int create_socket(int port_local, int port_ecoute, char * ip_distante, struct sockaddr_in * destination_addr){
    //On crée une socket de type datagramme en IPV4
    int sockfd;
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Erreur lors de l'initialisation de la socket");
        exit(EXIT_FAILURE);
    }

    //On prépare l'adresse locale
    struct sockaddr_in source_addr;
    source_addr.sin_family = AF_INET;
    source_addr.sin_port = htons(port_local);
    source_addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(sockfd, (struct sockaddr*) &source_addr, sizeof(source_addr)) < 0){
        perror("Erreur lors du bind de la socket");
        exit(EXIT_FAILURE);
    }

    //On prépare l'adresse distante
    destination_addr->sin_family = AF_INET;
    destination_addr->sin_port = htons(port_ecoute);
    destination_addr->sin_addr.s_addr = inet_addr(ip_distante);

    return sockfd;
}

//Incrémente le nombre d'essais...ou plante si le nombre d'essais max est atteint
int inc_essais(int nb_essais){
    if(MAX_ESSAI > nb_essais){

        return nb_essais+=1;
    }
    else{
        fprintf(stderr, "Timeout\n");
        exit(EXIT_FAILURE);
    }
}