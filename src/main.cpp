#include <iostream>
#include <memory>
#include <string>

#include "entity_manager.h"
#include "registry.h"

static int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #cond "\n"; \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

static void testRegistry() {
    Registry<std::string> r;
    CHECK(r.size() == 0);
    CHECK(!r.valid(0));

    std::string s = "copied";
    size_t a = r.store(s);                    // copy
    size_t b = r.store(std::string("moved"));  // move
    size_t c = r.store(std::string("third"));
    CHECK(a == 0 && b == 1 && c == 2);
    CHECK(s == "copied");  // lvalue was copied, not moved from
    CHECK(r[a] == "copied" && r[b] == "moved" && r[c] == "third");
    CHECK(r.size() == 3);

    r[b] = "changed";
    CHECK(r[b] == "changed");
    const auto& cr = r;
    CHECK(cr[b] == "changed");

    r.remove(b);
    CHECK(!r.valid(b));
    CHECK(r.valid(a) && r.valid(c));
    CHECK(r.size() == 2);

    // Recycled ticket is preferred over growing.
    size_t d = r.store(std::string("recycled"));
    CHECK(d == b);
    CHECK(r[d] == "recycled");
    CHECK(r.store(std::string("new")) == 3);
    CHECK(r.size() == 4);
    CHECK(!r.valid(100));

    // remove() releases the resources held by the stored object.
    Registry<std::shared_ptr<int>> tr;
    auto res = std::make_shared<int>(7);
    size_t id = tr.store(res);
    CHECK(res.use_count() == 2);
    tr.remove(id);
    CHECK(res.use_count() == 1);

    // Move-only types work.
    Registry<std::unique_ptr<int>> ur;
    size_t u = ur.store(std::make_unique<int>(42));
    CHECK(*ur[u] == 42);
    ur.remove(u);
    CHECK(ur.store(std::make_unique<int>(43)) == u);
    CHECK(*ur[u] == 43);
}

static void testEntityManager() {
    EntityManager<int, float, bool> em;

    // Example from the handout.
    EntityID e = em.createEntity();
    em.addComponent(e, 5);
    CHECK(em.hasComponent<int>(e));
    CHECK(!em.hasComponent<float>(e));
    CHECK(!em.hasComponent<bool>(e));
    CHECK(em.getComponent<int>(e) == 5);

    EntityID e1 = em.createEntity();
    EntityID e2 = em.createEntity();
    CHECK(e == 0 && e1 == 1 && e2 == 2);
    CHECK(em.validEntity(e1) && !em.validEntity(3) && !em.validEntity(12345));

    // lvalue, const lvalue and rvalue arguments.
    float f = 2.5f;
    const bool flag = true;
    em.addComponent(e1, f);
    em.addComponent(e1, flag);
    em.addComponent(e2, 9);
    CHECK(em.getComponent<float>(e1) == 2.5f);
    CHECK(em.getComponent<bool>(e1));
    CHECK(!em.hasComponent<int>(e1));
    CHECK(em.getComponent<int>(e2) == 9);

    // Components are mutable through the returned reference.
    em.getComponent<int>(e) += 1;
    CHECK(em.getComponent<int>(e) == 6);
    CHECK(em.getComponent<int>(e2) == 9);

    // Removal (optional for the lab) recycles entity IDs, and recycled entities start empty.
    em.removeEntity(e1);
    CHECK(!em.validEntity(e1));
    EntityID e3 = em.createEntity();
    CHECK(e3 == e1);
    CHECK(!em.hasComponent<float>(e3) && !em.hasComponent<bool>(e3));
    CHECK(em.createEntity() == 3);

    em.removeComponent<int>(e);
    CHECK(!em.hasComponent<int>(e));
    em.addComponent(e, 11);
    CHECK(em.getComponent<int>(e) == 11);
    CHECK(em.getComponent<int>(e2) == 9);

    // Non-trivial component types.
    EntityManager<std::string, std::unique_ptr<int>> em2;
    EntityID x = em2.createEntity();
    em2.addComponent(x, std::string("name"));
    em2.addComponent(x, std::make_unique<int>(3));
    CHECK(em2.getComponent<std::string>(x) == "name");
    CHECK(*em2.getComponent<std::unique_ptr<int>>(x) == 3);

    // em.getComponent<double>(e);  // static_assert: not a component type
    // em.getComponent<bool>(e);    // assertion failure: e has no bool
}

int main() {
    testRegistry();
    testEntityManager();
    if (failures == 0) {
        std::cout << "All tests passed\n";
        return 0;
    }
    std::cout << failures << " check(s) failed\n";
    return 1;
}
