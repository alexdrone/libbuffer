#ifndef buffer_h
#define buffer_h

#include "diff.h"
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <thread>
#include <assert.h>

namespace buffer {

  // Interface of a buffer subscriber.
  template <typename T>
  class Subscriber {
  public:
    // Callback called whenever the collection changes.
    virtual void onBufferChange(DiffType type, size_t index, T value) = 0;
  };

  // A lightweight object initialised with the callback closure.
  template <typename T>
  class VolatileSubscriber: public Subscriber<T> {
    std::function<void (DiffType, size_t, T)> callback;
    // Callback called whenever the collection changes.
    virtual void onBufferChange(DiffType type, size_t index, T value) {
      if (callback == nullptr) return;
      callback(type, index, value);
    }
  };

  template <typename T>
  class Buffer {
  private:
    std::vector<Subscriber<T>*> *subscribers_ = new std::vector<Subscriber<T>*>();
    std::vector<T> *back_buffer_ = new std::vector<T>();
    std::vector<T> *front_buffer_ = new std::vector<T>();
    std::__thread_id init_thread_id_ = std::this_thread::get_id();
    std::mutex lock_;

    // delegate funcs.
    std::function<bool (T, T)> compare_fnc_ = nullptr;
    std::function<std::vector<T> (const std::vector<T> &)> sort_fnc = nullptr;

    // flags.
    bool is_asynchronous_ = false;
    bool is_computing_changes_ = false;
    bool should_recompute_changes_ = false;

  public:
    // Adds a new subscriber to this buffer.
    void registerSubscriber(Subscriber<T> &subscriber) {
      if (std::find(subscribers_->begin(), subscribers_->end(), &subscriber)!= subscribers_->end()){
        // The subscriber is already registered.
        return;
      } else {
        subscribers_->push_back(&subscriber);
      }
    }

    // Remove the subscriber passed as argument.
    void unregisterSubscriber(Subscriber<T> &subscriber) {
      auto v = subscribers_;
      if(std::find(v->begin(), v->end(), &subscriber) != v->end()) {
        // Remove the subscriber at the found position.
        v.erase(std::remove(v.begin(), v.end(), &subscriber), v.end());
      }
    }

    // Returns all the element currently exposed from the buffer.
    std::vector<T> getCollection() {
      std::vector<T> result(front_buffer_);
      return result;
    }

    // Updates the collection, compute the diffs and notifies the subscribers.
    void setCollection(std::vector<T> collection) {
      assert(init_thread_id_ == std::this_thread::get_id());

      delete back_buffer_;
      back_buffer_ = new std::vector<T>(collection);
      refresh();
    }

    void refresh() {
      if (!is_asynchronous_) {
        computeChanges();
      } else {
        // check if is already applying the changes.
        if (is_computing_changes_) {
          should_recompute_changes_ = true;
          return;
        }
        std::thread bkg(&Buffer::computeChanges, this);
        bkg.detach();
      }
    }

    // Wheter the changes should be computed on a background thread or not.
    void setAsynchronous(bool asynchronous) {
      is_asynchronous_ = asynchronous;
    }

    // Override the '==' function for the collection wrapped.
    void setCompareFunction(const std::function<bool (T, T)> compare) {
      compare_fnc_ = compare;
    }

    // Sort function applied to the collection everytime is updated.
    void setSortFunction(const std::function<std::vector<T> (const std::vector<T> &)> sort) {
      sort_fnc = sort;
    }

    // Destructor.
    ~Buffer<T>(void) {
      delete back_buffer_;
      delete front_buffer_;
      delete subscribers_;
    }

  private:

    void computeChanges() {
      lock_.lock();
      is_computing_changes_ = true;
      if (should_recompute_changes_) {
        should_recompute_changes_ = false;
      }
      const auto new_collection = sort_fnc
          ? new std::vector<T>(sort_fnc(std::vector<T>(*back_buffer_)))
          : new std::vector<T>(*back_buffer_);

      auto diffs = diff(std::vector<T>(*front_buffer_), std::vector<T>(*new_collection));
      delete front_buffer_;
      front_buffer_ = new_collection;

      // Propagate the event change to all of the subscribers;
      for (auto diff : diffs) {
        for (auto subscriber : *subscribers_) {
          subscriber->onBufferChange(diff.type, diff.index, diff.value);
        }
      }
      is_computing_changes_ = false;
      lock_.unlock();

      // The collection changed while the changes where being computed.
      if (should_recompute_changes_) {
        computeChanges();
      }
    }
  };
}

#endif /* buffer_h */
