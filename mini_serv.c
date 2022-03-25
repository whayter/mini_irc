#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client {
    int id;
    int fd;
    char *data_in;
	struct s_client* next;
} t_client;

int sockfd = 0;
int id = 0;
int nfds = 1024;
t_client* clients = NULL;

void error(char* msg)
{
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\n", 1);
    exit(1);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0) {
		strcat(newbuf, buf);
	    free(buf);
    }
	strcat(newbuf, add);
	return (newbuf);
}

t_client* create_client()
{
    t_client* new_client;
    t_client* it;
    int fd;

    if ((fd = accept(sockfd, 0, 0)) < 0)
        error("Fatal error");
    if (!(new_client = (t_client*)malloc(sizeof(t_client))))
        error("Fatal error");
    new_client->id = id++;
    new_client->fd = fd;
    new_client->data_in = NULL;
	new_client->next = NULL;
    if (!(it = clients))
        clients = new_client;
    else
    {
        while (it->next)
            it = it->next;
    }
    return (new_client);
}

t_client* delete_client(fd_set* fds, t_client* client)
{
    t_client* it = clients;
    t_client* ret;

    close(client->fd);
    FD_CLR(client->fd, fds);
    if (it = client) {
        clients = client->next;
        ret = clients;
    }
    while (it)
    {
        if (it->next == client)
        {
            it->next = client->next;
            ret = client->next;
            break ;
        }
        it = it->next;
    }
    if (client->data_in)
        free(client->data_in);
    free(client);
    return (ret);
}

void broadcast_message(char* message, fd_set* fds, int client_fd)
{
    t_client* it = clients;

    while (it)
    {
        if (it->fd != client_fd && FD_ISSET(it->fd, &fds))
            send(it->fd, message, strlen(message), 0);
        it = it->next;
    }
}

void run()
{
    char buffer[1024];
    char* msg;
    char* extracted;
    fd_set fds, readfds;
    t_client* it;
    int ret;

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    while (1)
    {
        FD_COPY(&fds, &readfds);
        if (select(nfds, &readfds, 0, 0, 0) < 0)
            error("Fatal error");
        if (FD_ISSET(sockfd, &readfds))
        {
            t_client* new_client = create_client();
            sprintf(buffer, "server: client %d just arrived\n", new_client->id);
            broadcast_message(buffer, &fds, new_client->fd);
            FD_SET(new_client->fd, &fds);
        }
        it = clients;
        while (it)
        {
            if (FD_ISSET(it->fd, &readfds))
            {
                ret = recv(it->fd, buffer, sizeof(buffer) - 1, 0);
                if (ret <= 0)
                {
                    sprintf(buffer, "server: client %d just left\n", it->id);
					broadcast_message(buffer, &fds, it->fd);
                    it = delete_client(&fds, it);
					continue;
                }
                else
                {
                    buffer[ret] = 0;
                    it->data_in = str_join(it->data_in, buffer);
                    extracted = NULL;
                    while (extract_message(&(it->data_in), &extracted))
					{
						if (!(msg = (char*)malloc(100 + strlen(extracted))))
							error("Fatal error");
						sprintf(msg, "client %d: %s", it->id, extracted);
						broadcast_message(msg, &fds, it->fd);
						free(msg);
						free(extracted);
						extracted = NULL;
					}
                }
            }
            it = it->next;
        }
    }
}

int main(int ac, char** av)
{
	struct sockaddr_in servaddr; 

    if (ac != 2)
        error("Wrong number of arguments");

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        error("Fatal error");

	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(8081);

	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        error("Fatal error");

	if (listen(sockfd, 10) != 0)
        error("Fatal error");

    return (0);
}