#ifndef buffer_h
#define buffer_h

#include "levenshtein.h"
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
    bool is_using_bkg_queue_ = false;
    bool is_computing_changes_ = false;
    bool should_recompute_changes_ = false;

  public:

    // Adds a new subscriber to this buffer.
    void registerSubscriber(Subscriber<T> *subscriber) {
      auto v = this->subscribers_;
      if(std::find(v->begin(), v->end(), subscriber) != v->end()) {
        // The subscriber is already registered.
        return;
      } else {
        v->push_back(subscriber);
      }
    }

    // Remove the subscriber passed as argument.
    void unregisterSubscriber(Subscriber<T> *subscriber) {
      auto v = this->subscribers_;
      if(std::find(v->begin(), v->end(), subscriber) != v->end()) {
        // Remove the subscriber at the found position.
        v.erase(std::remove(v.begin(), v.end(), subscriber), v.end());
      }
    }

    // Returns all the element currently exposed from the buffer.
    std::vector<T> getCollection() {
      std::vector<T> result(this->front_buffer_);
      return result;
    }

    // Updates the collection, compute the diffs and notifies the subscribers.
    void setCollection(std::vector<T> &collection) {
      assert(this->init_thread_id_ == std::this_thread::get_id());

      delete this->back_buffer_;
      this->back_buffer_ = new std::vector<T>(collection);

      if (!this->is_using_bkg_queue_) {
        this->computeChanges();
      } else {
        // check if is already applying the changes.
        if (this->is_computing_changes_) {
          this->should_recompute_changes_ = true;
          return;
        }
        std::thread bkg(&Buffer::computeChanges, this);
        bkg.detach();
      }

    }

    void setShouldComputeChangesOnBackgroundThread(bool useBackgroundThread) {
      this->is_using_bkg_queue_ = useBackgroundThread;
    }

    // Destructor.
    ~Buffer<T>(void) {
      delete this->back_buffer_;
      delete this->front_buffer_;
      delete this->subscribers_;
    }

  private:

    // Compute the collection diffs on a background thread.
    void computeChanges() {
      this->is_computing_changes_ = true;
      if (this->should_recompute_changes_) {
        this->should_recompute_changes_ = false;
      }

      // Compute the diffs.
      auto diffs = diff(std::vector<T>(*this->front_buffer_), std::vector<T>(*this->back_buffer_));

      // Assigns the back buffer to the front buffer.
      delete this->front_buffer_;
      this->front_buffer_ = new std::vector<T>(*this->back_buffer_);

      // Propagate the event change to all of the subscribers;
      for (auto diff : diffs) {
        for (auto subscriber : *this->subscribers_) {
          subscriber->onBufferChange(diff.type, diff.index, diff.value);
        }
      }
      this->is_computing_changes_ = false;

      // The collection changed while the changes where being computed.
      if (this->should_recompute_changes_) {
        this->computeChanges();
      }
    }

  };
}

#endif /* buffer_h */
