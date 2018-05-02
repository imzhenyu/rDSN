#include <dsn/tool_api.h>
#include <dsn/tool-api/view_point.h>

#define THREAD_LOG_FILE_NAME_PREFIX "/tmp/ray_"

namespace dsn {

  view_point::view_point()
    : parent_(nullptr)
    , start_(0)
    , consumed_(0) {

  }

  void view_point::reset() {
      consumed_ = 0;
      start_ = dsn_now_ns();
  }

  void view_point::finish(uint64_t factor/* = 1*/) {
      consumed_ = elapsed(factor);
  }

  uint64_t view_point::elapsed(uint64_t factor/* = 1*/) {
      return (dsn_now_ns() - start_)/factor;
  }

  void view_point::name(const std::string &name) {
      name_ = name;
  }

  const std::string &view_point::name() {
      return name_;
  }

  void view_point::put(view_point *follow) {
      follow_.emplace_back(follow);
  }

  void view_point::gc(std::list<view_point*> &pool) {
      if (follow_.empty()) {
          return;
      }

      for (auto& vp : follow_) {
          vp->gc(pool);
      }

      pool.splice(pool.end(), follow_);
  }

  void view_point::dump(std::ostream& ostr) {
      std::string prefix;
      dump(ostr, prefix, false, 0);
  }

  void view_point::dump(std::ostream& ostr, std::string& prefix, bool has_next, int depth) {
    int len = 3*depth;

    char timestamp[20] = {0};
    ::dsn::utils::time_us_to_string(start_ / 1000, timestamp);

    ostr.write(prefix.data(), len) << "---" << name_ << " " << timestamp << " [ "<< consumed_ <<  " ]\n";

    if (!follow_.empty()) {
        if (!has_next && len > 0) {
            prefix[len-1] = ' ';
        }

        prefix.resize(len+3, ' ');
        prefix[len+2] = '|';
    }

    for (auto iter = follow_.begin(); iter != follow_.end(); ) {
        auto cur = iter++;
        (*cur)->dump(ostr, prefix, (iter != follow_.end()), depth+1);
    }
    prefix.resize(len);
  }

  /****************************************************************************************/
  thread_local std::list<view_point*> view_point_manager::pool_;
  thread_local std::stack<view_point*> view_point_manager::stack_;
  thread_local std::ofstream view_point_manager::ostr_;

  view_point_manager::~view_point_manager() {
      for (auto& vp : pool_) {
          delete vp;
      }

      while (!stack_.empty()) {
          delete stack_.top();
          stack_.pop();
      }

      if (ostr_.is_open()) {
          ostr_.close();
      }
  }

  view_point_wrapper<view_point> view_point_manager::new_view_point(std::string&& point, size_t factor) {
      view_point* vp = nullptr;
      if (pool_.empty()) {
          vp = new view_point;
      } else {
          vp = pool_.back();
          pool_.pop_back();
      }

      vp->name(std::move(point));

      stack_.push(vp);
      return view_point_wrapper<view_point>(vp, [factor](view_point* vp) {
          vp->finish(factor);
          stack_.pop();

          if (!stack_.empty()) {
              stack_.top()->put(vp);
          } else {
              if (!ostr_.is_open()) {
                  std::string file_name(THREAD_LOG_FILE_NAME_PREFIX);
                  const int thread_name_length = 64;
                  char thread_name[thread_name_length] = {0};
                  pthread_getname_np(pthread_self(), thread_name, thread_name_length);
                  file_name.append(std::to_string(getpid()))
                           .append("_")
                           .append(std::to_string(dsn::utils::get_current_tid()))
                           .append("_")
                           .append(dsn::utils::trim_string(thread_name));
                  ostr_.open(file_name);
              }

              vp->dump(ostr_);
              vp->gc(pool_);
              pool_.push_back(vp);
          }
      });
  }
}
