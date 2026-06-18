#ifndef SSDBUS_DBUS_EVENT_LOOP_HPP
#define SSDBUS_DBUS_EVENT_LOOP_HPP

#include <cstdio>

#include "Session.hpp"

namespace SSDbus {

class DbusEventLoop {
    SSDbus::Session& bus_;
    bool running_ = false;
    
public:
    explicit DbusEventLoop(SSDbus::Session& bus) : bus_(bus) {}
    
    void run() {
        running_ = true;
        int loop_count = 0;
        
        while (running_) {
            loop_count++;
            fprintf(stderr, "[LOOP] Iteration %d, running=%d\n", loop_count, running_);
            
            // 处理消息
            int r;
            int processed = 0;
            do {
                r = bus_.process();
                fprintf(stderr, "[LOOP] process() returned %d\n", r);
                if (r > 0) processed++;
                if (r < 0) {
                    fprintf(stderr, "[LOOP] process() error: %s\n", strerror(-r));
                    running_ = false;  // 明确标记
                    break;
                }
            } while (r > 0);
            
            if (!running_) break;
            
            fprintf(stderr, "[LOOP] Total processed: %d, entering wait...\n", processed);
            
            // 等待事件
            r = bus_.wait();
            fprintf(stderr, "[LOOP] wait() returned %d\n", r);
            
            if (r < 0) {
                fprintf(stderr, "[LOOP] wait() error: %s\n", strerror(-r));
                break;  // 这里退出！
            }
            // r == 0 表示超时，但你说 timeout 很大，不应该触发
        }
        
        fprintf(stderr, "[LOOP] Exited, total loops: %d\n", loop_count);
    }
    
    void stop() { running_ = false; }
};
}

#endif