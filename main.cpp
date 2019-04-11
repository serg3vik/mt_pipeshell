#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <string>

#include <sys/types.h>
#include <sys/wait.h>

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

#define READ_END    0
#define WRITE_END   1

//==============================================================================

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

#define MAX_ARGS_COUNT      (8)
#define MAX_CMDS_COUNT      (16)
#define MAX_CMD_LENGTH      (64)

#define MAX_INPUT_LENGTH    (MAX_CMD_LENGTH * MAX_CMDS_COUNT)


struct shellcmd_t {
    char *args[MAX_ARGS_COUNT];
    int pfds[2];
};

static shellcmd_t test_commands[MAX_CMDS_COUNT] = {0};

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

#if 1
    char raw_input[MAX_INPUT_LENGTH] = "who | sort | uniq -c | sort -nk1";
#else
    char raw_input[MAX_INPUT_LENGTH] = {0};
    gets(raw_input);
#endif


    shellcmd_t *command = NULL;

    int cmdcnt = 0;
    int argcnt = 0;
    char *pch = strtok(raw_input, "| ");
    while (pch) {
        if ((pch[0] == '-') && command) {
            command->args[argcnt++] = pch;
        } else {
            command = &test_commands[cmdcnt++];
            argcnt = 1;
            command->args[0] = pch;
        }

        pch = strtok(NULL, "| ");
    }

    const char *homedir = getenv("HOME");
    if (!homedir) {
        fprintf(stderr, "Can\'t find HOME directory!\n");
        return (EXIT_FAILURE);
    }

    char filepath[128];
    sprintf(filepath, "%s/result.out", homedir);

    int res_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);


    for (int i = 0; i < countof(test_commands); i++) {
        shellcmd_t *command = &test_commands[i];

        close(STDOUT_FILENO);
        dup2(res_fd, STDOUT_FILENO);


        pipe(command->pfds);

        pid_t child = fork();
        if (child == 0) {
            /* child */
            close(STDIN_FILENO);
            dup2(command->pfds[READ_END], STDIN_FILENO);
            close(command->pfds[WRITE_END]);
            close(command->pfds[READ_END]);
            execvp(command->args[0], command->args);
        } else if (child > 0) {
            /* parent */
            close(STDOUT_FILENO);
            dup2(command->pfds[WRITE_END], STDOUT_FILENO);
            close(command->pfds[WRITE_END]);
            close(command->pfds[READ_END]);
            int status = 0;
            waitpid(child, &status, WNOHANG);
        }
    }


    close(res_fd);

    return 0;
}
