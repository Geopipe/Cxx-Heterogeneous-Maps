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
		
		KeyBase(const std::string &ki, const KeyTagBase &ti);
		KeyBase(const KeyBase &k);
		virtual ~KeyBase();
		
		bool operator<(const KeyBase &k) const;
		bool operator==(const KeyBase &k) const;
		bool operator!=(const KeyBase &k) const;
	};
	
	template<typename V>
	struct Key : KeyBase {
		Key(const std::string &k)
		: KeyBase(k, KeyTag<V>::tag()) {}
		
		Key(const Key<V> &k)
		: KeyBase(k) {}
		
		template<class A>
		std::pair<std::reference_wrapper<const Key<V> >, V>
		operator,(A&& a) const {
			return std::make_pair(std::cref(*this), std::forward<A>(a));
		}
	};
}

class DynamicHMap {
	std::map<detail::KeyBase, std::any> map_;
	
public:
	using value_type = decltype(map_)::value_type;
	using iterator = decltype(map_)::iterator;
	using const_iterator = decltype(map_)::const_iterator;
	
	template<typename V> using specific_value_type = std::pair<detail::KeyBase, V&>;
	template<typename V> using const_specific_value_type = std::pair<detail::KeyBase, const V&>;

	template<typename ...Vs>
	friend DynamicHMap make_dynamic_hmap(Vs&& ...vs);

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
	
    template <typename V>
    auto extractOne(const detail::Key<V>& k) -> decltype(std::make_pair(k, std::move(map_.extract(k)))) {
        return std::make_pair(k, std::move(map_.extract(k)));
    }

    template <typename V>
    boost::optional<V> optCheckOutOne(const detail::Key<V>& k) {
        boost::optional<V> retval;
	auto mapNodeHandle = map_.extract(k);
	if (mapNodeHandle &&
	    (&(mapNodeHandle.key().tag.get()) == &(const detail::KeyTagBase&)KeyTag<V>::tag())) {
	  retval.emplace(std::any_cast<V&&>(std::move(mapNodeHandle.mapped())));
	}
	return retval;
    }

    template <typename... DataTypes, typename... Args, size_t... Is>
    void insertHelper(std::tuple<DataTypes...> dataTup, std::index_sequence<Is...>, Args&&... args) {
        (static_cast<void>(insertOne(args, std::get<Is>(dataTup).first, std::move(std::get<Is>(dataTup).second))), ...);
    }

	// Insert values from a compatible node handle into the map
	template <typename V, typename W, typename X>
	void insertOne(const detail::Key<V>& k, const detail::Key<W>& kPrime, X&& node_handle) {
		if constexpr (std::is_convertible_v<W, V>) {
			if (node_handle) {
				if constexpr (std::is_same<decltype(map_.get_allocator()),
				              decltype(node_handle.get_allocator())>::value) {
						if (map_.get_allocator() == node_handle.get_allocator()) {
							if (k == kPrime) {
								map_.insert(std::move(node_handle));
							} else {
								map_.try_emplace(k, std::move(node_handle.mapped()));
							}
						} else {
							// Make sure we copy the value
							map_.try_emplace(k, node_handle.mapped());
						}
				} else {
				    // Make sure we copy the value
					map_.try_emplace(k, node_handle.mapped());
				}
			}
        	}
	}

	template <typename V>
	boost::optional<const V&> optCopyOutOne(const detail::Key<V>& k) const {
		boost::optional<const V&> retval;
		if (&(k.tag.get()) == &(const detail::KeyTagBase&)KeyTag<V>::tag()) {
			if (auto it = map_.find(k); map_.cend() != it) {
				retval.emplace(std::any_cast<const V&>(map_.at(k)));
			}
		}
		return retval;
	}
	template <typename V>
	boost::optional<V&> optCopyOutOne(const detail::Key<V>& k) {
		boost::optional<V&> retval;
		if (&(k.tag.get()) == &(const detail::KeyTagBase&)KeyTag<V>::tag()) {
			if (auto it = map_.find(k); map_.end() != it) {
				retval.emplace(std::any_cast<V&>(map_.at(k)));
			}
		}
		return retval;
	}

	template <typename... DataTypes, typename... Args, size_t... Is>
	void optCheckInHelper(std::tuple<DataTypes...> dataTup, std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(optCheckInOne(std::move(std::get<Is>(dataTup)),
										 args)),
		 ...);
	}

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

template <typename... DataTypes, typename... Args, size_t... Is>
	void optCopyInHelper(std::tuple<DataTypes...> dataTup, std::index_sequence<Is...>, Args&&... args) {
		(static_cast<void>(optCopyInOne(std::move(std::get<Is>(dataTup)),
		                                args)),
		 ...);
	}
	
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
	
	DynamicHMap(const DynamicHMap& other);  // Copy Constructor
	DynamicHMap(DynamicHMap&& other);       // Move Constructor
	
	DynamicHMap& operator=(const DynamicHMap& other);  // Copy Assignment Operator
	DynamicHMap& operator=(DynamicHMap&& other);       // Move Assignment Operator
	
	template<typename ...Args>
	DynamicHMap(Args&& ...args)
	: map_(std::forward<Args>(args) ...) {}
	
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
	
	template<typename V>
	V& at(const detail::Key<V>& k) {
		// This will throw if it's not an appropriate type
		return std::any_cast<V&>(map_.at(k));
	}
	
	template<typename V>
	const V& at(const detail::Key<V>& k) const {
		// This will throw if it's not an appropriate type
		return std::any_cast<const V&>(map_.at(k));
	}
	
	template <typename... Args>
	auto extract(Args&&... args) {
		return std::make_tuple(extractOne(args)...);
	}
	
	template<typename ...Types, typename ...Args>
	void insert(std::tuple<Types...> &&tup,
	            Args&& ...args) {
		insertHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		             std::index_sequence_for<Args...>{},
		             std::forward<Args>(args)...);
	}
	
	template <typename... Args>
	auto optCheckOut(Args&&... args) {
		return std::make_tuple(optCheckOutOne(args)...);
	}
	
	template <typename... Args>
	auto optCopyOut(Args&&... args)
	{
		return std::make_tuple(optCopyOutOne(args)...);
	}
	
	template<typename ...Types, typename ...Args>
	void optCheckIn(std::tuple<Types...> &&tup,
	                Args&& ...args) {
		optCheckInHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		                 std::index_sequence_for<Args...>{},
		                 std::forward<Args>(args)...);
	}
	
	template<typename ...Types, typename ...Args>
	void optCopyIn(std::tuple<Types...> &&tup,
	               Args&& ...args) {
		optCopyInHelper(std::forward<std::tuple<Types...> >(std::move(tup)),
		                std::index_sequence_for<Args...>{},
						std::forward<Args>(args)...);
	}
	
    iterator begin();
	iterator end();
	template<typename V>
	auto end() {
		return boost::make_transform_iterator<AnyCaster<V> >(end());
	}
	
	const_iterator cbegin() const;
	const_iterator cend() const;
	template<typename V>
	auto cend() const {
		return boost::make_transform_iterator<ConstAnyCaster<V> >(cend());
	}
	
	size_t size() const;
	bool empty() const;
	
	
	template<typename V>
	auto find(const detail::Key<V>& k) {
		iterator found = map_.find(k);
		iterator theEnd = end();
		return boost::make_transform_iterator<AnyCaster<V> >(((found == theEnd) || (&(found->first.tag.get()) != &(const detail::KeyTagBase&)KeyTag<V>::tag())) ? theEnd : found);
	}
	
	template<typename V>
	auto find(const detail::Key<V>& k) const {
		const_iterator found = map_.find(k);
		const_iterator theEnd = cend();
		return boost::make_transform_iterator<ConstAnyCaster<V> >(((found == theEnd) || (&(found->first.tag.get()) != &(const detail::KeyTagBase&)KeyTag<V>::tag())) ? theEnd : found);
	}
	
	template<typename V>
	size_t erase(const detail::Key<V>& k) {
		auto found = map_.find(k);
		if((found == map_.end()) || (&(found->first.tag.get()) != &(const detail::KeyTagBase&)KeyTag<V>::tag())) {
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
	
template<typename V>
detail::Key<std::unique_ptr<V> > dUK(const std::string& k) {
	return detail::Key<std::unique_ptr<V> >(k);
}
	
template<typename ...Vs>
DynamicHMap make_dynamic_hmap(Vs&& ...vs) {
	DynamicHMap hmap;
	(static_cast<void>([&hmap](auto commaPair) {
	                       hmap.map_.try_emplace(commaPair.first,
	                                             std::move(commaPair.second));
	                       }(vs)),
	    ...);
	return hmap;
};
