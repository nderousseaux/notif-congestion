#include "utils.c"

//Argmuments du main
struct main_args{
    int mode, port_local, port_ecoute_src_pertupateur;
    char * ip_distante;
};
typedef struct main_args Args;

//Fichier de log
FILE* fichier = NULL;

//Vérification des argments du main
Args verify_arg(int argc, char *argv[]){
    Args args;

    if (argc != 5){
        fprintf(stderr, "Usage : ./source <mode> <IP_distante> <port_local> <port_ecoute_src_pertubateur>\n");
        exit(EXIT_FAILURE);
    }
    args.mode = atoi(argv[1]);
    if(args.mode != STOP_AND_WAIT && args.mode != GO_BACK_N){
        fprintf(stderr, "Value of the parameter 'mode' should be 1 or 2 (1: stop_and_wait; 2: go_back_n)\n");
        exit(EXIT_FAILURE);
    }
    args.ip_distante = argv[2];
    args.port_local = atoi(argv[3]);
    if(args.port_local < 1 || args.port_local > 65535){
        fprintf(stderr, "Value of the parameter 'port_local' should be beetween 1 and 65535.\n");
        exit(EXIT_FAILURE);
    }
    args.port_ecoute_src_pertupateur = atoi(argv[4]);
    if(args.port_ecoute_src_pertupateur < 1 || args.port_ecoute_src_pertupateur > 65535){
        fprintf(stderr, "Value of the parameter 'port_ecoute_src_pertupateur' should be beetween 1 and 65535.\n");
        exit(EXIT_FAILURE);
    }

    //On en profite pour initialiser le générateur d'aléatoire
    srand(time(NULL));

    return args;
}

void add_log(char * type, int taille_fen){
    struct timeval temps;
    gettimeofday(&temps, NULL);
    long temps_u = temps.tv_sec * 1000000 + temps.tv_usec;
    fprintf(fichier, "%s;%lu;%lu;%d\n",type, temps_u, clock(), taille_fen);
}

//Implémente le 3 ways handshake
void connection(int sockfd, struct sockaddr_in * destination_addr, int * seq_source, int * seq_destination){
    int nb_essais = 0; //Nombre d'essais avant abandon
    Paquet p_recu;

    //On prépare les variables pour le timeout
    fd_set lecture;
    struct timeval delai;
    delai.tv_sec = TIMEOUT_SEC;
    delai.tv_usec = 0;
    while(1){
        nb_essais = inc_essais(nb_essais); //On démarre une nouvelle tentative

        //On envoie un paquet SYN
        send_paquet(sockfd, destination_addr, *seq_source, 0, SYN, 0, 0);
        
        //On attend un paquet SYN + ACK
        FD_ZERO(&lecture); 
        FD_SET(sockfd, &lecture);
        select(sockfd+1, &lecture, NULL, NULL, &delai);
        if(FD_ISSET(sockfd, &lecture)){
            rcv_paquet(sockfd, destination_addr, &p_recu);
            //On vérifie que le paquet respecte l'implémentation
            if((p_recu.type == SYN + ACK) && (p_recu.num_acquittement == (*seq_source + 1)% MAX_SEQ)){
                //Si oui, on met à jours les numéro de séquence et on casse la boucle
                *seq_source = (*seq_source + 1) % MAX_SEQ;
                *seq_destination = (p_recu.num_sequence + 1)% MAX_SEQ;
                break;
            }            
        }
        else{//Si on ne reçoit rien, on reset le timer et on recommence
            delai.tv_sec = TIMEOUT_SEC;
            delai.tv_usec = 0;
        }
    }

    //On envoie un paquet ACK 
    send_paquet(sockfd, destination_addr, *seq_source, *seq_destination, ACK, 0, 0);
    *seq_source = (*seq_source + 1) % MAX_SEQ;
}

//Implémente la déconnection façon TCP
void deconnection(int sockfd, struct sockaddr_in * destination_addr, int * seq_source){
    int nb_essais = 0;
    Paquet p_recu;

    char recvAck = 0; //Bolléen, vrai si l'acquitement est reçu
    char recvFin = 0; //Bolléen, vrai si fin est reçu

    //On prépare les variables pour le timeout
    fd_set lecture;
    struct timeval delai;
    delai.tv_sec = TIMEOUT_SEC;
    delai.tv_usec = 0;
    while(recvAck < 1 || recvFin < 1){
        nb_essais = inc_essais(nb_essais); //On démarre une nouvelle tentative

        //On envoie un paquet de fin
        send_paquet(sockfd, destination_addr, *seq_source, 0, FIN, 0, 0);
        
        FD_ZERO(&lecture); 
        FD_SET(sockfd, &lecture);
        select(sockfd+1, &lecture, NULL, NULL, &delai);
        if(FD_ISSET(sockfd, &lecture)){ 
            rcv_paquet(sockfd, destination_addr, &p_recu);
            if(p_recu.num_acquittement == (*seq_source+1)%MAX_SEQ && p_recu.type == SYN+ACK){
                recvAck = 1;
                //On réinitialise le nombre d'essais, et le timer
                nb_essais = 0;
                delai.tv_sec = TIMEOUT_SEC;
                delai.tv_usec = 0;
            }
            else if (p_recu.type == ACK+FIN){
                recvFin = 1;
                send_paquet(sockfd, destination_addr, 0, (p_recu.num_sequence+1) % MAX_SEQ, ACK, 0, 0);

                //On réinitialise le nombre d'essais, et le timer
                nb_essais = 0;
                delai.tv_sec = TIMEOUT_SEC;
                delai.tv_usec = 0;
            }
        }
        else{
            delai.tv_sec = TIMEOUT_SEC;
            delai.tv_usec = 0;
        }
	}while (recvAck != 1 || recvFin != 1);

}

//Envoie les paquet selon le protocole stop and wait
void send_stop_and_wait(int sockfd, struct sockaddr_in * destination_addr, int * seq_source){
    int nb_essais = 0; //Nombre d'essais avant abandon
    Paquet p_recu;

    int nb_Paquets = 0; //Nombre de paquet envoyé.

    //On prépare les variables pour le timeout
    fd_set lecture;
    struct timeval delai;
    delai.tv_sec = TIMEOUT_SEC;
    delai.tv_usec = 0;
    while(nb_Paquets < NB_PAQUETS){
        nb_essais = inc_essais(nb_essais); //On démarre une nouvelle tentative

        //On envoie un paquet DATA
        send_paquet(sockfd, destination_addr, *seq_source, 0, DATA, 0, 0);
    
        //On attend un ACK
        FD_ZERO(&lecture); 
        FD_SET(sockfd, &lecture);
        select(sockfd+1, &lecture, NULL, NULL, &delai);
        if(FD_ISSET(sockfd, &lecture)){ 
            rcv_paquet(sockfd, destination_addr, &p_recu);
            //On vérifie que le paquet respecte l'implémentation
            if(p_recu.num_acquittement == (*seq_source+1)%2 && p_recu.type == ACK){
                *seq_source = (*seq_source +1)%2;
                nb_Paquets += 1;

                //On réinitialise le nombre d'essais, et le timer
                nb_essais = 0;
                delai.tv_sec = TIMEOUT_SEC;
                delai.tv_usec = 0;
            }
        }
        else{ //Si on ne reçoit rien, on reset le timer et on recommence
            delai.tv_sec = TIMEOUT_SEC;
            delai.tv_usec = 0;
        }
	}
}

//Envoie les paquet selon le protocole go back n
void send_go_back_n(int sockfd, struct sockaddr_in * destination_addr, int * seq_source){
    int nb_essais = 0; //Nombre d'essais avant abandon
    int taille_fenetre = 1; //Taille de la fenêtre courrante
    Paquet p_recu;

    int nb_paquets = 0; //Nombre de paquets envoyés.

    //On prépare les variables pour le timeout
    fd_set lecture;
    struct timeval delai;
    delai.tv_sec = TIMEOUT_SEC;
    delai.tv_usec = 0;
    while(nb_paquets < NB_PAQUETS){
        nb_essais = inc_essais(nb_essais); //On démarre une nouvelle tentative        

        int nb_paquet_non_ack = 0;
        //On envoie tout les paquets que nous permet la taille de la fenêtre 
        for(int i = 0; i < taille_fenetre; i++){
            send_paquet(sockfd, destination_addr, (*seq_source + i)%MAX_SEQ, 0, DATA, taille_fenetre, 0);
            nb_paquet_non_ack++;
        }

        //On reçoit la fenêtre
        while(nb_paquet_non_ack > 0){
            add_log("fenetre", taille_fenetre);
            FD_ZERO(&lecture);
            FD_SET(sockfd, &lecture);
            select(sockfd+1, &lecture, NULL, NULL, &delai);
            if(FD_ISSET(sockfd, &lecture)){
                rcv_paquet(sockfd, destination_addr, &p_recu);
                if(p_recu.type == ACK){
                    //On calcule combien de segments ont été acquités
                    int diff_seqnum = p_recu.num_acquittement - *seq_source;
                    if(diff_seqnum < 0){ //On corige le problème du modulo
                        diff_seqnum+=MAX_SEQ;
                    }

                    *seq_source = p_recu.num_acquittement; //on met à jour le nouveau numéro de séquence
                    nb_paquet_non_ack -= diff_seqnum;

                    //Nombre de paquet acquittés
                    nb_paquets += diff_seqnum;
 
                    //On aggrandit ou diminue la fenêtre
                    if(p_recu.ecn > 0){
                        add_log("ecn", 0);
                        if (taille_fenetre > 1) {
                           taille_fenetre *= 0.9;
                         }
                        break;
                    }                    
                    else if (taille_fenetre < 255) {
                        taille_fenetre++;
                    }

                    //On réinitialise le nombre d'essais, et le timer
                    nb_essais = 0;
                    delai.tv_sec = TIMEOUT_SEC;
                    delai.tv_usec = 0;
                }
            }
            else{ //Si il y a un timeout
                add_log("timeout", 0);
                //On divise la taille de la fenêtre
                if (taille_fenetre > 1) {
                    taille_fenetre /= 2;
                }

                //On reset le timer
                delai.tv_sec = TIMEOUT_SEC;
                delai.tv_usec = 0;
                break;
            }
        }
    }
}

int main(int argc, char *argv[]){

    //On vérifie les argments
    Args args = verify_arg(argc, argv);
    
    //On crée le système de logs
    fichier = fopen("./data/medium_peu_tolerant/ecn/4.log", "w+");
    if (fichier != NULL){
        fprintf(fichier, "type;temps;clock;fen\n");
        add_log("debut", 0);
    }
    else{
        printf("Impossible d'ouvrir le fichier de logs.");
    }

    //On crée une socket
    struct sockaddr_in destination_addr;
    int sockfd = create_socket(args.port_local, args.port_ecoute_src_pertupateur, args.ip_distante, &destination_addr);

    int seq_source = rand() % MAX_SEQ; //Numéro de séquence de la source, le mien
    int seq_destination; //Numéro de séquence de la destination, inconu pour l'instant

    connection(sockfd, &destination_addr, &seq_source, &seq_destination);

    if(args.mode == STOP_AND_WAIT){
        seq_source = seq_source%2;
        send_stop_and_wait(sockfd, &destination_addr, &seq_source);
    }
    else{
        send_go_back_n(sockfd, &destination_addr, &seq_source);
    }

    deconnection(sockfd, &destination_addr, &seq_source);
    
    close(sockfd);
    if (fichier != NULL){
        add_log("fin", 0);
        fclose(fichier);
    }


    return EXIT_SUCCESS;
}

