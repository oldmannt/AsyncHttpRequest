# AsyncHttpRequest

libuv and libcurl, c++ wrap

I consider to use libevent2, libev, libuv, at last I choose libuv, libev is better than libevent2, libev is good in Unix, but not perfect in windows, The Windows equivalent of kernel event notification mechanisms like kqueue or (e)poll is IOCP. libuv was an abstraction around libev or IOCP depending on the platform.