#pragma once
#include <bits/stdint-uintn.h>
#include <complex>
#include <initializer_list>
#include <type_traits>
#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <future>
#include "submodules/rwqueue/readerwriterqueue.h"
#include "thread_pool.hpp"
#include <GL/glew.h>

template<class FP>
class mand_generator{
    void func();
public:
    struct color{
        uint8_t r,g,b;
        color(){}
        color(std::initializer_list<float> lst){
            assert(lst.size() == 3);
            r = 255* *(lst.begin()+0);
            g = 255* *(lst.begin()+1);
            b = 255* *(lst.begin()+2);
        }
        color(std::initializer_list<uint8_t> lst){
            assert(lst.size() == 3);
            r = *(lst.begin()+0);
            g = *(lst.begin()+1);
            b = *(lst.begin()+2);
        }
        bool operator==(const color &rhs)const{
            return r == rhs.r &&
                g == rhs.g &&
                b == rhs.b;
        }
    };
    struct task{
        FP x_min, x_max, y_min, y_max;
        color belongs, not_belongs;
        unsigned lines, threads, iters;
        unsigned w, h;
        bool operator==(const task &rhs)const{
            return x_max == rhs.x_max &&
                x_min == rhs.x_min &&
                y_max == rhs.y_max &&
                y_min == rhs.y_min &&
                w == rhs.w &&
                h == rhs.h &&
                iters ==  rhs.iters &&
                belongs == rhs.belongs &&
                not_belongs == rhs.not_belongs;
        }
    };

    struct image{
        std::vector<GLubyte> pixels;
        int line_no;
        int x, y, w, h;
    };

    using task_q = moodycamel::BlockingReaderWriterQueue<task>;
    using img_q = moodycamel::ReaderWriterQueue<image>;

    mand_generator(task_q &tasks, img_q &imgs);
    ~mand_generator();
    void run();
    void stop();
private:
    std::atomic_bool m_run;
    std::thread thr;
    task_q &tasks;
    img_q &imgs;
};

template<class FP>
mand_generator<FP>::mand_generator(task_q &tasks, img_q &imgs)
    :tasks(tasks),
    imgs(imgs)
{}

template<class FP>
mand_generator<FP>::~mand_generator(){
    stop();
}

template<class FP>
void mand_generator<FP>::run(){
    m_run = true;
    if(thr.joinable()){
        thr.join();
    }
    thr = std::thread([this]{func();});
}

template<class FP>
void mand_generator<FP>::stop(){
    m_run = false;
    if(thr.joinable()){
        thr.join();
    }
}

template<class FP>
void mand_generator<FP>::func(){
    task prev_t;
    auto mand = [](const FP &x, const FP &y, const unsigned &lim)->float{
        const std::complex<FP> point(x, y);
        std::complex<FP> z(0, 0);
        size_t cnt = 0;
        while(std::abs(z) < 2 && cnt <= lim) {
            z = z * z + point;
            cnt++;
        }
        return cnt*1./lim;
    };
    auto calc_line = [&mand](auto y, auto w, auto h, auto line_h, const task &t)->image{
        image img;
        img.x = 0;
        img.y = y;
        img.w = w;
        img.h = line_h;
        auto &pixels = img.pixels;
        FP map_x = (t.x_max - t.x_min)/w;
        FP map_y = (t.y_max - t.y_min)/h;
        for(size_t j = y; j<y+line_h; j++){
            FP y_map = j*map_y + t.y_min;
            for(size_t i = 0; i<w; i++){
                FP x_map = i*map_x + t.x_min;
                auto fract = mand(x_map, y_map, t.iters);
                GLubyte r,g,b;
                r = fract>0.5?t.belongs.r*fract : t.not_belongs.r*(1-fract);
                g = fract>0.5?t.belongs.g*fract : t.not_belongs.g*(1-fract);
                b = fract>0.5?t.belongs.b*fract : t.not_belongs.b*(1-fract);
                pixels.emplace_back(r);
                pixels.emplace_back(g);
                pixels.emplace_back(b);
            }
        }
        return img;
    };

    while(m_run){
        task t;
        tasks.wait_dequeue(t);
        if(t == prev_t){
            continue;
        }

        auto line_h = t.h/t.lines;
        tortique::thread_pool tp(t.threads);
        auto run_pool = [this, &tp, &t, &mand, &calc_line, &line_h]
            (auto line_beg){
            std::vector<std::future<image>> results;
            for(auto l = line_beg; l < t.lines; l+=2){
                auto ftr = tp.emplace(calc_line, l*line_h,
                    t.w, t.h, line_h, t);
                results.emplace_back(std::move(ftr));
            }
            for (auto& ftr:results){
                imgs.emplace(ftr.get());
            }
        };
		run_pool(0);
		run_pool(1);
        prev_t = t;
    }
}
