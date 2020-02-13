#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#include "errors.h"

using namespace std;

#define BUFFLEN 300

// structura unui client ibank
struct ibank_client{
	string nume;
	string prenume;
	int numar_card;
	int pin;
	string parola_secreta;
	double sold;
	bool conectat = false;
	bool blocat = false;
	int incercari = 0;
    int sock_tcp = -1;
};


struct sockaddr_in serv_addr, cli_addr;
int cli_len = sizeof(cli_addr);
unordered_map<int, ibank_client> clienti;
fd_set read_fds;

//numarul maxim de conexiuni pe care asculta serverul
int MAX_CLIENTS = 0;

void get_clients(char *file) {

	int n;
    ifstream input;
    string f(file);
    input.open(f);
	input >> n;
	MAX_CLIENTS = n;

	for(int i = 0; i < n; i++) {
		ibank_client tmp;

		input >> tmp.nume; 
        input >> tmp.prenume;
        input >> tmp.numar_card;
        input >> tmp.pin; 
        input >> tmp.parola_secreta;
        input >> tmp.sold;
		
        clienti[tmp.numar_card] = tmp;
	}

	input.close();
}

// functie care se ocupa de login
void do_login(int sock_tcp, char *buffer) {

    int numar_card, pin;
    char send_buf[BUFFLEN];

    // citesc numarul de card primit si pinul
    sscanf(buffer, "%*s %d %d", &numar_card, &pin);

    // caz in care nu exista numarul de card in fisier 
    if(clienti.find(numar_card) == clienti.end()) {
        strcpy(send_buf, ibank);
        strcat(send_buf, eroare4);
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
    
    // caz inc are clientul e deja conectat pe o alta conexiune
    } else if(clienti[numar_card].conectat) {      
        strcpy(send_buf, ibank);
        strcat(send_buf, eroare2);
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);

    // caz in care cardul primit este blocat 
    } else if(clienti[numar_card].blocat) {
        strcpy(send_buf, ibank);
        strcat(send_buf, eroare5);
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
    
    // caz in care pinul nu este corect
    } else if( clienti[numar_card].pin != pin) {
        clienti[numar_card].incercari += 1;
        if(clienti[numar_card].incercari == 3) {
            strcpy(send_buf, ibank);
            strcat(send_buf, eroare5);
            clienti[numar_card].blocat = true;
        } else {
            strcpy(send_buf, ibank);
            strcat(send_buf, eroare3);
        }
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
    } else {
        //  caz in care este acceptat numarul de card si pinul
        clienti[numar_card].conectat = true;
        clienti[numar_card].sock_tcp = sock_tcp;
        clienti[numar_card].incercari = 0;
        strcpy(send_buf, ibank);
        string s = welcome + clienti[numar_card].nume + " " +
                     clienti[numar_card].prenume + "\n";
        strcat(send_buf, s.c_str());
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
    }
}

void do_logout(int sock_tcp) {

    int numar_card;
    char send_buf[BUFFLEN];

    // caut clientul care a trimits comanda de logout
    for( auto i : clienti) {
        if (i.second.sock_tcp == sock_tcp)  {
            numar_card = i.second.numar_card;
            break;
        }   
    }
    
    clienti[numar_card].conectat = false;
    sprintf(send_buf, "%s", deconectare);
    send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
}

void do_listsold(int sock_tcp) {
    
    int numar_card;
    char send_buf[BUFFLEN];

    // caut clientul care a trimits comanda de listsold
    for( auto i : clienti) {
        if (i.second.sock_tcp == sock_tcp)  {
            numar_card = i.second.numar_card;
            break;
        }   
    }
    
    // pun suma in pachet si o trimit la client
    sprintf(send_buf, "%s%.2lf\n", ibank, clienti[numar_card].sold);
    send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
}


void do_transfer(int sock_tcp, char *buffer) {

    int numar_card_dst, numar_card_src;
    char send_buf[BUFFLEN];
    char tmp_buf[BUFFLEN];
    double suma;

    // iau cardul catre care transfer
    sscanf(buffer, "%*s %d %lf", &numar_card_dst, &suma);
    
    for( auto i : clienti) {
        if (i.second.sock_tcp == sock_tcp)  {
            numar_card_src = i.second.numar_card;
            break;
        }   
    }

    // numarul de card nu este valid
    if(clienti.find(numar_card_dst) == clienti.end()) {
    
        sprintf(send_buf, "%s%s", ibank, eroare4);
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
    
    // soldul clientului de unde transfer este mai mic decat suma
    } else if(clienti[numar_card_src].sold < suma) {
    
        sprintf(send_buf, "%s%s", ibank, eroare8);
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
    
    } else {
        // cazul cand fac incepe transferul
        sprintf(send_buf, "%sTransfer %.2lf catre %s %s? [y/n]", ibank, suma, 
            clienti[numar_card_dst].nume.c_str(), clienti[numar_card_dst].prenume.c_str());
        send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
        recv(sock_tcp, tmp_buf, BUFFLEN, 0);

        // daca primesc 'y'
        if(tmp_buf[0] == 'y') {
            
            clienti[numar_card_dst].sold += suma;
            clienti[numar_card_src].sold -= suma;
            memset(send_buf, 0, BUFFLEN);
            sprintf(send_buf, "%s", succes_transfer);
            send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);

        // daca primesc altvceva    
        } else {
            memset(send_buf, 0, BUFFLEN);
            sprintf(send_buf, "%s%s", ibank, eroare9);
            send(sock_tcp, send_buf, strlen(send_buf) + 1, 0);
        }
    }
}

void do_unlock(int sock_udp, char *buffer) {

    char send_buf[BUFFLEN];
    int numar_card;   

    if(strstr(buffer, "unlock")) {
        sscanf(buffer, "%*s %d", &numar_card);
        // daca numarul de card nu exista
        if(clienti.find(numar_card) == clienti.end()) {
            sprintf(send_buf, "%s%s", unlock, eroare4);
            sendto(sock_udp, send_buf, strlen(send_buf)+1, 0, (struct sockaddr *) &cli_addr, (socklen_t) cli_len);
        // daca numarul de card primit nu e blocat
        } else if(!clienti[numar_card].blocat) {
            sprintf(send_buf, "%s%s", unlock, eroare6);
            sendto(sock_udp, send_buf, strlen(send_buf)+1, 0, (struct sockaddr *) &cli_addr, (socklen_t) cli_len);
        } else {
            // "Trimite parola..."
            sprintf(send_buf, "%s", trimite_parola);
            sendto(sock_udp, send_buf, strlen(send_buf)+1, 0, (struct sockaddr *) &cli_addr, (socklen_t) cli_len);
        }

    } else {
        // dupa ce primesc parola secreta
        char p[20];
        sscanf(buffer, "%d %s", &numar_card, p);
        string parola_secreta(p);
        if(clienti.find(numar_card) == clienti.end()) {
            sprintf(send_buf, "%s%s", unlock, eroare4);
            sendto(sock_udp, send_buf, strlen(send_buf)+1, 0, (struct sockaddr *) &cli_addr, (socklen_t) cli_len);
        } else if(!clienti[numar_card].blocat) {
            sprintf(send_buf, "%s%s", unlock, eroare6);
            sendto(sock_udp, send_buf, strlen(send_buf)+1, 0, (struct sockaddr *) &cli_addr, (socklen_t) cli_len);
        } else {
            if(clienti[numar_card].parola_secreta == parola_secreta) {
                clienti[numar_card].blocat = false;
                clienti[numar_card].incercari = 0;
                sprintf(send_buf, "%s", succes_deblocare);
                sendto(sock_udp, send_buf, strlen(send_buf)+1, 0, (struct sockaddr *) &cli_addr, (socklen_t) cli_len);
            } else {
                sprintf(send_buf, "%s%s", unlock, eroare7);
                sendto(sock_udp, send_buf, strlen(send_buf)+1, 0, (struct sockaddr *) &cli_addr, (socklen_t) cli_len);
            }
        }
    }
}

void do_quit(int sock_tcp) {
    int numar_card;
    bool is_active = false;
    
    // caut numarul de card al clientului care a facut quit
    for( auto i : clienti) {
        if (i.second.sock_tcp == sock_tcp)  {
            numar_card = i.second.numar_card;
            is_active = true;
            break;
        }   
    }

    close(sock_tcp);
    FD_CLR(sock_tcp, &read_fds);
    
    if(is_active) {
        clienti[numar_card].conectat = false;
        clienti[numar_card].incercari = 0;
    }

}

void parse_tcp_command(int fd, char *buffer) {

    if(strstr(buffer, "login")) {

        do_login(fd, buffer);
    
    } else if(strstr(buffer, "logout")) {
    
        do_logout(fd);
   
    } else if(strstr(buffer, "listsold")) {
   
        do_listsold(fd);
   
    } else if(strstr(buffer, "transfer")) {
    
        do_transfer(fd, buffer);
    
    } else if(strstr(buffer, "quit")) {

        do_quit(fd);   
    }
}

int main(int argc, char *argv[]) {

	int sock_tcp, sock_udp, fd_max;
	char buf[BUFFLEN];
	

	if (argc < 2) {
        fprintf(stderr,"Usage : %s port\n", argv[0]);
        exit(0);
    }

    get_clients(argv[2]);

	sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_tcp < 0) {
		cout << "-10 : Eroare la apel socket pe tcp\n";
	}

	sock_udp = socket(AF_INET, SOCK_DGRAM,0);
	if(sock_udp < 0) {
		cout << "-10 : Eroare la apel socket pe udp\n";
	}

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;	
    serv_addr.sin_port = htons( atoi(argv[1]) );
    
   
    if (bind(sock_udp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) { 
       cout << "-10 : Eroare la apel bind pe udp\n";
    }
    if (bind(sock_tcp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) { 
       cout << "-10 : Eroare la apel bind pe tcp\n";
    }
     
    
    listen(sock_tcp, MAX_CLIENTS);

   	FD_ZERO(&read_fds);
    FD_SET(fileno(stdin), &read_fds);
   	FD_SET(sock_udp, &read_fds);
    FD_SET(sock_tcp, &read_fds);
    fd_max = sock_tcp;


    while(1) {

    	fd_set tmp_fds = read_fds;
        if (select(fd_max + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
            cout << "-10 : Eroare la apel select\n";
        }

        for(int fd = 0; fd <= fd_max; fd++) {
    		
            // verificare daca am primit ceva de la tastatura
    		if(FD_ISSET(fd, &tmp_fds) && fd == fileno(stdin)) {
            	memset(buf, 0, BUFFLEN);
                fgets(buf, BUFFLEN, stdin);
                if(strstr(buf, "quit")) {
                    for (int i = 0; i < fd_max; i++) {
                        send(i, buf, strlen(buf)+ 1, 0);
                        close(i); 
                        FD_CLR(i, &read_fds);
                    }
                    cout << "Shuting down!\n";
                    close(sock_tcp);
                    close(sock_udp);
                    return 0;
                }
            // verificare daca am primit ceva pe socketul udp
            }else if(FD_ISSET(fd, &tmp_fds) && fd == sock_udp){
                recvfrom(sock_udp, buf, BUFFLEN, 0, (struct sockaddr *) &cli_addr, (socklen_t*) &cli_len);
                do_unlock(fd, buf);
            // verificare daca am primit o noua conexiune tcp
            }else if(FD_ISSET(fd, &tmp_fds) && fd == sock_tcp){
				int sock_new = accept(sock_tcp, (struct sockaddr *)&cli_addr, (socklen_t*) &cli_len);
    			FD_SET(sock_new, &read_fds);
				fd_max = max(sock_new, fd_max);
            
            // primsec mesaje de la clienti
        	} else if(FD_ISSET(fd, &tmp_fds)){
    			memset(buf, 0, BUFFLEN);
				recv(fd, buf, BUFFLEN,0);
				cout << "BUFFER: " << buf << endl;
                parse_tcp_command(fd, buf);
    		}
    	}
    }

    close(sock_tcp);
    close(sock_udp);
	return 0;
}