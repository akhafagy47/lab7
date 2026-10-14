#ifndef REGISTRY_H
#define REGISTRY_H

#include <cassert>
#include <cstddef>  // size_t
#include <type_traits>
#include <utility>  // std::forward
#include <vector>

// Stores all instances of a single component type T contiguously. Each stored object gets a
// ticket (its slot index). Removed slots are recycled before storage is grown.
template <typename T>
class Registry {
public:
    // Copies/moves `value` into the next available slot (recycled slots first) and returns the
    // slot's ticket. Amortized O(1).
    template <typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, T>
    size_t store(U&& value) {
        size_t id;
        if (!freeIds_.empty()) {
            id = freeIds_.back();
            freeIds_.pop_back();
            data_[id].value = std::forward<U>(value);
            alive_[id] = true;
        } else {
            id = data_.size();
            data_.push_back(Slot{std::forward<U>(value)});
            alive_.push_back(true);
        }
        ++size_;
        return id;
    }

    // Removes the object at a live ticket, releasing its resources by assigning a fresh T{} over
    // it, and marks the ticket for recycling. Amortized O(1).
    void remove(size_t id) {
        assert(valid(id));
        data_[id].value = T{};
        alive_[id] = false;
        freeIds_.push_back(id);
        --size_;
    }

    // O(1) access to the object stored under a live ticket.
    T& operator[](size_t id) {
        assert(valid(id));
        return data_[id].value;
    }

    const T& operator[](size_t id) const {
        assert(valid(id));
        return data_[id].value;
    }

    // True iff the slot exists and currently holds a stored object. O(1).
    bool valid(size_t id) const { return id < alive_.size() && alive_[id]; }

    // Number of currently stored (active) objects. O(1).
    size_t size() const { return size_; }

    void reserve(size_t capacity) {
        data_.reserve(capacity);
        alive_.reserve(capacity);
    }

private:
    // Wrapping T keeps std::vector<bool>'s bit-packing out of the component storage, so
    // operator[] can hand out a real T& even for Registry<bool>.
    struct Slot {
        T value;
    };

    std::vector<Slot> data_;
    std::vector<bool> alive_;     // alive_[id] iff ticket id is currently handed out
    std::vector<size_t> freeIds_;  // removed tickets waiting to be recycled
    size_t size_ = 0;
};

#endif  // REGISTRY_H
