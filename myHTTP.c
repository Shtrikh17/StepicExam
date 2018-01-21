//
// Created by shtrikh17 on 21.01.18.
//

#include "myHTTP.h"

HTTP_request* parseRequest(char* request){
    int N, nHeadres;
    char** head_and_body = split_c_string(request, "\r\n\r\n", &N);
    if(N<1)
        return NULL;
    HTTP_request* http_request = (HTTP_request*) malloc (sizeof(HTTP_request));
    if(N>1)
        http_request->body = head_and_body[1];
    else
        http_request->body = NULL;

    char** headers = split_c_string(head_and_body[0], "\r\n", &nHeadres);
    char** reqType = split_c_string(headers[0], " ", &N);
    http_request->method = reqType[0];
    http_request->protocol = reqType[2];
    http_request->path = split_c_string(reqType[1], "?", &N)[0];

    http_request->nHeaders = nHeadres-1;
    http_request->headers = (HTTP_header*) malloc ((nHeadres-1)*sizeof(HTTP_header));
    for(int j=1; j<nHeadres; j++){
        char** header = split_c_string(headers[j], ": ", &N);
        http_request->headers[j-1].key = header[0];
        http_request->headers[j-1].value = header[1];
        free(header);
    }

    free(headers);
    free(reqType);

    return http_request;
}

int handle_HTTP_request(char* req, char** res, char* rootDir){

    HTTP_request* request = parseRequest(req);
    HTTP_response* response = (HTTP_response*) malloc (sizeof(HTTP_response));
    response->protocol = "HTTP/1.0";
    response->headers = (HTTP_header*) malloc (50*sizeof(HTTP_header));
    response->nHeadres = 0;

    response->headers[response->nHeadres].key = "Content-Type";
    response->headers[response->nHeadres].value = "text/html";
    response->nHeadres++;

    if(request->path!=NULL){
        char* fullPath = (char*) malloc (strlen(request->path)+strlen(rootDir)+1);
        strcpy(fullPath, rootDir);
        strcat(fullPath, request->path);
        // Проверка на существование файла - F_OK
        if(access(fullPath, F_OK)==0){

            FILE* f = fopen(fullPath, "r");
            // Запрещенный к открытию файл не откроется
            if(f==NULL){
                response->code = 403;
                response->description = "FORBIDDEN";
            }
            else{
                fseek(f, 0, SEEK_END);
                long fileSize = ftell(f);
                fseek(f, 0, SEEK_SET);
                response->body = (char*) malloc (fileSize+1);



                // Упрощенный подход - лучше не читать файл в память, это слишком замедляет сервер
                fread(response->body, 1, fileSize, f);
                response->body[fileSize] = '\0';
                fclose(f);

                response->headers[response->nHeadres].key = "Content-Length";
                char buf[20];
                int N = sprintf(buf, "%d", fileSize)+1;
                response->headers[response->nHeadres].value = (char*) malloc (N*sizeof(char));
                strcpy(response->headers[response->nHeadres].value, buf);
                response->nHeadres++;

                response->code = 200;
                response->description = "OK";
            }
        }
        else{
            response->description = "NOT FOUND";
            response->code = 404;
        }
    }
    else{
        response->code = 400;
        response->description = "BAD REQUEST";
    }

    // Размер стоит подсчитать, для тестовой реализации - упрощенный вариант
    ssize_t resultSize = 4096;
    *res = (char*) malloc (4096*sizeof(char));
    memset(*res, '\0', resultSize);
    char* cur = *res;
    sprintf(cur, "%s %d %s\r\n", response->protocol, response->code, response->description);
    cur += strlen(cur);
    for(int j=0; j<response->nHeadres; j++){
        sprintf(cur, "%s: %s\r\n", response->headers[j].key, response->headers[j].value);
        cur += strlen(cur);
    }



    // Вообще говоря, неверно - в файле могут находиться \0, так что копировать нужно все до конца файла побайтово
    if(response->body!=NULL){
        sprintf(cur, "\r\n");
        cur += strlen(cur);
        sprintf(cur, "%s", response->body);
        cur += strlen(cur);
    }
    else{
        sprintf(cur, "Content-Length: 1\r\n\r\n");
        cur += strlen(cur);
        sprintf(cur, "a");
        cur += 1;
    }
        return 0;
}