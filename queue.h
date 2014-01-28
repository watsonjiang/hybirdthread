#ifndef QUEUE_H
#define QUEUE_H
#include <deque>
#include <queue>
#include <pthread.h>
template<class T>
class Queue : std::queue<T, std::deque<T> >
{
   typedef std::queue<T, std::deque<T> > BASE;
   public:
      Queue(int capacity);
      void push(const T& val);
      T pop();
   private:
      int _capacity;
      pthread_mutex_t _lock;
      pthread_cond_t _cond_not_full;
      pthread_cond_t _cond_not_empty;
};

class Guard
{
   public:
      Guard(pthread_mutex_t* lock)
         :_lock(lock)
      {
         pthread_mutex_lock(_lock);
      }

      ~Guard()
      {
         pthread_mutex_unlock(_lock);
      }
   private:
      pthread_mutex_t* _lock;
};

template<class T>
Queue<T>::Queue(int capacity)
   :_capacity(capacity)
{
   pthread_mutex_init(&_lock, NULL);
   pthread_cond_init(&_cond_not_full, NULL);
   pthread_cond_init(&_cond_not_empty, NULL);
}

template<class T>
void
Queue<T>::push(const T& val)
{
   Guard guard(&_lock);
   if(_capacity <= BASE::size())
   {
      pthread_cond_wait(&_cond_not_full, &_lock);
   }
   BASE::push(val);
   pthread_cond_signal(&_cond_not_empty);
}

template<class T>
T
Queue<T>::pop()
{
   Guard guard(&_lock);
   if(BASE::empty())
   {
      pthread_cond_wait(&_cond_not_empty, &_lock);
   }
   T& r = BASE::front();
   BASE::pop();
   pthread_cond_signal(&_cond_not_full);
   return r;
}

#endif //QUEUE_H
