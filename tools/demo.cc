#include "stdio.h"
#include "unistd.h"
#include "thread"
#include "iostream"

#include "slash/include/slash_mutex.h"

int main() {
  pthread_rwlock_t rwlock_;
  pthread_rwlock_init(&rwlock_, NULL);

  {
    {
      slash::RWLock rl11(&rwlock_, false);
      printf("read lock first\n");
      sleep(1);

      slash::RWLock rl12(&rwlock_, false);
      printf("read lock first\n");
      sleep(1);
    }

    slash::RWLock wl11(&rwlock_, true);
    printf("write lock first\n");
    sleep(1);
  }

  {
    {
      slash::RWLock rl21(&rwlock_, false);
      printf("read lock second\n");
      sleep(1);

      slash::RWLock rl22(&rwlock_, false);
      printf("read lock second\n");
      sleep(1);
    }

    slash::RWLock wl21(&rwlock_, true);
    printf("write lock second\n");
    sleep(1);
  }
  return 0;
}
