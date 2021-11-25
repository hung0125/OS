/*
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your group information here.
 *
 * Group No.: 3 (Join a project group in Canvas)
 * First member's full name: HUNG Yiu Hong
 * First member's email address: yiuhhung3-c@my.cityu.edu.hk
 * Second member's full name: LEUNG Tak Man
 * Second member's email address: takmleung2-c@my.cityu.edu.hk
 * Third member's full name: CHAU Yee Lee
 * Third member's email address: leeychau3-c@my.cityu.edu.hk
 */

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <dirent.h>

#define min(a, b)(((a) < (b)) ? (a) : (b))

char fileLS[65536][512]; //The list stores file paths
int fIndex = 0; //The index value for inserting files to the list
int sumResult[65536][26]; //Store character counts (results)
int maxThreads = 0; //Max threads to allocate will not be higher than available CPU cores, optimal number of threads to use is 16
char * content[65536]; //Store mmap outputs
int contentLen[65536]; //Store the total length of file content for each file
pthread_mutex_t mutex1;

struct args {
    int tid;
};

/*
The thread used for counting characters
Each thread counts part of the file contents and updates results
*/
void * worker(void * cfg) {
    int tid = ((struct args * ) cfg) -> tid;
    int count[fIndex][26];
    memset(count, 0, fIndex * 26 * sizeof(int));

    for (int i = 0; i < fIndex; i++) //iterate every file
    {
        if (strlen(fileLS[i]) == 0)
            break;

        int partition = 0;
        int actualLen = 0;
        int from = 0;
        if (contentLen[i] - 1 < maxThreads) //case: content length is smaller than the threads
        {
            if (tid == 0) {
                partition = contentLen[i];
                actualLen = partition;
            }
        } else //case: general
        {
            partition = (contentLen[i] - 1) / maxThreads + 1;
            from = tid * partition;
            actualLen = tid < maxThreads - 1 ? from + partition : from + partition + (contentLen[i]) % maxThreads;
        }

        for (int j = from; j < actualLen; j++) {
            if (content[i][j] == '\0')
                break;

            int dictPos = content[i][j] - 97;

            if (dictPos > -1 && dictPos < 26) //somehow without this check it does not work
                count[i][dictPos]++;
        }
    }

    //critical section starts
    pthread_mutex_lock( & mutex1);

    for (int i = 0; i < fIndex; i++) {
        for (int j = 0; j < 26; j++)
            sumResult[i][j] += count[i][j];
    }

    pthread_mutex_unlock( & mutex1);

    pthread_exit(NULL);
}

/*
Sort list of file paths in dictionary order
Algorithm used is similar to bubble sort
*/
void sortFile(char fileLS[][512], int flen) {
    char temp[512];
    for (int i = 0; i < flen; i++) {
        for (int j = 0; j < flen - 1 - i; j++) {
            if (strcmp(fileLS[j], fileLS[j + 1]) > 0) {
                //swap array[j] and array[j+1]
                strcpy(temp, fileLS[j]);
                strcpy(fileLS[j], fileLS[j + 1]);
                strcpy(fileLS[j + 1], temp);
            }
        }
    }
}

/*
Check if the path is referring to a file
*/
int isFile(const char * path) {
    struct stat path_stat;
    stat(path, & path_stat);
    return S_ISREG(path_stat.st_mode);
}

/*
Recursively scan files in a directory and update them to the file path list
*/
void processDir(const char * name) {
    DIR * dir;
    struct dirent * entry;

    if (!(dir = opendir(name)))
        return;

    while ((entry = readdir(dir)) != NULL) {
        if (!isFile(entry -> d_name)) {
            char path[512];
            if (strcmp(entry -> d_name, ".") == 0 || strcmp(entry -> d_name, "..") == 0)
                continue;

            snprintf(path, sizeof(path), "%s/%s", name, entry -> d_name);

            if (isFile(path)) {
                strcpy(fileLS[fIndex], path);
                fIndex++;
            }

            processDir(path);
        } else {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", name, entry -> d_name);

            strcpy(fileLS[fIndex], path);
            fIndex++;
        }
    }

    closedir(dir);
}

int main(int argc,
    const char * argv[]) {
    //Exception handling: no file
    if (argc - 1 == 0) {
        printf("pcount: file1 [file2 ...]\n");
        return 1;
    }

    //Scan and add files to list
    for (int i = 1; i < argc; i++) {
        if (isFile(argv[i])) {
            strcpy(fileLS[fIndex], argv[i]);
            fIndex++;
        } else {
            char fpath[512];
            strcpy(fpath, argv[i]);
            while (fpath[strlen(fpath) - 1] == '/')
                fpath[strlen(fpath) - 1] = '\0';

            processDir(fpath);
        }
    }

    //Sort list of file paths
    sortFile(fileLS, fIndex);

    //Main counting process
    int empty = 1;
    for (int i = 0; i < fIndex; i++) {
        //Process file data
        int fd = open(fileLS[i], O_RDONLY);
        contentLen[i] = lseek(fd, 0, SEEK_END);
        if (contentLen[i] == 0) continue;

        content[i] = (char * ) mmap(NULL, contentLen[i], PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        empty = 0;
    }

    if (!empty) {
        maxThreads = get_nprocs();
        pthread_t pid[maxThreads];
        for (int i = 0; i < maxThreads; i++) {
            //Config thread arguments: thread ID
            struct args * tconfig = (struct args * ) malloc(sizeof(struct args));
            tconfig -> tid = i;

            //Launch threads
            pthread_create( & pid[i], NULL, worker, (void * ) tconfig);
        }

        for (int i = 0; i < maxThreads; i++)
            pthread_join(pid[i], NULL);

    }

    //Print results
    for (int i = 0; i < fIndex; i++) {
        printf("%s\n", fileLS[i]);

        for (int j = 0; j < 26; j++)
            if (sumResult[i][j] > 0)
                printf("%c %d\n", 'a' + j, sumResult[i][j]);
    }

    return 0;

}
