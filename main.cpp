#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <string>

#include <sys/types.h>
#include <sys/wait.h>

#include <vector>
#include <iostream>

//==============================================================================
//==============================================================================
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

//==============================================================================
#define exitError()\
    do{char buf[128] = {0}; sprintf(buf, "Error! file:%s line:%d", __FILENAME__, __LINE__); perror(buf); exit(EXIT_FAILURE);}while(0)

//==============================================================================
#define exitErrorMsg(msg)\
    do{char buf[128] = {0}; sprintf(buf, "Error! %s file:%s line:%d", msg,  __FILENAME__, __LINE__); perror(buf); exit(EXIT_FAILURE);}while(0)

//==============================================================================
#ifndef countof
    #define countof(arr) ((sizeof(arr))/(sizeof(arr[0])))
#endif

//==============================================================================

#define READ_END 0
#define WRITE_END 1


/*
Пусть у нас есть длинная команда вида:

who | sort | uniq -c | sort -nk1

      1 sie      pts/0        2019-04-07 12:37 (:0)
      1 sie      pts/1        2019-04-07 12:38 (:0)
      1 sie      pts/2        2019-04-07 14:27 (:0)
      1 sie      tty7         2019-04-07 12:37 (:0)


Надо ее прочитать из STDIN, выполнить и STDOUT записать в файл /home/box/result.out

Тестовая система выполнит make. Она ожидает, что появится файл исполняемый файл - /home/box/shell.
После чего она исполнит его несколько раз, подавая на STDIN разные строчки и проверяя result.out.
*/

struct shcmd_t {
    std::string cmd;
    const char* args[];
};

struct shellcmd_t {
    const char *cmd;
    const char *args;
    int pfds[2];
};

static shellcmd_t TestCommands[] = {
    {"/usr/bin/who", NULL},
    {"/usr/bin/sort", NULL},
    {"/usr/bin/uniq" , "-c"},
    {"/usr/bin/sort",  "-nk1"}
};

/*
Cоздать неименованный канал.

int pipe(int fd[2]); // В fd[1] можно писать, из fd[0] читать. Канал односторонний.

Атомарно можно записать PIPE_BUF байт.

Для создания конвеера можно использовать вызовы pipe (создает канал) и dup2 (копирует дескриптор).
Основная идея - создаем канал fd, делаем fork. У первого процесса закрываем stdout (close(1)). Делаем dup2(fd[1], 1).
У второго процесса закрываем stdin (close(0)) и делаем dup2(fd[0], 0).
После чего у обоих процессов можно закрыть fd[0] и fd[1] - stdout первого процесса будет связан с stdin второго через созданный нами канал.

Существуют вызовы popen и pclose - аналоги fopen и fclose, но только для запуска процессов. Работают они через неименованные каналы.
*/

//==============================================================================
//
//==============================================================================
int main(int argc, char *argv[])
{
    std::string filepath(getenv("HOME"));
    filepath += "/result.out";

    const char *result_file = filepath.c_str();

    FILE *result = fopen(result_file, "w+");

    int res_fd = fileno(result);


//    char buf[1024];
//    ssize_t sz = readlink("/proc/self/exe", buf, sizeof(buf));
//    if (sz < 0) exitError();
//    buf[sz] = 0;
//    printf("%s\n", buf);

//    return 0;
    std::string rawcmds("who | sort | uniq -c | sort -nk1");
//    std::string rawcmds;
//    std::getline (std::cin, rawcmds);

    std::vector<std::string> commands;

    char *pch = strtok((char*)rawcmds.c_str(), "|");

    while (pch != NULL) {
	commands.push_back(std::string(pch));
	pch = strtok(NULL, "| ");
    }

    for (auto s : commands) {
	printf("%s\n", s.c_str());
    }
    return 0;

   /* char str[] ="- This, a sample string.";
  char * pch;
  printf ("Splitting string \"%s\" into tokens:\n",str);
  pch = strtok (str," ,.-");
  while (pch != NULL)
  {
    printf ("%s\n",pch);
    pch = strtok (NULL, " ,.-");
  }
  return 0;
    */

    int ffd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);

    for (int i = 0; i < countof(TestCommands); i++) {
	shellcmd_t shellcmd = TestCommands[i];

	close(STDOUT_FILENO);
	dup2(ffd, STDOUT_FILENO);


	pipe(shellcmd.pfds);

	const int &read_fd = shellcmd.pfds[0];  // read FROM pipe
	const int &write_fd = shellcmd.pfds[1]; // write TO pipe

	pid_t child = fork();
	if (child == 0) {
	    /* child */
	    close(STDIN_FILENO);
	    dup2(read_fd, STDIN_FILENO);
	    close(write_fd);
	    close(read_fd);
	    execlp(shellcmd.cmd, shellcmd.cmd, shellcmd.args, NULL);
	} else if (child > 0) {
	    /* parent */
	    close(STDOUT_FILENO);
	    dup2(write_fd, STDOUT_FILENO);
	    close(write_fd);
	    close(read_fd);
	    int status = 0;
	    waitpid(child, &status, WNOHANG);
	}
    }


    close(ffd);
    fclose(result);

    return 0;
}
