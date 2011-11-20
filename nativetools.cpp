#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <fcntl.h>

/*
 * Build using ndk-comp++ nativetools.cpp -o nativetools
 * or better: CyanogenMod's make
 *
 * Commands:
 * df > partition space info
 *     df path
 * go > retrieve a file's owner
 *     go path
 * fe > check whether a file exists
 *     go path
 * du > folder usage
 *     du path
 * ll > list symlinks
 *     ll path
 * ml > mount loop
 *     ml path
 * cp > recursive copy
 *     cp srcpath dstpath
 * rm > recursive rm
 *     rm path
 * co > recursive chown
 *     co path maxdepth uname
 * rf > read file
 *     rf path
 * mr > remount partition read-only
 *     mr /device /mountpoint
 * mw > remount partition read-write
 *     mw /device /mountpoint
 * cr > crawl all directories/files
 *     cr path
 */

/* If C compiler (not C++) declare these:
typedef enum {
    false = 0,
    true
} bool;
*/

/*
 * Notes:
 * Whenever I think about it, I need to allocate fixed-size memory on the stack, which is tremendously faster
 * than having a bunch of malloc/free.
 */
int df(char *s) {
    int ret = EXIT_SUCCESS;
    struct statfs st;

    if (statfs(s, &st) < 0) {
        ret = EXIT_FAILURE;
    }
    else {
        // D,total,used,available,block_size
        printf("D,%lld,%lld,%lld,%d",
            ((long long)st.f_blocks * (long long)st.f_bsize) / 1024,
            ((long long)(st.f_blocks - (long long)st.f_bfree) * st.f_bsize) / 1024,
            ((long long)st.f_bfree * (long long)st.f_bsize) / 1024,
            (int) st.f_bsize);
    }
    return ret;
}

int get_owner(char *s) {
    int ret = EXIT_SUCCESS;
    struct stat sf;

    if(lstat(s, &sf) < 0) {
        ret = EXIT_FAILURE;
    }
    else {
        char user[16];
        struct passwd *pw = getpwuid(sf.st_uid);
        if(pw) {
            strcpy(user, pw->pw_name);
        } else {
            sprintf(user, "%lu", sf.st_uid);
        }        
        // O,name
        printf("O,%s", user);
    }
    return ret;
}

int file_exists(char *s) {
    int ret = EXIT_SUCCESS;
    char exists;

    if(0 == access(s, F_OK)) {
        exists = 'y';
    }
    else {
        exists = 'n';
    }
    // E,exists
    printf("E,%c", exists);
    return ret;
}

// Private
int listdir_(char *s) {
    int ret = EXIT_SUCCESS;

    char tmp[4096];
    struct stat sf;

    DIR *d;
    if(0 == (d  = opendir(s))) {
        ret = EXIT_FAILURE;
    }
    else {
        struct dirent *entry;
        while(0 != (entry = readdir(d))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            snprintf(tmp, sizeof(tmp), "%s/%s", s, entry->d_name);
            if(lstat(tmp, &sf) < 0) {
                ret = EXIT_FAILURE;
                // TODO: Break or not?
            }
            else {
                char f_type = S_ISLNK(sf.st_mode) ? 'l' : S_ISDIR(sf.st_mode) ? 'd' : 'f';
                char f_exec = f_type != 'l' && sf.st_mode & S_IXUSR ? 'x' : '-';
                if(f_type == 'l') {
                    char dest[4096];
                    if(readlink(tmp, dest, 4096) < 0) {
                        ret = EXIT_FAILURE;
                    }
                    else {
                        if(strstr(dest, "/asec/") ||
                           strstr(dest, "/openfeint/")) {
                            f_type = 'L';
                        }
                    }
                }
                printf("%c,%c,%lld,%lld,%s\n", f_type, f_exec, sf.st_size, sf.st_blocks, entry->d_name);
            }
        }
        rewinddir(d);
        while(0 != (entry = readdir(d))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            snprintf(tmp, sizeof(tmp), "%s/%s", s, entry->d_name);
                if(lstat(tmp, &sf) < 0) {
                    ret = EXIT_FAILURE;
                    // TODO: Break or not?
                }
                else {
                    if (S_ISDIR(sf.st_mode)) {
                        printf("%s:\n", tmp);
                        listdir_(tmp);
                    }
                }
        }
        closedir(d);
    }

    return ret;
}

int du(char *s) {
    int ret = EXIT_SUCCESS;
    struct stat sf;

    if(lstat(s, &sf) < 0) {
        ret = EXIT_FAILURE;
    }
    else {
        printf("\n%s:\n", s);
        ret = listdir_(s);
    }

    return ret;
}

int list_links(char *s) {
    int ret = EXIT_SUCCESS;

    char tmp[4096];
    struct stat sf;

    DIR *d;
    if(0 == (d  = opendir(s))) {
        ret = EXIT_FAILURE;
    }
    else {
        struct dirent *entry;
        while(0 != (entry = readdir(d))) {
            snprintf(tmp, sizeof(tmp), "%s/%s", s, entry->d_name);
            if(lstat(tmp, &sf) < 0) {
                ret = EXIT_FAILURE;
                // TODO: Break or not?
            }
            else {
                if (S_ISLNK(sf.st_mode)) {
                    char dest[4096];
                    if(readlink(tmp, dest, 4096) < 0) {
                        ret = EXIT_FAILURE;
                    }
                    else {
                        printf("l,%s,%s\n", entry->d_name, dest);
                    }
                }
            }
        }
    }

    return ret;
}

// For some weird reason this is dead code..?
int mount_loop(char *s) {
    int ret = EXIT_SUCCESS;

    if(*s){}; // Keep compiler happy
    dev_t deviceid;
    int fMountPoint = -1, fDevFile = -1;
    const char *devfile = "/sdcard/data/noensp/bogus";
    const char *mountpoint = "/data/bogus";

    fMountPoint = open(mountpoint, O_RDWR);
    if(fMountPoint < 0) {
        printf("Error #1\n");
        ret = EXIT_FAILURE;
        goto mount_over;
    }
    char devname[128];
    sprintf(devname, "/dev/loop%d", 99);
    deviceid = makedev(7, 99);
    if(mknod(devname, S_IFBLK | 0644, deviceid) < 0) {
        printf("Error #2\n");
        ret = EXIT_FAILURE;
        goto mount_over;
    }
    fDevFile = open(devfile, O_RDWR);
    if(fDevFile < 0) {
        printf("Error #3\n");
        ret = EXIT_FAILURE;
        goto mount_over;
    }
    if(ioctl(fDevFile, 0x4C00, fMountPoint) < 0) {
        printf("Error #4\n");
        ret = EXIT_FAILURE;
        goto mount_over;
    }

mount_over:
    if(fDevFile > -1)
        close(fDevFile);
    if(fMountPoint > -1)
        close(fMountPoint);
    return ret;
}

// Private
int cpfile_(char *s, char *dest, char *filename, struct stat* sf) {
    int ret = EXIT_SUCCESS;

    char *srcpath = (char*)malloc(sizeof(char) * (strlen(s) + strlen(filename) + 2));
    snprintf(srcpath, sizeof(char)*(strlen(s) + strlen(filename) + 2), "%s/%s", s, filename);
    char *destpath = (char*)malloc(sizeof(char) * (strlen(dest) + strlen(filename) + 2));
    snprintf(destpath, sizeof(char)*(strlen(dest) + strlen(filename) + 2), "%s/%s", dest, filename);

    int src_fd = open(srcpath, O_RDONLY);
    if(-1 < src_fd) {
        int dest_fd = open(destpath, O_WRONLY | O_CREAT | O_TRUNC, sf->st_mode);
        if(-1 < dest_fd) {
            char buf[4096], *bufptr; // aligned on 4 bytes -- 8 if needed.
            int bufsize = sizeof(buf);
            int readcount = 0;
            int remcount = 0;
            int writecount = 0;
            do {
                do {
                    readcount = read(src_fd, buf, bufsize);
                } while(0 > readcount && errno == EINTR);
                if(0 < readcount) {
                    bufptr = buf;
                    remcount = readcount;
                    do {
                        do {
                            writecount = write(dest_fd, bufptr, remcount);
                        } while(0 > writecount && errno == EINTR);
                        if(0 < writecount) {
                            bufptr += writecount;
                            remcount -= writecount;
                        }
                        else {
                            ret = EXIT_FAILURE;
                        }
                    } while(0 < remcount && ret != EXIT_FAILURE);
                }
                else if(0 > readcount) {
                    ret = EXIT_FAILURE;
                }
            } while(0 < readcount && ret != EXIT_FAILURE);

            close(dest_fd);
        }
        else {
            ret = EXIT_FAILURE;
        }

        close(src_fd);
    }
    else {
        ret = EXIT_FAILURE;
    }

    if(ret != EXIT_FAILURE) {
        chown(destpath, sf->st_uid, sf->st_gid);
    }

    free(destpath);
    free(srcpath);

    return ret;
}

int cpdir_(char *s, char *dest) {
    int ret = EXIT_SUCCESS;

    struct stat sf;

    DIR *d;
    if(0 == (d  = opendir(s))) {
        ret = EXIT_FAILURE;
    }
    else {
        struct dirent *entry;
        while(ret != EXIT_FAILURE && 0 != (entry = readdir(d))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char *srcpath = (char*)malloc(sizeof(char) * (strlen(entry->d_name) + strlen(s) + 2));
            snprintf(srcpath, sizeof(char)*(strlen(entry->d_name) + strlen(s) + 2), "%s/%s", s, entry->d_name);
            if(lstat(srcpath, &sf) < 0) {
                ret = EXIT_FAILURE;
                // TODO: Break or not?
            }
            else {
                if(!S_ISDIR(sf.st_mode)) {
                    ret = cpfile_(s, dest, entry->d_name, &sf);
                }
            }
            free(srcpath);
        }
        if(ret != EXIT_FAILURE) {
            rewinddir(d);
            while(ret != EXIT_FAILURE && 0 != (entry = readdir(d))) {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
                char *srcpath = (char*)malloc(sizeof(char) * (strlen(entry->d_name) + strlen(s) + 2));
                snprintf(srcpath, sizeof(char)*(strlen(entry->d_name) + strlen(s) + 2), "%s/%s", s, entry->d_name);
                // Be sure to always use lstat ot we may end up in hairy situations
                if(lstat(srcpath, &sf) < 0) {
                    ret = EXIT_FAILURE;
                }
                else {
                    if (S_ISDIR(sf.st_mode)) {
                        char *destpath = (char*)malloc(sizeof(char) * (strlen(entry->d_name) + strlen(dest) + 2));
                        snprintf(destpath, sizeof(char)*(strlen(entry->d_name) + strlen(dest) + 2), "%s/%s", dest, entry->d_name);
                        if(0 == mkdir(destpath, sf.st_mode) || errno == EEXIST) {
                            chown(destpath, sf.st_uid, sf.st_gid);
                            ret = cpdir_(srcpath, destpath);
                        }
                        else {
                            ret = EXIT_FAILURE;
                        }
                        free(destpath);
                    }
                }
                free(srcpath);
            }
        }
        closedir(d);
    }

    return ret;
}

int recursive_cp(char *s, char *dest) {
    int ret = EXIT_SUCCESS;

    struct stat sf;

    if(lstat(s, &sf) < 0) {
        ret = EXIT_FAILURE;
    }
    else {
        char *dirname = strrchr(s, '/');
        if(dirname) {
            ++dirname;
        }
        else {
            dirname = s;
        }
        char *destpath = (char*)malloc(sizeof(char) * (strlen(dirname) + strlen(dest) + 2));
        snprintf(destpath, sizeof(char)*(strlen(dirname) + strlen(dest) + 2), "%s/%s", dest, dirname);
        if(0 == mkdir(destpath, sf.st_mode) || errno == EEXIST) {
            chown(destpath, sf.st_uid, sf.st_gid);
            ret = cpdir_(s, destpath);
        }
        else {
            ret = EXIT_FAILURE;
        }
        free(destpath);
    }

    if(ret == EXIT_FAILURE) {
        printf("#Error\n");
    }
    return ret;
}

// Private
int rmdir_(char *s) {
    int ret = EXIT_SUCCESS;

    struct stat sf;

    DIR *d;
    if(0 == (d  = opendir(s))) {
        ret = EXIT_FAILURE;
    }
    else {
        struct dirent *entry;
        while(ret != EXIT_FAILURE && 0 != (entry = readdir(d))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char *subpath = (char*)malloc(sizeof(char) * (strlen(entry->d_name) + strlen(s) + 2));
            snprintf(subpath, sizeof(char)*(strlen(entry->d_name) + strlen(s) + 2), "%s/%s", s, entry->d_name);
            if(lstat(subpath, &sf) < 0) {
                ret = EXIT_FAILURE;
            }
            else {
                if (S_ISDIR(sf.st_mode)) {
                    ret = rmdir_(subpath);
                    if(ret != EXIT_FAILURE) {
                        if(0 != rmdir(subpath)) {
                            ret = EXIT_FAILURE;
                        }
                    }
                }
            }
            free(subpath);
        }
        if(ret != EXIT_FAILURE) {
            rewinddir(d);
            while(ret != EXIT_FAILURE && 0 != (entry = readdir(d))) {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
                char *subpath = (char*)malloc(sizeof(char) * (strlen(entry->d_name) + strlen(s) + 2));
                snprintf(subpath, sizeof(char)*(strlen(entry->d_name) + strlen(s) + 2), "%s/%s", s, entry->d_name);
                if(lstat(subpath, &sf) < 0) {
                    ret = EXIT_FAILURE;
                }
                else {
                    if(!S_ISDIR(sf.st_mode)) {
                        if(0 != unlink(subpath)) {
                            ret = EXIT_FAILURE;
                        }
                    }
                }
                free(subpath);
            }
        }
        closedir(d);
    }

    return ret;
}

int recursive_remove(char *s) {
    int ret = EXIT_SUCCESS;
    struct stat sf;

    if(strlen(s) > strlen("/data/noensp/")) {
        if(lstat(s, &sf) < 0) {
            ret = EXIT_FAILURE;
        }
        else {
            ret = rmdir_(s);
            if(ret != EXIT_FAILURE) {
                if(0 != rmdir(s)) {
                    ret = EXIT_FAILURE;
                }
            }
        }
    }
    else {
        ret = EXIT_FAILURE;
    }
    
    if(ret == EXIT_FAILURE) {
        printf("#Error\n");
    }
    return ret;
}

// Private
unsigned int work_index = 0;
int fileop_(char *s, int curdepth, int parentindex, int (*cb)(char *, int, struct stat *)) {
/* If C Compiler:
int fileop_(char *s, int curdepth, int parentindex, int (*cb)()) {
*/
    int ret = EXIT_SUCCESS;

    struct stat sf;

    DIR *d;
    if(0 == (d  = opendir(s))) {
if(ret==EXIT_FAILURE) printf("CRAP#3\n");
        ret = EXIT_FAILURE;
    }
    else {
        -- curdepth;

        struct dirent *entry;
        while(ret != EXIT_FAILURE && 0 != (entry = readdir(d))) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char *subpath = (char*)malloc(sizeof(char) * (strlen(entry->d_name) + strlen(s) + 2));
            snprintf(subpath, sizeof(char)*(strlen(entry->d_name) + strlen(s) + 2), "%s/%s", s, entry->d_name);
            if(lstat(subpath, &sf) < 0) {
if(ret==EXIT_FAILURE) printf("CRAP#4\n");
                ret = EXIT_FAILURE;
            }
            else {
                if (S_ISDIR(sf.st_mode)) {
                    ++ work_index;
                    int my_index = work_index;
                    if(curdepth > 0) {
                        ret = fileop_(subpath, curdepth, my_index, cb);
                    }
                    if(ret != EXIT_FAILURE) {
                        int saved_index = work_index;
                        work_index = my_index;
                        if(EXIT_SUCCESS != cb(subpath, parentindex, &sf)) {
                            ret = EXIT_FAILURE;
                        }
                        work_index = saved_index;
                    }
                }
            }
            free(subpath);
        }
        if(ret != EXIT_FAILURE) {
            rewinddir(d);
            while(ret != EXIT_FAILURE && 0 != (entry = readdir(d))) {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
                char *subpath = (char*)malloc(sizeof(char) * (strlen(entry->d_name) + strlen(s) + 2));
                snprintf(subpath, sizeof(char)*(strlen(entry->d_name) + strlen(s) + 2), "%s/%s", s, entry->d_name);
                if(lstat(subpath, &sf) < 0) {
if(ret==EXIT_FAILURE) printf("CRAP#5\n");
                    ret = EXIT_FAILURE;
                }
                else {
                    if(!S_ISDIR(sf.st_mode)) {
                        ++ work_index;
                        if(0 != cb(subpath, parentindex, &sf)) {
if(ret==EXIT_FAILURE) printf("CRAP#6\n");
                            ret = EXIT_FAILURE;
                        }
                    }
                }
                free(subpath);
            }
        }
        closedir(d);
    }

    return ret;
}

unsigned int work_uid;

int recursive_chown_(char *path, int parentindex, struct stat *sf) {
    int ret = EXIT_SUCCESS;
    if(parentindex){}; // This function does not care about parentindex
    if(sf){}; // This function does not use isdir
    if(0 != lchown(path, work_uid, work_uid)) {
        ret = EXIT_FAILURE;
    }
    return ret;
}

int recursive_chown(char *s, int depth, char *uname) {
    int ret = EXIT_SUCCESS;
    struct stat sf;

    if(strlen(s) > strlen("/data/noensp/")) {
        if(lstat(s, &sf) < 0) {
            ret = EXIT_FAILURE;
        }
        else {
            work_uid = atoi(uname);
            if(0 == work_uid) {
                struct passwd *pwd = getpwnam(uname);
                if(pwd) {
                    work_uid = pwd->pw_uid;
                }
            }
            if(0 != work_uid) {
                ret = fileop_(s, depth, 0, recursive_chown_);
                if(ret != EXIT_FAILURE) {
                    if(0 != recursive_chown_(s, 0, 0)) {
                        ret = EXIT_FAILURE;
                    }
                }
            }
            else {
                ret = EXIT_FAILURE;
            }
        }
    }
    else {
        ret = EXIT_FAILURE;
    }
    
    if(ret == EXIT_FAILURE) {
        printf("#Error\n");
    }
    return ret;
}

int recursive_crawl_(char *path, int parentindex, struct stat *sf) {
    int ret = EXIT_SUCCESS;
    char f_type = S_ISLNK(sf->st_mode) ? 'l' : S_ISDIR(sf->st_mode) ? 'd' : 'f';
    char f_exec = f_type != 'l' && sf->st_mode & S_IXUSR ? 'x' : '-';
    printf("%u,%u,%c,%c,%lld,%lld,%s\n", work_index, parentindex, f_type, f_exec, sf->st_size, sf->st_blocks, path);
if(ret==EXIT_FAILURE) printf("CRAP#1\n");
    return ret;
}

int recursive_crawl(char *s) {
    int ret = EXIT_SUCCESS;
    struct stat sf;

    if(lstat(s, &sf) < 0) {
        ret = EXIT_FAILURE;
if(ret==EXIT_FAILURE) printf("CRAP#0\n");
    }
    else {
        work_index = 0;
        ret = fileop_(s, 99, 0, recursive_crawl_);
        if(ret != EXIT_FAILURE) {
//            if(0 != recursive_crawl_(true, s)) {
//                ret = EXIT_FAILURE;
//            }
        }
    }
    
    if(ret == EXIT_FAILURE) {
        printf("#Error\n");
    }
    return ret;
}

int read_file(char *s) {
    int ret = EXIT_SUCCESS;
    struct stat sf;

    if(lstat(s, &sf) < 0) {
        ret = EXIT_FAILURE;
    }
    else {
        FILE *f = fopen(s, "r");
        if(!f) {
            ret = EXIT_FAILURE;
        }
        else {
            int c;
            // content
            while((c = fgetc(f)) != EOF) {
                printf("%c", c);
            }
            fclose(f);
        }
    }
    return ret;
}

// Not implemented, not invoked
int mount_read_write(bool readwrite, char *dev, char *mountpoint) {
    int ret = EXIT_SUCCESS;

    if(readwrite && dev && mountpoint){};
    return ret;
}

int main(int argc, char **argv)
{
    int exit_code = EXIT_FAILURE;
    if(argc > 1) {
        if(!strcmp(argv[1], "df"))
            exit_code = df(argv[2]);
        else if(!strcmp(argv[1], "go"))
            exit_code = get_owner(argv[2]);
        else if(!strcmp(argv[1], "fe"))
            exit_code = file_exists(argv[2]);
        else if(!strcmp(argv[1], "du"))
            exit_code = du(argv[2]);
        else if(!strcmp(argv[1], "ll"))
            exit_code = list_links(argv[2]);
        else if(!strcmp(argv[1], "ml"))
            exit_code = mount_loop(argv[2]);
        else if(!strcmp(argv[1], "cp"))
            exit_code = recursive_cp(argv[2], argv[3]);
        else if(!strcmp(argv[1], "rm"))
            exit_code = recursive_remove(argv[2]);
        else if(!strcmp(argv[1], "co"))
            exit_code = recursive_chown(argv[2], atoi(argv[3]), argv[4]);
        else if(!strcmp(argv[1], "rf"))
            exit_code = read_file(argv[2]);
        else if(!strcmp(argv[1], "mr"))
            exit_code = mount_read_write(false, argv[2], argv[3]);
        else if(!strcmp(argv[1], "mw"))
            exit_code = mount_read_write(true, argv[2], argv[3]);
        else if(!strcmp(argv[1], "cr"))
            exit_code = recursive_crawl(argv[2]);
    }
    return exit_code;
}
