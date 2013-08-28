/*
 * ftp.cpp: main server and client socket handling
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <jjolano@me.com> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return John Olano
 * ----------------------------------------------------------------------------
 */

#include <map>
#include <vector>
#include <utility>
#include <string>
#include <sstream>
#include <algorithm>

#include <NoRSX.h>
#include <sys/thread.h>
#include <net/net.h>
#include <netinet/in.h>
#include <netdb.h>

#include "ftp.h"
#include "defs.h"

using namespace std;

typedef map<int,int> ftp_drefs;
typedef map<string,cmdhnd> ftp_chnds;
typedef map<int,void (*)(ftp_client* clnt)> ftp_dhnds;
typedef map<int,ftp_client> ftp_clnts;
typedef vector<pollfd> ftp_socks;

ftp_socks pfd;
ftp_chnds command;
ftp_drefs datarefs;
ftp_dhnds datafunc;
ftp_clnts client;

extern int running;

void event_client_drop(ftp_client* clnt);
void register_cmds();

void closesocket(int socket)
{
	if(socket != -1)
	{
		netClose((socket)&~SOCKET_FD_MASK);
	}
}

// Terminates FTP server and all connections
void ftpTerminate()
{
	for(ftp_clnts::iterator cit = client.begin(); cit != client.end(); cit++)
	{
		ftp_client* clnt = &(cit->second);
		clnt->control_sendCode(421, "Server is shutting down");
		event_client_drop(clnt);
	}

	for(ftp_socks::iterator it = pfd.begin(); it != pfd.end(); it++)
	{
		closesocket(it->fd);
	}
}

// Registers an FTP command to a function
void register_cmd(string cmd, cmdhnd handler)
{
	command.insert(make_pair(cmd, handler));
}

void ftp_client::control_sendCustom(string message)
{
	// append proper line ending
	message += '\r';
	message += '\n';

	send(sock_control, message.c_str(), message.size(), 0);
}

void ftp_client::control_sendCode(unsigned int code, string message, bool multi)
{
	// format string
	ostringstream out;
	out << code << (multi ? '-' : ' ');

	// send to control socket
	control_sendCustom(out.str() + message);
}

void ftp_client::control_sendCode(unsigned int code, string message)
{
	control_sendCode(code, message, false);
}

bool ftp_client::data_open(void (*handler)(ftp_client* clnt), short events)
{
	// try to get data connection
	if(sock_data == -1)
	{
		if(sock_pasv == -1)
		{
			// legacy PORT 20 connection
			sockaddr_in sa;
			socklen_t len = sizeof(sa);
			
			getpeername(sock_control, (sockaddr*)&sa, &len);
			sa.sin_port = htons(20);

			sock_data = socket(PF_INET, SOCK_STREAM, 0);

			if(connect(sock_data, (sockaddr*)&sa, len) == -1)
			{
				// connect failed
				closesocket(sock_data);
				sock_data = -1;
				return false;
			}
		}
		else
		{
			// passive mode accept connection
			sock_data = accept(sock_pasv, NULL, NULL);

			closesocket(sock_pasv);
			sock_pasv = -1;

			if(sock_data == -1)
			{
				// accept failed
				return false;
			}
		}
	}

	// register pollfd for data connection
	pollfd data_pfd;
	data_pfd.fd = sock_data;
	data_pfd.events = events;
	pfd.push_back(data_pfd);

	// reference
	datarefs[sock_data] = sock_control;
	datafunc[sock_data] = handler;
	return true;
}

void ftp_client::data_close()
{
	if(sock_data != -1)
	{
		// remove from pollfd
		nfds_t nfds = pfd.size();
		u16 i;
		
		for(i = 0; i < nfds; i++)
		{
			if(pfd[i].fd == sock_data)
			{
				pfd.erase(pfd.begin() + i);
				break;
			}
		}

		// remove references
		datafunc.erase(sock_data);
		datarefs.erase(sock_data);

		// close socket
		closesocket(sock_data);

		sock_data = -1;
	}

	if(sock_pasv != -1)
	{
		// close listener
		closesocket(sock_pasv);
		sock_pasv = -1;
	}
}

void ftpInitialize(void* arg)
{	
	// Create server socket
	int sock_listen = socket(PF_INET, SOCK_STREAM, 0);
	
	sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(21);
	myaddr.sin_addr.s_addr = INADDR_ANY;

	if(bind(sock_listen, (struct sockaddr*)&myaddr, sizeof myaddr) == -1)
	{
		running = 0;
		closesocket(sock_listen);
		sysThreadExit(0x1337BEEF);
	}

	listen(sock_listen, LISTEN_BACKLOG);

	// Register FTP command handlers
	register_cmds();

	// Allocate memory for commands
	char* data = new char[CMDBUFFER];

	if(data == NULL)
	{
		// how did this happen?
		running = 0;
		closesocket(sock_listen);
		sysThreadExit(0x1337CAFE);
	}

	memset(data, 0, CMDBUFFER);

	// Add server to poll
	pollfd listen_pfd;
	listen_pfd.fd = sock_listen;
	listen_pfd.events = POLLIN;
	pfd.push_back(listen_pfd);

	// Main thread loop
	while(running)
	{
		int p = poll(&pfd[0], pfd.size(), 500);

		if(p == -1)
		{
			running = 0;
			delete [] data;
			data = NULL;
			ftpTerminate();
			sysThreadExit(0x1337DEAD);
		}

		// Loop if poll > 0
		u16 i;
		for(i = 0; (p > 0 && i < pfd.size()); i++)
		{
			if(pfd[i].revents == 0)
			{
				continue;
			}

			p--;

			int sock_fd = pfd[i].fd;

			// Listener socket event
			if(sock_fd == sock_listen)
			{
				if(pfd[i].revents & POLLIN)
				{
					// accept new connection
					int nfd = accept(sock_listen, NULL, NULL);

					if(nfd == -1)
					{
						continue;
					}

					// add to pollfds
					pollfd new_pfd;
					new_pfd.fd = nfd;
					new_pfd.events = DATA_EVENT_RECV;
					pfd.push_back(new_pfd);

					// add to clients
					client[nfd].sock_control = nfd;
					client[nfd].sock_data = -1;
					client[nfd].sock_pasv = -1;

					// welcome
					client[nfd].control_sendCode(220, APP_NAME " version " APP_VERSION " by " APP_AUTHOR, true);
					client[nfd].control_sendCode(220, "Client ID: " + i);
				}
				else
				{
					// server fail
					running = 0;
					delete [] data;
					data = NULL;
					ftpTerminate();
					sysThreadExit(0x1337DEAD);
				}

				continue;
			}

			// get client info
			ftp_drefs::iterator it;
			ftp_client* clnt;

			it = datarefs.find(sock_fd);

			if(it != datarefs.end())
			{
				clnt = &(client[it->second]);
			}
			else
			{
				clnt = &(client[sock_fd]);
			}

			// Disconnect event
			if((pfd[i].revents & (POLLNVAL|POLLHUP|POLLERR)))
			{
				if(it == datarefs.end())
				{
					// client dropped
					event_client_drop(clnt);
					closesocket(sock_fd);
					
					client.erase(sock_fd);
					pfd.erase(pfd.begin() + i);
				}
				else
				{
					// data connection
					datafunc.erase(clnt->sock_data);
					datarefs.erase(clnt->sock_data);
					closesocket(clnt->sock_data);
					clnt->sock_data = -1;

					pfd.erase(pfd.begin() + i);
				}
				
				continue;
			}

			// Sending data event
			if(pfd[i].revents & DATA_EVENT_SEND)
			{
				// call data handler
				(datafunc[sock_fd])(clnt);
				continue;
			}

			// Receiving data event
			if(pfd[i].revents & DATA_EVENT_RECV)
			{
				if(it == datarefs.end())
				{
					// Control socket
					int bytes = recv(sock_fd, data, CMDBUFFER - 1, 0);

					if(bytes <= 0)
					{
						// invalid, drop client
						event_client_drop(clnt);
						closesocket(sock_fd);
						
						client.erase(sock_fd);
						pfd.erase(pfd.begin() + i);
						continue;
					}

					if(bytes <= 2)
					{
						// invalid command, continue
						clnt->control_sendCode(500, "Invalid command", false);
						continue;
					}

					// handle command
					string cmdstr(data);

					string::size_type pos = cmdstr.find('\r', 0);

					if(pos == string::npos)
					{
						// invalid, continue
						clnt->control_sendCode(500, "Invalid command", false);
						continue;
					}

					cmdstr = cmdstr.substr(0, pos);

					pos = cmdstr.find(' ', 0);

					string cmd = cmdstr.substr(0, pos);
					string args;

					if(pos != string::npos)
					{
						args = cmdstr.substr(pos + 1);
					}

					transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

					// handle quit internally
					if(cmd == "QUIT")
					{
						clnt->control_sendCode(221, "Goodbye");
						
						event_client_drop(clnt);
						closesocket(sock_fd);

						client.erase(sock_fd);
						pfd.erase(pfd.begin() + i);
						continue;
					}

					// find command handler
					ftp_chnds::iterator cit = command.find(cmd);

					if(cit != command.end())
					{
						// call handler
						(cit->second)(clnt, cmd, args);
					}
					else
					{
						// command not found
						clnt->control_sendCode(502, cmd + " not implemented", false);
					}
				}
				else
				{
					// Data socket
					// call data handler
					(datafunc[sock_fd])(clnt);
				}

				continue;
			}
		}
	}

	delete [] data;
	data = NULL;
	ftpTerminate();
	sysThreadExit(0);
}
