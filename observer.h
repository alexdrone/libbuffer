#ifndef observer_h
#define observer_h

#include <stdio.h>
#include <vector>
#include <assert.h>

namespace Buffer {

  template <typename E, typename T>
  class Observer {
  public:
    virtual void onChange(T object, E key) = 0;
  };

  template <typename E, typename T>
  class Observable {
  private:
    std::unique_ptr<std::vector<Observer<T, E>*>> observers_{};
    std::mutex observers_lock_;

  public:

    void initObservable() {
      observers_ = std::unique_ptr<std::vector<Observer<T, E>*>>(new std::vector<Observer<T, E>>());
    }

    void notifyObservers(E event) {
      observers_lock_.lock();
      for (auto &observer : observers_)
        observer->onChange(this, event);
      observers_lock_.unlock();
    }

    void registerObserver(Observer<T, E> &observer) {
      observers_lock_.lock();
      if (std::find(observers_->begin(), observers_->end(), &observer)!= observers_->end()) {
        // The subscriber is already registered.
        observers_lock_.unlock();
        return;
      } else {
        observers_->push_back(&observer);
        observers_lock_.unlock();
      }
    }

    void unregisterSubscriber(Observer<T, E> &observer) {
      observers_lock_.lock();
      auto v = observers_;
      if(std::find(v->begin(), v->end(), &observer) != v->end())
        // Remove the subscriber at the found position.
        v.erase(std::remove(v.begin(), v.end(), &observer), v.end());
      observers_lock_.unlock();
    }
  };

}

#endif /* observer_h */
