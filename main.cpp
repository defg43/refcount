#include <iostream>
#include <memory>
#include <utility>
#include <stdio.h>
#include <atomic>
#include <compare>
#include <assert.h>
#include <vector>
#include <bits/stdc++.h>

#ifndef container_of
#define container_of(ptr, type, member) ({                                  \
    const decltype( ((type *)0)->member )                                   \
    *__mptr = (ptr);                                                        \
    reinterpret_cast<type *>( (uint8_t *)__mptr - offsetof(type,member) );})
#endif // container_of


using std::atomic_size_t;
using std::move;
using std::forward;
using std::string;
using std::initializer_list;
using std::nullptr_t;
using std::vector;

using deleter_t = void(*)(void *);

template <typename T>
struct refcount_t {
    atomic_size_t refs;
    deleter_t deleter;
    T data;
    ~refcount_t() {}
};

void ArcAllocationErrorHandler(void) {
    exit(EXIT_FAILURE);
}

template<typename T, deleter_t default_deleter = free> struct Arc {
    Arc() : ptr(nullptr) {
        printf("empty constructor called\nthe pointer from the empty constructor should be: %p\n", this->ptr);
    }

    Arc(T &x) : ptr(nullptr) {
        printf("reference constructor called\n");
        this->ptr = reinterpret_cast<T *>((uint8_t *)malloc(sizeof(refcount_t<T>)) + offsetof(refcount_t<T>, data));
        if(this->ptr == reinterpret_cast<T *>((uint8_t *)nullptr + offsetof(refcount_t<T>, data))) {
            ArcAllocationErrorHandler();
        }
        container_of(this->ptr, refcount_t<T>, data)->refs = {1};
        new(this->ptr) T(x);
        container_of(this->ptr, refcount_t<T>, data)->deleter = default_deleter;
    }

    Arc(T &&x) : ptr(nullptr) {
        printf("rvalue ref constructor called\n");
        this->ptr = reinterpret_cast<T *>((uint8_t *)malloc(sizeof(refcount_t<T>)) + offsetof(refcount_t<T>, data));
        if(this->ptr == reinterpret_cast<T *>((uint8_t *)nullptr + offsetof(refcount_t<T>, data))) {
            ArcAllocationErrorHandler();
        }
        container_of(this->ptr, refcount_t<T>, data)->refs = {1};
        container_of(this->ptr, refcount_t<T>, data)->deleter = default_deleter;
        new(this->ptr) T(move(x));
    }

    Arc(initializer_list<T> x) {
        constructArcFromInitList(x);
    }
    
    Arc(const Arc<T> &x) : ptr(nullptr) {
        printf("calling move constructor\n");
        this->ptr = x.ptr;
        if(this->ptr == nullptr) {
            return;
        }
        container_of(this->ptr, refcount_t<T>, data)->refs++;
    }

    Arc<T> clone() {
        Arc<T> to_return;
        to_return.ptr = static_cast<T *>((malloc(sizeof(refcount_t<T>)) + offsetof(refcount_t<T>, data)));
        if(to_return.ptr == reinterpret_cast<T *>((uint8_t *)nullptr + offsetof(refcount_t<T>, data))) {
            ArcAllocationErrorHandler();
        }
        new(to_return.ptr) T(this->ptr);
        container_of(to_return.ptr, refcount_t<T>, data)->deleter = container_of(this->ptr, refcount_t<T>, data)->deleter;
        container_of(to_return.ptr, refcount_t<T>, data)->refs = 1;
        return to_return;
    }


    void constructArcFromInitList(initializer_list<T> x) {
        container_of(this->ptr, refcount_t<T>, data) = static_cast<T *>(malloc(sizeof(refcount_t<T>)) + offsetof(refcount_t<T>, data));
        if(this->ptr == reinterpret_cast<T *>((uint8_t *)nullptr + offsetof(refcount_t<T>, data))) {
            ArcAllocationErrorHandler();
        }
        printf("handed out address is %p\n", container_of(this->ptr, refcount_t<T>, data));
        container_of(this->ptr, refcount_t<T>, data)->refs = {1};
        new(this->ptr) T(x);
    }
    
    T operator*() {
        return *(this->ptr);
    }

#pragma region relational_operators
    bool operator!=(Arc<T> other) {
        return this->ptr != other.ptr;
    }

    bool operator==(Arc<T> other) {
        return this->ptr == other.ptr;
    }

    bool operator>=(Arc<T> other) {
        return this->ptr >= other.ptr;
    }

    bool operator<=(Arc<T> other) {
        return this->ptr <= other.ptr;
    }

    bool operator>(Arc<T> other) {
        return this->ptr > other.ptr;
    }

    bool operator<(Arc<T> other) {
        return this->ptr < other.ptr;
    }
#pragma endregion

    operator T *(){
        return this->ptr;
    }

    Arc<T> operator=(Arc<T> other) {
        if(this->ptr == other.ptr) {
            return other;
        } else if(this->ptr == nullptr && other.ptr != nullptr) {
            container_of(other.ptr, refcount_t<T>, data)->refs++;
            this->ptr = other.ptr;
            return Arc(other);
        } else if(this->ptr != nullptr && other.ptr == nullptr) {
            this->drop();
            return Arc();
        } else if(this->ptr != nullptr && other.ptr != nullptr) {
            this->drop();
            return (*this = Arc(other));
        }
        return Arc();
    }

    nullptr_t operator=(nullptr_t other) {
        if(this->ptr != nullptr) {
            this->drop();
        }
        return other;
    }

    ~Arc() {
        printf("dropping arc data from destructor\n");
        drop();
    }
    T *getPtr() {
        return this->ptr;
    }

    private:
        T *ptr;
        void drop() {
            if(this->ptr != nullptr) {
                printf("the ref count is currently %ld\n", (size_t) container_of(this->ptr, refcount_t<T>, data)->refs);
                if(--(container_of(this->ptr, refcount_t<T>, data)->refs) == 0) {
                    this->ptr->~T();
                    container_of(this->ptr, refcount_t<T>, data)->deleter(container_of(this->ptr, refcount_t<T>, data));
                    printf("dropping pointer %p\n", container_of(this->ptr, refcount_t<T>, data));
                    this->ptr = nullptr;    
                }
            } else {
                printf("nothing should happen as pointer null\n");
            }
        }
};

void printdestructor(void *x) {
    printf("freeing memory at: %p\n", x);
    free(x);
}

struct foo {
    Arc<int> a;
    Arc<int> b;
    string damn;
    foo(string init, int i1, int i2) {
        a = Arc(i1);
        b = Arc(i2);
        damn = init;
        printf("a ptr: %p, b ptr: %p\n", a, b);
        assert(a.getPtr() != b.getPtr());
    };
    ~foo() {
        printf("foo is being destroyed\n");
        printf("the string was: %s\n", damn.c_str());
    }
};

int main() {
    Arc<int> test;
    printf("test points to %p\n", test.getPtr());
    printf("test size %ld\n", sizeof(test));
    test = Arc(5);
    printf("the pointer to T is %p\n", test);
    printf("which should be the same as %p\n", test.getPtr());
    // auto test4 = foo("hello", 2, 3);
    printf("\n\nend of main function\n\n\n");
    return 0;
}
