#pragma once
#include <Arduino.h>
#include <freertos/ringbuf.h>


/**
 * @brief A ringbuffer class based on the ESP32 FreeRTOS ringbuffer implementation
 * 
 * The main use case is to convert a stream buffer (typically bytes) to a larger fixed item
 * size (such as 16bit samples).
 * 
 * Pushing into the buffer can be done on the granularity of the ET item size, 
 * popping returns always a full output item.  
 * 
 * The implementation is multi-core and multi-thread safe. Use methods ...FromISR
 * in an interrupt service routine. 
 */
template <
    typename IT,
    typename OT,
    size_t S>

class Esp32RingBuffer
{
    RingbufHandle_t rbh;

public:
    Esp32RingBuffer()
    {
        static_assert((sizeof(OT) % sizeof(IT)) == 0, "sizeof of OT must be a multiple of sizeof of ET");
        rbh = xRingbufferCreate(S * sizeof(OT), RINGBUF_TYPE_BYTEBUF);
    }


    /* Push an input item to the end of the buffer */
    bool push(const IT inElement)
    {
        return pdTRUE == xRingbufferSend(rbh, &inElement, sizeof(IT), pdMS_TO_TICKS(10));
    }
    
    /* Push an item array to the end of the buffer */
    bool push(const IT *const inElement_p, size_t len = 1) 
    {
        return pdTRUE == xRingbufferSend(rbh, inElement_p, sizeof(IT)*len, pdMS_TO_TICKS(10));
    }

    /* Pop the data at the beginning of the buffer */
    bool pop(OT &outElement)
    {
        bool retval = false;
        size_t item_size;
        if (size() >= sizeof(OT))
        {
            OT *item_p = static_cast<OT *>(xRingbufferReceiveUpTo(rbh, &item_size, pdMS_TO_TICKS(100), sizeof(OT)));
            if (item_p != NULL)
            {
                if (item_size != sizeof(OT))
                {
                    Serial.println("Did not receive enough data, this should not happen");
                }
                else
                {
                    outElement = *item_p;
                    retval = true;
                }
                vRingbufferReturnItem(rbh, item_p);
            }
        }
        return retval;
    }

    /* Push an input item to the end of the buffer from within an interrupt service routine */
    bool pushFromISR(const IT inElement)
    {
        return pdTRUE == xRingbufferSendFromISR(rbh, &inElement, sizeof(IT), pdMS_TO_TICKS(10));
    }

    /* Push an input item array to the end of the buffer from within an interrupt service routine */
    bool pushFromISR(const IT *const inElement_p, size_t len = 1) 
    {
        return pdTRUE == xRingbufferSendFromISR(rbh, inElement_p, sizeof(IT)*len, pdMS_TO_TICKS(10));
    }

    /* Pop the data from the beginning of the buffer from within an interrupt service routine */
    bool popFromISR(OT &outElement)
    {
        bool retval = false;
        size_t item_size;
        if (size() >= sizeof(OT))
        {
            OT *item_p = static_cast<OT *>(xRingbufferReceiveUpToFromISR(rbh, &item_size, sizeof(OT)));
            if (item_p != NULL)
            {
                if (item_size != sizeof(OT))
                {
                    Serial.println("Did not receive enough data, this should not happen");
                }
                else
                {
                    outElement = *item_p;
                    retval = true;
                }
                vRingbufferReturnItemFromISR(rbh, item_p);
            }
        }
        return retval;
    }

    /* Return true if the buffer is full */
    bool isFull() { return xRingbufferGetCurFreeSize(rbh) == 0; }

    /* Return true if the buffer is empty */
    bool isEmpty() { return xRingbufferGetCurFreeSize(rbh) == xRingbufferGetMaxItemSize(rbh); }
    
    /* Reset the buffer  to an empty state */
    void clear()
    { /* not implemented */
    }
    /* return the used size of the buffer in bytes */
    size_t size() { return xRingbufferGetMaxItemSize(rbh) - xRingbufferGetCurFreeSize(rbh); }

    /* return the maximum size of the buffer in bytes*/
    size_t maxSize() { return xRingbufferGetMaxItemSize(rbh); }

    /* return the free size of the buffer in bytes*/
    size_t freeSize() { return xRingbufferGetMaxItemSize(rbh); }
};
