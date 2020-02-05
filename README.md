# Cxx-Heterogeneous-Maps
A library for dictionaries which can safely contain values of multiple types.
Probably requires C++17.

Provides two data structures:
 - One is a header-only type-safe map from strings to arbitrary values, which can be updated at runtime, but for which all keys and their types must be known at compile-time. All operations on the map itself are computed at compile-time, allowing type inference to be performed when looking up keys.
 - The second is type-safe map from strings to arbitrary values allowing the key-set to be determined at runtime. This relaxation means that type inference is not supported; however, the keys stored in the map contain a tag which can be pattern-matched upon using Mach7 to help recover type information if the stored types were previously forgotten in client code.

Note that these data-structures are both currently fairly "no-frills" in terms of the supported operations; however, we are happy to accept pull requests to support more advanced behaviors.
