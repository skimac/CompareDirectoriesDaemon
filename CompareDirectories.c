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

int sleep_time_sec = 300;
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

    if((in=open(source, O_RDONLY))<0)
    {
        return -1; //blad
    }
    if((out=open(dest, O_CREAT | O_WRONLY | O_TRUNC, 0777))<0)
    {
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

time_t file_modified_time(const char* path){
    struct stat path_stat;
    stat(path, &path_stat);
    return path_stat.st_mtime;
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

void synchronization(char* path1, char* path2){

    DIR *src_dir = opendir(path1);

    struct  dirent* src_ent;

    /* PORÓWNYWANIE KATALOGÓW */
    while( (src_ent = readdir(src_dir)) != NULL){

        if(recursion_flag == 1){

            if(src_ent->d_type == DT_DIR && strcmp(src_ent->d_name, ".") != 0 && strcmp(src_ent->d_name, "..") != 0 ){

                syslog(LOG_NOTICE, "znaleziono podkatalog w katalogu źróðłowym: %s", src_ent->d_name);

                char src_path[1024];
                snprintf(src_path, 1024,"%s/%s",path1, src_ent->d_name);

                char dest_path[1024];
                snprintf(dest_path, 1024,"%s/%s",path2, src_ent->d_name);

                if( !dir_exist(dest_path)){
                    syslog(LOG_NOTICE, "tworzenie katalogu %s", dest_path);
                    mkdir(dest_path, 0777);
                }
                synchronization(src_path, dest_path);
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
    }
    closedir(src_dir);

    DIR *dest_dir = opendir(path2);

    struct dirent* dest_ent;

    while( (dest_ent = readdir(dest_dir)) != NULL ){

        if(recursion_flag == 1){

            if(dest_ent->d_type == DT_DIR && strcmp(dest_ent->d_name, ".") != 0 && strcmp(dest_ent->d_name, "..") != 0 ){

                char src_path[1024];
                snprintf(src_path, 1024,"%s/%s",path1, dest_ent->d_name);

                char dest_path[1024];
                snprintf(dest_path, 1024,"%s/%s",path2, dest_ent->d_name);

                if( !dir_exist(src_path)){     //Sprawdzenie, czy jest taki podkatalog w katalogu źródłowym
                    syslog(LOG_NOTICE, "usuwanie katalogu %s", dest_path);
                    remove_directory(dest_path);
                }
            }
        }

        if( dest_ent->d_type == DT_REG){

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

    closedir(dest_dir);
}

int main(int argc, char* argv[]) {

    setlogmask(LOG_UPTO(LOG_NOTICE));   //Ustawienie maski logu
    syslog(LOG_NOTICE,"program uruchomiony przez użytkownika %d", getuid());

    char path1[1024], path2[1024];
    strcpy(path1, argv[1]);
    strcpy(path2, argv[2]);

    if( is_dir(path1) && is_dir(path2) ){

        // Id procesu, id sesji
        __pid_t  pid, sid;

        /* Fork off the parent process */
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
                    break;
            }
        }


        /* Fork off the parent process */
        if(pid < 0){    //W przypadku niepowodzenia
            exit(EXIT_FAILURE);
        }

        if(pid > 0){    //W przypadku powodzenia
            syslog(LOG_NOTICE, "program stał się demonem");
            exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);

        /* Create a new SID for the child process */
        sid = setsid();

        if(sid < 0){    //W przypadku niepowodzenia
            exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if(chdir("/") < 0){     //W przypadku niepowodzenia
            exit(EXIT_FAILURE);
        }

        signal(SIGUSR1, signal_handler);      //Przechwytywanie sygnału SIGUSR1

        while (1){

            sigusr1_flag = 0;

            syslog(LOG_NOTICE, "demon przechodzi w stan uśpienia");
            sleep(sleep_time_sec);
            if( !sigusr1_flag){
                syslog(LOG_NOTICE, "naturalne wybudzenie demona");
            }

            synchronization(path1, path2);
        }

    } else{
        /*  KOMUNIKAT BŁĘDU */
        printf("BŁĄD: OBIE ŚCIEŻKI MUSZĄ BYĆ KATALOGAMI!\n");
    }

    return 0;
}