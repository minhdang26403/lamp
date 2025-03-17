#ifndef SCOPED_LOCK_H_
#define SCOPED_LOCK_H_

template<typename Lock>
class ScopedLock {
 public:
  explicit ScopedLock(Lock& lock) : lock_(lock) { lock_.lock(); }

  ScopedLock(const ScopedLock<Lock>&) = delete;

  auto operator=(const ScopedLock<Lock>&) -> ScopedLock<Lock>& = delete;

  ~ScopedLock() { lock_.unlock(); }

 private:
  Lock& lock_;
};

#endif  // SCOPED_LOCK_H_
