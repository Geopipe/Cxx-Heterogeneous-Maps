#pragma once
/************************************************************************************
 *
 * Author: Thomas Dickerson
 * Copyright: 2019 - 2020, Geopipe, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ************************************************************************************/

#include <algorithm>
#include <any>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>
#include <utility>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/optional/optional.hpp>

namespace detail {
	struct KeyTagBase {
		virtual ~KeyTagBase();
		
	};
}

template<typename V>
class KeyTag : public detail::KeyTagBase {
	KeyTag() {}
	KeyTag(const KeyTag&) = delete;
	KeyTag(KeyTag&&) = delete;
  public:
	static const KeyTag<V>& tag() {
		static KeyTag<V> theTag_ = KeyTag<V>();
		return theTag_;
	}
};

namespace detail {
	struct KeyBase {
		std::string key;
		std::reference_wrapper<const KeyTagBase> tag;
		
		KeyBase(const std::string &ki, const KeyTagBase &ti)
		    : key(ki), tag(std::cref(ti)) {}
		KeyBase(const KeyBase&) = default;
		KeyBase(KeyBase&&) = default;
		KeyBase& operator= (const KeyBase&) = default;
		KeyBase& operator= (KeyBase&&) = default;
		virtual ~KeyBase();
		
		bool operator<(const KeyBase &k) const {
			return key < k.key ||
		           (key == k.key && &(tag.get()) < &(k.tag.get()));
		}
	
		bool operator==(const KeyBase &k) const {
			return key == k.key &&
			       &(tag.get()) == &(k.tag.get());
		}
	
		bool operator!=(const KeyBase &k) const {
			return key != k.key || &(tag.get()) != &(k.tag.get());
		}

	};
	
	template<typename V>
	struct Key : KeyBase {
		Key(const std::string &k)
		: KeyBase(k, KeyTag<V>::tag()) {}
		
		Key(const Key<V> &k)
		: KeyBase(k) {}
		
		// Build versions of DynamicHMap map key-value pairs
		// (note that KeyBase is left non-const to allow use of std::sort...
		// in the actual key-value pair, KeyBase will be const)
		template<class A>
		std::pair<KeyBase, std::any>
		operator,(A&& a) const {
			return std::pair<KeyBase, std::any>(std::piecewise_construct,
			                                    std::forward_as_tuple(std::cref(*((KeyBase *)this))),
			                                    std::forward_as_tuple(std::any{std::in_place_type<V>,
			                                                                   std::forward<A>(a)}));
		}
	};
}


// Dynamic HMap class
// As suggested by C++ Core Guidelines, Rule C.20 ("If you can avoid defining
// default operations, do so"), this class uses the Rule of Zero

class DynamicHMap {
	std::map<detail::KeyBase, std::any> map_;
	
  public:
	using value_type = decltype(map_)::value_type;
	using iterator = decltype(map_)::iterator;
	using const_iterator = decltype(map_)::const_iterator;
	
	template<typename V> using specific_value_type = std::pair<detail::KeyBase, V&>;
	template<typename V> using const_specific_value_type = std::pair<detail::KeyBase, const V&>;

  private:
	
	template<typename V>
	struct AnyCaster {
		specific_value_type<V> operator()(value_type &v) const {
			return specific_value_type<V>(v.first, std::any_cast<V&>(v.second));
		}
	};
	
	template<typename V>
	struct ConstAnyCaster {
		const_specific_value_type<V> operator()(const value_type &v) const {
			return const_specific_value_type<V>(v.first, std::any_cast<const V&>(v.second));
		}
	};
	
	// loadHMap implementation function
	// By inserting pairs into map storage in ascending key
	// order using a hint, each insertion is done in constant time
	template <size_t N, std::size_t... I>
	constexpr void loadHMapImpl(std::array<std::pair<detail::KeyBase, std::any>, N>&& a,
	                            std::index_sequence<I...>) {
		(static_cast<void>(map_.emplace_hint(map_.cend(), std::move(a[I]))),
		 ...);
}
	
	// Move N sorted key-value pairs into array in key order in O(N) time
	template <size_t N, typename Indices = std::make_index_sequence<N> >
	constexpr void loadHMap(std::array<std::pair<detail::KeyBase, std::any>, N>&& a) {
		loadHMapImpl(std::move(a), Indices{});
	}
	
	// Extract a single key-value pair from the map, returning a type tag-node handle pair
	template <typename V>
	auto extractOne(const detail::Key<V>& k) -> decltype(std::make_pair(k, std::move(map_.extract(k)))) {
		return std::make_pair(k, std::move(map_.extract(k)));
	}
	
	// Extract a single key-value pair from the map, returning an
	// optional that contains the value
	template <typename V>
	boost::optional<V> optCheckOutOne(const detail::Key<V>& k) {
		boost::optional<V> retval;
		auto mapNodeHandle = map_.extract(k);
		if (mapNodeHandle) {
			retval.emplace(std::any_cast<V&&>(std::move(mapNodeHandle.mapped())));
		}
		return retval;
	}
	
	// Insert values from compatible node handles into the map
	template <typename... DataTypes, typename... Args, size_t... Is>
	void insertHelper(std::tuple<DataTypes...> dataTup,
	                  std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(insertOne(args, std::get<Is>(dataTup).first,
		                             std::move(std::get<Is>(dataTup).second))),
		 ...);
	}
	
	// Insert a value from a compatible node handle into the map
	template <typename V, typename W, typename X>
	void insertOne(const detail::Key<V>& k, const detail::Key<W>& kPrime, X&& node_handle) {
		static_assert(std::is_convertible_v<W, V>);
		if (node_handle) {
			// If allocators are type compatible, use move construction
			// TODO: Loosen this a bit for polymorphic allocators
			if constexpr (std::is_same<decltype(map_.get_allocator()),
			              decltype(node_handle.get_allocator())>::value) {
				if (map_.get_allocator() == node_handle.get_allocator()) {
					// If we can use the same key, do a node handle insert
					// If we cannot, use try_emplace with move construction of
					// the value portion of the key-value pair
					if (k == kPrime) {
						map_.insert(std::move(node_handle));
					} else {
						map_.try_emplace(k, std::move(node_handle.mapped()));
					}
				} else {
					// Make sure we copy the value instead of moving it
					map_.try_emplace(k, node_handle.mapped());
				}
			} else {
				// Make sure we copy the value instead of moving it
				map_.try_emplace(k, node_handle.mapped());
			}
		}
	}
	
	// Copy a value from from the map, returning a boost<const V&>
	template <typename V>
	boost::optional<const V&> optCopyOutOne(const detail::Key<V>& k) const {
		boost::optional<const V&> retval;
		if (auto it = map_.find(k); map_.cend() != it) {
			retval.emplace(std::any_cast<const V&>(map_.at(k)));
		}
		return retval;
	}
	
	// Copy a value from from the map, returning a boost<V&>
	template <typename V>
	boost::optional<V&> optCopyOutOne(const detail::Key<V>& k) {
		boost::optional<V&> retval;
		if (auto it = map_.find(k); map_.end() != it) {
			retval.emplace(std::any_cast<V&>(map_.at(k)));
		}
		return retval;
	}
	
	// Copy a value from from the map, returning a const V*
	template <typename V>
	const V* ptrCopyOutOne(const detail::Key<V>& k) const {
		const V* retval = nullptr;
		if (auto it = map_.find(k); map_.cend() != it) {
			retval = std::any_cast<const V*>(map_.at(k));
		}
		return retval;
	}
	
	// Copy a value from from the map, returning a V*
	template <typename V>
	V* ptrCopyOutOne(const detail::Key<V>& k) {
		V* retval = nullptr;
		if (auto it = map_.find(k); map_.end() != it) {
			retval = std::any_cast<V*>(map_.at(k));
		}
		return retval;
	}
	
	// Insert values from boost::optional objects into the map as
	// directed by the corresponding keys
	template <typename... DataTypes, typename... Args, size_t... Is>
	void optCheckInHelper(std::tuple<DataTypes...> dataTup, std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(optCheckInOne(std::move(std::get<Is>(dataTup)),
		                                 args)),
		 ...);
	}
	
	// Move a value from a boost::optional object into the map as
	// directed by the corresponding key
	template <typename V>
	void optCheckInOne(boost::optional<V>&& arg, const detail::Key<V>& k) {
		if (boost::none != arg) {
			std::any& vHolder = map_[k];
			if (!vHolder.has_value()) {
				vHolder.emplace<V>();  // The type must be default constructible
			}
			V& vRef = std::any_cast<V&>(vHolder);
			vRef = std::move(arg).value();
		}
	}
	
	// Copy values from boost::optional<V&> objects into the map as
	// directed by the corresponding keys
	template <typename... DataTypes, typename... Args, size_t... Is>
	void optCopyInHelper(std::tuple<DataTypes...> dataTup, std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(optCopyInOne(std::move(std::get<Is>(dataTup)),
		                                args)),
		 ...);
	}
	
	// Copy a value from a boost::optional<V&> object into the map as
	// directed by the corresponding key
	template <typename V>
	void optCopyInOne(boost::optional<V&>&& arg, const detail::Key<V>& k) {
		if (boost::none != arg) {
			std::any& vHolder = map_[k];
			if (!vHolder.has_value()) {
				vHolder.emplace<V>();	 // The type must be default constructible
			}
			V& vRef = std::any_cast<V&>(vHolder);
			vRef = std::move(arg).value();
		}
	}

  public:

	template<typename ...Args>
	DynamicHMap(Args&& ...args)
	: map_(std::forward<Args>(args) ...) {}
	
	// Construct a DynamicHMap by using Key<V>, std::any pairs,
	// casting Key<V> to KeyBase to perform type erasure.
	DynamicHMap(std::in_place_t) { }

	template<typename ...Vs>
	DynamicHMap(std::in_place_t, Vs&& ...vs) {
		std::array<std::pair<detail::KeyBase, std::any>, sizeof...(Vs)> argArray
		    { vs... };
		std::sort(argArray.begin(), argArray.end(),
		              [] (const std::pair<detail::KeyBase, std::any>& left,
		                  const std::pair<detail::KeyBase, std::any>& right)
		              {
		                  return left.first < right.first;
		              });
		loadHMap(std::move(argArray));
	}

	template<typename V>
	V& operator[](const detail::Key<V>& k) {
		std::any& vHolder = map_[k];
		if(!vHolder.has_value()) {
			vHolder.emplace<V>(); // The type must be default constructible
		}
		// This will throw if it's not an appropriate type
		return std::any_cast<V&>(vHolder);
	}
	
	// Doesn't currently support various "fancy" operations
	// If you want it, add it.
	
	// Find a matching key value pair and return a reference to the value

	// Since map_.at will throw if there is no matching key and the
	// test compares the whole key... the std::string and the underlying
	// KeyBase singleton to which it refers... this means that the
	// std::any& it returns can be safely cast to V& (because the std::any
	// must contain a V)

	template<typename V>
	V& at(const detail::Key<V>& k) {
		return std::any_cast<V&>(map_.at(k));
	}
	
	template<typename V>
	const V& at(const detail::Key<V>& k) const {
		// This will throw if it's not an appropriate type
		return std::any_cast<const V&>(map_.at(k));
	}
	
    template<typename V, typename ...Args>
	auto try_emplace(const detail::Key<V>& k, Args&& ...args) {
		auto [iter, inserted] =
				map_.try_emplace(k, std::any{std::in_place_type<V>,
							std::forward<Args>(args)...});
		return std::make_pair(boost::make_transform_iterator<AnyCaster<V> >(iter),
							  inserted);
	}

    template<typename V, typename ...Args>
	auto insert_or_assign(const detail::Key<V>& k, Args&& ...args) {
		auto [iter, inserted] =
				map_.insert_or_assign(k, std::any{std::in_place_type<V>,
							std::forward<Args>(args)...});
		return std::make_pair(boost::make_transform_iterator<AnyCaster<V> >(iter),
							  inserted);
	}

	// Extract key-value pairs from map for insert into another map
	template <typename... Args>
	auto extract(Args&&... args) {
		return std::make_tuple(extractOne(args)...);
	}
	
	// Insert key-value pairs into map from extract from another map
	template<typename ...Types, typename ...Args>
	void insert(std::tuple<Types...> &&tup,
	            Args&& ...args) {
		insertHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		             std::index_sequence_for<Args...>{},
		             std::forward<Args>(args)...);
	}
	
    // Extract key-value pairs from the map, returning optionals
	// that contain the values
	template <typename... Args>
	auto optCheckOut(Args&&... args) {
		return std::make_tuple(optCheckOutOne(args)...);
	}
	
	// Copy values from the map, returning pointers
	// that refer to the values.
	template <typename... Args>
	auto ptrCopyOut(Args&&... args)
	{
		return std::make_tuple(ptrCopyOutOne(args)...);
	}
	
	// Copy values from the map, returning optionals
	// that refer to the values.
	template <typename... Args>
	auto optCopyOut(Args&&... args)
	{
		return std::make_tuple(optCopyOutOne(args)...);
	}
	
	// Insert values from boost::optional objects into the map as
	// directed by the corresponding keys
	template<typename ...Types, typename ...Args>
	void optCheckIn(std::tuple<Types...> &&tup,
	                Args&& ...args) {
		optCheckInHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		                 std::index_sequence_for<Args...>{},
		                 std::forward<Args>(args)...);
	}
	
	// Copy values from boost::optional<V&> objects into the map as
	// directed by the corresponding keys
	template<typename ...Types, typename ...Args>
	void optCopyIn(std::tuple<Types...> &&tup,
	               Args&& ...args) {
		optCopyInHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		                std::index_sequence_for<Args...>{},
						std::forward<Args>(args)...);
	}
	
    iterator begin();
	iterator end();

	// Add a version of end() that can be compared against the iterator
	// returned by find for a non-const DynamicHMap
	template<typename V>
	auto end() {
		return boost::make_transform_iterator<AnyCaster<V> >(end());
	}
	
	const_iterator cbegin() const;
	const_iterator cend() const;

	// Add a version of cend() that can be compared against the iterator
	// returned by find for a const DynamicHMap
	template<typename V>
	auto cend() const {
		return boost::make_transform_iterator<ConstAnyCaster<V> >(cend());
	}
	
	size_t size() const;
	bool empty() const;
	
	
	template<typename V>
	auto find(const detail::Key<V>& k) {
		iterator found = map_.find(k);
		return boost::make_transform_iterator<AnyCaster<V> >(found);
	}
	
	template<typename V>
	auto find(const detail::Key<V>& k) const {
		const_iterator found = map_.find(k);
		return boost::make_transform_iterator<ConstAnyCaster<V> >(found);
	}
	
	template<typename V>
	size_t erase(const detail::Key<V>& k) {
		auto found = map_.find(k);
		if(map_.end() == found) {
			return 0;
		} else {
			map_.erase(found);
			return 1;
		}
	}

};

template<typename V>
detail::Key<V> dK(const std::string& k) {
	return detail::Key<V>(k);
}

template<typename V>
detail::Key<std::shared_ptr<V> > dSK(const std::string& k) {
	return detail::Key<std::shared_ptr<V> >(k);
}
	
template<typename ...Vs>
DynamicHMap make_dynamic_hmap(Vs&& ...vs) {
	return DynamicHMap(std::in_place, std::forward<Vs>(vs)...);
};
