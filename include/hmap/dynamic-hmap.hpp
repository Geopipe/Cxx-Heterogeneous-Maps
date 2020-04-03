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

#include <any>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <utility>
#include <algorithm>

#include <boost/iterator/transform_iterator.hpp>

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
		const std::string key;
		const KeyTagBase &tag;
		
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
		std::pair<const KeyBase, std::any> operator,(A&& a) const {
			auto ret = std::pair<KeyBase, std::any>(*this, std::any());
			ret.second = V(std::forward<A>(a)); // Does this work better than emplace?
			return ret;
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
	
public:
	
	DynamicHMap(const DynamicHMap& other); // Copy Constructor
	DynamicHMap(DynamicHMap&& other); // Move Constructor
	
	DynamicHMap& operator=(const DynamicHMap& other); // Copy Assignment Operator
	DynamicHMap& operator=(DynamicHMap&& other); // Move Assignment Operator


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
	
	iterator begin();
	iterator end();
	template<typename V>
	auto end() {
		return boost::make_transform_iterator<AnyCaster<V>>(end());
	}
	
	const_iterator cbegin() const;
	const_iterator cend() const;
	template<typename V>
	auto cend() const {
		return boost::make_transform_iterator<ConstAnyCaster<V>>(cend());
	}
	
	size_t size() const;
	bool empty() const;
	
	
	template<typename V>
	auto find(const detail::Key<V>& k) {
		iterator found = map_.find(k);
		iterator theEnd = end();
		return boost::make_transform_iterator<AnyCaster<V>>(((found == theEnd) || (&(found->first.tag) != &(const detail::KeyTagBase&)KeyTag<V>::tag())) ? theEnd : found);
	}
	
	template<typename V>
	auto find(const detail::Key<V>& k) const {
		const_iterator found = map_.find(k);
		const_iterator theEnd = cend();
		return boost::make_transform_iterator<ConstAnyCaster<V>>(((found == theEnd) || (&(found->first.tag) != &(const detail::KeyTagBase&)KeyTag<V>::tag())) ? theEnd : found);
	}
	
	template<typename V>
	size_t erase(const detail::Key<V>& k) {
		auto found = map_.find(k);
		if((found == map_.end()) || (&(found->first.tag) != &(const detail::KeyTagBase&)KeyTag<V>::tag())) {
			return 0;
		} else {
			map_.erase(found);
			return 1;
		}
	}
};

template<typename V>
detail::Key<V> dK(const std::string&k){
	return detail::Key<V>(k);
}

template<typename ...Vs>
DynamicHMap make_dynamic_hmap(Vs&& ...vs) {
	return DynamicHMap((std::initializer_list<std::pair<const detail::KeyBase, std::any>>){std::forward<Vs>(vs)...});
};
