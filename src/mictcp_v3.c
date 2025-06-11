#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdbool.h>
#define TABMAX 1000
#define MAXTIMEOUT 100
#define NUM_SEQ_INIT 0
#define PERCENT_LOSS 5
#define MAX_PERCENT 100
#define BUFFER_SIZE 100
/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

int num_seq = NUM_SEQ_INIT;
int fd_compteur = 0;
int id_send = 0;
int buffer_lost [BUFFER_SIZE];
struct mic_tcp_sock tab[TABMAX]; //on met 1000 de manière arbitraire pour l'instant

bool needToSend(){
    int nb_loss = 0;
    for(int i = 0; i < BUFFER_SIZE; i++){
        nb_loss += buffer_lost[i];
    }
    return ((float)nb_loss/BUFFER_SIZE)*MAX_PERCENT > PERCENT_LOSS;
}

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
   if(socket > TABMAX){
    return -1;}
   tab[socket].local_addr=addr;
   return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
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
    struct mic_tcp_pdu* pdu = malloc(sizeof(struct mic_tcp_pdu));

    pdu->header.dest_port =  tab[mic_sock].remote_addr.port;
    pdu->header.seq_num = num_seq;
    pdu->payload.data = msg;
    pdu->payload.size = msg_size;
    int sent_data = IP_send(*pdu, tab[mic_sock].remote_addr.ip_addr);
    id_send = (id_send + 1)%BUFFER_SIZE;

    int recv = IP_recv(&pk,&local_addr,&remote_addr,100);
    if(recv != -1 && pk.header.ack == 1 && pk.header.ack_num == num_seq){
        buffer_lost[id_send] = 0;
    } else {
        buffer_lost[id_send] = 1;
        if(needToSend()){
            while(1){
                int recv = IP_recv(&pk,&local_addr,&remote_addr,100);
                if(recv != -1 && pk.header.ack==1 && pk.header.ack_num == num_seq){
                    break;
                }
                sent_data = IP_send(*pdu, tab[mic_sock].remote_addr.ip_addr);
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
    app_buffer_put(pdu.payload);

    struct mic_tcp_pdu pduAck;
    printf("num_seq : %d\n", pdu.header.seq_num);
    pduAck.header.ack_num = pdu.header.seq_num;
    pduAck.header.ack = 1;
    pduAck.header.syn = 0;
    int sent_data = IP_send(pduAck, remote_addr);
}

