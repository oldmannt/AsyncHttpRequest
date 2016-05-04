//
//  Network.hpp
//  nb_uv_run
//
//  Created by dyno on 5/1/16.
//  Copyright © 2016 dyno. All rights reserved.
//

#ifndef Network_hpp
#define Network_hpp

#include "uv.h"
#include "curl.h"
#include <string>
#include <functional>
#include <map>

enum {
    ASYNC_TYPE_NONE = 0,
    ASYNC_TYPE_HTTP = 1,
};

struct ASYNC_RT;

typedef std::function<void(int, ASYNC_RT*)> ResultCallback;

struct ASYNC_RT {
    virtual int getType() const =0;
    virtual ~ASYNC_RT(){}
    ASYNC_RT():id(0),result(false),callback(0){}
    int id;
    bool result;
    ResultCallback callback;
};

/*struct ASYNC_RT_TIMER:public ASYNC_RT{
    ASYNC_RT_TIMER():ASYNC_RT(),elapse(0.0f),period(0.0f),repead_count(0),rest_count(0){}
    float elapse;
    float period;
    int repead_count;
    int rest_count;
};*/

struct ASYNC_RT_HTTP:public ASYNC_RT {
    ASYNC_RT_HTTP():ASYNC_RT(),response_code(0){}
    long response_code;
    std::string url;
    std::string header;
    std::string content;
    std::string err;
};

class AsyncMgr {
public:
    
    // interface
    bool init();
    void process();
    void close();
    bool httpGet(int id, const char* url, ResultCallback objserver);
    bool addTimer(int id, float second, ResultCallback objserver);
    void addObserver(int id, ResultCallback objserver);
    void removeObserver(int id);
    const char* getLastErr() const;
    
private:
    
    typedef std::map<int, ASYNC_RT*> MAP_ASYNC_DATA;
    
    struct ASYNC_DATA_HTTP : public ASYNC_RT_HTTP {
        ASYNC_DATA_HTTP():ASYNC_RT_HTTP(),handle(0){}
        int getType() const{return ASYNC_TYPE_HTTP;}

        CURL* handle;
    };
    
    /*struct ASYNC_DATA_TIMER: public ASYNC_RT_TIMER {
        int getType() const{return ASYNC_TYPE_TIMER;}
        ASYNC_DATA_TIMER():ASYNC_RT_TIMER(){}
        uv_timer_t timer;
    };*/
    
public:
    AsyncMgr(){}
    virtual ~AsyncMgr(){}
    
    static AsyncMgr& instance() {
        static AsyncMgr s_async;
        return s_async;
    }
    
    CURLM* getCurlm(){ return m_curlm_handle;}
    uv_timer_t* getHttpUVTimer(){return &m_httpuvtimeout;}
    void idel();
    
    void handleHttpResult(CURLMsg* message);
    
private:
    void handleData(ASYNC_RT* data);
    
    std::string m_lasterr;
    uv_idle_t m_idler;
    uv_timer_t m_httpuvtimeout;
    CURLM * m_curlm_handle;
    MAP_ASYNC_DATA m_mapObjservers;
};

#endif /* Network_hpp */
