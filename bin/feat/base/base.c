#include "base/base.h"

bool data_list(struct Client* client)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	int* fd = (int*) client_get_cvar(client, "fd");

	CellFsDirent dirent;
	uint64_t nread;

	if(cellFsReaddir(*fd, &dirent, &nread) != 0)
	{
		cellFsClosedir(*fd);
		*fd = -1;

		client_send_code(client, 452, FTP_452);
		return true;
	}

	if(nread == 0)
	{
		cellFsClosedir(*fd);
		*fd = -1;

		client_send_code(client, 226, FTP_226);
		return true;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, dirent.d_name);

	CellFsStat st;
	if(cellFsStat(path, &st) == 0)
	{
		char* buffer = client->server_ptr->buffer_data;

		char mode[11];
		get_file_mode(mode, &st);

		char tstr[16];
		strftime(tstr, 15, "%b %e %H:%M", localtime(&st.st_mtime));

		ssize_t len = sprintf(buffer,
			"%s %3d %-10d %-10d %10" PRIu64 " %s %s\r\n",
			mode, 1, st.st_uid, st.st_gid, (uint64_t) st.st_size, tstr, dirent.d_name
		);

		ssize_t nwrite = send(client->socket_data, buffer, (size_t) len, 0);

		if(nwrite == -1 || nwrite < len)
		{
			cellFsClosedir(*fd);
			*fd = -1;

			client_send_code(client, 451, FTP_451);
			return true;
		}
	}

	return false;
}

bool data_nlst(struct Client* client)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	int* fd = (int*) client_get_cvar(client, "fd");

	CellFsDirent dirent;
	uint64_t nread;

	if(cellFsReaddir(*fd, &dirent, &nread) != 0)
	{
		cellFsClosedir(*fd);
		*fd = -1;

		client_send_code(client, 452, FTP_452);
		return true;
	}

	if(nread == 0)
	{
		cellFsClosedir(*fd);
		*fd = -1;

		client_send_code(client, 226, FTP_226);
		return true;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, dirent.d_name);

	CellFsStat st;
	if(cellFsStat(path, &st) == 0)
	{
		char* buffer = client->server_ptr->buffer_data;

		ssize_t len = sprintf(buffer, "%s\r\n", dirent.d_name);
		ssize_t nwrite = send(client->socket_data, buffer, (size_t) len, 0);

		if(nwrite == -1 || nwrite < len)
		{
			cellFsClosedir(*fd);
			*fd = -1;

			client_send_code(client, 451, FTP_451);
			return true;
		}
	}

	return false;
}

bool data_retr(struct Client* client)
{
	char* buffer = client->server_ptr->buffer_data;
	int* fd = (int*) client_get_cvar(client, "fd");

	uint64_t nread;

	if(cellFsRead(*fd, buffer, BUFFER_DATA, &nread) != 0)
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 452, FTP_452);
		return true;
	}

	if(nread == 0)
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 226, FTP_226);
		return true;
	}

	ssize_t nwrite = send(client->socket_data, buffer, (size_t) nread, 0);

	if(nwrite == -1 || (uint64_t) nwrite < nread)
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 451, FTP_451);
		return true;
	}

	return false;
}

bool data_stor(struct Client* client)
{
	char* buffer = client->server_ptr->buffer_data;
	int* fd = (int*) client_get_cvar(client, "fd");

	ssize_t nread = recv(client->socket_data, buffer, BUFFER_DATA, 0);

	if(nread == 0)
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 226, FTP_226);
		return true;
	}

	if(nread == -1)
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 451, FTP_451);
		return true;
	}

	uint64_t nwrite;

	if(cellFsWrite(*fd, buffer, (uint64_t) nread, &nwrite) != 0
	|| nwrite < (uint64_t) nread)
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 452, FTP_452);
		return true;
	}

	return false;
}

void cmd_abor(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	client_data_end(client);
	client_send_code(client, 226, FTP_226A);
}

void cmd_acct(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(*auth)
	{
		client_send_code(client, 202, FTP_202);
		return;
	}

	*auth = true;
	client_send_code(client, 230, FTP_230A);
}

void cmd_allo(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	client_send_code(client, 202, FTP_200);
}

void cmd_cdup(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(cwd->num_levels > 0)
	{
		cwd->dir = (struct Directory*) realloc(cwd->dir, --cwd->num_levels * sizeof(struct Directory));
	}

	client_send_code(client, 250, FTP_250);
}

void cmd_cwd(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	CellFsStat st;

	if(cellFsStat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
	{
		set_working_directory(cwd, path);
		client_send_code(client, 250, FTP_250);
	}
	else
	{
		client_send_code(client, 550, FTP_550);
	}
}

void cmd_dele(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	if(cellFsUnlink(path) == 0)
	{
		client_send_code(client, 250, FTP_250);
	}
	else
	{
		client_send_code(client, 550, FTP_550);
	}
}

void cmd_help(struct Client* client, const char command_name[32], const char* command_params)
{
	client_send_code(client, 214, WELCOME_MSG);
}

void cmd_list(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");
	int* fd = (int*) client_get_cvar(client, "fd");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(*fd != -1)
	{
		client_send_code(client, 450, FTP_450);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	if(cellFsOpendir(cwd_str, fd) != 0)
	{
		client_send_code(client, 550, FTP_550);
		return;
	}

	if(client_data_start(client, data_list, POLLOUT|POLLWRNORM))
	{
		client_send_code(client, 150, FTP_150);
	}
	else
	{
		cellFsClosedir(*fd);
		*fd = -1;

		client_send_code(client, 425, FTP_425);
	}
}

void cmd_mkd(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	if(cellFsMkdir(path, 0777) == 0)
	{
		char* buffer = client->server_ptr->buffer_data;

		sprintf(buffer, FTP_257, path);
		client_send_code(client, 257, buffer);
	}
	else
	{
		client_send_code(client, 550, FTP_550);
	}
}

void cmd_mode(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(strcmp(command_params, "s") == 0
	|| strcmp(command_params, "S") == 0)
	{
		client_send_code(client, 200, FTP_200);
	}
	else
	{
		client_send_code(client, 504, FTP_504);
	}
}

void cmd_nlst(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");
	int* fd = (int*) client_get_cvar(client, "fd");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(*fd != -1)
	{
		client_send_code(client, 450, FTP_450);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	if(cellFsOpendir(cwd_str, fd) != 0)
	{
		client_send_code(client, 550, FTP_550);
		return;
	}

	if(client_data_start(client, data_nlst, POLLOUT|POLLWRNORM))
	{
		client_send_code(client, 150, FTP_150);
	}
	else
	{
		cellFsClosedir(*fd);
		*fd = -1;

		client_send_code(client, 425, FTP_425);
	}
}

void cmd_noop(struct Client* client, const char command_name[32], const char* command_params)
{
	client_send_code(client, 200, FTP_200);
}

void cmd_pass(struct Client* client, const char command_name[32], const char* command_params)
{
	char* user = (char*) client_get_cvar(client, "user");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(*auth)
	{
		client_send_code(client, 202, FTP_202);
		return;
	}

	if(strcmp(client->lastcmd, "USER") != 0)
	{
		client_send_code(client, 503, FTP_503);
		return;
	}

	*auth = true;

	char* buffer = client->server_ptr->buffer_data;
	sprintf(buffer, FTP_230, user);

	client_send_code(client, 230, buffer);
}

void cmd_pasv(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	struct sockaddr_in pasv_addr;

	if(client_pasv_enter(client, &pasv_addr))
	{
		char* buffer = client->server_ptr->buffer_data;

		sprintf(buffer, FTP_227,
			((htonl(pasv_addr.sin_addr.s_addr) & 0xff000000) >> 24),
			((htonl(pasv_addr.sin_addr.s_addr) & 0x00ff0000) >> 16),
			((htonl(pasv_addr.sin_addr.s_addr) & 0x0000ff00) >>  8),
			(htonl(pasv_addr.sin_addr.s_addr) & 0x000000ff),
			((htons(pasv_addr.sin_port) & 0xff00) >> 8),
			(htons(pasv_addr.sin_port) & 0x00ff)
		);

		client_send_code(client, 227, buffer);
	}
	else
	{
		client_send_code(client, 425, FTP_425);
	}
}

void cmd_port(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	unsigned short port_tuple[6];
	int port_argc = parse_port_tuple(port_tuple, command_params);

	if(port_argc != 6)
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	if(client->socket_data != -1)
	{
		client_send_code(client, 450, FTP_450);
		return;
	}

	struct sockaddr_in* port_addr;
	void* cvar_port_addr_ptr = client_get_cvar(client, "port_addr");

	if(cvar_port_addr_ptr != NULL)
	{
		port_addr = (struct sockaddr_in*) cvar_port_addr_ptr;
	}
	else
	{
		port_addr = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in*));
		client_set_cvar(client, "port_addr", (void*) port_addr);
	}

	port_addr->sin_family = AF_INET;
	port_addr->sin_port = htons(port_tuple[4] << 8 | port_tuple[5]);
	port_addr->sin_addr.s_addr = htonl(
		((unsigned char)(port_tuple[0]) << 24) +
		((unsigned char)(port_tuple[1]) << 16) +
		((unsigned char)(port_tuple[2]) << 8) +
		((unsigned char)(port_tuple[3]))
	);

	client_send_code(client, 200, FTP_200);
}

void cmd_pwd(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char* buffer = client->server_ptr->buffer_data;
	sprintf(buffer, FTP_257, cwd_str);

	client_send_code(client, 257, buffer);
}

void cmd_quit(struct Client* client, const char command_name[32], const char* command_params)
{
	client_send_code(client, 221, FTP_221);
	client_socket_disconnect(client, client->socket_control);
}

void cmd_rest(struct Client* client, const char command_name[32], const char* command_params)
{
	uint64_t* rest = (uint64_t*) client_get_cvar(client, "rest");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	int64_t rest_param = 0;

	if(command_params[0] != '\0')
	{
		rest_param = atoll(command_params);
	}

	if(rest_param >= 0)
	{
		*rest = rest_param;

		char* buffer = client->server_ptr->buffer_data;
		sprintf(buffer, FTP_350, *rest);

		client_send_code(client, 350, buffer);
	}
	else
	{
		client_send_code(client, 554, FTP_554);
	}
}

void cmd_retr(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");
	int* fd = (int*) client_get_cvar(client, "fd");
	uint64_t* rest = (uint64_t*) client_get_cvar(client, "rest");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(*fd != -1)
	{
		client_send_code(client, 450, FTP_450);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	if(!file_exists(path))
	{
		client_send_code(client, 550, FTP_550);
		return;
	}

	if(cellFsOpen(path, CELL_FS_O_RDONLY, fd, NULL, 0) != 0)
	{
		client_send_code(client, 550, FTP_550);
		return;
	}

	uint64_t pos;
	cellFsLseek(*fd, *rest, SEEK_SET, &pos);
	*rest = 0;

	if(client_data_start(client, data_retr, POLLOUT|POLLWRNORM))
	{
		client_send_code(client, 150, FTP_150);
	}
	else
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 425, FTP_425);
	}
}

void cmd_rmd(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	if(cellFsRmdir(path) == 0)
	{
		client_send_code(client, 250, FTP_250);
	}
	else
	{
		client_send_code(client, 550, FTP_550);
	}
}

void cmd_rnfr(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");
	char* rnfr = (char*) client_get_cvar(client, "rnfr");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	if(file_exists(path))
	{
		strcpy(rnfr, path);

		client_send_code(client, 350, FTP_350A);
	}
	else
	{
		client_send_code(client, 550, FTP_550);
	}
}

void cmd_rnto(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");
	char* rnfr = (char*) client_get_cvar(client, "rnfr");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	if(strcmp(client->lastcmd, "RNFR") != 0)
	{
		client_send_code(client, 503, FTP_503);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	if(cellFsRename(rnfr, path) == 0)
	{
		client_send_code(client, 250, FTP_250);
	}
	else
	{
		client_send_code(client, 550, FTP_550);
	}
}

void cmd_site(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	struct Command* site_command = (struct Command*) malloc(sizeof(struct Command));

	char sitecmd_name[32];
	char* sitecmd_params = client->server_ptr->buffer_control;

	parse_command_string(sitecmd_name, sitecmd_params, command_params);

	if(!command_call(site_command, sitecmd_name, sitecmd_params, client))
	{
		client_send_code(client, 502, FTP_502);
	}

	free(site_command);
}

void cmd_stat(struct Client* client, const char command_name[32], const char* command_params)
{
	char* buffer = client->server_ptr->buffer_data;

	char* user = (char*) client_get_cvar(client, "user");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	client_send_multicode(client, 211, WELCOME_MSG);

	sprintf(buffer, "Username: %s", user);
	client_send_multimessage(client, buffer);

	sprintf(buffer, "Authenticated: %d", auth);
	client_send_multimessage(client, buffer);

	sprintf(buffer, "Total connections: %d", client->server_ptr->num_clients);
	client_send_multimessage(client, buffer);

	client_send_code(client, 211, "End.");
}

void cmd_stor(struct Client* client, const char command_name[32], const char* command_params)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	bool* auth = (bool*) client_get_cvar(client, "auth");
	int* fd = (int*) client_get_cvar(client, "fd");
	uint64_t* rest = (uint64_t*) client_get_cvar(client, "rest");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(*fd != -1)
	{
		client_send_code(client, 450, FTP_450);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	char cwd_str[MAX_PATH];
	get_working_directory(cwd_str, cwd);

	char path[MAX_PATH];
	get_absolute_path(path, cwd_str, command_params);

	uint32_t oflags = CELL_FS_O_WRONLY;

	if(!file_exists(path))
	{
		oflags |= CELL_FS_O_CREAT;
	}

	if(strcmp(command_name, "APPE") == 0)
	{
		oflags |= CELL_FS_O_APPEND;
	}
	else
	{
		if(*rest == 0)
		{
			oflags |= CELL_FS_O_TRUNC;
		}
	}

	if(cellFsOpen(path, oflags, fd, NULL, 0) != 0)
	{
		client_send_code(client, 550, FTP_550);
		return;
	}

	if(oflags & CELL_FS_O_CREAT)
	{
		cellFsChmod(path, 0777);
	}

	uint64_t pos;
	cellFsLseek(*fd, *rest, SEEK_SET, &pos);
	*rest = 0;

	if(client_data_start(client, data_stor, POLLIN|POLLRDNORM))
	{
		client_send_code(client, 150, FTP_150);
	}
	else
	{
		cellFsClose(*fd);
		*fd = -1;

		client_send_code(client, 425, FTP_425);
	}
}

void cmd_stru(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	if(strcmp(command_params, "f") == 0
	|| strcmp(command_params, "F") == 0)
	{
		client_send_code(client, 200, FTP_200);
	}
	else
	{
		client_send_code(client, 504, FTP_504);
	}
}

void cmd_syst(struct Client* client, const char command_name[32], const char* command_params)
{
	client_send_code(client, 215, FTP_215);
}

void cmd_type(struct Client* client, const char command_name[32], const char* command_params)
{
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(!*auth)
	{
		client_send_code(client, 530, FTP_530);
		return;
	}

	client_send_code(client, 200, FTP_200);
}

void cmd_user(struct Client* client, const char command_name[32], const char* command_params)
{
	char* user = (char*) client_get_cvar(client, "user");
	bool* auth = (bool*) client_get_cvar(client, "auth");

	if(*auth)
	{
		client_send_code(client, 202, FTP_202);
		return;
	}

	if(command_params[0] == '\0')
	{
		client_send_code(client, 501, FTP_501);
		return;
	}

	strcpy(user, command_params);

	char* buffer = client->server_ptr->buffer_data;
	sprintf(buffer, FTP_331, user);

	client_send_code(client, 331, buffer);
}

void base_connect(struct Client* client)
{
	// allocate cvars
	struct Path* cwd = (struct Path*) malloc(sizeof(struct Path));
	char* user = (char*) malloc(MAX_USERNAME_LEN * sizeof(char));
	char* rnfr = (char*) malloc(MAX_PATH * sizeof(char));
	bool* auth = (bool*) malloc(sizeof(bool));
	int* fd = (int*) malloc(sizeof(int));
	uint64_t* rest = (uint64_t*) malloc(sizeof(uint64_t));

	// initialize values
	cwd->dir = NULL;
	cwd->num_levels = 0;

	*auth = false;
	*fd = -1;
	*rest = 0;

	client_set_cvar(client, "cwd", (void*) cwd);
	client_set_cvar(client, "user", (void*) user);
	client_set_cvar(client, "rnfr", (void*) rnfr);
	client_set_cvar(client, "auth", (void*) auth);
	client_set_cvar(client, "fd", (void*) fd);
	client_set_cvar(client, "rest", (void*) rest);
}

void base_disconnect(struct Client* client)
{
	struct Path* cwd = (struct Path*) client_get_cvar(client, "cwd");
	char* user = (char*) client_get_cvar(client, "user");
	char* rnfr = (char*) client_get_cvar(client, "rnfr");
	bool* auth = (bool*) client_get_cvar(client, "auth");
	int* fd = (int*) client_get_cvar(client, "fd");
	uint64_t* rest = (uint64_t*) client_get_cvar(client, "rest");

	void* cvar_port_addr_ptr = client_get_cvar(client, "port_addr");

	if(cvar_port_addr_ptr != NULL)
	{
		struct sockaddr_in* port_addr = (struct sockaddr_in*) cvar_port_addr_ptr;
		free(port_addr);
	}

	free(cwd);
	free(user);
	free(rnfr);
	free(auth);
	free(fd);
	free(rest);
}

void base_command_import(struct Command* command)
{
	command_register_connect(command, base_connect);
	command_register_disconnect(command, base_disconnect);

	command_register(command, "ABOR", cmd_abor);
	command_register(command, "ACCT", cmd_acct);
	command_register(command, "ALLO", cmd_allo);
	command_register(command, "APPE", cmd_stor);
	command_register(command, "CDUP", cmd_cdup);
	command_register(command, "CWD", cmd_cwd);
	command_register(command, "DELE", cmd_dele);
	command_register(command, "HELP", cmd_help);
	command_register(command, "LIST", cmd_list);
	command_register(command, "MKD", cmd_mkd);
	command_register(command, "MODE", cmd_mode);
	command_register(command, "NLST", cmd_nlst);
	command_register(command, "NOOP", cmd_noop);
	command_register(command, "PASS", cmd_pass);
	command_register(command, "PASV", cmd_pasv);
	command_register(command, "PORT", cmd_port);
	command_register(command, "PWD", cmd_pwd);
	command_register(command, "QUIT", cmd_quit);
	command_register(command, "REST", cmd_rest);
	command_register(command, "RETR", cmd_retr);
	command_register(command, "RMD", cmd_rmd);
	command_register(command, "RNFR", cmd_rnfr);
	command_register(command, "RNTO", cmd_rnto);
	command_register(command, "SITE", cmd_site);
	command_register(command, "STAT", cmd_stat);
	command_register(command, "STOR", cmd_stor);
	command_register(command, "STRU", cmd_stru);
	command_register(command, "SYST", cmd_syst);
	command_register(command, "TYPE", cmd_type);
	command_register(command, "USER", cmd_user);
	
	command_register(command, "XCUP", cmd_cdup);
	command_register(command, "XCWD", cmd_cwd);
	command_register(command, "XMKD", cmd_mkd);
	command_register(command, "XPWD", cmd_pwd);
	command_register(command, "XRMD", cmd_rmd);
}
