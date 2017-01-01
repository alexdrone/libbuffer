#ifndef buffer_h
#define buffer_h

#include "diff.h"
#include <stdio.h>
#include <vector>
#include <thread>
#include <assert.h>

namespace buffer {

  // Interface of a buffer subscriber.
  template <typename T>
  class Subscriber {
  public:
    virtual void onBufferWillChange() = 0;
    virtual void onBufferDidChange() = 0;

    // Callback called whenever the collection changes.
    virtual void onBufferChange(DiffType type, size_t index, T value) = 0;
  };

  template <typename T>
  class Buffer {
  private:

    std::unique_ptr<std::vector<T>> back_buffer_{};
    std::unique_ptr<std::vector<T>> front_buffer_{};
    std::unique_ptr<std::vector<Subscriber<T>*>> subscribers_{};
    std::__thread_id init_thread_id_ = std::this_thread::get_id();
    std::mutex buffer_lock_;
    std::mutex subscribers_lock_;

    // delegate funcs.
    std::function<bool (T, T)> compare_fnc_ = nullptr;
    std::function<std::vector<T> (const std::vector<T> &)> sort_fnc = nullptr;

    // flags.
    bool is_asynchronous_ = false;
    bool is_computing_changes_ = false;
    bool should_recompute_changes_ = false;

  public:

    Buffer() {
      back_buffer_ = std::unique_ptr<std::vector<T>>(new std::vector<T>());
      front_buffer_ = std::unique_ptr<std::vector<T>>(new std::vector<T>());
      subscribers_ = std::unique_ptr<std::vector<Subscriber<T>*>>(new std::vector<Subscriber<T>*>());
    }

    // Adds a new subscriber to this buffer.
    void registerSubscriber(Subscriber<T> &subscriber) {
      subscribers_lock_.lock();
      if (std::find(subscribers_->begin(), subscribers_->end(), &subscriber)!= subscribers_->end()){
        // The subscriber is already registered.
        subscribers_lock_.unlock();
        return;
      } else {
        subscribers_->push_back(&subscriber);
        subscribers_lock_.unlock();
      }
    }

    // Remove the subscriber passed as argument.
    void unregisterSubscriber(Subscriber<T> &subscriber) {
      subscribers_lock_.lock();
      auto v = subscribers_;
      if(std::find(v->begin(), v->end(), &subscriber) != v->end())
        // Remove the subscriber at the found position.
        v.erase(std::remove(v.begin(), v.end(), &subscriber), v.end());
      subscribers_lock_.unlock();
    }

    // Returns all the element currently exposed from the buffer.
    std::vector<T> getCollection() {
      std::vector<T> result(front_buffer_);
      return result;
    }

    // Updates the collection, compute the diffs and notifies the subscribers.
    void setCollection(std::vector<T> collection) {
      assert(init_thread_id_ == std::this_thread::get_id());
      back_buffer_ = std::unique_ptr<std::vector<T>>(new std::vector<T>(collection));
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

  private:

    void computeChanges() {
      buffer_lock_.lock();
      is_computing_changes_ = true;
      if (should_recompute_changes_)
        should_recompute_changes_ = false;

      const auto new_collection = sort_fnc
          ? new std::vector<T>(sort_fnc(std::vector<T>(*back_buffer_)))
          : new std::vector<T>(*back_buffer_);

      auto diffs = diff(*front_buffer_, *new_collection);
      auto is_changed = diffs.size();

      if (is_changed)
        for (auto subscriber : *subscribers_)
          subscriber->onBufferWillChange();

      front_buffer_ = std::unique_ptr<std::vector<T>>(std::move(new_collection));

      // Propagate the event change to all of the subscribers;
      for (auto diff : diffs)
        for (auto subscriber : *subscribers_)
          subscriber->onBufferChange(diff.type, diff.index, diff.value);

      if (is_changed)
        for (auto subscriber : *subscribers_)
          subscriber->onBufferDidChange();

      is_computing_changes_ = false;
      buffer_lock_.unlock();

      // The collection changed while the changes where being computed.
      if (should_recompute_changes_)
        computeChanges();
    }
  };
}

#endif /* buffer_h */
