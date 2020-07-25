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

#include <hmap/hmap.hpp>
#include <hmap/dynamic-hmap.hpp>

template<typename Value, char ...Cs>
detail::Key<Value> staticToDynamicKey(detail::KeyType<Value, Cs...>) {
	return dK<Value>(detail::KeyType<Value, Cs...>::c_str());
}
