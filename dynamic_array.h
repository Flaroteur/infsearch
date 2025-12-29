#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <cstdlib>
#include <cstring>
#include <new>

template<typename T>
class DynamicArray {
private:
    T* data;
    size_t size_;
    size_t capacity_;
    
    void resize(size_t new_capacity) {
        T* new_data = static_cast<T*>(malloc(new_capacity * sizeof(T)));
        if (!new_data) {
            throw std::bad_alloc();
        }
        
        
        for (size_t i = 0; i < size_; i++) {
            new (&new_data[i]) T(data[i]);
            data[i].~T();
        }
        
        free(data);
        data = new_data;
        capacity_ = new_capacity;
    }
    
public:
    DynamicArray() : data(nullptr), size_(0), capacity_(0) {}
    
    explicit DynamicArray(size_t initial_capacity) 
        : data(nullptr), size_(0), capacity_(0) {
        if (initial_capacity > 0) {
            data = static_cast<T*>(malloc(initial_capacity * sizeof(T)));
            if (!data) {
                throw std::bad_alloc();
            }
            capacity_ = initial_capacity;
        }
    }
    
    ~DynamicArray() {
        for (size_t i = 0; i < size_; i++) {
            data[i].~T();
        }
        free(data);
    }
    
    
    DynamicArray(const DynamicArray& other) 
        : data(nullptr), size_(0), capacity_(0) {
        if (other.size_ > 0) {
            data = static_cast<T*>(malloc(other.capacity_ * sizeof(T)));
            if (!data) {
                throw std::bad_alloc();
            }
            capacity_ = other.capacity_;
            for (size_t i = 0; i < other.size_; i++) {
                new (&data[i]) T(other.data[i]);
                size_++;
            }
        }
    }
    
    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            
            for (size_t i = 0; i < size_; i++) {
                data[i].~T();
            }
            free(data);
            
            
            data = nullptr;
            size_ = 0;
            capacity_ = 0;
            
            if (other.size_ > 0) {
                data = static_cast<T*>(malloc(other.capacity_ * sizeof(T)));
                if (!data) {
                    throw std::bad_alloc();
                }
                capacity_ = other.capacity_;
                for (size_t i = 0; i < other.size_; i++) {
                    new (&data[i]) T(other.data[i]);
                    size_++;
                }
            }
        }
        return *this;
    }
    
    void push_back(const T& value) {
        if (size_ >= capacity_) {
            size_t new_capacity = (capacity_ == 0) ? 4 : capacity_ * 2;
            resize(new_capacity);
        }
        new (&data[size_]) T(value);
        size_++;
    }
    
    void pop_back() {
        if (size_ > 0) {
            size_--;
            data[size_].~T();
        }
    }
    
    T& operator[](size_t index) {
        return data[index];
    }
    
    const T& operator[](size_t index) const {
        return data[index];
    }
    
    size_t size() const {
        return size_;
    }
    
    size_t capacity() const {
        return capacity_;
    }
    
    bool empty() const {
        return size_ == 0;
    }
    
    void clear() {
        for (size_t i = 0; i < size_; i++) {
            data[i].~T();
        }
        size_ = 0;
    }
    
    void reserve(size_t new_capacity) {
        if (new_capacity > capacity_) {
            resize(new_capacity);
        }
    }
    
    T* begin() {
        return data;
    }
    
    T* end() {
        return data + size_;
    }
    
    const T* begin() const {
        return data;
    }
    
    const T* end() const {
        return data + size_;
    }
    
    
    void sort() {
        for (size_t i = 1; i < size_; i++) {
            T key = data[i];
            int j = i - 1;
            while (j >= 0 && data[j] > key) {
                data[j + 1] = data[j];
                j--;
            }
            data[j + 1] = key;
        }
    }
    
    
    void quicksort(int left, int right) {
        if (left >= right) return;
        
        T pivot = data[(left + right) / 2];
        int i = left, j = right;
        
        while (i <= j) {
            while (data[i] < pivot) i++;
            while (data[j] > pivot) j--;
            
            if (i <= j) {
                T temp = data[i];
                data[i] = data[j];
                data[j] = temp;
                i++;
                j--;
            }
        }
        
        if (left < j) quicksort(left, j);
        if (i < right) quicksort(i, right);
    }
    
    void sort_quick() {
        if (size_ > 1) {
            quicksort(0, size_ - 1);
        }
    }
};

#endif 