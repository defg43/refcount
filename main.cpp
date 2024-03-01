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
#define container_of(ptr, type, member)             \
    (typeof(((type *)0)->member) *)((char *)ptr -   \
    offsetof(type, member))
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
        printf("empty constructor called\nthe pointer from the empty constructor should be: %p\n", this->getPtr());
    }

    Arc(T &x) : ptr((refcount_t<T> *)malloc(sizeof(refcount_t<T>))) {
        printf("reference constructor called\n");
        if(this->ptr == nullptr) {
            ArcAllocationErrorHandler();
        }
        this->ptr->refs = {1};
        new(&(this->ptr->data)) T(x);
        this->ptr->deleter = default_deleter;
    }

    Arc(T &&x) : ptr((refcount_t<T> *)malloc(sizeof(refcount_t<T>))) {
        printf("rvalue ref constructor called\n");
        if(this->ptr == nullptr) {
            ArcAllocationErrorHandler();
        }
        this->ptr->refs = {1};
        this->ptr->deleter = default_deleter;
        new(&(this->ptr->data)) T(move(x));
    }

    Arc(initializer_list<T> x) {
        constructArcFromInitList(x);
    }
    
    Arc(const Arc<T> &x) : ptr(x.ptr) {
        this->ptr->refs++;
        printf("calling move constructor\n");
    }

    Arc<T> clone() {
        Arc<T> to_return;
        to_return.ptr = (refcount_t<T> *)malloc(sizeof(refcount_t<T>));
        if(to_return.ptr == nullptr) {
            ArcAllocationErrorHandler();
        }
        new(&(to_return.ptr->data)) T(this->ptr->data);
        to_return.ptr->deleter = this->ptr->deleter;
        to_return.ptr->refs = 1;
        return to_return;
    }


    void constructArcFromInitList(initializer_list<T> x) {
        this->ptr = (refcount_t<T> *)malloc(sizeof(refcount_t<T>));
        if(this->ptr == nullptr) {
            ArcAllocationErrorHandler();
        }
        printf("handed out address is %p\n", this->ptr);
        this->ptr->refs = {1};
        new(&(this->ptr->data)) T(x);
    }
    
    T operator*() {
        return this->ptr->data;
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
        return &this->ptr->data;
    }

    Arc<T> operator=(Arc<T> other) {
        if(this->ptr == other.ptr) {
            return other;
        } else if(this->ptr == nullptr && other.ptr != nullptr) {
            other.ptr->refs++;
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
            drop();
        }
        return other;
    }

    ~Arc() {
        printf("dropping arc data from destructor\n");
        drop();
    }
    T *getPtr() {
        if(this->ptr != nullptr)
            return &this->ptr->data;
        else 
            return nullptr;
    }

    private:
        refcount_t<T> *ptr;
        void drop() {
            using U = refcount_t<T>;
            static vector<U *> pointers_freed_so_far;
            if(this->ptr != nullptr) {
                pointers_freed_so_far.push_back(this->ptr);
                printf("the ref count is currently %ld\n", (size_t)this->ptr->refs);
                if(--(this->ptr->refs) == 0) {
                    this->ptr->data.~T();
                    this->ptr->deleter(this->ptr);
                    printf("dropping pointer %p\n", this->ptr);
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
        printf("yes this is called\n");
        a = Arc(i1);
        printf("help\n");
        b = Arc(i2);
        printf("---------------------\n");
        damn = init;
        printf("a ptr: %p, b ptr: %p\n", a.getPtr(), b.getPtr());
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
    // auto test4 = foo("hello", 2, 3);
    printf("\n\nend of main function\n\n\n");
    return 0;
}
