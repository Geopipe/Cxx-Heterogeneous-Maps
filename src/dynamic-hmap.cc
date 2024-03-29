/************************************************************************************
 *
 * Author: Thomas Dickerson
 * Copyright: 2019 - 2022, Geopipe, Inc.
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

#include <hmap/dynamic-hmap.hpp>

#include <sstream>

DynamicHMap::iterator DynamicHMap::begin() {
	return map_.begin();
}
DynamicHMap::iterator DynamicHMap::end() {
	return map_.end();
}

DynamicHMap::const_iterator DynamicHMap::cbegin() const {
	return map_.cbegin();
}
DynamicHMap::const_iterator DynamicHMap::cend() const {
	return map_.cend();
}

size_t DynamicHMap::size() const { return map_.size(); }
bool DynamicHMap::empty() const { return map_.empty(); }
void DynamicHMap::clear() { map_.clear(); }
[[noreturn]] void DynamicHMap::keyNotFound(const detail::KeyBase& k) {
	std::stringstream msg;
	msg << "DynamicHMap: '" << k.key << "' (type '" << k.info().name() << "') not present." << std::endl;
	throw std::out_of_range(msg.str());
}