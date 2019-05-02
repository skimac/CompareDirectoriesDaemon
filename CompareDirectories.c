#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <syslog.h>
#include <ctype.h>
#include <signal.h>

int sleep_time_sec = 10;    //Domyślny czas spania, ustawić na 300s
int recursion_flag = 0;
int sigusr1_flag = 0;

int is_dir(char* path){
    DIR *directory;

    if( (directory = opendir(path)) == 0 ){
        return 0;
    }
    closedir(directory);
    return 1;
}

int copy(char *source, char *dest)
{
    int in, out, nbytes;
    char buffer[131072];

/*    char *log_buffer = "COPYING STARTED...\n";
    size_t bytes_written = strlen(log_buffer);
    write(daemonlog, log_buffer, bytes_written);*/

    if((in=open(source, O_RDONLY))<0)
    {
/*        log_buffer= "SRC OPENING FAILURE\n";
        bytes_written = strlen(log_buffer);
        write(daemonlog, log_buffer, bytes_written);*/
        return -1; //blad
    }
    if((out=open(dest, O_CREAT | O_WRONLY | O_TRUNC, 0777))<0)
    {
/*        log_buffer= "DEST OPENING FAILURE\n";
        bytes_written = strlen(log_buffer);
        write(daemonlog, log_buffer, bytes_written);*/
        return -2; //blad
    }
    while(nbytes=read(in,buffer,1024))
    {
        if(nbytes<0)
        {
            close(in);
            close(out);
            return 1;
        }
        if(write(out,buffer,nbytes)<0)
        {
            close(in);
            close(out);
            return 1;
        }
    }

/*    log_buffer= "COPYING FINISHED\n";
    bytes_written = strlen(log_buffer);
    write(daemonlog, log_buffer, bytes_written);*/

    close(in);
    close(out);
    return 0;
}

int fileexist(char* filename){
    int fd;

    if( (fd = open(filename, O_WRONLY)) <0 ){
        return 0;
    }

    close(fd);
    return 1;
}

int direxist(char* dirname){
    DIR *dir;

    if(opendir(dir) == NULL){
        return 0;
    }

    closedir(dir);
    return 1;
}

time_t file_modified_time(const char* path){
    struct stat path_stat;
    stat(path, &path_stat);
    return path_stat.st_mtime;
}

void remove_file(char* path){
    unlink(path);
}

void signal_handler(int signum){
    if(signum == SIGUSR1){
        syslog(LOG_NOTICE, "wybudzenie demona po otrzymaniu sygnału SIGUSR1");
        sigusr1_flag = 1;
    }
}

int dir_exist(char* path){
    DIR * directory;

    if( (directory = opendir(path)) == NULL ){
        return 0;
    }

    closedir(directory);
    return 1;
}

void dir_synchronization(char* path1, char* path2){

    DIR* src_dir = opendir(path1);
    DIR* dest_dir = opendir(path2);

    struct dirent* src_ent;
    struct dirent* dest_ent;


    while( (src_ent = readdir(src_dir)) != NULL ){
        dest_ent = readdir(dest_dir);

        if(src_ent->d_type == DT_DIR){

            char src_path[1024];
            snprintf(src_path, 1024,"%s/%s",path1, src_ent->d_name);

            char dest_path[1024];
            snprintf(dest_path, 1024,"%s/%s",path2, src_ent->d_name);

            syslog(LOG_NOTICE, "synchronizacja katalogu %s", src_path);


        }

        if(dest_ent->d_type == DT_DIR){

        }

        if(src_ent->d_type == DT_REG){

            char src_path[1024];
            snprintf(src_path, 1024,"%s/%s",path1, src_ent->d_name);

            char dest_path[1024];
            snprintf(dest_path, 1024,"%s/%s",path2, src_ent->d_name);

            //synchronization(path1, path2);    //Za jakiegoś powodu wywala warning

            if( !fileexist(dest_path) ){    //Sprawdzenie, czy jest taki plik w katalogu docelowym
                copy(src_path, dest_path);
                syslog(LOG_NOTICE, "skopiowano plik %s", src_path);
            } else if( file_modified_time(src_path) > file_modified_time(dest_path) ){
                copy(src_path, dest_path);   //Kiedy plik istnieje w obu katalogach, ale plik w katalogu źróðłowym ma późniejszą datę modyfikacji
                syslog(LOG_NOTICE, "zaktualizowano plik %s", src_path);
            }

        }

        if( dest_ent != NULL && dest_ent->d_type == DT_REG){

            char dest_path[1024];
            snprintf(dest_path, 1024,"%s/%s",path2, dest_ent->d_name);

            char src_path[1024];
            snprintf(src_path, 1024,"%s/%s",path1, dest_ent->d_name);

            if(dest_ent->d_type == DT_REG){

                if( !fileexist(src_path)){      //Sprawdzenie, czy jest taki plik w katalogu źróðłowym
                    unlink(dest_path);       //Usunięcie pliku
                    syslog(LOG_NOTICE, "usunięto plik %s", dest_path);
                }
            }
        }


    }

    closedir(src_dir);
    closedir(dest_dir);
}

int remove_directory(const char *path)
{
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;
    if (d)
    {
        struct dirent *p;
        r = 0;

        while (!r && (p=readdir(d)))
        {
            int r2 = -1;
            char *buf;
            size_t len;

            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if (buf)
            {
                struct stat statbuf;

                snprintf(buf, len, "%s/%s", path, p->d_name);

                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        r2 = remove_directory(buf);
                    }
                    else
                    {
                        r2 = unlink(buf);
                    }
                }

                free(buf);
            }

            r = r2;
        }

        closedir(d);
    }

    if (!r)
    {
        r = rmdir(path);
    }

    return r;
}

void dir_sync2(char * path1, char* path2){

    struct dirent *file;
    DIR *src_dir = opendir(path1);
    DIR *dest_dir = opendir(path2);

    char dest_path[1024];

    char src_path[1024];

    while ( (file = readdir(dest_dir)) != NULL ){
        if( file->d_type == DT_DIR){
            snprintf(src_path, 1024,"%s/%s",path1, file->d_name);
            snprintf(dest_path, 1024,"%s/%s",path2, file->d_name);

            if( dir_exist(src_path) ){
                dir_sync2(src_path, dest_path);
            }else{
                remove_directory(dest_path);
            }
        }
    }

    closedir(src_dir);
    closedir(dest_dir);
}

void synchronization(char* path1, char* path2){

    DIR *src_dir = opendir(path1);
    syslog(LOG_NOTICE, "owarto katalog %s", path1);
    DIR *dest_dir = opendir(path2);
    syslog(LOG_NOTICE, "otwarto katalog %s", path2);

    struct  dirent* src_ent;
    struct dirent* dest_ent;

    syslog(LOG_NOTICE, "synchronizacja katalogów %s i %s", path1, path2);

    /* PORÓWNYWANIE KATALOGÓW */
    while( (src_ent = readdir(src_dir)) != NULL){
        dest_ent = readdir(dest_dir);

        syslog(LOG_NOTICE, "plik SRC: = %s", src_ent->d_name);
        syslog(LOG_NOTICE, "plik DEST: = %s", dest_ent->d_name);

/*                buffer = "READING...\n";
                bytes_written = strlen(buffer);
                write(daemonlog, buffer, bytes_written);*/

        if(recursion_flag == 1){
            if(src_ent->d_type == DT_DIR && strcmp(src_ent->d_name, ".") != 0 && strcmp(src_ent->d_name, "..") != 0 ){

                char src_path[1024];
                snprintf(src_path, 1024,"%s/%s",path1, src_ent->d_name);

                char dest_path[1024];
                snprintf(dest_path, 1024,"%s/%s",path2, src_ent->d_name);

                if( !dir_exist(dest_path)){
                    syslog(LOG_NOTICE, "tworzenie katalogu %s", dest_path);
                    mkdir(dest_path, 0777);
                }

                syslog(LOG_NOTICE, "src_path = %s, dest_path = %s", src_path, dest_path);
                synchronization(src_path, dest_path);
            }

            if(dest_ent->d_type == DT_DIR && strcmp(dest_ent->d_name, ".") != 0 && strcmp(dest_ent->d_name, "..") != 0 ){

                char src_path[1024];
                snprintf(src_path, 1024,"%s/%s",path1, src_ent->d_name);

                char dest_path[1024];
                snprintf(dest_path, 1024,"%s/%s",path2, src_ent->d_name);

                if( !dir_exist(dest_path)){     //Sprawdzenie, czy jest taki podkatalog w katalogu źródłowym
                    syslog(LOG_NOTICE, "usuwanie katalogu %s", dest_path);
                    remove_directory(dest_path);
                }
            }
        }

        if(src_ent->d_type == DT_REG){

            char src_path[1024];
            snprintf(src_path, 1024,"%s/%s",path1, src_ent->d_name);

            char dest_path[1024];
            snprintf(dest_path, 1024,"%s/%s",path2, src_ent->d_name);

            if( !fileexist(dest_path) ){    //Sprawdzenie, czy jest taki plik w katalogu docelowym
                copy(src_path, dest_path);
                syslog(LOG_NOTICE, "skopiowano plik %s", src_path);
            } else if( file_modified_time(src_path) > file_modified_time(dest_path) ){
                copy(src_path, dest_path);   //Kiedy plik istnieje w obu katalogach, ale plik w katalogu źróðłowym ma późniejszą datę modyfikacji
                syslog(LOG_NOTICE, "zaktualizowano plik %s", src_path);
            }

        }
        if( dest_ent != NULL && dest_ent->d_type == DT_REG){

/*                    buffer = dest_ent->d_name;
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);*/

            char dest_path[1024];
            snprintf(dest_path, 1024,"%s/%s",path2, dest_ent->d_name);

/*
                    char print_dest_path[1024];
                    snprintf(print_dest_path, 1024, "%s%s%s", "\n", dest_path, "\n");

                    buffer = print_dest_path;
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);
*/

            char src_path[1024];
            snprintf(src_path, 1024,"%s/%s",path1, dest_ent->d_name);

            if(dest_ent->d_type == DT_REG){

                if( !fileexist(src_path)){      //Sprawdzenie, czy jest taki plik w katalogu źróðłowym
                    unlink(dest_path);       //Usunięcie pliku
                    syslog(LOG_NOTICE, "usunięto plik %s", dest_path);
                }
            }
/*
            if(dest_ent->d_type == DT_DIR){     //Przenieść do synchronizacji katalogów
                if( !fileexist(src_path)){
                    //Usuwanie katalogu
                    syslog(LOG_NOTICE, "katalog do usunięcia: %s", dest_path);
                }
            }*/

/*
                    buffer = src_path;
                    bytes_written = strlen(buffer);
                    write(daemonlog, buffer, bytes_written);
*/

        }
    }
    closedir(src_dir);
    closedir(dest_dir);
}

int main(int argc, char* argv[]) {
    FILE *logfile = fopen("/home/maciej/CLionProjects/CompareDirectories/logs/log.txt", "w");
/*    fprintf(logfile, "argv[0]: %s\n"
            "argv[1]: %s\n"
            "argv[2]: %s\n"
            "is_directory(src): %d\n"
            "is_directory(dest): %d\n", argv[0], argv[1], argv[2], is_dir(argv[1]), is_dir(argv[2]));*/

    setlogmask(LOG_UPTO(LOG_NOTICE));   //Ustawienie maski logu
    syslog(LOG_NOTICE,"program uruchomiony przez użytkownika %d", getuid());

    char path1[1024], path2[1024];
    strcpy(path1, argv[1]);
    strcpy(path2, argv[2]);

    if( is_dir(path1) && is_dir(path2) ){
        __pid_t  pid, sid;
        pid = fork();

        int c;

        while((c=getopt(argc,argv,":RT:"))!=-1)
        {
            switch(c)
            {
                case 'R':
                    recursion_flag=1;
                    break;
                case 'T':
                    sleep_time_sec=atoi(optarg);
                    fprintf(logfile, "sleep_time_sec = %d\n", sleep_time_sec);
                    break;

/*                default:
                    abort();*/
            }
        }


        /* Fork off the parent process */
        if(pid < 0){    //W przypadku niepowodzenia
            fprintf(logfile, "PID FAILURE\n");
            fclose(logfile);
            exit(EXIT_FAILURE);
        }

        if(pid > 0){    //W przypadku powodzenia
            fprintf(logfile, "PID SUCCESS\n");
            fclose(logfile);
            syslog(LOG_NOTICE, "program stał się demonem");
            exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);

        /* Open any logs here */
/*        int daemonlog = open("/home/maciej/CLionProjects/CompareDirectories/logs/daemon_log.txt",
                O_CREAT | O_WRONLY | O_TRUNC);
        char *buffer = "START SUCCESS\n";
        size_t bytes_written = strlen(buffer);
        write(daemonlog, buffer, bytes_written);*/

        /* Create a new SID for the child process */
        sid = setsid();

        if(sid < 0){    //W przypadku niepowodzenia
            /* Log any failures here */
/*            buffer = "SID FAILURE\n";
            bytes_written = strlen(buffer);
            write(daemonlog, buffer, bytes_written);
            close(daemonlog);*/
            exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if(chdir("/") < 0){     //W przypadku niepowodzenia
            /* Log any failures here */
/*            buffer = "CHDIR FAILURE\n";
            bytes_written = strlen(buffer);
            write(daemonlog, buffer, bytes_written);
            close(daemonlog);*/
            exit(EXIT_FAILURE);
        }

        signal(SIGUSR1, signal_handler);      //Przechwytywanie sygnału SIGUSR1

        /* Close out the standard file descriptors */
/*        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);*/

        /* Daemon-specific initialization goes here */

        /* The Big Loop */
        while (1){
            //Do some task here ...
/*            buffer = "ENTERED BIG LOOP\n";
            bytes_written = strlen(buffer);
            write(daemonlog, buffer, bytes_written);*/
            sigusr1_flag = 0;

            syslog(LOG_NOTICE, "demon przechodzi w stan uśpienia");
            sleep(sleep_time_sec);
            if( !sigusr1_flag){
                syslog(LOG_NOTICE, "naturalne wybudzenie demona");
            }

            synchronization(path1, path2);

/*            if(recursion_flag){
                //dir_synchronization(path1, path2);
                dir_sync2(path1, path2);
            }*/

            //break;      //Usunąć break, demon ma działać cały czas!!!
        }
        exit(EXIT_SUCCESS);

    } else{
        /*  KOMUNIKAT BŁĘDU */
        printf("BŁĄD: OBIE ŚCIEŻKI MUSZĄ BYĆ KATALOGAMI!\n");
    }

    return 0;
}