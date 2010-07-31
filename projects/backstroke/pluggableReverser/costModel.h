#ifndef BACKSTROKE_COSTMODEL_H
#define	BACKSTROKE_COSTMODEL_H

/*! This simple cost model just records the count of stores to stacks. */
class SimpleCostModel
{
    int store_count_;

public:

    SimpleCostModel()
    : store_count_(0)
    {}

    void increaseStoreCount(int size = 0) { ++store_count_; }

    int getCost() const { return store_count_; }

    SimpleCostModel& operator += (const SimpleCostModel& cost)
    {
        store_count_ += cost.store_count_;
        return *this;
    }
};




#endif	/* BACKSTROKE_COSTMODEL_H */

