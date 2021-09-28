#pragma once
/************************************************************************************
 * @file dynamic-hmap.hpp Like @ref hmap.hpp, provides machinery for type-safe lookup 
 * between string-like keys and values of arbitrary type. Failed lookups are detected
 * at runtime like a `std::map`. Useful for loading semi-structured data (or structured
 * data for which you don't have a formal/complete schema).
 * 
 * @author Thomas Dickerson
 * @copyright 2019 - 2021, Geopipe, Inc.
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
#include <stdexcept>
#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>
#include <utility>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/optional/optional.hpp>

namespace detail {
	/// Polymorphic base class for `KeyTag`s.
	struct KeyTagBase {
		virtual ~KeyTagBase();
	};
}

/****************************************************
 * Each `KeyTag<V>` is a singleton. The address of 
 * each `detail::KeyTagBase` base object will give 
 * us a total ordering on types.
 * 
 * Contains nothing and has no other purpose except
 * to provide a unique memory location per-type.
 ****************************************************/
template<typename V>
class KeyTag : public detail::KeyTagBase {
	KeyTag() = default;
	KeyTag(const KeyTag&) = delete;
	KeyTag(KeyTag&&) = delete;
  public:
	/// Obtain the singleton
	static const KeyTag<V>& tag() {
		static KeyTag<V> theTag_ = KeyTag<V>();
		return theTag_;
	}
};

namespace detail {
	/// Polymorphic base class for `Key`, stores the string id + reference to type tag's base object.
	struct KeyBase {
		std::string key; ///< The string we're looking up.
		std::reference_wrapper<const KeyTagBase> tag; ///< A "type tag" which is guaranteed to have a unique address for each unique type.
		
		KeyBase(const std::string &ki, const KeyTagBase &ti)
		    : key(ki), tag(std::cref(ti)) {}
		KeyBase(const KeyBase&) = default;
		KeyBase(KeyBase&&) = default;
		KeyBase& operator= (const KeyBase&) = default;
		KeyBase& operator= (KeyBase&&) = default;
		virtual ~KeyBase();
		
		/// Compare lexicographically first, then by type tag.
		bool operator<(const KeyBase &k) const {
			return key < k.key ||
		           (key == k.key && &(tag.get()) < &(k.tag.get()));
		}

		/// @return `true` if both fields are identical, `false` otherwise
		bool operator==(const KeyBase &k) const {
			return key == k.key &&
			       &(tag.get()) == &(k.tag.get());
		}
	
		/// @return `false` if both fields are identical, `true` otherwise
		bool operator!=(const KeyBase &k) const {
			return key != k.key || &(tag.get()) != &(k.tag.get());
		}
	};
	
	/// A key mapping a string to a value of type `V`.
	template<typename V>
	struct Key : KeyBase {
		Key(const std::string &k)
		: KeyBase(k, KeyTag<V>::tag()) {}
		
		Key(const Key<V> &k)
		: KeyBase(k) {}
		
		/****************************************************
		 * Create a key-value pair from this `Key`.
		 * @arg a The mapped value. Must be able to construct
		 * a `V` from this.
		 ****************************************************/
		template<class A>
		std::pair<KeyBase, std::any>
		operator,(A&& a) const {
			return {std::piecewise_construct,
			    std::forward_as_tuple(std::cref(*((KeyBase *)this))),
			    std::forward_as_tuple(std::any{std::in_place_type<V>,
			                                   std::forward<A>(a)})};
		}

	private:
		/// Unsafe constructor for use with `Key::try_rehydrate`.
		template<typename FwdKeyBase, std::enable_if_t<std::is_same_v<std::remove_reference_t<std::remove_cv_t<FwdKeyBase>>, KeyBase>, bool> = true>
		Key(FwdKeyBase && fwdKeyBase) 
		: KeyBase(std::forward<FwdKeyBase>(fwdKeyBase)) {}
	
	public:
		/// "Upcast" a `KeyBase` back into `Key<V>` if the type-tags match.
		template<typename FwdKeyBase, std::enable_if_t<std::is_same_v<std::remove_reference_t<std::remove_cv_t<FwdKeyBase>>, KeyBase>, bool> = true>
		static Key<V> try_rehydrate(FwdKeyBase&& fwdKeyBase) {
			if(&(fwdKeyBase.tag.get()) == &static_cast<const KeyTagBase&>(KeyTag<V>::tag())) {
				return Key<V>(std::forward<FwdKeyBase>(fwdKeyBase));
			} else {
				throw std::invalid_argument("Cannot rehydrate: mismatched KeyTagBase addresses");
			}
		}

	};
}


/******************************************************
 * A "dynamic" `HMap`. It is dynamic in the sense that
 * the presence of individual keys is determined at
 * run-time, rather than at compile-time.
 * 
 * Backed by a `std::map`, with the various performance
 * guarantees that entails. 
 * 
 * Also supports a functional-style lookup operation via
 * `operator(const Key<V>&)`, which returns a 
 * `boost::optional<K&>` for success (or `boost::none` on
 * failure).
 * 
 * @warning Allows two keys with the same string value
 * and different type tags, on the theory that we may
 * want to be able to talk about both 
 * [`Key<RecordLabel>("Apple")` and `Key<TechCompany>("Apple")`](https://en.wikipedia.org/wiki/Apple_Corps#Apple_Corps_v._Apple_Computer)
 * simultaneously.
 * 
 * @note Doesn't currently support various "fancy" operations
 * from `std::map`. PRs happily accepted.
 ******************************************************/
class DynamicHMap {
	std::map<detail::KeyBase, std::any> map_; ///< Backing store
	
  public:
	using value_type = decltype(map_)::value_type; ///< Type-unsafe key-value pairs
	using const_iterator = decltype(map_)::const_iterator; ///< const iterator over contents.
	
	template<typename V> using specific_value_type = std::pair<detail::KeyBase, V&>; ///< Type-safe key-value pairs
	template<typename V> using const_specific_value_type = std::pair<detail::KeyBase, const V&>; ///< Immutable type-safe key-value pairs.

	static constexpr struct multi_tag {} multi; ///< [tag-dispatch](https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Tag_Dispatching) for `operator()`.

  protected:
	using iterator = decltype(map_)::iterator; ///< Exposing a mutable iterator on backing `std::any` store breaks soundness.
	iterator begin(); ///< Permits unsound modifications to backing store, use with care
	iterator end(); ///< Permits unsound modifications to backing store, use with care

  public: 
	/// Find the `V` mapped by `k`, if present.
	template<typename V>
	boost::optional<const V&> operator()(const detail::Key<V>& k) const {
		const auto it = find(k);
		return ((cend<V>() != it)
		         ? boost::optional<const V&>(it->second)
				 : boost::none);
	}

	/// Find the `V` mapped by `k`, if present.
	template<typename V>
	boost::optional<V&> operator()(const detail::Key<V>& k) {
		const auto it = find(k);
		return ((end<V>() != it)
		         ? boost::optional<V&>(it->second)
				 : boost::none);
	}
	
	// Clear the map
	void clear();

  private:
	/// Functor for use with boost::transform_iterator
	template<typename V>
	struct AnyCaster {
		specific_value_type<V> operator()(value_type &v) const {
			return specific_value_type<V>(v.first, std::any_cast<V&>(v.second));
		}
	};
	/// Functor for use with boost::transform_iterator
	template<typename V>
	struct ConstAnyCaster {
		const_specific_value_type<V> operator()(const value_type &v) const {
			return const_specific_value_type<V>(v.first, std::any_cast<const V&>(v.second));
		}
	};
	
	
	/********************************************************
	 * `loadHMap` helper function.
	 * By inserting pairs into map storage in ascending key
	 * order using a hint, each insertion is done in constant time
	 ********************************************************/
	template <size_t N, std::size_t... I>
	constexpr void loadHMapImpl(std::array<std::pair<detail::KeyBase, std::any>, N>&& a,
	                            std::index_sequence<I...>) {
		(static_cast<void>(map_.emplace_hint(map_.cend(), std::move(a[I]))),
		 ...);
	}
	
	/// Move N sorted key-value pairs into array in key order in O(N) time
	template <size_t N, typename Indices = std::make_index_sequence<N> >
	constexpr void loadHMap(std::array<std::pair<detail::KeyBase, std::any>, N>&& a) {
		loadHMapImpl(std::move(a), Indices{});
	}
	
	/// `extract` a single key-value pair from the map, returning a type tag-node handle pair
	template <typename V>
	decltype(auto) extract1(const detail::Key<V>& k) {
		return std::make_pair(k, std::move(map_.extract(k)));
	}
	
	/// `extract` a single key-value pair from the map, returning an optional that contains the value
	template <typename V>
	boost::optional<V> optCheckOut1(const detail::Key<V>& k) {
		boost::optional<V> retval;
		auto mapNodeHandle = map_.extract(k);
		if (mapNodeHandle) {
			retval.emplace(std::any_cast<V&&>(std::move(mapNodeHandle.mapped())));
		}
		return retval;
	}
	
	/// Insert values from compatible node handles into the map
	template <typename... DataTypes, typename... Args, size_t... Is>
	void insertHelper(std::tuple<DataTypes...> dataTup,
	                  std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(insert1(args, std::get<Is>(dataTup).first,
		                             std::move(std::get<Is>(dataTup).second))),
		 ...);
	}
	
	/// Insert a value from a compatible node handle into the map
	template <typename V, typename W, typename X>
	void insert1(const detail::Key<V>& k, const detail::Key<W>& kPrime, X&& node_handle) {
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
	
	/// Insert values from boost::optional objects into the map as directed by the corresponding keys
	template <typename... DataTypes, typename... Args, size_t... Is>
	void optCheckInHelper(std::tuple<DataTypes...> dataTup, std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(optCheckIn1(std::move(std::get<Is>(dataTup)),
		                                 args)),
		 ...);
	}
	
	/// Move a value from a boost::optional object into the map as directed by the corresponding key
	template <typename V>
	void optCheckIn1(boost::optional<V>&& arg, const detail::Key<V>& k) {
		if (boost::none != arg) {
			std::any& vHolder = map_[k];
			if (!vHolder.has_value()) {
				vHolder.emplace<V>();  // The type must be default constructible
			}
			V& vRef = std::any_cast<V&>(vHolder);
			vRef = std::move(arg).value();
		}
	}
	
	/// Copy values from boost::optional<V&> objects into the map as directed by the corresponding keys
	template <typename... DataTypes, typename... Args, size_t... Is>
	void optCopyInHelper(std::tuple<DataTypes...> dataTup, std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(optCopyIn1(std::move(std::get<Is>(dataTup)),
		                                args)),
		 ...);
	}
	
	/// Copy a value from a boost::optional<V&> object into the map as directed by the corresponding key
	template <typename V>
	void optCopyIn1(boost::optional<V&>&& arg, const detail::Key<V>& k) {
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
	/// Pass args through to the underlying `std::map`.
	template<typename ...Args>
	DynamicHMap(Args&& ...args)
	: map_(std::forward<Args>(args) ...) {}

	/// Initialize map with `Vs...` key-value pairs. Do a fast array-based sort, and then linear build with insert hints.
	template<typename ...Vs>
	DynamicHMap(std::in_place_t, Vs&& ...vs) {
		// See closed [Geopipe/Cxx-Heterogeneous-Maps#5](https://github.com/Geopipe/Cxx-Heterogeneous-Maps/pull/5) 
		// for why this is fine even if `sizeof...(Vs) == 0`.
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



	/// Find a matching key value pair, _or_ default construct one, and return a reference to the value
	template<typename V>
	V& operator[](const detail::Key<V>& k) {
		std::any& vHolder = map_[k];
		if(!vHolder.has_value()) {
			vHolder.emplace<V>(); // The type must be default constructible
		}
		// This will throw if it's not an appropriate type (but it has to be, since lookup succeeded)
		return std::any_cast<V&>(vHolder);
	}
	
	/// Find a matching key value pair and return a reference to the value
	template<typename V>
	V& at(const detail::Key<V>& k) {
		return std::any_cast<V&>(map_.at(k));
	}
	/// Find a matching key value pair and return a reference to the value
	template<typename V>
	const V& at(const detail::Key<V>& k) const {
		// This will throw if it's not an appropriate type
		return std::any_cast<const V&>(map_.at(k));
	}
	/*****************************************************************
	 * Find a matching key value pair and return a `const` reference
	 * tothe value. `V` has been erased, but you might have sufficient
	 * information in client code, you might be able to rehydrate it.
	 * No non-`const` overload is provided as that permit unsound stores.
	 *****************************************************************/
	const std::any& at(const detail::KeyBase& kb) const {
		return map_.at(kb);
	}
	
	/// cf. `std::map::try_emplace`.
    template<typename V, typename ...Args>
	auto try_emplace(const detail::Key<V>& k, Args&& ...args) {
		auto [iter, inserted] =
				map_.try_emplace(k, std::any{std::in_place_type<V>,
							std::forward<Args>(args)...});
		return std::make_pair(boost::make_transform_iterator<AnyCaster<V> >(iter),
							  inserted);
	}
	/// cf. `std::map::insert_or_assign`.
    template<typename V, typename ...Args>
	auto insert_or_assign(const detail::Key<V>& k, Args&& ...args) {
		auto [iter, inserted] =
				map_.insert_or_assign(k, std::any{std::in_place_type<V>,
							std::forward<Args>(args)...});
		return std::make_pair(boost::make_transform_iterator<AnyCaster<V> >(iter),
							  inserted);
	}
	/*****************************************************************
	 * Direct access to store to the underlying map. 
	 * @pre `a` must be a `std::any` storing a type which matches the
	 * type tag in `kB`.
	 * 
	 * @warning Permits unsound stores. Use with extreme cautious.
	 *****************************************************************/
	template<typename A>
	auto unsafe_insert_or_assign(const detail::KeyBase& kB, A&& a) {
		return map_.insert_or_assign(kB, std::forward<A>(a));
	}

	/// Extract key-value pairs from map for insert into another map
	template <typename... Args>
	auto extract(Args&&... args) {
		return std::make_tuple(extract1(args)...);
	}

	/// Insert key-value pairs into map from extract from another map
	template<typename ...Types, typename ...Args>
	void insert(std::tuple<Types...> &&tup,
	            Args&& ...args) {
		insertHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		             std::index_sequence_for<Args...>{},
		             std::forward<Args>(args)...);
	}
	
    /// Extract key-value pairs from the map, returning optionals that contain the values
	template <typename... Args>
	auto optCheckOut(Args&&... args) {
		return std::make_tuple(optCheckOut1(args)...);
	}
	
	/// Convenience method to lookup multiple keys simultaneously. Returns a tuple of optional references.
	template <typename... Vs>
	auto operator()(DynamicHMap::multi_tag, const detail::Key<Vs>& ...ks)
	{
		return std::make_tuple((*this)(ks)...);
	}
	
	/// Convenience method to lookup multiple keys simultaneously. Returns a tuple of optional references.
	template <typename... Vs>
	auto operator()(DynamicHMap::multi_tag, const detail::Key<Vs>& ...ks) const
	{
		return std::make_tuple((*this)(ks)...);
	}

	// Insert values from boost::optional objects into the map as directed by the corresponding keys
	template<typename ...Types, typename ...Args>
	void optCheckIn(std::tuple<Types...> &&tup,
	                Args&& ...args) {
		optCheckInHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		                 std::index_sequence_for<Args...>{},
		                 std::forward<Args>(args)...);
	}

	// Copy values from boost::optional<V&> objects into the map as directed by the corresponding keys
	template<typename ...Types, typename ...Args>
	void optCopyIn(std::tuple<Types...> &&tup,
	               Args&& ...args) {
		optCopyInHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		                std::index_sequence_for<Args...>{},
						std::forward<Args>(args)...);
	}

	/// Add a version of end() that can be compared against the iterator returned by `find` for a non-const DynamicHMap
	template<typename V>
	auto end() {
		return boost::make_transform_iterator<AnyCaster<V> >(end());
	}
	
	const_iterator cbegin() const;
	const_iterator cend() const;

	/// Add a version of cend() that can be compared against the iterator returned by `find` for a const DynamicHMap
	template<typename V>
	auto cend() const {
		return boost::make_transform_iterator<ConstAnyCaster<V> >(cend());
	}
	
	size_t size() const; ///< Number of entries
	bool empty() const; ///< `true` if `size() == 0`, `false` otherwise.
	
	/// Return an iterator to the located key-value pair, or to `end<V>()` if none exists. Prefer `operator()`.
	template<typename V>
	auto find(const detail::Key<V>& k) {
		iterator found = map_.find(k);
		return boost::make_transform_iterator<AnyCaster<V> >(found);
	}
	/// Return an iterator to the located key-value pair, or to `cend<V>()` if none exists. Prefer `operator()`.
	template<typename V>
	auto find(const detail::Key<V>& k) const {
		const_iterator found = map_.find(k);
		return boost::make_transform_iterator<ConstAnyCaster<V> >(found);
	}
	/// Return an iterator to the located type-erased key-value pair, or to `cend()` if none exists.
	auto find(const detail::KeyBase& kb) const {
		return map_.find(kb);
	}
	
	/// cf. `std::map::erase`.
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

/// Construct a `detail::Key<V>` with string key `k`.
template<typename V>
detail::Key<V> dK(const std::string& k) {
	return detail::Key<V>(k);
}
/// Construct a `detail::Key<std::shared_ptr<V>>` with string key `k`.
template<typename V>
detail::Key<std::shared_ptr<V> > dSK(const std::string& k) {
	return detail::Key<std::shared_ptr<V> >(k);
}
/*************************************************
 * Construct a `DynamicHMap` from a sequence of 
 * key-value pairs (`std::pair<detail::Key<V>,V>`).
 * Typical usage:
 * <pre class="markdeep">
 * ```c++
 * auto myMap = make_dynamic_hmap((dK<int>("foo"), 1), (dK<float>("bar"), 2.), (dK<std::string>("baz"),"hello"));
 * ```
 * </pre>
 *************************************************/
template<typename ...Vs>
DynamicHMap make_dynamic_hmap(Vs&& ...vs) {
	return DynamicHMap(std::in_place, std::forward<Vs>(vs)...);
}
