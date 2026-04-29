#ifndef ADB_STOPPOINT_COLLECTION_HPP
#define ADB_STOPPOINT_COLLECTION_HPP

#include <vector>
#include <memory>
#include <algorithm>
#include "./types.hpp"
#include "./error.hpp"

namespace adb {
    template <class StopPoint>
    class stopPoint_collection {
        public:
            StopPoint& push(std::unique_ptr<StopPoint> bs);

            bool contains_id(typename StopPoint::id_type id) const;
            bool contains_address(virt_addr address) const;
            bool enabled_stopPoint_at_address(virt_addr address) const;

            StopPoint& get_by_id(typename StopPoint::id_type id);
            const StopPoint& get_by_id(typename StopPoint::id_type id) const;
            StopPoint& get_by_address(virt_addr address);
            const StopPoint& get_by_address(virt_addr address) const;

            void remove_by_id(typename StopPoint::id_type id);
            void remove_by_address(virt_addr address);

            template <class F>
            void for_each(F f);
            template <class F>
            void for_each(F f) const;

            std::size_t size() const {return stopPoints_.size();}
            bool empty() const {return stopPoints_.empty();}
        private:
            using points_t = std::vector<std::unique_ptr<StopPoint>>;

            typename points_t::iterator find_by_id(typename StopPoint::id_type id);
            typename points_t::const_iterator find_by_id(typename StopPoint::id_type id) const;
            typename points_t::iterator find_by_address(virt_addr address);
            typename points_t::const_iterator find_by_address(virt_addr address) const;

            points_t stopPoints_;
    };
}
// Push
namespace adb {
    template <class StopPoint>
    StopPoint& stopPoint_collection<StopPoint>::push(std::unique_ptr<StopPoint> breaksite)
    {
        stopPoints_.push_back(std::move(breaksite));
        return *stopPoints_.back();
    }
}
// Iterators
namespace adb {
    template <class StopPoint>
    auto stopPoint_collection<StopPoint>::find_by_id(typename StopPoint::id_type id) ->
        typename points_t::iterator {
            return std::find_if(begin(stopPoints_), end(stopPoints_),
                [=](auto& point) { return point->id() == id; });
        }
    template <class StopPoint>
    auto stopPoint_collection<StopPoint>::find_by_id(typename StopPoint::id_type id) const ->
        typename points_t::const_iterator {
            return const_cast<stopPoint_collection*>(this)->find_by_id(id);
        }
    template <class StopPoint>
    auto stopPoint_collection<StopPoint>::find_by_address(virt_addr address) ->
        typename points_t::iterator {
            return std::find_if(begin(stopPoints_), end(stopPoints_),
                [=](auto& point) { return point->at_address(address); });
        }

    template <class StopPoint>
    auto stopPoint_collection<StopPoint>::find_by_address(virt_addr address) const ->
        typename points_t::const_iterator {
            return const_cast<stopPoint_collection*>(this)->find_by_address(address);
            }
}
// Booleans
namespace adb {
    template <class StopPoint>
    bool stopPoint_collection<StopPoint>::contains_id(typename StopPoint::id_type id) const {
         return find_by_id(id) != end(stopPoints_);
    }

    template <class StopPoint>
    bool stopPoint_collection<StopPoint>::contains_address(virt_addr address) const {
        return find_by_address(address) != end(stopPoints_);
    }

    template<class StopPoint>
    bool stopPoint_collection<StopPoint>::enabled_stopPoint_at_address(virt_addr address) const {
        return contains_address(address) and get_by_address(address).is_enabled();
    }
}
// Getters
namespace adb {
    template <class StopPoint>
    StopPoint& stopPoint_collection<StopPoint>::get_by_id(typename StopPoint::id_type id) {
        auto it = find_by_id(id);
        if(it == end(stopPoints_))
            error::send("Invalid stopPoint id");
        return **it; // dereference twice. Once for the iterator. Twice for the unique_ptr it references
    }

    template <class StopPoint>
    const StopPoint& stopPoint_collection<StopPoint>::get_by_id(typename StopPoint::id_type id) const {
        return const_cast<stopPoint_collection*>(this)->get_by_id(id);
    }

    template <class StopPoint>
    StopPoint& stopPoint_collection<StopPoint>::get_by_address(virt_addr address) {
        auto it = find_by_address(address);
        if(it == end(stopPoints_))
            error::send("StopPoint with given address not found");
        return **it;
    }

    template<class StopPoint>
    const StopPoint& stopPoint_collection<StopPoint>::get_by_address(virt_addr address) const {
        return const_cast<stopPoint_collection*>(this)->get_by_address(address);
    }
}
// Remove Functions
namespace adb {
    template <class StopPoint>
    void stopPoint_collection<StopPoint>::remove_by_id(typename StopPoint::id_type id) {
        auto it = find_by_id(id);
        (**it).disable();
        stopPoints_.erase(it);
    }

    template <class StopPoint>
    void stopPoint_collection<StopPoint>::remove_by_address(virt_addr address) {
        auto it = find_by_address(address);
        (**it).disable();
        stopPoints_.erase(it);
    }
}
// For-Each Functions
namespace adb {
    template <class StopPoint>
    template <class F>
    void stopPoint_collection<StopPoint>::for_each(F f) {
        for(auto& point : stopPoints_) {
            f(*point);
        }
    }

    template <class StopPoint>
    template <class F>
    void stopPoint_collection<StopPoint>::for_each(F f) const {
        for(const auto& point : stopPoints_) {
            f(*point);
        }
    }
}
#endif
