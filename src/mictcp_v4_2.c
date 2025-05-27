#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdbool.h>
#define TABMAX 1000
#define MAXTIMEOUT 100
#define NUM_SEQ_INIT 0
#define PERCENT_LOSS 5
#define MAX_PERCENT 100
#define BUFFER_SIZE 100

// Données correspondantes à la gestion d'échange de messages
int num_seq = NUM_SEQ_INIT;
int id_send = 0;
int buffer_lost [BUFFER_SIZE];

// Données correspondantes aux sockets
int fd_compteur = 0;
struct mic_tcp_sock tab[TABMAX];
struct mic_tcp_sock server_socket;

// Données utilisées pour gérer les états
bool connected = false;
pthread_mutex_t lockState = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_syn_recv = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_established = PTHREAD_COND_INITIALIZER;


/*
 * Verifie le pourcentage de pertes actuels de message
 * Retourne la necessité de renvoyer un message ou non
 */
bool needToSend(){
    int nb_loss = 0;
    for(int i = 0; i < BUFFER_SIZE; i++){
        nb_loss += buffer_lost[i];
    }
    return ((float)nb_loss/BUFFER_SIZE)*MAX_PERCENT > PERCENT_LOSS;
}

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   struct mic_tcp_sock mysock;  
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   int result = initialize_components(sm);
   set_loss_rate(10);
   mysock.fd = fd_compteur;
   fd_compteur +=1;
   mysock.state = IDLE;
   tab[mysock.fd]=mysock;
    return mysock.fd;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   server_socket.local_addr = addr;
   return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    struct mic_tcp_pdu pduAck;
    printf("Etat courant : IDLE\n");
    pthread_mutex_lock(&lockState);
    while(server_socket.state != SYN_RECEIVED) {
          pthread_cond_wait(&cond_syn_recv, &lockState);
    }
    printf("Etat courant : SYN_RECEIVED\n");
    while(server_socket.state != ESTABLISHED) {
          pthread_cond_wait(&cond_established, &lockState);
    }
    pthread_mutex_unlock(&lockState);
    printf("Etat courant : ESTABLISHED\n");
    return 1;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    if(socket > TABMAX){
    return -1;}

    printf("Connection : socket ok à l'envoie \n");
    struct mic_tcp_pdu pk;
    pk.payload.size = 0;
    struct mic_tcp_ip_addr remote_addr;
    struct mic_tcp_ip_addr local_addr;
    struct mic_tcp_pdu pdu;

    pdu.header.dest_port = addr.port;
    pdu.header.syn = 1;
    pdu.header.ack = 0;
    pdu.payload.size = 0;
    int sent_data = IP_send(pdu, addr.ip_addr);
    while(1){
        int recv = IP_recv(&pk,&local_addr,&remote_addr,100);
        if(recv != -1 && pk.header.ack==1 && pk.header.syn == 1){
            break;
        }
        //sent_data = IP_send(pdu, addr.ip_addr);
    }
    pdu.header.syn = 0;
    pdu.header.ack = 1;
    sent_data = IP_send(pdu, addr.ip_addr);
    tab[socket].remote_addr = addr;
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */

int mic_tcp_send (int mic_sock, char* msg, int msg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    
    struct mic_tcp_pdu pk;
    pk.payload.size = 0;
    struct mic_tcp_ip_addr remote_addr;
    struct mic_tcp_ip_addr local_addr;
    struct mic_tcp_pdu pdu;

    pdu.header.dest_port =  tab[mic_sock].remote_addr.port;
    pdu.header.seq_num = num_seq;
    pdu.header.ack = 0;
    pdu.header.syn = 0;
    pdu.payload.data = msg;
    pdu.payload.size = msg_size;
    if(!connected){
        pdu.header.ack = 1;
    }
    int sent_data = IP_send(pdu, tab[mic_sock].remote_addr.ip_addr);
    id_send = (id_send + 1)%BUFFER_SIZE;
    int recv = IP_recv(&pk,&local_addr,&remote_addr,100);
    if(recv != -1 && pk.header.ack == 1 && pk.header.syn==0){
        if(!connected){
            connected = true;
        }
        if(pk.header.ack_num == num_seq){
            buffer_lost[id_send] = 0;
        }
    } else {
        buffer_lost[id_send] = 1;
        if(needToSend()){
            while(1){
                int recv = IP_recv(&pk,&local_addr,&remote_addr,100);
                if(recv != -1 && pk.header.ack==1 && pk.header.syn==0 && pk.header.ack_num == num_seq){
                    break;
                }
                sent_data = IP_send(pdu, tab[mic_sock].remote_addr.ip_addr);
            }
            buffer_lost[id_send] = 0;
        }
    }
    num_seq = (num_seq + 1)%2;
    return sent_data;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* msg, int max_msg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_payload payload;
    payload.data = msg;
    payload.size = max_msg_size;
    int wrote = app_buffer_get(payload);

    return wrote;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    tab[socket].state = CLOSED;
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    if(server_socket.state == ESTABLISHED){
        app_buffer_put(pdu.payload);
        struct mic_tcp_pdu pduAck;
        pduAck.header.ack_num = pdu.header.seq_num;
        pduAck.header.syn = 0;
        pduAck.header.ack = 1;
        int sent_data = IP_send(pduAck, remote_addr);
    }

    if(!pdu.header.syn && pdu.header.ack){
        pthread_mutex_lock(&lockState);
        if(server_socket.state == SYN_RECEIVED){
            server_socket.state = ESTABLISHED;
            pthread_cond_broadcast(&cond_established);
        }
        pthread_mutex_unlock(&lockState);
    }

    if(pdu.header.syn && !pdu.header.ack){
        pthread_mutex_lock(&lockState);
        if(server_socket.state == IDLE){
            server_socket.state = SYN_RECEIVED;
            server_socket.remote_addr.ip_addr = remote_addr;
            server_socket.remote_addr.port = pdu.header.source_port;
            struct mic_tcp_pdu pduAck;
            pduAck.header.syn = 1;
            pduAck.header.ack = 1;
            pduAck.header.dest_port = server_socket.remote_addr.port;
            int sent_data = IP_send(pduAck, server_socket.remote_addr.ip_addr);
            pthread_cond_broadcast(&cond_syn_recv);
        }

        pthread_mutex_unlock(&lockState);
    }

}

