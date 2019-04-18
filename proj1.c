#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>


void my_prompt();
char * my_read();
char ** my_parse(char * line);
char * parse_whitespace(char * line);
char ** parse_arguments(char * line);
char ** expand_variables(char ** args);
char ** resolve_paths(char ** args);
int is_command(char ** args, int i);
char * expand_path(char * path, int cmd_p);
char * to_parent_dir(char * parent, int iter);
void my_execute(char ** cmd);
void my_pipe(int * dim1, int * dim2, char ** cmd1, char ** cmd2);
void myPipe(int * dim1, int * dim2,
			char * cmd1[dim1[1]], char * cmd2[dim2[1]]);
char * expand_pipe_path(char * path);


void my_clean(char ** arr);


int main()
{
	// REPL: Read-Eval-Print Loop
	while (1)
	{
		// Print prompt
		my_prompt();

		// Read input
		char * line = my_read();
	
		if (!line)
		{
			printf("Exiting shell...\n");
			exit(1);
		}
		
		if (line[0] != '\n')
		{	
			// Transform input
			char **	cmd = my_parse(line);

			if (cmd && strcmp(cmd[0], "echo") != 0 &&
				strcmp(cmd[0], "cd") != 0 &&
				strcmp(cmd[0], "etime") != 0 &&
				strcmp(cmd[0], "io") != 0)
			{
				// Execute command
				my_execute(cmd);
			}			

			my_clean(cmd);
		}
	}

	return 0;
}


void my_prompt()
{
	char * user = getenv("USER");
	char * machine = getenv("MACHINE");
	char * pwd = getenv("PWD");
	printf("%s@%s::%s=> ", user, machine, pwd);
}

char * my_read()
{
	static char line[256];

	return fgets(line, sizeof(line), stdin);
}

char ** my_parse(char * line)
{
	char ** args;

	line = parse_whitespace(line);
	args = parse_arguments(line);
	args = expand_variables(args);
	args = resolve_paths(args);

	return args;
}

char * parse_whitespace(char * line)
{
	int i, j, k;

	if (line[0] == ' ')
	{
		for (i = 0; line[i] == ' '; i++){}
		
		for (j = i; line[j] != '\0'; j++){}

		for (k = 0; k < j; k++)
		{
			line[k] = line[k + i];
			line[k + i] = ' ';
		}
	}

	for (i = 1; line[i] != '\0'; i++)
	{
		if (line[i] == '\n')
			line[i] = '\0';

		if ((line[i] == ')' || line[i] == '|' ||
			 line[i] == '<' || line[i] == '>' ||
			 line[i] == '&' || line[i] == '$') &&
			(line[i - 1] != ' ' || line[i + 1] != ' '))
		{
			if (line[i - 1] != ' ')
			{
				for (j = i; line[j] != '\0'; j++){}

				for (k = j + 1; k > i; k--)
					line[k] = line[k - 1];

				line[k] = ' ';
			}

			if (line[i + 1] != ' ')
			{
				for (j = i; line[j] != '\0'; j++){}

				for (k = j + 1; k > i + 1; k--)
					line[k] = line[k - 1];

				line[k] = ' ';
			}
		}

		for (k = 0; line[k] != '\0'; k++){}

		if (!isprint(line[k - 1]))
		{
			for (--k; (!isprint(line[k])) || line[k] == ' '; k--)
				line[k] = '\0';
		}


		if ((line[i] == ' ') && (line[i - 1] != ' ') &&
			(line[i + 1] == '\0' || line[i + 1] == ' ')) 
		{
			for (j = 1; line[i + j] == ' '; j++){}

			if (line[i + j] == '\0')
			{
				for (k = j; line[k - 1] == ' '; k--)
					line[k] = '\0';

				break;
			}

			for (k = i + j; line[k] != ' ' && line[k] != '\0'; k++){}
			
			int l;
			for (l = i + 1; l < k; l++)
			{
				line[l] = line[l + j - 1];
				line[l + j - 1] = ' ';	
			}
		}	
	}

	return line;
}

char ** parse_arguments(char * line)
{
	int i, argCount = 1;
	for (i = 0; line[i] != '\0'; i++)
	{
		if (line[i] == ' ')
			argCount++;
	}

	char ** args = calloc(argCount + 1, sizeof(char*));
	
	for (i = 0; i < argCount; i++)
		args[i] = calloc(151, sizeof(char));

	int j = 0;
	int k = 0;
	for (i = 0; line[i] != '\0'; i++)
	{
		if (line[i] != ' ')
		{
			args[j][k] = line[i];
			k++;
		}
		else
		{
			args[j][k] = '\0';
			j++;
			k = 0;
		}
	}

	return args;
}

char ** expand_variables(char ** args)
{
	int i, j, k;

	char * env;

	for (i = 0; args[i] != NULL; i++)
	{
		if (args[i][0] == '$')	
		{
			for (j = 0; args[i + 1][j] != '\0'; j++){}			

			env = calloc(j + 1, sizeof(char));

			env[0] = '$';

			for (k = 0; k < j; k++)
				env[k] = args[i + 1][k];
		
			if (getenv(env))
			{
				args[i] = getenv(env);
			
				for (j = i + 1; args[j] != NULL; j++)
					args[j] = args[j + 1];

				free(env);
			}
			else
			{
				printf("\n\n");
				
				free(env);
				my_clean(args);
				return NULL;
			}
		}
	}

	return args;
}

char ** resolve_paths(char ** args)
{
	int i, j;

	for (i = 0; args[i] != NULL; i++)
		args[i] = expand_path(args[i], is_command(args, i));

	return args;
}

int is_command(char ** args, int i)
{
	// returns 0 for argument, 1 for external command, 2 for cd,
	// 3 for other built-in commands, 4 if there is an error

	if (strcmp(args[i], "exit") == 0)
	{
		printf("Exiting shell...\n");
		exit(1);
	}
	else if (strcmp(args[i], "echo") == 0)
	{
		int j;
		for (j = 1; args[i + j] != NULL; j++)
		{
			if (args[i + j][0] == '$')
			{
				printf("%s ", getenv(args[i + j]));
			}
			else
			{
				printf("%s ", args[i + j]);
			}
		}
		
		printf("\n");	

		return 3;
	}
	else if (strcmp(args[i], "etime") == 0) // etime command
	{
			
		struct timeval time1, time2;
		int k;
		for (k = 0; args[k] != NULL; k++)
			args[k] = args[k + 1];

		gettimeofday(&time1, NULL);
	
		if (args[0] != NULL && strcmp(args[0], "etime") != 0) 
			my_execute(args);

		gettimeofday(&time2, NULL);

		if (i == 0)
		{
			if (time2.tv_usec < time1.tv_usec)
			{
				time2.tv_usec += 1000000;
				time2.tv_sec--;
			}

			printf("Elapsed Time: %fs\n",
			((float)(time2.tv_usec - time1.tv_usec) / 
			1000000 + (float)(time2.tv_sec - time1.tv_sec)));
		}

		int j;
		for (j = 1; args[j] != NULL; j++)
			args[j] = args[j - 1];
		
		strcpy(args[0], "etime");

		return 3;
	}
	else if (strcmp(args[i], "io") == 0) // io command
	{
		my_execute(args);

		int j;
		for (j = 1; args[j] != NULL; j++)
			args[j] = args[j - 1];		

		args[0] = "io";

		return 3;
	}
	else if (strcmp(args[i], "cd") == 0)
	{
		if (!args[i + 1] || (args[i + 1][0] == '~'))
		{	
			if (args[i + 1] && args[i + 1][0] == '~')
				strcpy(args[i + 1], getenv("HOME"));
	
			setenv("PWD", getenv("HOME"), 1);
			return 2;
		}
		else if (args[i + 2] != NULL)
		{
			printf("Error: Invalid syntax\n");
			return 4;
		}
	}
	else if (i != 0 && strcmp(args[i - 1], "cd") == 0)
	{
		return 2;
	}
	else
	{
		// Determines whether args[i] is a command
		DIR * dir;
		struct dirent * d_stream;
		dir = opendir("/bin");

		while ((d_stream = readdir(dir)) != NULL)
		{
			if (strstr(args[i], d_stream->d_name) != NULL && i != 0)
			{
				closedir(dir);
				return 1;
			}
		}
		
		closedir(dir);
	}

	return 0;
}

char * expand_path(char * path, int cmd_p)
{
	if (!cmd_p || cmd_p == 5)
	{
		if (strncmp(path, "..", 2) == 0)
		{
			int i, j, count = 2;

			for (i = 2; path[i] != '\0'; i++)
			{
				if (path[i] == '.')
					count++;
			}
		
			if (strcmp(getenv("PWD"), "/") == 0)
				path = getenv("PWD");
			else
				path = to_parent_dir(path, count / 2);
		}
		else if (path[0] == '.' ||
			 	(path[0] == '.' && path[1] == '/'))
		{
			if (path[2] != '\0')
			{
				char path2[strlen(path)];
				const char * pwd = getenv("PWD");
				char PWD[strlen(pwd)];

				
				int i;
				for (i = 0; pwd[i] != '\0'; i++)
					PWD[i] = pwd[i];

				PWD[i] = '\0';

				for (i = 1; path[i] != '\0'; i++)
					path2[i - 1] = path[i];

				path2[i - 1] = '\0';

				path = strcat(PWD, path2);
			}
			else
			{
				path = getenv("PWD");
			}
		}
		else if (path[0] == '~')
		{
			if (path[2] != '\0')
			{
				char path2[strlen(path)];

				int i;
				for (i = 1; path[i] != '\0'; i++)
					path2[i - 1] = path[i];

				path2[i - 1] = '\0';

				path = strcat(getenv("HOME"), path2);
			}
			else
			{
				path = getenv("HOME");
			}
		}
	}

	if (cmd_p == 2 && strcmp(path, "cd") != 0)
	{
		if (strncmp(path, "..", 2) == 0)
		{
			int i, j, count = 2;

			for (i = 2; path[i] != '\0'; i++)
			{
				if (path[i] == '.')
					count++;
			}
	
			path = to_parent_dir(path, count / 2);
		}
		else if (path[0] == '.')
		{
			if (path[1] != '\0')
			{	
				char path2[strlen(path)];

				int i;
				for (i = 1; path[i] != '\0'; i++)
					path2[i - 1] = path[i];
	
				path2[i - 1] = '\0';

				if (chdir(expand_path(path2, 2)) != -1)	
					path = strcat(getenv("PWD"), path2);
			}
			else
			{
				path = getenv("PWD");
			}
		}
		else
		{
			if (path[0] != '/' && path[0] != '~')
			{
				char path2[strlen(path)];
				const char * pwd = getenv("PWD");
				char PWD[strlen(pwd)];

				int i;
				for (i = 0; pwd[i] != '\0'; i++)
					PWD[i] = pwd[i];

				PWD[i] = '\0';

				if (PWD[strlen(pwd) - 1] != '/')
				{
					path2[0] = '/';	
					
					for (i = 1; path[i - 1] != '\0'; i++)
						path2[i] = path[i - 1];
				}
				else
				{
					for (i = 0; path[i] != '\0'; i++)
						path2[i] = path[i];
				}

				path2[i] = '\0';

				if (chdir(strcat(PWD, path2)) != -1)
				{
					for (i = 0; PWD[i] != '\0'; i++)
						path[i] = PWD[i];

					path[i] = '\0';
				}
				else
				{
					printf("Error: Not a directory\n");
					
					for (i = 0; pwd[i] != '\0'; i++)
						path[i] = pwd[i];

					path[i] = '\0';
				}
			}
		}
	
		if (chdir(path) != -1)
			setenv("PWD", path, 1);
	}

	return path;
}

char * to_parent_dir(char * parent, int iter)
{
	char * pwd = getenv("PWD");
	char * ptr = strrchr(pwd, '/');

	if (parent[strlen(parent) + 1] = '/')
		parent[strlen(parent) + 1] = '\0';

	char * ptr_parent = strrchr(parent, '/');
	char path[strlen(parent)];

	int i, j;
	if (strlen(parent) > 2 &&
	  !((parent[ptr_parent - parent + 1] == '.') &&
		(parent[ptr_parent - parent + 2] == '.')))
	{
		for (i = strlen(parent); ; i--)
		{
			if ((parent[i] == '/') &&
				(parent[i - 1] == '.') &&
				(parent[i - 2] == '.'))
				break;
		}

		for (j = i; parent[j] != '\0'; j++)
			path[j - i] = parent[j];

		path[j - i] = '\0';
	}
	else
	{
		if (parent[strlen(parent)] != '/')
		{
			parent[strlen(parent)] = '/';
			parent[strlen(parent) + 1] = '\0';
		}
	
		path[0] = '\0';
	}

	strncpy(parent, pwd, ptr - pwd);

	for (i = 1; i < iter; i++)
	{
		strcpy(pwd, parent);
		ptr = strrchr(pwd, '/');
		strncpy(parent, pwd, ptr - pwd);
		parent[ptr - pwd] = '\0';
	}

	parent[ptr - pwd] = '\0';

	if (path[0] != '\0')
		strcat(parent, path);

	return parent;
}

void my_execute(char ** cmd)
{
	bool io = false;
	if (strcmp(cmd[0], "io") == 0)
	{
		int i;
		for (i = 0; cmd[i] != NULL; i++)
			cmd[i] = cmd[i + 1];

		cmd[i] = NULL;

		io = true;
	}

	char bin[strlen(cmd[0]) + 7];
	strcpy(bin, "/bin/");

	if (cmd[0][0] != '/')
	{
		strcat(bin, cmd[0]);
	}
	else
	{	
		int i;
		for (i = 0; cmd[0][i] != '\0'; i++)
			bin[i] = cmd[0][i];
	
		bin[i] = '\0';
	}


	int status;
		
	pid_t pid = fork();

	// check cmd for > (input redirection)
	int i, in = 0;
	for (i = 0; cmd[i] != NULL; i++)
	{
		if (strcmp(cmd[i], "<") == 0)
		{
			if (i == 0 || cmd[i + 1] == NULL)
			{
				printf("Error: Invalid syntax\n");
				return ;
			}
		
			in = 1;
			break;
		}
	}

	// check cmd for > (output redirection)
	int j, out = 0;
	for (j = 0; cmd[j] != NULL; j++)
	{
		if (strcmp(cmd[j], ">") == 0)
		{
			if (j == 0 || cmd[j + 1] == NULL)
			{
				printf("bash: syntax error near unexpected token `newline'\n");
				return ;
			}
		
			out = 1;
			break;
		}
	}

	int pipesFound = 0;
	int z;
	for (z = 0; cmd[z] != NULL; z++)
	{
		// loop through cmd and check for pipe
		if (strcmp(cmd[z], "|") == 0)
		{
			if (z == 0 || strcmp(cmd[z + 1], "|") == 0 ||
				strcmp(cmd[z + 1], "\0") == 0)
			{
			// checking for pipe as first arg,
			// pipe followed by another pipe, pipe followed by nothing
				printf("bash: syntax error near unexpected token `|'\n");
				return ;
			}
			else
				pipesFound++; // holds number of pipes found
		}
	}
	
	if (pid == -1)
	{
		printf("Error with execution. Exiting shell...\n");
		my_clean(cmd);
		exit(1);
	}
	else if (pid == 0)
	{
		// child process

		if (in)
		{
			// input redirection

			int fd = open(cmd[i + 1], O_RDONLY, 0777);
			close(STDIN_FILENO);
			dup2(fd, 0);
			close(fd);

			cmd[i] = NULL;
			cmd[i + 1] = NULL;
		}
		if (out)
		{
			// output redirection

			int fd = creat(cmd[j + 1], 0644);
			close(STDOUT_FILENO);
       		dup2(fd, 1);
			close(fd);
    		
		  	cmd[j] = NULL;
			cmd[j + 1] = NULL;
   		}
		
		
		if (pipesFound)
		{
			int x, y, pipeIndex, maxStrlen = 0;
			int cmd_dim[2], cmd2_dim[2];

			for (x = 0; strcmp(cmd[x], "|"); x++)
			{
				for (z = 0; cmd[x][z] != '\0'; z++){}

				if (z > maxStrlen)
					maxStrlen = z;
			}

			z = maxStrlen;
			cmd_dim[0] = x;
			cmd_dim[1] = z;
			pipeIndex = x;

	
			maxStrlen = 0;
			for (y = 0; cmd[x + y + 1] != NULL; y++)
			{
				for (z = 0; cmd[y][z] != '\0'; z++){}

				if (z > maxStrlen)
					maxStrlen = z;
			}
		
			z = maxStrlen;
			cmd2_dim[0] = y;
			cmd2_dim[1] = z;

			char ** cmd2 = (char **)calloc(cmd2_dim[0], sizeof(char *));
				
			for (x = 0; x < cmd2_dim[0]; x++)
				cmd2[x] = (char *)calloc(cmd2_dim[1], sizeof(char));

			for (x = 0; cmd[pipeIndex + 1 + x] != NULL; x++)
				strcpy(cmd2[x], cmd[pipeIndex + 1 + x]);

			for (x = pipeIndex; cmd[x] != NULL; x++)
				cmd[x] = NULL;
	
			my_pipe(cmd_dim, cmd2_dim, cmd, cmd2);
			my_clean(cmd2);
		}

		usleep(1);	

		execv(bin, cmd);
		printf("bash: %s: Command not found.\n", bin);
		my_clean(cmd);
		exit(1);
	}
	else
	{
		// parent

		if (io)
		{
			int value;
			char _pid[16];

			sprintf(_pid, "/proc/%u/io", (long)pid);

			char ** cat = calloc(2, sizeof(char *)); 
		
			cat[0] = calloc(strlen(_pid), sizeof(char));
			cat[1] = calloc(strlen(_pid), sizeof(char));

			strcpy(cat[0],"cat");
			cat[0][3] = '\0';
			strcpy(cat[1],_pid);
			cat[1][strlen(_pid)] = '\0';

			my_execute(cat);
			my_clean(cat);
		}

		waitpid(pid, &status, 0);
	}
}

// Calls the recursive myPipe function if necessary, otherwise
// will execute the pipe processes passed in from my_execute
void my_pipe(int * dim1, int * dim2, char ** cmd1, char ** cmd2)
{
	int numPipes = 0;
	char ** cmd_1;
	char ** cmd_2;

	int i;
	for (i = 0; i < dim2[0]; i++)
	{
		if (strcmp(cmd2[i], "|") == 0)
			numPipes++;
	}

	if (numPipes != 0)
	{
		int pipeIndex, j, max = 0;
		for (pipeIndex = 0; strcmp(cmd2[pipeIndex], "|"); pipeIndex++){}

		for (i = pipeIndex; i < dim2[0]; i++)
		{
			for (j = 0; j < dim2[1]; j++){}
			
			if (j > max)
				max = j;
		}

		dim1[0] = pipeIndex;
		dim1[1] = max;

		cmd_1 = calloc(dim1[0], sizeof(char *));

		for (i = 0; i < dim1[0]; i++)
			cmd_1[i] = calloc(dim1[1], sizeof(char));

		for (j = 0; j < pipeIndex; j++)
			strcpy(cmd_1[j], cmd2[j]);

		cmd_1[j] = NULL;

		char * CMD_1[] = { cmd_1[0], cmd_1[1], cmd_1[2], cmd_1[3],
						   cmd_1[4], cmd_1[5], cmd_1[6], cmd_1[7],
						   cmd_1[8], cmd_1[9], cmd_1[10], cmd_1[11],
						   cmd_1[12], cmd_1[13], cmd_1[14], cmd_1[15],
						   cmd_1[16], cmd_1[17], cmd_1[18], cmd_1[19],
						   cmd_1[20], cmd_1[21], cmd_1[22], cmd_1[23],
						   cmd_1[24], cmd_1[25], cmd_1[26], cmd_1[27],
						   cmd_1[28], cmd_1[29], cmd_1[30], cmd_1[31],
						   cmd_1[32], cmd_1[33], cmd_1[34], cmd_1[35],
						   cmd_1[36], cmd_1[37], cmd_1[38], cmd_1[39],
						   cmd_1[40], cmd_1[41], cmd_1[42], cmd_1[43],
						   cmd_1[44], cmd_1[45], cmd_1[46], cmd_1[47],
						   cmd_1[48], cmd_1[49], cmd_1[50], cmd_1[51],
						   cmd_1[52], cmd_1[53], cmd_1[54], cmd_1[55],
						   cmd_1[56], cmd_1[57], cmd_1[58], cmd_1[59],
						   cmd_1[60], cmd_1[61], cmd_1[62], cmd_1[63], 0};
						   
		for (i = 0; strcmp(cmd2[i], "|"); i++){}

		max = 0;
		int argCount;
		for (argCount = 0; pipeIndex + 1 + argCount < dim2[0]; argCount++)
		{
			for (i = 0; i < dim2[1]; i++){}

			if (i > max)
				max = i;
		}

		dim2[0] = argCount;
		dim2[1] = max;

		cmd_2 = calloc(dim2[0], sizeof(char *));

		for (i = 0; i < dim2[0]; i++)
			cmd_2[i] = calloc(dim2[1], sizeof(char));
		
		for (i = 0; i < dim2[0]; i++)
			strcpy(cmd_2[i], cmd2[pipeIndex + 1 + i]);

		cmd_2[i] = NULL;

		char * CMD_2[] = { cmd_2[0], cmd_2[1], cmd_2[2], cmd_2[3],
						   cmd_2[4], cmd_2[5], cmd_2[6], cmd_2[7],
						   cmd_2[8], cmd_2[9], cmd_2[10], cmd_2[11],
						   cmd_2[12], cmd_2[13], cmd_2[14], cmd_2[15],
						   cmd_2[16], cmd_2[17], cmd_2[18], cmd_2[19],
						   cmd_2[20], cmd_2[21], cmd_2[22], cmd_2[23],
						   cmd_2[24], cmd_2[25], cmd_2[26], cmd_2[27],
						   cmd_2[28], cmd_2[29], cmd_2[30], cmd_2[31],
						   cmd_2[32], cmd_2[33], cmd_2[34], cmd_2[35],
						   cmd_2[36], cmd_2[37], cmd_2[38], cmd_2[39],
						   cmd_2[40], cmd_2[41], cmd_2[42], cmd_2[43],
						   cmd_2[44], cmd_2[45], cmd_2[46], cmd_2[47],
						   cmd_2[48], cmd_2[49], cmd_2[50], cmd_2[51],
						   cmd_2[52], cmd_2[53], cmd_2[54], cmd_2[55],
						   cmd_2[56], cmd_2[57], cmd_2[58], cmd_2[59],
						   cmd_2[60], cmd_2[61], cmd_2[62], cmd_2[63], 0};
	
		int fd[2];
		pipe(fd);
	
		if (!fork())
		{
			dup2(fd[0], 0); 

			close(fd[1]);

			myPipe(dim1, dim2, CMD_1, CMD_2);
		} 
		else
		{
			dup2(fd[1], 1);

			close(fd[0]);

			char firstArg[strlen(cmd1[0])];
			strcpy(firstArg, cmd1[0]);
	
			execv(expand_pipe_path(firstArg), cmd1);
			printf("bash: %s: command not found\n", firstArg);
			my_clean(cmd2);
			exit(1);
		}

		my_clean(cmd2);
	}
	else // If numPipes = 0, no need to recurse
	{
		int fd[2];
		pipe(fd);

		if (!fork())
		{
			dup2(fd[0], 0);

			close(fd[1]);

			char firstArg[strlen(cmd2[0])];
			strcpy(firstArg, cmd2[0]);

			execv(expand_pipe_path(firstArg), cmd2);
			printf("bash: %s: command not found\n", firstArg);
			my_clean(cmd2);
			exit(1);
		} 
		else
		{
			dup2(fd[1], 1); 

			close(fd[0]);

			char firstArg[strlen(cmd1[0])];
			strcpy(firstArg, cmd1[0]);
			execv(expand_pipe_path(firstArg), cmd1);
			printf("bash: %s: command not found\n", firstArg);
			my_clean(cmd2);
			exit(1);
		}

		my_clean(cmd2);
	}
}

// Recursively executes pipe processes
void myPipe(int * dim1, int * dim2,
			char * cmd1[dim1[1]], char * cmd2[dim2[1]])
{
	// RECURSIVE
	int numPipes = 0;

	int i;
	for (i = 0; i < dim2[0]; i++)
	{
		if (strcmp(cmd2[i], "|") == 0)
			numPipes++;   // holds number of pipes found
	}

	if (numPipes != 0)
	{
		int j, max = 0;
		for (i = 0; strcmp(cmd2[i], "|"); i++)
		{
			for (j = 0; cmd2[i][j] != '\0'; j++){}

			if (j > max)
				max = j;
		}

		j = max + 1;
		dim1[0] = i;
		dim1[1] = j;

		int pipeIndex = i;

		int k;
		for (j = 0; cmd2[j + i + 1] != NULL; j++)
		{
			for (k = 0; cmd2[j + i + 1][k] != '\0'; k++){}
			
			if (k > max)
				max = k;
		}

		k = max + 1;
		dim2[0] = j;
		dim2[1] = k;
		
		char cmd_1[dim1[0]][dim1[1]];

		for (j = 0; j < pipeIndex; j++)
			strcpy(cmd_1[j], cmd2[j]);
			

		char * CMD_1[] = { cmd_1[0], cmd_1[1], cmd_1[2], cmd_1[3],
						   cmd_1[4], cmd_1[5], cmd_1[6], cmd_1[7],
						   cmd_1[8], cmd_1[9], cmd_1[10], cmd_1[11],
						   cmd_1[12], cmd_1[13], cmd_1[14], cmd_1[15],
						   cmd_1[16], cmd_1[17], cmd_1[18], cmd_1[19],
						   cmd_1[20], cmd_1[21], cmd_1[22], cmd_1[23],
						   cmd_1[24], cmd_1[25], cmd_1[26], cmd_1[27],
						   cmd_1[28], cmd_1[29], cmd_1[30], cmd_1[31],
						   cmd_1[32], cmd_1[33], cmd_1[34], cmd_1[35],
						   cmd_1[36], cmd_1[37], cmd_1[38], cmd_1[39],
						   cmd_1[40], cmd_1[41], cmd_1[42], cmd_1[43],
						   cmd_1[44], cmd_1[45], cmd_1[46], cmd_1[47],
						   cmd_1[48], cmd_1[49], cmd_1[50], cmd_1[51],
						   cmd_1[52], cmd_1[53], cmd_1[54], cmd_1[55],
						   cmd_1[56], cmd_1[57], cmd_1[58], cmd_1[59],
						   cmd_1[60], cmd_1[61], cmd_1[62], cmd_1[63], 0};
	

		char cmd_2[dim2[0]][dim2[1]];

		for (j = 0; j < dim2[0]; j++)
			strcpy(cmd_2[j], cmd2[pipeIndex + 1 + j]);

		char * CMD_2[] = { cmd_2[0], cmd_2[1], cmd_2[2], cmd_2[3],
						   cmd_2[4], cmd_2[5], cmd_2[6], cmd_2[7],
						   cmd_2[8], cmd_2[9], cmd_2[10], cmd_2[11],
						   cmd_2[12], cmd_2[13], cmd_2[14], cmd_2[15],
						   cmd_2[16], cmd_2[17], cmd_2[18], cmd_2[19],
						   cmd_2[20], cmd_2[21], cmd_2[22], cmd_2[23],
						   cmd_1[24], cmd_1[25], cmd_1[26], cmd_1[27],
						   cmd_1[28], cmd_1[29], cmd_1[30], cmd_1[31],
						   cmd_1[32], cmd_1[33], cmd_1[34], cmd_1[35],
						   cmd_1[36], cmd_1[37], cmd_1[38], cmd_1[39],
						   cmd_1[40], cmd_1[41], cmd_1[42], cmd_1[43],
						   cmd_1[44], cmd_1[45], cmd_1[46], cmd_1[47],
						   cmd_1[48], cmd_1[49], cmd_1[50], cmd_1[51],
						   cmd_1[52], cmd_1[53], cmd_1[54], cmd_1[55],
						   cmd_1[56], cmd_1[57], cmd_1[58], cmd_1[59],
						   cmd_1[60], cmd_1[61], cmd_1[62], cmd_1[63], 0};
	

		int fd[2];
		pipe(fd);
		
		CMD_1[dim1[0]] = NULL;
		CMD_2[dim2[0]] = NULL;

		if (!fork())
		{
			dup2(fd[0], 0);	// make stdout same as pfds[1] 

			close(fd[1]);

			myPipe(dim1, dim2, CMD_1, CMD_2);
		} 
		else			
		{
			dup2(fd[1], 1);	// make stdin same as pfds[0] 

			close(fd[0]);
		
			char firstArg[strlen(cmd1[0])];
			strcpy(firstArg, cmd1[0]);

			execv(expand_pipe_path(firstArg), cmd1);
			printf("bash: %s: command not found\n", firstArg);
			exit(1);
		}
	}
	else // Base case: numPipes = 0
	{
		int fd[2];
		pipe(fd);

		if (!fork())
		{
			dup2(fd[0], 0);	// make stdout same as pfds[1] 

			close(fd[1]);
	
			char firstArg[strlen(cmd2[0])];
			strcpy(firstArg, cmd2[0]);

			execv(expand_pipe_path(firstArg), cmd2);
			printf("bash: %s: command not found\n", firstArg);
			exit(1);
		} 
		else
		{
			dup2(fd[1], 1);	// make stdin same as pfds[0] 

			close(fd[0]); 

			char firstArg[strlen(cmd1[0])];
			strcpy(firstArg, cmd1[0]);

			execv(expand_pipe_path(firstArg), cmd1);
			printf("bash: %s: command not found\n", firstArg);
			exit(1);
		}
	}
}

// Expands path of command/executable name
char * expand_pipe_path(char * path)
{
	char bin[strlen(path) + 7];
	strcpy(bin, "/bin/");
	
	if (path[0] != '/')
	{
		if (path[0] == '.')
		{
			int i;
			for (i = 0; path[i] != '\0'; i++)
				path[i] = path[i + 1];

			path[i - 1] = '\0';

			path = strcat(getenv("PWD"), path);
		}
		else
		{
			strcat(bin, path);
			strcpy(path, bin);
		}
	}
	
	return path;
}

// Frees dynamically allocated memory
void my_clean(char ** arr)
{
	int i;
	for (i = 0; arr[i] != NULL; i++)
		free(arr[i]);

	free(arr);
}
