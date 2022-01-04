#include "async.h"
#include <string.h>

const int MAX_ELEMENTS = 16;

class AsyncEntry {
  public:
    const char* id;
    Async* async;

    AsyncEntry(const char *id, Async *async)  : id(id), async(async) { }
};

AsyncEntry* list[MAX_ELEMENTS] = { nullptr };
unsigned int listSize = 0;

#ifdef ASSERT_MODE
void plot() {
  Serial.print(" queue: ");
  auto it = list;
  for (unsigned int idx = 0; idx < listSize; ++idx, ++it)  {
    auto entry = *it;
    auto async = entry == NULL ? NULL : entry->async;
    Serial.print(idx);
    Serial.print(":");
    if (async == NULL)  {
      Serial.print("null ");
    } else {
      Serial.print("@");
      Serial.print((int)async);
      if (async->isCanceled())  {
        Serial.print("[C]");
      }
      Serial.print(" ");
    }
  }
  Serial.print(" -> ");
  Serial.print(listSize);
  Serial.println("!");
}
#endif


void AsyncRegister::add(const char* id, Async* async) {
  if (id != nullptr) {
    AsyncEntry** it = list;
    
    for (unsigned int idx = 0; idx < listSize; ++idx, ++it) {
      auto asyncEntry = *it;
      if (asyncEntry == nullptr) {
        continue;
      }
      if (asyncEntry->id != nullptr && strcmp(asyncEntry->id, id) == 0 && !asyncEntry->async->isCanceled()) {
        asyncEntry->async->cancel();
      }
    }
  }

  if (listSize >= MAX_ELEMENTS)  {
    // oops
    return;
  }

  if (async != nullptr)  {
    list[listSize++] = new AsyncEntry(id, async);
  }
}


void AsyncRegister::loop(Micros now)  {
#ifdef ASSERT_MODE
  Serial.println("loop");
  plot();
#endif

  // we use it to through the list
  AsyncEntry** it = list;
  AsyncEntry** firstEmpty = nullptr;
#ifdef ASSERT_MODE
  int firstEmptyIndex = 0;
#endif
  Async* async;
  AsyncEntry* asyncEntry;
  // to be able to add a task at any time, i.e., listSize can grow we keep track of that one as well
  for (unsigned int idx = 0; idx < listSize; ++idx, ++it)  {
    asyncEntry = *it;
    async = asyncEntry == nullptr ? nullptr : asyncEntry->async;
    if (async == nullptr)  {
      if (firstEmpty == nullptr) {
        firstEmpty = it;
#ifdef ASSERT_MODE
        firstEmptyIndex = idx;
#endif
      }
      continue;
    }
    if (!async->isCanceled())  {
      if (!async->isInitialized()) {
        async->setup(now);
      } else {
        async->loop(now);
      }
    }
    if (async->isCanceled())  {
#ifdef ASSERT_MODE
      Serial.print("  cancel @");
      Serial.println((int)async);
#endif
      delete asyncEntry;
      delete async;
      it[0] = nullptr;
      if (firstEmpty != nullptr)
        firstEmpty = it;
    }
  }
#ifdef ASSERT_MODE
  if (firstEmpty != nullptr) {
    Serial.print("  empty: ");
    Serial.println(firstEmptyIndex);
  }
#endif


  // here we reclaim the end of the list (one by one)
  if (listSize >= 1 && list[listSize - 1] == nullptr) {
    --listSize;
#ifdef ASSERT_MODE
    Serial.println("  reclaim end");
#endif
  }
  // here we try to reclaim an entry in the list, note firstEmpty is by definition not the end of the list
  else if (firstEmpty != nullptr && listSize > 1) {
#ifdef ASSERT_MODE
    Serial.println("  reclaim emptyFirstIdx");
#endif
    *firstEmpty = list[--listSize];
  }
#ifdef ASSERT_MODE
  plot();
#endif
  return;
}
