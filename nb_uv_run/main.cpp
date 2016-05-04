

//
//  main.cpp
//  nb_uv_run
//
//  Created by dyno on 5/1/16.
//  Copyright © 2016 dyno. All rights reserved.
//

#include "uv.h"
#include <iostream>
#include <unistd.h>
#include "Network.hpp"
#include "HttpRequest.hpp"

int64_t counter = 0;

void wait_for_a_while(uv_idle_t* handle) {
    counter++;
    
    if (counter >= 100000)
        uv_idle_stop(handle);
}

class Test {
public:
    void callback(HttpRequest::HTTP_RESULT* result){
        fprintf(stderr, "head:%s \n content:%s \n", result->header.c_str(), result->content.c_str());
    }
    
    void request(){
        http.setRequest("http://query.yahooapis.com/v1/public/yql?q=select%20*%20from%20yahoo.finance.xchange%20where%20pair%20in%20(%22USDCNY%22)&format=json&env=store%3A%2F%2Fdatatables.org%2Falltableswithkeys&callback=",
                           std::bind(&Test::callback, this, std::placeholders::_1));
        http.start();
    }
private:
    HttpRequest http;
};


int main(int argc, const char * argv[]) {
    // insert code here...
    /*
    uv_idle_t idler;
    
    uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, wait_for_a_while);
    
    printf("Idling...\n");
    
    while (true) {
        if (0 == uv_run(uv_default_loop(), UV_RUN_NOWAIT))
            break;
        printf("uv_run %lld ...\n", counter);
    }
    
    uv_loop_close(uv_default_loop());
    return 0;
     */

    AsyncMgr::instance().init();
    
    Test t;
    int w = 40;
    while (w>=0) {
        if (w==40){
            t.request();
        }
        AsyncMgr::instance().process();
        --w;
        usleep(1000*1000);
    }
    
    AsyncMgr::instance().close();
    
}
