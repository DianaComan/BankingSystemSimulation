#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <fstream>

#include "errors.h"

using namespace std;

#define BUFFLEN 300

struct sockaddr_in serv_addr;
int serv_len = sizeof(serv_addr);

bool conectat = false;
int numar_card;
ofstream log_file;


void do_login(int sock_tcp, char *buffer) {
	
	char recv_buf[BUFFLEN];
	
	if(conectat) {
		log_file << buffer;
		log_file << eroare2 << "\n";
		cout << eroare2 << "\n";
	} else {
		char buf_aux[BUFFLEN];
		strcpy(buf_aux, buffer);
		// iau numarul de card al clientului
		sscanf(buf_aux, "%*s %d", &numar_card);;
		
		// trimti cererea de login
		send(sock_tcp, buffer, strlen(buffer) + 1, 0);
		recv(sock_tcp, recv_buf, BUFFLEN, 0);

		if (strstr(recv_buf, "Welcome")) {
			conectat = true;
		}

		log_file << buffer;
		log_file << recv_buf << "\n";
		cout << recv_buf << "\n";
	}
}

void do_logout(int sock_tcp, char *buffer) {
	char recv_buf[BUFFLEN];

	if(!conectat) {
		log_file << buffer;
		log_file << eroare1 << "\n";
		cout << eroare1 << "\n";
	} else {
		// trimti cererea de logout
		send(sock_tcp, buffer, strlen(buffer) + 1, 0);
		recv(sock_tcp, recv_buf, BUFFLEN, 0);
		
		if (strstr(recv_buf, "IBANK> Clientul")) {
			conectat = false;
		}

		log_file << buffer;
		log_file << recv_buf << "\n";
		cout << recv_buf << "\n";

	}
}

void do_listsold(int sock_tcp, char *buffer) {
	char recv_buf[BUFFLEN];

	if(!conectat) {
		log_file << buffer;
		log_file << eroare1 << "\n";
		cout << eroare1 << "\n";
	} else {
		// trimti cererea de listsold
		send(sock_tcp, buffer, strlen(buffer) + 1, 0);
		recv(sock_tcp, recv_buf, BUFFLEN, 0);

		log_file << buffer;
		log_file << recv_buf << "\n";
		cout << recv_buf << "\n";
	}

}

void do_transfer(int sock_tcp, char *buffer) {

	char recv_buf[BUFFLEN];
	char tmp_buf[BUFFLEN];

	if(!conectat) {
		log_file << buffer;
		log_file << eroare1 << "\n";
		cout << eroare1 << "\n";
	} else {

		// trimti cererea de transfer
		send(sock_tcp, buffer, strlen(buffer) + 1, 0);
		recv(sock_tcp, recv_buf, BUFFLEN, 0);
		log_file << buffer;
		log_file << recv_buf << "\n";
		cout << recv_buf << "\n";

		if(strstr(recv_buf, "Transfer")) {
			// raspund la server daca se va efectua sau nu transferul
			fgets(tmp_buf, BUFFLEN, stdin);
			send(sock_tcp, tmp_buf, strlen(tmp_buf) + 1, 0);
			memset(recv_buf, 0, BUFFLEN);
			recv(sock_tcp, recv_buf, BUFFLEN, 0);
			log_file << tmp_buf;
			log_file << recv_buf << "\n";
			cout << recv_buf << "\n";
		}
	}

}

void do_unlock(int sock_udp, char *buffer) {
	char recv_buf[BUFFLEN];
	char tmp_buf[BUFFLEN];

	// creez si trimit mesajul de unlock
	strcpy(tmp_buf, "unlock ");
	strcat(tmp_buf, to_string(numar_card).c_str());
	sendto(sock_udp, tmp_buf, strlen(tmp_buf)+1, 0, (struct sockaddr *) &serv_addr, (socklen_t) serv_len);
	recvfrom(sock_udp, recv_buf, BUFFLEN, 0, (struct sockaddr *) &serv_addr, (socklen_t*) &serv_len);
	log_file << buffer;
	log_file << recv_buf << "\n";
	cout << recv_buf << "\n";

	if(strstr(recv_buf, "Trimite parola secreta")) {
		// in functie de raspunsul serverului trimit sau nu parola secreta
		char buf[BUFFLEN];
		fgets(buf, BUFFLEN, stdin);
		memset(tmp_buf, 0, BUFFLEN);
		sprintf(tmp_buf, "%s %s",to_string(numar_card).c_str(), buf);
		sendto(sock_udp, tmp_buf, strlen(tmp_buf)+1, 0, (struct sockaddr *) &serv_addr, (socklen_t) serv_len);
		recvfrom(sock_udp, recv_buf, BUFFLEN, 0, (struct sockaddr *) &serv_addr, (socklen_t*) &serv_len);

		log_file << buffer;
		log_file << recv_buf << "\n";
		cout << recv_buf << "\n";
	}
}

void do_quit(int sock_tcp) {

	// quit de la tastatura
	send(sock_tcp, "quit", 4, 0);
	log_file << "quit\n";
	
}

void parse_command(int sock_udp, int sock_tcp, char *buffer) {

	if(strstr(buffer, "login")) {

		do_login(sock_tcp, buffer);	

	}else if(strstr(buffer, "logout")) {

		do_logout(sock_tcp, buffer);

	} else if(strstr(buffer, "listsold")) {

		do_listsold(sock_tcp, buffer);
	
	} else if(strstr(buffer, "transfer")) {

		do_transfer(sock_tcp, buffer);
	
	}  else if(strstr(buffer, "unlock")) {

		do_unlock(sock_udp, buffer);
	
	} 
}

// deschid un logfile pentru client
void set_log_file() {
	
	string file_name = "client-" + to_string(getpid()) + ".log";
	log_file.open(file_name);
}

int main(int argc, char *argv[]) {

	int sock_tcp, sock_udp, fd_max;
	char buf[BUFFLEN];
	fd_set read_fds;

	if (argc < 3) {
       fprintf(stderr,"Usage %s server_addr port\n", argv[0]);
       exit(0);
    }  

   set_log_file();

	sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_tcp < 0) {
		cout << "-10 : Eroare la apel socket pe tcp\n";
		log_file << "-10 : Eroare la apel socket pe tcp\n";
	}

	sock_udp = socket(AF_INET, SOCK_DGRAM,0);
	if(sock_udp < 0) {
		cout << "-10 : Eroare la apel socket pe tcp\n";
		log_file << "-10 : Eroare la apel socket pe udp\n";
	}

	FD_ZERO(&read_fds);
	FD_SET(fileno(stdin),&read_fds);
	FD_SET(sock_udp, &read_fds);
	FD_SET(sock_tcp, &read_fds);
	fd_max = sock_tcp;

	serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    if (connect(sock_tcp,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) { 
        cout << "-10 : Eroare la apel connect\n"; 
        log_file << "-10 : Eroare la apel connect\n";   
    }

    while(1) {

    	fd_set tmp_fds = read_fds;
        if (select(fd_max + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
            log_file << "-10 : Eroare la apel select\n";
        }

        // citesc comenzi de la tastatura
       if(FD_ISSET(fileno(stdin), &tmp_fds)) {
			memset(buf, 0, BUFFLEN);
        	fgets(buf, BUFFLEN, stdin);
        	if(strstr(buf, "quit")) {
        		do_quit(sock_tcp);
        		close(sock_tcp);
				close(sock_udp);
				log_file.close();
				return 0;
        	}else {
        		parse_command(sock_udp, sock_tcp, buf);
        	}      
        	
       	// primesc comanda de quit de la server
        } else if(FD_ISSET(sock_tcp, &tmp_fds)) {
        	memset(buf, 0, BUFFLEN);
        	int r = recv(sock_tcp, buf, BUFFLEN, 0);
        	if (r == 0) {
        		break;
        	}
        	cout << buf << endl;
        	if(strstr(buf, "quit")) {
        		log_file << "quit\n";
  				log_file.close();
        		close(sock_tcp);
   				close(sock_udp);
				return 0;
        	}
        } 
    }

    log_file << "quit\n";
    log_file.close();
    close(sock_tcp);
    close(sock_udp);
	return 0;
}