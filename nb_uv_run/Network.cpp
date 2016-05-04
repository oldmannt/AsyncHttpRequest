//
//  Network.cpp
//  nb_uv_run
//
//  Created by dyno on 5/1/16.
//  Copyright © 2016 dyno. All rights reserved.
//

#include <stdlib.h>

#include "Network.hpp"

void idel_cb(uv_idle_t* handle) {
    AsyncMgr::instance().idel();
}

bool AsyncMgr::init(){
    // add idle loop
    uv_idle_init(uv_default_loop(), &m_idler);
    uv_idle_start(&m_idler, idel_cb);

    return true;
}

void AsyncMgr::process(){
    ::uv_run(uv_default_loop(),UV_RUN_NOWAIT);
}

void AsyncMgr::close(){
    ::uv_stop(uv_default_loop());
}

void AsyncMgr::idel() {
    static uint64_t last = uv_now(uv_default_loop());
    uint64_t now = uv_now(uv_default_loop());
    fprintf(stderr, "idel:%llu \n",now - last);
    last = now;
}