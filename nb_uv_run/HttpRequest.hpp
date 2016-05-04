//
//  HttpRequest.hpp
//  nb_uv_run
//
//  Created by dyno on 5/3/16.
//  Copyright © 2016 dyno. All rights reserved.
//

#ifndef HttpRequest_hpp
#define HttpRequest_hpp

#include "uv.h"
#include "curl.h"
#include <string>
#include <functional>

class HttpRequest{
public:
    typedef enum {
        REQUEST_SYNC,
        REQUEST_ASYNC,
    }RequestType;
    
    typedef enum {
        REQUEST_OK =CURLE_OK,
    }RequestResult;
    
    struct HTTP_RESULT;
    typedef std::function<void(HTTP_RESULT*)> ResultCallback;
    
    struct HTTP_RESULT {
        HTTP_RESULT():response_code(0),result(false){}
        bool result;
        long response_code;
        std::string url;
        std::string header;
        std::string content;
        std::string err;
    };
    
    HttpRequest();
    ~HttpRequest();
    
    int setRequestUrl(const std::string& url);
    int setResultCallback(ResultCallback rc);
    int setRetryTimes(int retry_times);
    int setRequestTimeout(long time_out);
    int setRequest(const std::string& url, ResultCallback cb, unsigned long timeout = 60000,
                   int retry_times = 0);
    
    int setPostData(const std::string& message);
    int setPostData(const void* data, unsigned long size);
    int setRequestProxy(const std::string& proxy, int proxy_port);
    
    int start();
    
    void handleResult(CURLMsg* message);
    size_t writeHeader(char* data, unsigned long size);
    size_t writeContent(char* data, unsigned long size);
    void cleanUP();
   
private:
    int Retry();
    unsigned long m_timeout;
    int m_retry_times;
    ResultCallback m_callback;
    std::string m_proxy;
    int m_port;
    HTTP_RESULT m_result;
    char*  m_post_data;
    CURL* m_handle;
};

class HttpRequestMgr{
public:
    HttpRequestMgr():m_curlm_handle(0),m_init(false){};
    
    static HttpRequestMgr& instance(){
        static HttpRequestMgr s_instance;
        return s_instance;
    }
    
    bool init();
    uv_timer_t* getUVTimer(){return &m_httpuvtimeout;}
    CURLM* getCurlm(){ return m_curlm_handle;}
    
private:
    bool m_init;
    CURLM * m_curlm_handle;
    uv_timer_t m_httpuvtimeout;
};


#endif /* HttpRequest_hpp */
