#include <ember/dom/Container.hpp>
#include <s101/PackageFlag.hpp>
#include <type_traits>
#include <utility>

int main()
{
    using Container = libember::dom::Container;
    using ConstIterator = Container::const_iterator;
    using Dereference = decltype(*std::declval<ConstIterator>());
    using Arrow = decltype(std::declval<ConstIterator>().operator->());

    static_assert(std::is_same<Dereference, const libember::dom::Node&>::value,
                  "libember Container::const_iterator dereference contract changed; update EmberClient::visitContainer");
    static_assert(std::is_same<Arrow, const libember::dom::Node*>::value,
                  "libember Container::const_iterator arrow contract changed; update EmberClient::visitContainer");

    // Deep GetDirectory traversal relies on the S101 sequence boundary, not on
    // the first decoded DOM root. Pin the package-flag contract we use in
    // EmberClient::handleS101Packet().
    const unsigned char last = libs101::PackageFlag::LastPackage;
    const unsigned char first = libs101::PackageFlag::FirstPackage;
    if (last == 0 || first == 0 || last == first) return 2;
    if ((last & libs101::PackageFlag::LastPackage) == 0) return 3;
    if ((first & libs101::PackageFlag::LastPackage) != 0) return 4;

    return 0;
}
