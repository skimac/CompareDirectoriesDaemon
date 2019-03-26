#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>

/*
 * Zwraca 1, jeśli podana ścieżka jest plikiem, w każdym innym przypadku zwraca 0
 * (nawet jeśli podana ścieżka nie istnieje).
 * */

int is_regular_file(const char *path){
    struct stat path_stat;          //Struktura 'stat' jest już domyślnie zdefiniowana w <sys/stat.h>
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/*
 *  Zwraca wartość różną od 0, jeśli podana ścieżka jest katalogiem (dziwnie działa, sprawdzić działanie funkcji)
 * */
int is_directory(const char *path){
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}


int main(int argc, const char *argv[]) {
    FILE *logfile = fopen("log.txt", "w");
    fprintf(logfile, "argv[0]: %s\n"
            "argv[1]: %s\n"
            "argv[2]: %s'\n"
            "is_directory(src): %d\n"
            "is_directory(dest): %d\n", argv[0], argv[1], argv[2], is_directory(argv[1]), is_directory(argv[2]));


    if( is_directory(argv[1]) != 0 && is_directory(argv[2]) != 0 ){
        __pid_t  pid, sid;
        pid = fork();

        /* Fork off the parent process */
        if(pid < 0){    //W przypadku niepowodzenia
            fprintf(logfile, "PID FAILURE\n");
            fclose(logfile);
            exit(EXIT_FAILURE);
        }

        if(pid > 0){    //W przypadku powodzenia
            fprintf(logfile, "PID SUCCESS\n");
            fclose(logfile);
            exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);

        /* Open any logs here */

        int daemonlog = open("daemon_log.txt",
                O_CREAT | O_WRONLY | O_TRUNC);
        char *buffer = "START SUCCESS\n";
        size_t bytes_written = strlen(buffer);
        write(daemonlog, buffer, bytes_written);

        buffer = "BUFFER TEXT CHANGED\n";
        bytes_written = strlen(buffer);
        write(daemonlog, buffer, bytes_written);

        /* Create a new SID for the child process */
        sid = setsid();

        if(sid < 0){    //W przypadku niepowodzenia
            /* Log any failures here */
            buffer = "SID FAILURE\n";
            bytes_written = strlen(buffer);
            write(daemonlog, buffer, bytes_written);
            close(daemonlog);
            exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if(chdir("/") < 0){     //W przypadku niepowodzenia
            /* Log any failures here */
            buffer = "CHDIR FAILURE\n";
            bytes_written = strlen(buffer);
            write(daemonlog, buffer, bytes_written);
            close(daemonlog);
            exit(EXIT_FAILURE);
        }

        /* Close out the standard file descriptors */
/*        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);*/

        /* Daemon-specific initialization goes here */

        /* The Big Loop */
        while (1){
            /* Do some task here ... */
            buffer = "ENTERED BIG LOOP\n";
            bytes_written = strlen(buffer);
            write(daemonlog, buffer, bytes_written);
            if(argv[2] >= 0)
            {
                //sleep(10);
            } else{
                sleep(atoi(argv[0]));
            }
            DIR *src = opendir(argv[1]);
            DIR *dest = opendir(argv[2]);
            struct  dirent *src_ent, *dest_ent;

            /* PORÓWNYWANIE KATALOGÓW */
            while( (src_ent = readdir(src)) != NULL){ //Wcześniej było != NUll w alternatywie
                dest_ent = readdir(dest);
                buffer = "READING...\n";
                bytes_written = strlen(buffer);
                write(daemonlog, buffer, bytes_written);

                if(src_ent->d_type == DT_REG){
                    char *filename = src_ent->d_name;
                    buffer = strcat(filename, "\n");
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);

                    int cp_src, cp_dest;
                    char *src_path = argv[1];
                    strcat(src_path, "/");
                    strcat(src_path, src_ent->d_name);

                    buffer = strcat(argv[2], "\n");
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);

                    char *dest_path = argv[2];
                    strcat(dest_path, "/");
                    strcat(dest_path, src_ent->d_name);

                    /*  WYPISYWANIE ŚCIEŻEK NA PRÓBĘ*/
                    buffer = strcat(src_path, "\n");
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);


                    buffer = strcat(dest_path, "\n");
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);

                    //cp_src = open(src_path, O_RDONLY);
                    //cp_dest = open(dest_path, O_WRONLY | O_CREAT);


                }
                if( dest_ent != NULL && dest_ent->d_type == DT_REG){
                    char *filename = dest_ent->d_name;
                    buffer = strcat(filename, "\n");
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);
                }
            }
            closedir(src);
            closedir(dest);
            break;  //USUNĄĆ BREAK, DAEMON MA DZIAŁAĆ CAŁY CZAS!!!
        }
        close(daemonlog);
        exit(EXIT_SUCCESS);

    } else{
        /*  KOMUNIKAT BŁĘDU */
        printf("ERROR: BOTH PATHS MUST BE DIRECTORIES!\n");
        //errno();
    }

    return 0;
}