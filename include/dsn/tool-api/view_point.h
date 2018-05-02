#pragma once

#include <dsn/utility/singleton.h>

#include <string>
#include <list>
#include <stack>
#include <memory>
#include <ostream>
#include <fstream>

namespace dsn {

    class view_point {
    public:
        view_point();

    public:
        void reset();
        void finish(uint64_t factor = 1);
        uint64_t elapsed(uint64_t factor = 1);

        const std::string &name();
        void name(const std::string& name);

        void put(view_point* follow);
        void dump(std::ostream& ostr);
        void gc(std::list<view_point*>&  pool);

    private:
        void dump(std::ostream& ostr, std::string& prefix, bool has_next, int depth);

    private:
        std::string name_;
        view_point* parent_;

        uint64_t start_;
        uint64_t consumed_;

        std::list<view_point*> follow_;
    };

    template<typename T>
    class view_point_wrapper {
    public:
        view_point_wrapper(view_point_wrapper&& wrapper) {
            vp_ = wrapper.vp_;
            deleter_ = std::move(wrapper.deleter_);

            wrapper.vp_ = nullptr;
            wrapper.deleter_ = nullptr;
        }

        view_point_wrapper(T* vp, std::function<void(T*)>&& deleter) {
            vp_ = vp;
            deleter_ = std::move(deleter);
        }

        ~view_point_wrapper() {
            if (deleter_) {
                deleter_(vp_);
            }
        }

        T* operator -> () const {
            return vp_;
        }

    private:
        view_point_wrapper(const view_point_wrapper&);
        view_point_wrapper& operator = (const view_point_wrapper&);

    private:
        T* vp_;
        std::function<void(T*)> deleter_;
    };

    class view_point_manager : public dsn::utils::singleton<view_point_manager> {

    public:
        view_point_manager() = default;
        ~view_point_manager();

    public:
        view_point_wrapper<view_point> new_view_point(std::string&& point, size_t factor = 1);

    private:
        thread_local static std::list<view_point*> pool_;
        thread_local static std::stack<view_point*> stack_;
        thread_local static std::ofstream ostr_;
    };

}

//#define ENABLE_VIEW_POINT
#ifdef ENABLE_VIEW_POINT

#define __SPLITE__(X,Y) X##Y
#define __DECLARE_NAME__(X,Y) __SPLITE__(X,Y)
#define LINE_NO_BASED_VAR __DECLARE_NAME__(var, __LINE__)

#define __INSTALL_VIEW_POINT__(V, F) \
    auto LINE_NO_BASED_VAR = dsn::view_point_manager::instance().new_view_point(V, 1000); LINE_NO_BASED_VAR->reset();

#define INSTALL_VIEW_POINT(V) __INSTALL_VIEW_POINT__(V, 1000)

#else

#define INSTALL_VIEW_POINT(V)

#endif
