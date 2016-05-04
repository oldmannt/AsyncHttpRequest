/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2016, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

/* <DESC>
 * multi_socket API using libuv
 * </DESC>
 */
/* Example application code using the multi socket interface to download
 multiple files at once, but instead of using curl_multi_perform and
 curl_multi_wait, which uses select(), we use libuv.
 It supports epoll, kqueue, etc. on unixes and fast IO completion ports on
 Windows, which means, it should be very fast on all platforms..
 
 Written by Clemens Gruber, based on an outdated example from uvbook and
 some tests from libuv.
 
 Requires libuv and (of course) libcurl.
 
 See http://nikhilm.github.com/uvbook/ for more information on libuv.
 */

#include <string>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include "libuv/include/uv.h"
#include <curl/curl.h>

uv_loop_t *loop;
CURLM *curl_handle;
uv_timer_t uvtimeout;
std::list<std::string*> headers;
std::list<std::string*> contents;

typedef struct curl_context_s {
    uv_poll_t poll_handle;
    curl_socket_t sockfd;
} curl_context_t;

curl_context_t* create_curl_context(curl_socket_t sockfd)
{
    curl_context_t *context;
    
    context = (curl_context_t *) malloc(sizeof *context);
    
    context->sockfd = sockfd;
    
    uv_poll_init_socket(loop, &context->poll_handle, sockfd);
    context->poll_handle.data = context;
    
    return context;
}

void curl_close_cb(uv_handle_t *handle)
{
    curl_context_t *context = (curl_context_t *) handle->data;
    free(context);
}

void destroy_curl_context(curl_context_t *context)
{
    uv_close((uv_handle_t *) &context->poll_handle, curl_close_cb);
}

size_t RetriveHeaderFunction(char *buffer, size_t size, size_t nitems, void *userdata)
{
    std::string* receive_header = reinterpret_cast<std::string*>(userdata);
    if (receive_header && buffer)
    {
        receive_header->append(reinterpret_cast<const char*>(buffer), size * nitems);
    }
    
    return nitems * size;
}

size_t RetriveContentFunction(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    std::string* receive_content = reinterpret_cast<std::string*>(userdata);
    if (receive_content && ptr)
    {
        receive_content->append(reinterpret_cast<const char*>(ptr), size * nmemb);
    }
    
    return nmemb * size;
}

void add_download(const char *url, int num)
{
    char filename[50];
    //FILE *file;
    CURL *handle;
    
    snprintf(filename, 50, "%d.download", num);

    /*
    file = fopen(filename, "wb");
    if(!file) {
        fprintf(stderr, "Error opening %s\n", filename);
        return;
    }*/
    
    std::string* pstrHeader = new std::string("");
    std::string* pstrContent = new std::string("content:");
    headers.push_back(pstrHeader);
    contents.push_back(pstrContent);
    
    handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, RetriveHeaderFunction);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, pstrHeader);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, RetriveContentFunction);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, pstrContent);
    curl_easy_setopt(handle, CURLOPT_PRIVATE, pstrContent);
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_multi_add_handle(curl_handle, handle);
    fprintf(stderr, "Added download %s -> %s\n", url, filename);
}

static void check_multi_info(void)
{
    int running_handles;
    char *done_url;
    CURLMsg *message;
    int pending;
    FILE *file;
    std::string* prive;
    
    while((message = curl_multi_info_read(curl_handle, &pending))) {
        switch(message->msg) {
            case CURLMSG_DONE:
                curl_easy_getinfo(message->easy_handle, CURLINFO_EFFECTIVE_URL,
                                  &done_url);
                curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &prive);
                printf("%s DONE\n", done_url);
                printf("%s \n", prive->c_str());
                
                curl_multi_remove_handle(curl_handle, message->easy_handle);
                curl_easy_cleanup(message->easy_handle);

                break;
                
            default:
                fprintf(stderr, "CURLMSG default\n");
                break;
        }
    }
}

void curl_perform(uv_poll_t *req, int status, int events)
{
    int running_handles;
    int flags = 0;
    curl_context_t *context;
    char *done_url;
    CURLMsg *message;
    int pending;
    
    uv_timer_stop(&uvtimeout);
    
    if(events & UV_READABLE)
        flags |= CURL_CSELECT_IN;
    if(events & UV_WRITABLE)
        flags |= CURL_CSELECT_OUT;
    
    context = (curl_context_t *) req;
    
    curl_multi_socket_action(curl_handle, context->sockfd, flags,
                             &running_handles);
    
    check_multi_info();
}

void on_timeout(uv_timer_t *req)
{
    int running_handles;
    curl_multi_socket_action(curl_handle, CURL_SOCKET_TIMEOUT, 0,
                             &running_handles);
    check_multi_info();
}

void start_timeout(CURLM *multi, long timeout_ms, void *userp)
{
    if(timeout_ms <= 0)
        timeout_ms = 1; /* 0 means directly call socket_action, but we'll do it in
                         a bit */
    uv_timer_start(&uvtimeout, on_timeout, timeout_ms, 0);
}

int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp,
                  void *socketp)
{
    curl_context_t *curl_context = 0;
    if(action == CURL_POLL_IN || action == CURL_POLL_OUT) {
        if(socketp) {
            curl_context = (curl_context_t *) socketp;
        }
        else {
            curl_context = create_curl_context(s);
        }
        curl_multi_assign(curl_handle, s, (void *) curl_context);
    }
    
    switch(action) {
        case CURL_POLL_IN:
            uv_poll_start(&curl_context->poll_handle, UV_READABLE, curl_perform);
            break;
        case CURL_POLL_OUT:
            uv_poll_start(&curl_context->poll_handle, UV_WRITABLE, curl_perform);
            break;
        case CURL_POLL_REMOVE:
            if(socketp) {
                uv_poll_stop(&((curl_context_t*)socketp)->poll_handle);
                destroy_curl_context((curl_context_t*) socketp);
                curl_multi_assign(curl_handle, s, NULL);
            }
            break;
        default:
            abort();
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    loop = uv_default_loop();
    
    
    if(curl_global_init(CURL_GLOBAL_ALL)) {
        fprintf(stderr, "Could not init cURL\n");
        return 1;
    }
    
    uv_timer_init(loop, &uvtimeout);
    
    curl_handle = curl_multi_init();
    curl_multi_setopt(curl_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);
    curl_multi_setopt(curl_handle, CURLMOPT_TIMERFUNCTION, start_timeout);
    
    //while(argc-- > 1) {
    add_download("http://query.yahooapis.com/v1/public/yql?q=select%20*%20from%20yahoo.finance.xchange%20where%20pair%20in%20(%22USDCNY%22)&format=json&env=store%3A%2F%2Fdatatables.org%2Falltableswithkeys&callback=", 1);
    add_download("http://example.com", 2);
    //}
    
    uv_run(loop, UV_RUN_DEFAULT);
    curl_multi_cleanup(curl_handle);
    
    return 0;
}