#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include <array>
#include <cassert>
#include <cstddef>  // size_t
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>  // std::forward
#include <vector>

#include "pack.h"  // Pack from prep
#include "registry.h"

using EntityID = size_t;

template <typename... ComponentTypes>
class EntityManager {
    using Components = Pack<ComponentTypes...>;
    static constexpr size_t NUM_COMPONENT_TYPES = sizeof...(ComponentTypes);

public:
    static constexpr size_t INVALID_COMPONENT_IDX = std::numeric_limits<size_t>::max();

    // Returns a fresh entity ID, recycling removed IDs first. The new entity has no components.
    EntityID createEntity() {
        EntityID id;
        if (!freeEntityIds_.empty()) {
            id = freeEntityIds_.back();
            freeEntityIds_.pop_back();
            alive_[id] = true;
        } else {
            id = componentIdx_.size();
            componentIdx_.emplace_back();
            alive_.push_back(true);
        }
        componentIdx_[id].fill(INVALID_COMPONENT_IDX);
        return id;
    }

    // True iff the entity has been created more recently than it has been removed.
    bool validEntity(EntityID id) const { return id < alive_.size() && alive_[id]; }

    template <typename ComponentType>
    bool hasComponent(EntityID id) const {
        static_assert(Components::template contains<ComponentType>, "not a component type");
        assert(validEntity(id));
        return componentIdx_[id][typeIndex<ComponentType>] != INVALID_COMPONENT_IDX;
    }

    template <typename ComponentType>
    ComponentType& getComponent(EntityID id) {
        static_assert(Components::template contains<ComponentType>, "not a component type");
        assert(hasComponent<ComponentType>(id));
        return registry<ComponentType>()[componentIdx_[id][typeIndex<ComponentType>]];
    }

    template <typename ComponentType>
    const ComponentType& getComponent(EntityID id) const {
        static_assert(Components::template contains<ComponentType>, "not a component type");
        assert(hasComponent<ComponentType>(id));
        return registry<ComponentType>()[componentIdx_[id][typeIndex<ComponentType>]];
    }

    template <typename ComponentType>
    void addComponent(EntityID id, ComponentType&& component) {
        using T = std::remove_cvref_t<ComponentType>;
        static_assert(Components::template contains<T>, "not a component type");
        assert(!hasComponent<T>(id));
        componentIdx_[id][typeIndex<T>] =
            registry<T>().store(std::forward<ComponentType>(component));
    }

    // Not required for the lab, but the bookkeeping above is written with these in mind.
    template <typename ComponentType>
    void removeComponent(EntityID id) {
        static_assert(Components::template contains<ComponentType>, "not a component type");
        assert(hasComponent<ComponentType>(id));
        size_t& idx = componentIdx_[id][typeIndex<ComponentType>];
        registry<ComponentType>().remove(idx);
        idx = INVALID_COMPONENT_IDX;
    }

    void removeEntity(EntityID id) {
        assert(validEntity(id));
        (removeComponentIfPresent<ComponentTypes>(id), ...);
        alive_[id] = false;
        freeEntityIds_.push_back(id);
    }

private:
    template <typename ComponentType>
    static constexpr size_t typeIndex = Components::template index<ComponentType>;

    template <typename ComponentType>
    Registry<ComponentType>& registry() {
        return std::get<Registry<ComponentType>>(registries_);
    }

    template <typename ComponentType>
    const Registry<ComponentType>& registry() const {
        return std::get<Registry<ComponentType>>(registries_);
    }

    template <typename ComponentType>
    void removeComponentIfPresent(EntityID id) {
        if (hasComponent<ComponentType>(id)) removeComponent<ComponentType>(id);
    }

    // One registry (SoA storage) per component type.
    std::tuple<Registry<ComponentTypes>...> registries_;
    // componentIdx_[entity][typeIndex<C>] is the entity's ticket in Registry<C>, or
    // INVALID_COMPONENT_IDX. Each entity's row is a fixed-size array, so the whole mapping is one
    // contiguous allocation with O(1) lookup.
    std::vector<std::array<size_t, NUM_COMPONENT_TYPES>> componentIdx_;
    std::vector<bool> alive_;
    std::vector<EntityID> freeEntityIds_;
};

#endif  // ENTITY_MANAGER_H
