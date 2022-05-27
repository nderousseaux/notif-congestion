#include "utils.c"

//Argmuments du main
struct main_args{
    int port_local, port_ecoute_dst_pertupateur;
    char * ip_distante;
};
typedef struct main_args Args;

//Vérification des argments du main
Args verify_arg(int argc, char *argv[]){
    Args args;

    if (argc != 4){
        //./destination <IP_distante> <port_local> <port_ecoute_dst_pertubateur>
        fprintf(stderr, "Usage : ./destination <IP_distante> <port_local> <port_ecoute_dst_pertubateur>\n");
        exit(EXIT_FAILURE);
    }
    args.ip_distante = argv[1];
    args.port_local = atoi(argv[2]);
    if(args.port_local < 1 || args.port_local > 65535){
        fprintf(stderr, "Value of the parameter 'port_local' should be beetween 1 and 65535.\n");
        exit(EXIT_FAILURE);
    }
    args.port_ecoute_dst_pertupateur = atoi(argv[3]);
    if(args.port_ecoute_dst_pertupateur < 1 || args.port_ecoute_dst_pertupateur > 65535){
        fprintf(stderr, "Value of the parameter 'port_ecoute_dst_pertupateur' should be beetween 1 and 65535.\n");
        exit(EXIT_FAILURE);
    }

    return args;
}

//Implémente le 3 ways handshake
void connection(int sockfd, struct sockaddr_in * source_addr, int * seq_source, int * seq_destination){
    int nb_essais = 0;
    Paquet p_recu;

    //On prépare les variables pour le timeout
    fd_set lecture;
    struct timeval delai;
    delai.tv_sec = TIMEOUT_SEC;
    delai.tv_usec = 0;
    //On attend un paquet SYN
    do{
        rcv_paquet(sockfd, source_addr, &p_recu);
    }while(p_recu.type != SYN);
    *seq_source = (p_recu.num_sequence + 1) % MAX_SEQ;

    while (1){
        nb_essais = inc_essais(nb_essais);

        //On envoie un paquet SYN+ACK
        send_paquet(sockfd, source_addr, *seq_destination, *seq_source, SYN+ACK, 0, 0);
        
        //On attend un paquet ACK
        FD_ZERO(&lecture); 
        FD_SET(sockfd, &lecture);
        select(sockfd+1, &lecture, NULL, NULL, &delai);
        if(FD_ISSET(sockfd, &lecture)){
            rcv_paquet(sockfd, source_addr, &p_recu);
            //On vérifie que le paquet respecte l'implémentation
            if((p_recu.type == ACK) && (p_recu.num_sequence == *seq_source) && (p_recu.num_acquittement == (*seq_destination + 1) % MAX_SEQ)){
                //Sinon, tout vas bien, on casse la boucle
                *seq_destination = (*seq_destination + 1) % MAX_SEQ;            
                break;
            }
        }
        else{
            //Si on ne reçoit rien, on reset le timer et on recommence
            delai.tv_sec = TIMEOUT_SEC;
            delai.tv_usec = 0;
        }
    }
    *seq_source = (*seq_source + 1) % MAX_SEQ;
}

//Implémente la déconnection façon TCP
void deconnection(int sockfd, struct sockaddr_in * source_addr, int * seq_source, int * seq_destination){
    int nb_essais = 0;
    Paquet p_recu;

    //On prépare les variables pour le timeout
    fd_set lecture;
    struct timeval delai;
    delai.tv_sec = TIMEOUT_SEC;
    delai.tv_usec = 0;
    send_paquet(sockfd, source_addr, 0, (*seq_source + 1) % MAX_SEQ, SYN + ACK, 0, 0);
    
    while(1){
        send_paquet(sockfd, source_addr, *seq_destination, 0, FIN + ACK, 0, 0);
        FD_ZERO(&lecture); 
        FD_SET(sockfd, &lecture);
        select(sockfd+1, &lecture, NULL, NULL, &delai);
        if(FD_ISSET(sockfd, &lecture)){ //Si on recoit le paquet ACK, avec le bon num de séquence
            rcv_paquet(sockfd, source_addr, &p_recu);
            if(p_recu.num_acquittement == (*seq_destination+1)%MAX_SEQ && p_recu.type == ACK){
                break;
            }
            
        }
        else{
            delai.tv_sec = TIMEOUT_SEC;
            delai.tv_usec = 0;
        }
    }
} 

//Réception des données
void recv_data(int sockfd, struct sockaddr_in * source_addr, int * seq_source){
    int nbEssais = 0; //Nombre d'essais avant abandon
    Paquet p_recu;

    int seqMax = MAX_SEQ; //Numéro de séquence maximal auquel on aura droit (utile dans le stop and wait)
    do{
        //On attend un paquet data
        rcv_paquet(sockfd, source_addr, &p_recu);

        //Si la taille de la fenêtre est de 0, alors il s'agit du stop and wait
        if(p_recu.fenetre == 0){ 
            seqMax = 2;
        }
		if (p_recu.num_sequence == (*seq_source)%seqMax && p_recu.type == DATA){
            send_paquet(sockfd, source_addr, 0, (p_recu.num_sequence + 1) % seqMax, ACK, 0, p_recu.ecn);
            *seq_source = (*seq_source + 1)%seqMax;
		}        
	}while(p_recu.type != FIN || p_recu.num_sequence != *seq_source);
}


int main(int argc, char *argv[]){

    //On vérifie les argments
    Args args = verify_arg(argc, argv);

    //On crée une socket
    struct sockaddr_in source_addr;
    int sockfd = create_socket(args.port_local, args.port_ecoute_dst_pertupateur, args.ip_distante, &source_addr);

    int seq_source; //Numéro de séquence de la source, inconnu pour l'instant
    int seq_destination = rand() % MAX_SEQ; //Numéro de séquence de la destination, le mien

    connection(sockfd, &source_addr, &seq_source, &seq_destination);

    recv_data(sockfd, &source_addr, &seq_source);

    deconnection(sockfd, &source_addr, &seq_source, &seq_destination);

    return EXIT_SUCCESS;
}