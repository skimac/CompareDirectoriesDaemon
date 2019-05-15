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
#include <time.h>

int sleep_time_sec = 300;
int recursion_flag = 0;
int sigusr1_flag = 0;

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

        time_t now;
        struct tm * timeinfo;
        time ( &now );
        timeinfo = localtime ( &now );

        syslog(LOG_NOTICE, "%s: wybudzenie demona po otrzymaniu sygnału SIGUSR1", asctime(timeinfo));

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
    if (d)      //Jeżeli otwieranie katalogu zakończy się powodzeniem
    {
        struct dirent *p;
        r = 0;

        while (!r && (p=readdir(d)))    //Czytanie po katalogu
        {
            int r2 = -1;
            char *buf;
            size_t len;

            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))    //Pomijamy dowiązania symboliczne
            {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if (buf)    //Jeżeli alokowanie pamięci zakończy się powodzeniem
            {
                struct stat statbuf;

                snprintf(buf, len, "%s/%s", path, p->d_name);   //Stworzenie ścieżki do odczytanego pliku

                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))       //Jeśli odczytany plik jest katalogiem
                    {
                        r2 = remove_directory(buf);

                        time_t now;
                        struct tm * timeinfo;
                        time ( &now );
                        timeinfo = localtime ( &now );

                        syslog(LOG_NOTICE, "%s: usunięto katalog %s", asctime(timeinfo), buf);
                    }
                    else
                    {
                        r2 = unlink(buf);       //Usunięcie pojedynczego pliku

                        time_t now;
                        struct tm * timeinfo;
                        time ( &now );
                        timeinfo = localtime ( &now );

                        syslog(LOG_NOTICE, "%s: usunięto plik %s", asctime(timeinfo), buf);
                    }
                }

                free(buf);      //Zwolnienie pamięci zaalokowanej przez bufor
            }

            r = r2;
        }

        closedir(d);
    }

    if (!r)     //Jeżeli usuwanie plików z katalogu zakończy się powodzeniem (funkcja remove_directory zwróci 0)
    {
        r = rmdir(path);    //Usunięcie pustego katalogu
    }

    return r;
}

void synchronization(char* path1, char* path2){

    DIR *src_dir = opendir(path1);

    struct  dirent* src_ent;


    while( (src_ent = readdir(src_dir)) != NULL){

        if(recursion_flag == 1){

            if(src_ent->d_type == DT_DIR && strcmp(src_ent->d_name, ".") != 0 && strcmp(src_ent->d_name, "..") != 0 ){

                char src_path[1024];
                snprintf(src_path, 1024,"%s/%s",path1, src_ent->d_name);

                char dest_path[1024];
                snprintf(dest_path, 1024,"%s/%s",path2, src_ent->d_name);

                if( !dir_exist(dest_path)){

                    mkdir(dest_path, 0777);

                    time_t now;
                    struct tm * timeinfo;
                    time ( &now );
                    timeinfo = localtime ( &now );

                    syslog(LOG_NOTICE, "%s: stworzono katalog %s", asctime(timeinfo), dest_path);

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

                time_t now;
                struct tm * timeinfo;
                time ( &now );
                timeinfo = localtime ( &now );

                syslog(LOG_NOTICE, "%s: skopiowano plik %s", asctime(timeinfo), src_path);

            } else if( file_modified_time(src_path) > file_modified_time(dest_path) ){

                copy(src_path, dest_path);   //Kiedy plik istnieje w obu katalogach, ale plik w katalogu źróðłowym ma późniejszą datę modyfikacji

                time_t now;
                struct tm * timeinfo;
                time ( &now );
                timeinfo = localtime ( &now );

                syslog(LOG_NOTICE, "%s: zaktualizowano plik %s", asctime(timeinfo),src_path);
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

                    remove_directory(dest_path);

                    time_t now;
                    struct tm * timeinfo;
                    time ( &now );
                    timeinfo = localtime ( &now );

                    syslog(LOG_NOTICE, "%s: usunięto katalog %s", asctime(timeinfo), dest_path);
                }
            }
        }

        if( dest_ent->d_type == DT_REG){

            char dest_path[1024];
            snprintf(dest_path, 1024,"%s/%s",path2, dest_ent->d_name);

            char src_path[1024];
            snprintf(src_path, 1024,"%s/%s",path1, dest_ent->d_name);

            if( !fileexist(src_path)){      //Sprawdzenie, czy jest taki plik w katalogu źróðłowym

                unlink(dest_path);       //Usunięcie pliku

                time_t now;
                struct tm * timeinfo;
                time ( &now );
                timeinfo = localtime ( &now );

                syslog(LOG_NOTICE, "%s: usunięto plik %s", asctime(timeinfo), dest_path);
            }
        }

    }

    closedir(dest_dir);
}

int main(int argc, char* argv[]) {

    char path1[1024], path2[1024];
    strcpy(path1, argv[1]);
    strcpy(path2, argv[2]);

    if( dir_exist(path1) && dir_exist(path2) ){

        setlogmask(LOG_UPTO(LOG_NOTICE));   //Ustawienie maski logu

        time_t now;
        struct tm * timeinfo;
        time ( &now );
        timeinfo = localtime ( &now );

        syslog(LOG_NOTICE, "%s: program uruchomiony przez użytkownika %d", asctime(timeinfo), getuid());

        // Id procesu, id sesji
        __pid_t  pid, sid;

        //Stworzenie procesu potomnego
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

        /* OPUSZCZENIE PROCESU, KTÓRY STWORZYŁ PROCES POTOMNY */
        if(pid < 0){    //Jeżeli nie udało się uzyskać id procesu
            exit(EXIT_FAILURE);
        }

        if(pid > 0){    //Jeżeli udało się uzyskać id procesu

            time_t now;
            struct tm * timeinfo;
            time ( &now );
            timeinfo = localtime ( &now );

            syslog(LOG_NOTICE, "%s: program stał się demonem", asctime(timeinfo));
            exit(EXIT_SUCCESS);
        }

        //Ustawienie maski do tworzenia plików
        umask(0);

        //Uzyskanie id sesji
        sid = setsid();

        if(sid < 0){    //Jeżeli nie uda się uzyskać id sesji
            exit(EXIT_FAILURE);
        }

        //Zmiana katalogu, w którym pracuje demon
        if(chdir("/") < 0){     //Jeżeli nie uda się odnaleźć podanej ścieżki
            exit(EXIT_FAILURE);
        }

        signal(SIGUSR1, signal_handler);      //Powiązanie sygnału SIGUSR1 ze zdefiniowanym handlerem

        while (1){

            sigusr1_flag = 0;

            time_t now;
            struct tm * timeinfo;
            time ( &now );
            timeinfo = localtime ( &now );

            syslog(LOG_NOTICE, "%s: demon przechodzi w stan uśpienia", asctime(timeinfo));
            sleep(sleep_time_sec);
            if( !sigusr1_flag){

                time_t now;
                struct tm * timeinfo;
                time ( &now );
                timeinfo = localtime ( &now );

                syslog(LOG_NOTICE, "%s: naturalne wybudzenie demona", asctime(timeinfo));
            }

            synchronization(path1, path2);
        }

    } else{
        /*  KOMUNIKAT BŁĘDU */
        printf("BŁĄD: OBIE ŚCIEŻKI MUSZĄ BYĆ KATALOGAMI!\n");
    }

    return 0;
}