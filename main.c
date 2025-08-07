#include <ctype.h>
#include <ctype.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include "dep.h"

#define MAX_CONNECTIONS 5

struct Memory {
    char *data;
    size_t size;
};

struct SpeedContext {
    curl_off_t last_dl;
    struct timespec last_time;
};

struct myprogress {
  curl_off_t lastruntime; 
  CURL *curl;
};

typedef struct {
    curl_off_t dltotal;
    curl_off_t dlnow;
    int done;
} ProgressInfo;

typedef struct page_options{
    char* page_url;
    char* title; 
} popts;

typedef struct {
    int fd;           // File descriptor
    long offset;      // Offset to write
    long written;     // How many bytes already written
} rnghldr;


void checkUnrar(void){
    if (system("which rar > /dev/null 2>&1") != 0) {
    fprintf(stderr, "rar not found in PATH. Please install it.\n");
    exit(1);
    }
    system("mkdir -p ./output");  
}

size_t fileSizeCallback(char *contents, size_t size, size_t nmemb, void *userp){
    size_t total_size = size*nmemb;
    char* sizeout = (char*)userp;
    if(strncasecmp(contents, "content-range:", 14) == 0){
        size_t len = strlen(&contents[12]);
        char** temp = split(&contents[12], &len, "/");
        strcpy(sizeout,temp[1]);
        free(temp[0]);
        free(temp[1]);
        free(temp);
    }
    return total_size;
}

char* getFileSize(char* _link){
    CURL* curl; 
    CURLcode res;
    char* result = (char*) malloc(128);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, _link);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_RANGE, "-1");
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, fileSizeCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, result);
    res = curl_easy_perform(curl);
    if(res != CURLE_OK){
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        exit(1);
    }
    curl_easy_cleanup(curl);
    return result;
}

FILE* reserveSpace(char* size){
    FILE* fp = fopen("./output/bin.rar", "wb+");
    if (fseek(fp, atoll(size) - 1, SEEK_SET) != 0){
        printf("Error reserving space.....Exiting!");
        exit(1);
    }

    if (fputc('\0', fp) == EOF) {
        perror("fputc");
        fclose(fp);
        exit(1);
    }

    rewind(fp);
    return fp;
}

int extractFiles(void){
    return system("rar x ./output/bin.rar ./output/ -idq");
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;

    char *ptr = realloc(mem->data, mem->size + real_size + 1);
    if (ptr == NULL) {
        // Out of memory!
        fprintf(stderr, "realloc() failed\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->data[mem->size] = '\0';

    return real_size;
}

size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t total_size = size * nitems;
    char *redirect_out = (char *)userdata;

    if (strncasecmp(buffer, "hx-redirect:", 12) == 0) {
        const char *start = buffer + 12;
        while (*start == ' ' || *start == '\t') start++;

        const char *end = buffer + total_size;
        while (end > start && (end[-1] == '\n' || end[-1] == '\r')) --end;

        size_t len = end - start;
        if (len >= 2048) len = 2048 - 1;
        strncpy(redirect_out, start, len);
        redirect_out[len] = '\0';
    }

    return total_size;
}

double seconds_since(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
}
 
static int xferinfo_callback(void *p, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow) {
    ProgressInfo *info = (ProgressInfo*) p;
    info->dltotal = dltotal;
    info->dlnow = dlnow;
    return 0;
}

static int write_to_file(void* ptr, size_t size, size_t nmemb, long *userdata){
 rnghldr *offset = (rnghldr *)userdata;
    size_t total = size * nmemb;
    ssize_t written = pwrite(offset->fd, ptr, total, offset->offset + offset->written);
    if (written < 0) return 0;
    offset->written += written;
    return written;}


void gameDownload(char* _link,char* size_str, FILE* fp){
    CURLM *multi;
    CURL *handles[MAX_CONNECTIONS];
    rnghldr offsets[MAX_CONNECTIONS];
    ProgressInfo progress_data[MAX_CONNECTIONS];
    long size = atol(size_str);
    long range = size / MAX_CONNECTIONS;
    int still_running;
    int fd = fileno(fp);

    curl_global_init(CURL_GLOBAL_ALL);
    multi = curl_multi_init();

    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        handles[i] = curl_easy_init();
        long range0 = range * i;
        long range1 = (i == MAX_CONNECTIONS - 1) ? size - 1 : (range * (i + 1)) - 1;

        char range_header[64];
        sprintf(range_header, "%ld-%ld", range0, range1);

        offsets[i].fd = fd;
        offsets[i].offset = range0;
        offsets[i].written = 0;

        curl_easy_setopt(handles[i], CURLOPT_URL, _link);
        curl_easy_setopt(handles[i], CURLOPT_RANGE, range_header);
        curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, write_to_file);
        curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, &offsets[i]);
        curl_easy_setopt(handles[i], CURLOPT_XFERINFOFUNCTION, xferinfo_callback);
        curl_easy_setopt(handles[i], CURLOPT_XFERINFODATA, &progress_data[i]);
        curl_easy_setopt(handles[i], CURLOPT_NOPROGRESS, 0L);
        curl_multi_add_handle(multi, handles[i]);
    }

    double last_size = 0;

    curl_multi_perform(multi, &still_running);
    do {
        curl_multi_perform(multi, &still_running);

        // Print combined progress every 0.5s
        static time_t last_print = 0;
        time_t now = time(NULL);
        if (now != last_print) {
            last_print = now;

            curl_off_t total_now = 0, total_total = 0;
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                total_now += progress_data[i].dlnow;
                total_total += progress_data[i].dltotal;
            }

            double percent = total_total > 0 ? (double)total_now / total_total * 100.0 : 0.0;
            printf("\rDownloading... %.2f%% (%.2f / %.2f MB)\t**DL Speed %.2f MB/s",
                percent,
                total_now / (1024.0 * 1024),
                total_total / (1024.0 * 1024), (total_now - last_size)/(1024*1024));
            fflush(stdout);
            last_size = total_now;
        }

        // Optionally wait a bit
        curl_multi_poll(multi, NULL, 0, 100, NULL);
    } while (still_running);

    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        curl_multi_remove_handle(multi, handles[i]);
        curl_easy_cleanup(handles[i]);
    }
    curl_multi_cleanup(multi);
    curl_global_cleanup();

}

char* getGameLink(char* _link){
    CURL *curl; 
    CURLcode res;
    char url[1024];
    char referrer[1024];
    sprintf(referrer, "https:%s",_link);
    sprintf(url, "https:%s/download", _link);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    char* result = (char *) malloc(2048);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, result);
    curl_easy_setopt(curl, CURLOPT_REFERER, referrer);
    res = curl_easy_perform(curl);
    if(res != CURLE_OK){
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }
    curl_easy_cleanup(curl);
    return result;
}

char* getSearchWebpage(char* query){
    CURL* curl;
    CURLcode res;
    char url[1024];
    sprintf(url, "https://steamrip.com/?s=%s", query);

    struct Memory chunk;
    chunk.data = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.data);
        chunk.data = NULL;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return chunk.data;
}

char* getGameWebPage(char* _id){
    CURL* curl;
    CURLcode res;
    char url[1024];
    sprintf(url, "https://steamrip.com/%s", _id);

    struct Memory chunk;
    chunk.data = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.data);
        chunk.data = NULL;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return chunk.data;
}

void displayGameReq(char* sys_req){
    printf("\nSYSTEM REQUIREMENTS:\n");
    int cont_index = 0, last = 0;
    size_t len = strlen(sys_req);
    char** new_data = split(sys_req, &len, "</li>");
    size_t length = strlen(new_data[0]);
    char** temp = split(new_data[0], &length, "</strong>");
    printf("%s %s\n",&temp[0][8], temp[1]);
    free(temp[0]); free(temp[1]); free(temp);
    for (int i = 1; i < len-1; i++){
        length = strlen(new_data[i]);
        temp = split(&new_data[i][4], &length, "</strong>");
        if (i == len-2){
            temp[1][strlen(temp[1]) - 6] = '\0';
            printf("%s %s\n", &temp[0][8], temp[1]);
        }
        else {
            printf("%s %s\n", &temp[0][8], temp[1]);
        }
        free(temp[0]); free(temp[1]);  free(temp);
    }
}

void displayGameInfo(char* game_info){ 
    size_t len, last; 
    printf("\nGAME INFO:\n");
    char* clean_game_info = slice(game_info, find(game_info, "<li>"), strlen(game_info));
    len = strlen(clean_game_info);
    char** temp = split(clean_game_info, &len, "<li>");
    free(temp[0]);
    for (int i = 1; i < len; i++){
        char** out = split(&temp[i][8], &last, "</strong>");
        out[1][strlen(out[1]) - 5] = '\0';
        if (i == len-1){
            out[1][strlen(out[1]) - 7] = '\0';
        }
        printf("%s %s\n", out[0], out[1]);
        free(out[0]), free(out[1]), free(out), free(temp[i]);
    }
    free(clean_game_info);
    free(temp);

}

char* extractLinks(char* links){
    if (!count(links, "Buzzheavier")){
        printf("No Buzzheavier links available. Exiting!");
        exit(1);
    }
    int cont = find(links, "Buzzheavier");
    char* out = slice(links, cont + 35, find(&links[cont], "\" target") + cont);
    // printf("%s", out);
    return out;
}

int main(){
    checkUnrar();
    char* query = (char*) malloc(2048);
    printf("Enter the game name you want to search: ");
    fgets(query, 2048, stdin);
    while (1){
        strip(query);
        if (!*query){
            printf("Enter a valid query: ");
            fgets(query, 2048, stdin);
        }
        else{
            break;
        }
    }
    size_t len = strlen(query);
    char** d = split(query, &len, " ");
    char* fina = join(d, len, "+");
    char* data = getSearchWebpage(fina);
    free(fina), free(query);
    for (int i = 0; i < len; i++){
        free(d[i]);
    }
    free(d);
    len = strlen(data);
    char** temp = split(data, &len, "\n");
    free (data);
    data = strdup(temp[42]);
    for (int i = 0; i<len; i++){
        free(temp[i]);
    }
    free(temp);
    popts parsed[5];
    int total = 0, cont_index = 0, last = 0;
    size_t cout = count(data, "<div class=\"slide lazyload\"");
    if (cout > 5) {
        cout = 5;
    }
    if (cout < 1){
        printf("Query didn't return any results. Try again.\n\n");
        fflush(stdout);
        main();
    }
    while (total < cout){
        cont_index = find(&data[last], "<div class=\"slide lazyload\"") + last;
        if (cont_index >= strlen(data)){
            break;
        }
        char* result = slice(data, cont_index, find(&data[cont_index], "</h2>")+cont_index);
        last = cont_index+1;
        char* pars_search = slice(result, find(result, "<h2 class=\"thumb-title\">")+33, strlen(result)-4);
        len = strlen(pars_search);
        char** final = split(pars_search, &len, ">");
        final[0][strlen(final[0])-1] = '\0';
        parsed[total].page_url = final[0]; parsed[total].title = final[1];
        printf("%d. %s\n", total+1 ,parsed[total].title);
        total++;
    }
    printf("Enter the game to download: ");
    char* choice = (char*) malloc(8);
    fgets(choice, 8, stdin);
    while (1){
        if (isdigit(*choice) && 1 <= atoi(choice) && atoi(choice) <= 5){break;}
        else{
            printf("Enter a valid query: ");
            fgets(choice, 8, stdin);
        }
    }
    size_t choi = atoi(choice)-1;
    char* raw = getGameWebPage(parsed[choi].page_url);    
    size_t len_n = strlen(raw);
    char** intermediate_page = split(raw, &len_n, "\n");
    free(raw);
    char* webpage = strdup(intermediate_page[42]);
    for (int i = 0; i < len_n; i++){
        free(intermediate_page[i]);
    }
    free(intermediate_page);
    size_t index = find(webpage, "SYSTEM REQUIREMENTS");
    size_t st_point = find(&webpage[index], "</li> ")+index+6;
    size_t end_point = find(&webpage[st_point+1], "<div id=\"post-extra-info\">")+st_point;
    char* slic = slice(webpage, st_point, end_point);
    len_n = strlen(slic);
    char** new_data = split(slic, &len_n, "</ul>");
    free(slic);
    char* game_info = new_data[1], *sys_req = strdup(&new_data[0][4]), *links = new_data[2];
    free(new_data[0]); free(new_data);
    char* clean_game_info; 
    clean_game_info = slice(game_info, find(game_info, "<li>"), strlen(game_info));
    strip(clean_game_info), strip(sys_req), strip(links);
    displayGameReq(sys_req);
    displayGameInfo(game_info);
    free(sys_req); free(game_info);
    // printf("%s",links);
    char* buzz = extractLinks(links);
    char* n = getGameLink(buzz);
    char* length = getFileSize(n);
    FILE* file = reserveSpace(length);
    printf("\nStarting Download...\n");
    gameDownload(n,length, file);
    printf("\033[2K\r");
    printf("Extracting.....");
    fflush(stdout);
    int status = extractFiles();
    if (!status){
        printf("\nFiles extracted successfully.");
    }
    else{
        printf("\nError extracting files. Please remove output folder and try again.");
    }
    fclose(file);
}