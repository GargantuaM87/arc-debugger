#ifndef ADB_STOPPOINT_COLLECTION_HPP
#define ADB_STOPPOINT_COLLECTION_HPP

#include <vector>
#include <memory>
#include "./types.hpp"

namespace adb {
    template <class StopPoint>
    class stopPoint_collection {
        public:
            StopPoint& push(std::unique_ptr<StopPoint> bs);

            bool contains_id(typename StopPoint::id_type id) const;
            bool contains_adress(virt_addr adress) const;
            bool enabled_stopPoint_at_adress(virt_addr adress) const;

            StopPoint& get_by_id(typename StopPoint::id_type id);
            const StopPoint& get_by_id(typename StopPoint::id_type id) const;
            StopPoint& get_by_adress(virt_addr address);
            const StopPoint& get_by_adress(virt_addr address) const;

            void remove_by_id(typename StopPoint::id_type id);
            void remove_by_adress(virt_addr address);

            template <class F>
            void for_each(F f);
            template <class F>
            void for_each(F f) const;

            std::size_t size() const {return stopPoints_.size();}
            bool empty() const {return stopPoints_.empty();}
        private:
            using points_t = std::vector<std::unique_ptr<StopPoint>>;
            points_t stopPoints_;
    };
}
#endif
