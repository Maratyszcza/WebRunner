#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <webserver/options.h>
#include <webserver/logs.h>
#include <webrunner.h>

int main(int argc, char** argv) {
	struct options options = parse_options(argc, argv);
	setup_logs(options.error_log, options.access_log);

	int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == -1) {
		log_fatal("failed to create socket: %s\n", strerror(errno));
	}

	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) == -1) {
		log_error("failed to set SO_REUSEADDR socket option: %s\n", strerror(errno));
	}

	struct sockaddr_in server_address = { 0 };
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(options.port);

	if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) == -1) {
		log_fatal("failed to bind socket: %s\n", strerror(errno));
	}

	if (listen(server_socket, options.queue_size) == -1) {
		log_fatal("failed to listen on socket: %s\n", strerror(errno));
	}

	while (1) {
		struct sockaddr_in client_address;
		unsigned int client_address_size = sizeof(client_address);

		int client_socket = accept(server_socket, (struct sockaddr*) &client_address, &client_address_size);
		if (client_socket == -1) {
			log_error("failed to accept socket: %s\n", strerror(errno));
			switch (errno) {
				case ECONNABORTED:
				case EINTR:
				case EPROTO:
					break;
				default:
					exit(EXIT_FAILURE);
			}
		} else {
			pid_t fork_process = fork();
			if (fork_process == -1) {
				log_error("failed to fork a process: %s\n", strerror(errno));
			} else if (fork_process == 0) {
				/* Child process */
				close(server_socket);
				process_request(client_socket);
				exit(0);
			} else {
				/* Parent process */
				close(client_socket);
				int status = 0;
				waitpid(fork_process, &status, 0);
			}
		}
	}

	return EXIT_SUCCESS;
}
