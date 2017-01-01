//
//  main.cpp
//  bufferlib
//
//  Created by Alex Usbergo on 2016-12-24.
//  Copyright © 2016 Alex Usbergo. All rights reserved.
//

#include <iostream>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include "buffer.h"
#include "observer.h"

class Foo: public buffer::Subscriber<int> {
  virtual void onBufferWillChange();
  virtual void onBufferDidChange();
  virtual void onBufferChange(buffer::DiffType type, size_t index, int value);
};


void Foo::onBufferWillChange() {
  std::cout << "will change\n";
}

void Foo::onBufferDidChange() {
  std::cout << "did change\n";
}

void Foo::onBufferChange(buffer::DiffType type, size_t index, int value) {
  switch (type) {
    case buffer::INSERT:
      std::cout << "INSERT ";
      break;
    case buffer::DELETE:
      std::cout << "DELETE ";
      break;
    case buffer::SUBSTITUTE:
      std::cout << "SUBSTITUTE ";
      break;
    default:
      break;
  }
  std::cout << value << " at index: " << index << "\n";
}

int main(int argc, const char * argv[]) {
  std::vector<int> o = {1, 5, 3, 2};
  std::vector<int> n = {1, 3, 2, 6, 6};

  Foo sub;
  buffer::Buffer<int> buffer;

  auto sort = [](const std::vector<int> &v) {
    auto s = std::vector<int>(v);
    std::sort(s.begin(), s.end(), std::less_equal<int>());
    return s;
  };

  buffer.setSortFunction(sort);
  buffer.registerSubscriber(sub);
  buffer.setCollection(o);
  buffer.setCollection(n);
  buffer.setCollection(o);
  return 0;
}
