#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct s_client
{
    int id;
    int fd;
    char* dt;
    struct s_client* next;
} t_client;

int id = 0;
int sockfd = 0;
int nfds = 1024;
t_client* clients = NULL;

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
	if (buf != 0)
    {
		strcat(newbuf, buf);
	    free(buf);
    }
	strcat(newbuf, add);
	return (newbuf);
}

void error(char* msg, int code)
{
    if (code == 1)
    {
        close(sockfd);
        t_client* it = clients;
        t_client* ite = clients;
        while (it)
        {
            ite = it->next;
            if (it->dt)
                free(it->dt);
            free(it);
            it = ite;
        }
    }
    write(2, msg, strlen(msg));
    write(2, "\n", 1);
    exit(1);
}

t_client* create_client()
{
    t_client* new_client;
    t_client* it = clients;
    int fd;

    if ((fd = accept(sockfd, 0, 0)) < 0)
        error("Fatal error", 1);
    if (!(new_client = (t_client*)malloc(sizeof(t_client))))
        error("Fatal error", 1);
    new_client->id = id++;
    new_client->fd = fd;
    new_client->dt = NULL;
    new_client->next = NULL;
    if (!it)
        clients = new_client;
    else
    {
        while (it->next)
            it = it->next;
        it->next = new_client;
    }
    return (new_client);
}

t_client* destroy_client(fd_set* fds, t_client* client)
{
    t_client* it = clients;
    t_client* next = client->next;

    if (it == client)
        clients = next;
    else
    {
        while (it->next != client)
            it = it->next;
        it->next = next;
    }
    close(client->fd);
    FD_CLR(client->fd, fds);
    if (client->dt)
        free(client->dt);
    free(client);
    return (next);
}

void broadcast_msg(fd_set* fds, char* msg, t_client* client)
{
    t_client* it = clients;
    while (it)
    {
        if (it != client && FD_ISSET(it->fd, fds))
            send(it->fd, msg, strlen(msg), 0);
        it = it->next;
    }
}

void run()
{
    char buf[1024];
    char* extr;
    char* msg;
    fd_set fds, rfds;
    int r;

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    while (1)
    {
        FD_COPY(&fds, &rfds);
        if (select(nfds, &rfds, 0, 0, 0) < 0)
            error("Fatal error", 1);
        if (FD_ISSET(sockfd, &rfds))
        {
            t_client* new_client = create_client();
            printf("new_client id = %d\n", new_client->id);
            sprintf(buf, "server: client %d just arrived\n", new_client->id);
            broadcast_msg(&fds, buf, new_client);
            FD_SET(new_client->fd, &fds);
        }
        t_client* it = clients;
        while (it)
        {
            if (FD_ISSET(it->fd, &rfds))
            {
                if ((r = recv(it->fd, buf, sizeof(buf) - 1, 0)) <= 0)
                {
                    sprintf(buf, "server: client %d just left\n", it->id);
                    broadcast_msg(&fds, buf, it);
                    it = destroy_client(&fds, it);
                    continue;
                }
                else
                {
                    buf[r] = '\0';
                    it->dt = str_join(it->dt, buf);
                    while (extract_message(&(it->dt), &extr))
                    {
                        if (!(msg = (char*)malloc(sizeof(char) * (20 + strlen(extr)))))
                            error("Fatal error", 1);
                        sprintf(msg, "client %d: %s", it->id, extr);
                        broadcast_msg(&fds, msg, it);
                        free(msg);
                        free(extr);
                        extr = NULL;
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
        error("Wrong number of arguments", 0);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        error("Fatal error", 0);

	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 

	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        error("Fatal error", 1);

	if (listen(sockfd, 0) != 0)
		error("Fatal error", 1);

    run();

    return (0);
}