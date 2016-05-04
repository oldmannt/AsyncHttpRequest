//
//  HttpRequest.cpp
//  nb_uv_run
//
//  Created by dyno on 5/3/16.
//  Copyright © 2016 dyno. All rights reserved.
//

#include "HttpRequest.hpp"
#include <stdlib.h>

typedef struct curl_context_s {
    uv_poll_t poll_handle;
    curl_socket_t sockfd;
} curl_context_t;

curl_context_t* create_curl_context(curl_socket_t sockfd)
{
    curl_context_t *context;
    
    context = (curl_context_t *) malloc(sizeof *context);
    
    context->sockfd = sockfd;
    
    uv_poll_init_socket(uv_default_loop(), &context->poll_handle, sockfd);
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

static void check_multi_info(void);
void curl_perform(uv_poll_t *req, int status, int events)
{
    int running_handles;
    int flags = 0;
    curl_context_t *context;
    //    char *done_url;
    //    CURLMsg *message;
    //    int pending;
    
    uv_timer_stop(HttpRequestMgr::instance().getUVTimer());
    
    if(events & UV_READABLE)
        flags |= CURL_CSELECT_IN;
    if(events & UV_WRITABLE)
        flags |= CURL_CSELECT_OUT;
    
    context = (curl_context_t *) req;
    
    curl_multi_socket_action(HttpRequestMgr::instance().getCurlm(), context->sockfd, flags,
                             &running_handles);
    
    check_multi_info();
}

void on_timeout(uv_timer_t *req)
{
    int running_handles;
    curl_multi_socket_action(HttpRequestMgr::instance().getCurlm(), CURL_SOCKET_TIMEOUT, 0,
                             &running_handles);
    check_multi_info();
}

void start_timeout(CURLM *multi, long timeout_ms, void *userp)
{
    if(timeout_ms <= 0)
        timeout_ms = 1; /* 0 means directly call socket_action, but we'll do it in
                         a bit */
    uv_timer_start(HttpRequestMgr::instance().getUVTimer(), on_timeout, timeout_ms, 0);
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
        curl_multi_assign(HttpRequestMgr::instance().getCurlm(), s, (void *) curl_context);
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
                curl_multi_assign(HttpRequestMgr::instance().getCurlm(), s, NULL);
            }
            break;
        default:
            abort();
    }
    
    return 0;
}

bool HttpRequestMgr::init(){
    if (m_init)
        return true;
    
    if(curl_global_init(CURL_GLOBAL_ALL)) {
        fprintf(stderr, "Could not init cURL\n");
        return false;
    }
    
    uv_timer_init(uv_default_loop(), &m_httpuvtimeout);
    m_curlm_handle = curl_multi_init();
    if (!m_curlm_handle){
        fprintf(stderr, "curl_multi_init failed ");
        return false;
    }
    CURLMcode curl_code = curl_multi_setopt(m_curlm_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);
    if (curl_code!=CURLM_OK){
        fprintf(stderr, "CURLMOPT_SOCKETFUNCTION failed CURLMcode:%d %s",
                curl_code, curl_multi_strerror(curl_code));
        return false;
    }
    curl_code = curl_multi_setopt(m_curlm_handle, CURLMOPT_TIMERFUNCTION, start_timeout);
    if (curl_code!=CURLM_OK){
        fprintf(stderr, "CURLMOPT_TIMERFUNCTION failed CURLMcode:%d %s",
                curl_code, curl_multi_strerror(curl_code));
        return false;
    }
    return true;
}

static void check_multi_info(void)
{
    CURLMsg *message;
    int pending;
    HttpRequest* http_request = 0;
    
    while((message = curl_multi_info_read(HttpRequestMgr::instance().getCurlm(), &pending))) {
        switch(message->msg) {
            case CURLMSG_DONE:{
                //message->data.result
                CURLcode curl_code = curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &http_request);
                if (curl_code!=CURLE_OK){
                    fprintf(stderr, "CURLINFO_PRIVATE failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
                }
                if (http_request)
                    http_request->handleResult(message);
                
                curl_multi_remove_handle(HttpRequestMgr::instance().getCurlm(), message->easy_handle);
                curl_easy_cleanup(message->easy_handle);
            }
                break;
                
            default:
                fprintf(stderr, "CURLMSG default\n");
                break;
        }
    }
}

size_t RetriveHeaderFunction(char *buffer, size_t size, size_t nmemb, void *userdata)
{
    std::string* receive_header = reinterpret_cast<std::string*>(userdata);
    if (receive_header && buffer)
    {
        receive_header->append(reinterpret_cast<const char*>(buffer), size * nmemb);
    }
    
    return nmemb * size;
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

void HttpRequest::cleanUP(){
    if (m_handle == 0)
        return;
    curl_multi_remove_handle(HttpRequestMgr::instance().getCurlm(), m_handle);
    curl_easy_cleanup(m_handle);
    m_handle = 0;
}

size_t HttpRequest::writeHeader(char* data, unsigned long size){
    if (data)
        m_result.header.append(data,size);
    return size;
}

size_t HttpRequest::writeContent(char* data, unsigned long size){
    if (data)
        m_result.content.append(data, size);
    return size;
}

void HttpRequest::handleResult(CURLMsg* message){
    if (!message){
        return;
    }
    
    CURLcode curl_code = curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &m_result.response_code);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLINFO_RESPONSE_CODE failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
    }
    
    if (message->data.result == CURLE_OK && m_result.response_code == 200) {
        m_result.result = true;
    }
    else {
        const char* err_string = curl_easy_strerror(message->data.result);
        m_result.err = err_string;
        m_result.result = false;
        
        if (this->Retry()>0)
            return;
    }
    
    if (m_callback){
        m_callback(&m_result);
    }
}

int HttpRequest::Retry(){
    if (m_retry_times <= 0) {
        return 0;
    }
    
    m_result.header = "";
    m_result.content = "";
    m_result.err = "";
    
    this->start();
    return m_retry_times--;
}

HttpRequest::HttpRequest():m_handle(0),m_timeout(0),m_retry_times(0),
                        m_port(0),m_callback(0),m_post_data(0)
{
    HttpRequestMgr::instance().init();
    m_handle = curl_easy_init();
    
    CURLcode curl_code = curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, RetriveHeaderFunction);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_HEADERFUNCTION failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
    }

    curl_code = curl_easy_setopt(m_handle, CURLOPT_HEADERDATA, &m_result.header);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_HEADERDATA failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
    }
    
    curl_code = curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, RetriveContentFunction);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_WRITEFUNCTION failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
    }
    
    curl_code = curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, &m_result.content);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_WRITEDATA failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
    }
    
    curl_code = curl_easy_setopt(m_handle, CURLOPT_PRIVATE, this);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_PRIVATE failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
    }
};

HttpRequest::~HttpRequest(){
    this->cleanUP();
    delete m_post_data;
}

int HttpRequest::setRequestUrl(const std::string& url){
    m_result.url=url;
    CURLcode curl_code = curl_easy_setopt(m_handle, CURLOPT_URL, url.c_str());
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_URL failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
    }
    return curl_code;
}

int HttpRequest::setResultCallback(HttpRequest::ResultCallback rc){
    m_callback=rc;
    return 0;
}

int HttpRequest::setRetryTimes(int retry_times){
    m_retry_times=retry_times;
    return 0;
}

int HttpRequest::setRequestTimeout(long timeout){
    m_timeout=timeout;
    CURLcode curl_code = curl_easy_setopt(m_handle, CURLOPT_TIMEOUT_MS, timeout);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_TIMEOUT_MS failed CURLE:%d %s",
                curl_code,curl_easy_strerror(curl_code));
    }
    return curl_code;
}

int HttpRequest::setRequest(const std::string& url, ResultCallback cb,
                            unsigned long timeout /*= 60000*/, int retry_times /*= 0*/){
    int rt = this->setRequestUrl(url);
    if (rt != REQUEST_OK) {
        return rt;
    }
    
    rt = this->setRequestTimeout(timeout);
    if (rt != REQUEST_OK) {
        return rt;
    }
    
    rt = this->setRetryTimes(retry_times);
    if (rt != REQUEST_OK) {
        return rt;
    }
    
    this->setResultCallback(cb);
    return REQUEST_OK;
}

int HttpRequest::setPostData(const std::string& message){
    return this->setPostData(message.c_str(), message.size());
}

int HttpRequest::setPostData(const void* data, unsigned long size){
    if (0==data){
        return -1;
    }
    
    CURLcode curl_code = curl_easy_setopt(m_handle, CURLOPT_POST, 1);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_POST failed CURLE:%d %s",curl_code,curl_easy_strerror(curl_code));
        return curl_code;
    }
    

    delete m_post_data;
    
    if (size == 0) {
        curl_code = curl_easy_setopt(m_handle, CURLOPT_POSTFIELDS, "");
    }
    else{
        m_post_data = new char[size];
        memcpy(m_post_data, data, size);
        curl_code = curl_easy_setopt(m_handle, CURLOPT_POSTFIELDS, m_post_data);
        
        if (curl_code!=CURLE_OK){
            fprintf(stderr, "CURLOPT_POSTFIELDS failed CURLE:%d %s",
                    curl_code,curl_easy_strerror(curl_code));
            return curl_code;
        }
    }

    
    curl_code = curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE, size);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "CURLOPT_POSTFIELDSIZE failed CURLE:%d %s",
                curl_code,curl_easy_strerror(curl_code));
    }
    
    return curl_code;
}

int HttpRequest::setRequestProxy(const std::string& proxy, int proxy_port){
    CURLcode curl_code = curl_easy_setopt(m_handle, CURLOPT_PROXYPORT, proxy_port);
    if (curl_code!=CURLE_OK){
        fprintf(stderr, "failed %s %d curl_code:%d %s",
                proxy.c_str(), proxy_port, curl_code, curl_easy_strerror(curl_code));
        return curl_code;
    }
    curl_code = curl_easy_setopt(m_handle, CURLOPT_PROXY, proxy.c_str());
    return curl_code;

}

int HttpRequest::start(){
    CURLMcode curl_code = curl_multi_add_handle(HttpRequestMgr::instance().getCurlm(), m_handle);
    if (curl_code!=CURLM_OK){
        fprintf(stderr, "CURLMcode:%d %s", curl_code, curl_multi_strerror(curl_code));
    }
    return curl_code;
}
