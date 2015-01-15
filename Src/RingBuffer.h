#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <atomic>

template<class kind, int length> class RingBuffer {
    public:
        RingBuffer();
        int          capacity();
        void         push_back(kind object);
        void         pop_front(kind &object);
        bool         empty() const { return head == tail; }
        uint32_t     get_overflow() const { return overflow; }

    private:
        kind         buffer[length];
        uint8_t      tail, head;
        uint32_t     overflow;
};

template<class kind, int length> RingBuffer<kind, length>::RingBuffer(){
    this->tail = 0;
    this->head = 0;
    this->overflow = 0;
}

template<class kind, int length>  int RingBuffer<kind, length>::capacity(){
    return length-1;
}

template<class kind, int length> void RingBuffer<kind, length>::push_back(kind object){
    this->buffer[this->head] = object;
    this->head = (head+1)&(length-1);
    if(this->head == this->tail) {
        // we wrap discarding the old tail
        this->tail = (this->tail+1)&(length-1);
        overflow++;
    }
}

template<class kind, int length> void RingBuffer<kind, length>::pop_front(kind &object){
    __disable_irq();
    object = this->buffer[this->tail];
    this->tail = (this->tail+1)&(length-1);
    __enable_irq();
}
#endif
