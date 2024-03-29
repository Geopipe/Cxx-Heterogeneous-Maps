#pragma once
/************************************************************************************
 * @file hmap.hpp "Static Heterogeneous Maps" - Provide type-safe lookup between 
 * string-like keys and values of arbitrary type. Failed lookups result in
 * compile-time errors rather than runtime exceptions. Essentially a metaprogrammable
 * struct, which could be used to implement 
 * the [Scrap-Your-Boilerplate](https://www.microsoft.com/en-us/research/wp-content/uploads/2003/01/hmap.pdf)
 * pattern ("SYB").
 * 
 * The documented parts are either:
 *  (a) useful/reusable,
 *  (b) otherwise impenetrable and impossible to maintain
 * 
 * @author Thomas Dickerson
 * @copyright 2019 - 2020, Geopipe, Inc.
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

#include <string_view>
#include <type_traits>
#include <utility>
#include <tuple>

/*****************************************************************************
 * @defgroup TupleTools Helper code for wrangling tuples
 * `tuple_slice` + `detail::slice_impl` via: https://stackoverflow.com/a/40836163/2252298
 * This should probably go in a separate header at some point:
 * @{
 **/
namespace detail
{
	/// Helper code for `tuple_slice`
	template <std::size_t Ofst, class Tuple, std::size_t... I>
	constexpr auto slice_impl(Tuple&& t, std::index_sequence<I...>)
	{
		return std::forward_as_tuple(
									 std::get<I + Ofst>(std::forward<Tuple>(t))...);
	}
}

/****************************************************
 * Extract a slice from a tuple - like `std::get` but
 * for a range of indices. Can also be used to undo
 * `std::tuple_cat`.
 * @tparam T1 The start index (inclusive).
 * @tparam T2 The end index (exlcusive).
 * @tparam Cont Some tuple-like container type.
 * @arg t An instance of `Cont`.
 * @return The slice of `t` given by the range `[T1, T2)`
 ****************************************************/
template <std::size_t I1, std::size_t I2, class Cont>
constexpr auto tuple_slice(Cont&& t)
{
	static_assert(I2 >= I1, "invalid slice");
	static_assert(std::tuple_size<std::decay_t<Cont>>::value >= I2,
				  "slice index out of bounds");
	
	return detail::slice_impl<I1>(std::forward<Cont>(t),
								  std::make_index_sequence<I2 - I1>{});
}
/**
 * @}
 ****************************************************************************/

namespace detail {
	using std::integral_constant;
	using std::make_tuple;
	using std::size_t;
	using std::tuple;
	
	/*****************************************************************************
	 * @defgroup KeyType Our basic building blocks for static key/value pairs
	 * @{
	 **/

	/// Compile-time string, with entry-points to produce runtime values
	template<char ...Cs> 
	struct CharList {
		/// How many bytes to store
		constexpr static const size_t storage_size = sizeof...(Cs);
		/// "strlen", assuming nul-terminated
		constexpr static const size_t c_str_length = storage_size - 1;
		/// Storage for obtaining runtime representation
		constexpr static const char data[storage_size] = {Cs ...};
		/// Validate that this is actually nul-terminated
		constexpr static const bool is_c_str = data[c_str_length] == '\0';
		/// Length as a C-string
		constexpr static const size_t length() {
			static_assert(is_c_str, "C strings must be null-terminated");
			return c_str_length;	
		}
		/// Access the character data as a C-string at runtime
		constexpr static const decltype(data)& c_str() {
			static_assert(is_c_str, "C strings must be null-terminated");
			return data;
		}
	};
	
	/// Metafunction to test if `CharList`, generically `false`.
	template<typename T>
	struct IsCharList {
		constexpr static const bool value = false;
	};
	/// Metafunction specialization matching `CharList`s. Returns `true`.
	template<char ...Cs>
	struct IsCharList<CharList<Cs...>> {
		constexpr static const bool value = true;
	};
	
	namespace detail {
		/// A Key-Value pair, indexed by `KeyType<Value, Cs...>`, storing value of type `Value`.
		template<typename Value, char ...Cs> struct ValueType;
	};
	
	/// A key, indexed by `_Value` and the string encoded by `CharList<Cs...>`
	template<typename _Value, char ...Cs> 
	struct KeyType : CharList<Cs...> {
		using Typeless = CharList<Cs...>; ///< Type-encoding of the string key
		using Typeless::c_str;
		using Typeless::length;
		using Value = _Value; ///< The type of the indexed value
		using ValueType = detail::ValueType<Value, Cs...>; ///< A Key-Value pair
		
		/// Promote expression `(KeyType, Value)` into `ValueType`
		template<typename Arg>
		constexpr ValueType operator,(Arg &&a) {
			return ValueType(std::forward<Arg>(a));
		}
		
	};
	
	/// Metafunction to test if `KeyType`, generically `false`.
	template<typename T>
	struct IsKeyType {
		constexpr static const bool value = false;
	};
	/// Metafunction specialization matching `KeyType`s. Returns `true`.
	template<typename Value, char ...Cs>
	struct IsKeyType<KeyType<Value, Cs...>> {
		constexpr static const bool value = true;
	};
	
	namespace detail {
		template<typename Value, char ...Cs> struct ValueType : KeyType<Value, Cs...> {
			using ValueKeyType = KeyType<Value, Cs...>; ///< Our matching `KeyType`
			using ValueKeyType::c_str;
			using ValueKeyType::length;
			Value v; ///< The stored `Value` for this Key-Value pair
			constexpr ValueType(Value vi) : v(vi) {}
		};
	}
	
	/*************************************
	 * Helper metafunction for `KeyLess`
	 * @tparam b Return `Left` if `true`, `Right` if `false`.
	 *************************************/
	template<typename Left, typename Right, bool b>
	struct TakeLesser {
		constexpr static Left apply(Left l, Right) { return l; }
	};
	/// Specialization for case when `Right` is lesser  key.
	template<typename Left, typename Right>
	struct TakeLesser<Left, Right, false> {
		constexpr static Right apply(Left, Right r) { return r; }
	};
	
	/// Compare `Left` and `Right` keys.
	template<typename Left, typename Right> 
	struct KeyLess {
		/// `true` if `Left` is lexicographically less than `Right`, `false` otherwise.
		constexpr static const bool value = std::string_view(Left::c_str(), Left::length()) < std::string_view(Right::c_str(), Right::length());
		/// Return stored value of lesser key.
		constexpr static auto apply(Left l, Right r) {
			return TakeLesser<Left, Right, value>::apply(l, r);
		}
	};
	
	/// Turns a constexpr lambda into a KeyType encoding its return value.
	template<typename Value, typename StringHolder, size_t ...I>
	constexpr auto keyTypeImpl(StringHolder holder, std::index_sequence<I...>) {
		return KeyType<Value, holder()[I] ...>();
	}
	
	/// Turns a constexpr lambda into a CharList encoding its return value.
	template<typename StringHolder, size_t ...I>
	constexpr auto inferredKeyTypeImpl(StringHolder holder, std::index_sequence<I...>) {
		return CharList<holder()[I] ...>();
	}
	/**
	 * @}
	 ****************************************************************************/
	
	
	/*****************************************************************************
	 * @defgroup TwoWayMerge Our building blocks for merge-sorting two sequences.
	 * @{
	 **/

	/*******************************************
	 * A recursive compile-time sorted merge of 
	 * two sorted sequences.
	 * 
	 * Generic template is the base-case when
	 *  `L` and `R` subsequences are empty.
	 * This means the `Head` subsequence is
	 * completely sorted.
	 * @tparam L The left subsequence
	 * @tparam R The right subsequence
	 * @tparam Head The finished prefix of the
	 * output.
	 * @pre `L`, `R`, and `Head` should be 
	 * completely sorted already.
	 *******************************************/
	template<typename L, typename R, typename Head = tuple<>>
	struct Merge {
		using type = Head; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static type apply(const type& dummy){
			return dummy;
		}
	};
	
	/*******************************************
	 * Base-case of `Merge` when `L` 
	 * subsequence has at least 1 element and `R`
	 * is empty.
	 *******************************************/
	template<typename L, typename ...Ls, typename ...Hs>
	struct Merge<tuple<L, Ls...>, tuple<>, tuple<Hs...>> {
		using type = tuple<Hs..., L, Ls...>; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static type apply(const tuple<L, Ls...>& l, const tuple<>&, const tuple<Hs...>& h) {
			return std::tuple_cat(h, l);
		}
	};
	
	
	/*******************************************
	 * Base-case of `Merge` when `R` 
	 * subsequence has at least 1 element and `L`
	 * is empty.
	 *******************************************/
	template<typename R, typename ...Rs, typename ...Hs>
	struct Merge<tuple<>, tuple<R, Rs...>, tuple<Hs...>> {
		using type = tuple<Hs..., R, Rs...>; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static type apply(const tuple<>&, tuple<R, Rs...>& r, tuple<Hs...>& h) {
			return std::tuple_cat(h, r);
		}
	};
	
	/// Helper for `Merge` to obtain next element in output sequence.
	template<typename PrevL, typename PrevR, typename PrevH, typename LeftThunk, typename RightThunk, bool Left>
	struct MergeLesser;
	
	/*******************************************
	 * Recursive case of `Merge` when `R` and `L`
	 * subsequences both have at least 1 element.
	 *******************************************/
	template<typename L, typename ...Ls, typename R, typename ...Rs, typename ...Hs>
	struct Merge<tuple<L, Ls...>, tuple<R, Rs...>, tuple<Hs...>> {
	private:
		constexpr static const bool TakeLeft = KeyLess<L, R>::value; ///< Which subsequence has least first element?
		using LeftThunk = Merge<tuple<Ls...>, tuple<R, Rs...>, tuple<Hs..., L>>; ///< Speculatively create the case where `L` is least
		using RightThunk =  Merge<tuple<L, Ls...>, tuple<Rs...>, tuple<Hs..., R>>; ///< Speculatively create the case where `R` is least
	public:
		using Left = tuple<L, Ls...>; ///< Type of left subsequence
		using Right = tuple<R, Rs...>; ///< Type of right subsequence
		using Head = tuple<Hs...>; ///< Type of computed output prefix
		/***********************************
		 * Determine type of output sequence
		 * @warning TODO: figure out if the
		 * speculative executions are computed
		 * lazily or to completion.
		 * If to completion, big efficiency 
		 * gains could be made by replacing
		 * `std::conditional` with a helper 
		 * thunk to avoid exponential branching.
		 ***********************************/
		using type = typename std::conditional<TakeLeft, typename LeftThunk::type, typename RightThunk::type>::type;
		
		/// Obtain output sequence at runtime.
		constexpr static type apply(const Left &l, const Right &r, const Head &h) {
			return MergeLesser<Left, Right, Head, LeftThunk, RightThunk, TakeLeft>::apply(l, r, h);
		}
	};
	
	/// Generic implementation for case when `Left` subsequence contains next element.
	template<	typename PrevL, typename PrevR, typename PrevH,
	typename LeftThunk, typename RightThunk, bool Left>
	struct MergeLesser {
		/// Slice front off `L` and append it to `Head`.
		constexpr static typename LeftThunk::type apply(const PrevL &l, const PrevR &r, const PrevH &h) {
			auto nextL = tuple_slice<1, std::tuple_size<PrevL>::value>(l);
			auto nextH = std::tuple_cat(h, tuple_slice<0,1>(l));
			return LeftThunk::apply(nextL, r, nextH);
		}
	};
	
	/// Specialization of `MergeLesser` for case when `Right` subsequence contains next element.
	template<	typename PrevL, typename PrevR, typename PrevH,
	typename LeftThunk, typename RightThunk>
	struct MergeLesser<PrevL, PrevR, PrevH, LeftThunk, RightThunk, false> {
		/// Slice front off `R` and append it to `Head`.
		constexpr static typename RightThunk::type apply(const PrevL &l, const PrevR &r, const PrevH &h) {
			auto nextR = tuple_slice<1, std::tuple_size<PrevR>::value>(r);
			auto nextH = std::tuple_cat(h, tuple_slice<0,1>(r));
			return RightThunk::apply(l, nextR, nextH);
		}
	};
	/**
	 * @}
	 ****************************************************************************/
	
	/*****************************************************************************
	 * @defgroup NWayMerge Our building blocks for merge-sorting N sequences.
	 * @{
	 **/

	/*******************************************
	 * A 2D recursive compile-time sorted merge of 
	 * N sorted sequences.
	 * 
	 * Generic template is the base-case when
	 * all subsequences are empty.
	 * This means there is nothing to sort.
	 * @tparam Head A sequence of sorted subsequences,
	 * each resulting from a 2-way `Merge`.
	 * @tparam Ls A parameter pack of sorted subsequences
	 * to be 2-way `Merge`d. 
	 *******************************************/
	template<typename Head, typename ...Ls>
	struct MergeN {
		using type = tuple<>; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static type apply(const Head& head, const Ls& ...l) {
			return make_tuple();
		}
	};

	/// Outer base-case of `MergeN` when we have a single sorted subsequence in `Ls`, which can be returned directly.
	template<typename L>
	struct MergeN<tuple<>, L> {
		using type = L; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static type apply(const tuple<> &, const L& l){
			return l;
		}
	};
	
	/*******************************************
	 * Inner base-case of `MergeN` when all `Ls...`
	 * have been two-way `Merge`d and buffered into
	 * `Head`, recurse by shunting them back into
	 * `Ls...` (and emptying `Head`).
	 *******************************************/ 
	template<typename H, typename ...Hs>
	struct MergeN<tuple<H, Hs...>> {
	private:
		using MergeThunk = MergeN<tuple<>, H, Hs... >; ///< Proceed with recursion
	public:
		using type = typename MergeThunk::type; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static auto apply(const tuple<H, Hs...> &head){
			auto args = std::tuple_cat(make_tuple(make_tuple()),head);
			return std::apply(MergeThunk::apply, args);
		}
	};
	
	/*******************************************
	 * Special case of `MergeN` when we had an
	 * odd number of `Ls...`, and all but the last
	 * have been `Merge`d. Just shunt it into
	 * `Head` untouched and proceed to the inner
	 * base-case.
	 *******************************************/
	template<typename H, typename ...Hs, typename L>
	struct MergeN<tuple<H, Hs...>, L> {
	private:
		using MergeThunk = MergeN<tuple<>, H, Hs ..., L >; ///< Proceed with recursion
	public:
		using type = typename MergeThunk::type; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static auto apply(const tuple<H, Hs...> &head, const L& l){
			auto args = std::tuple_cat(make_tuple(make_tuple()),head,make_tuple(l));
			return std::apply(MergeThunk::apply, args);
		}
	};
	
	/*******************************************
	 * General case of `MergeN` when we have at
	 * least two `Ls...`. `Merge` them and shunt
	 * result to `Head`.
	 *******************************************/
	template<typename ...Hs, typename L1, typename L2, typename ...Ls>
	struct MergeN<tuple<Hs...>, L1, L2, Ls...> {
	private:
		using MergeThunk = MergeN<tuple<Hs..., typename Merge<L1, L2>::type>, Ls...>; ///< Proceed with recursion
	public:
		using type = typename MergeThunk::type; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		constexpr static auto apply(const tuple<Hs...> &head, const L1 &l1, const L2 &l2, const Ls& ...ls){
			auto nextHead = std::tuple_cat(head, make_tuple(Merge<L1, L2>::apply(l1, l2, make_tuple())));
			auto args = std::tuple_cat(make_tuple(nextHead), make_tuple(ls...));
			return std::apply(MergeThunk::apply, args);
		}
	};
	/**
	 * @}
	 ****************************************************************************/
	
	/*****************************************************************************
	 * @defgroup MergeSortEntry Compile-time merge-sort on types.
	 * The entry-point for our @ref NWayMerge and @ref TwoWayMerge routines.
	 * @{
	 **/

	/// Perform a lexicographical merge-sort on `KeyType`s or `CharList`s.
	template<typename ...KTs>
	struct MergeSort {
	private:
		using MergeThunk = MergeN<tuple<>, tuple<KTs>...>; ///< `MergeN` on singleton sequences.
	public:
		using type = typename MergeThunk::type; ///< Type of output sequence.
		
		/// Obtain output sequence at runtime.
		template<typename ...Args> constexpr static type apply(Args...args) {
			tuple<tuple<>, tuple<KTs>...> appArgs = make_tuple(tuple<>(), tuple<KTs>(std::forward<Args>(args))...);
			return std::apply(MergeThunk::apply, appArgs);
		}
	};
	/**
	 * @}
	 ****************************************************************************/
	
	/*****************************************************************************
	 * @defgroup Trees Our building blocks for the binary trees used for lookups.
	 * @{
	 **/

	/// A Node with a stored value, and left and right children.
	template<class V, class L, class R>
	struct Node {
		V v; ///< Stored value
		L l; ///< Left child
		R r; ///< Right child
		constexpr Node(V vi, L li, R ri) : v(vi), l(li), r(ri) {}
	};
	
	/// A Node with a stored value, and right child.
	template<class V, class R>
	struct Node<V, void, R> {
		V v; ///< Stored value
		R r; ///< Right child
		constexpr Node(V vi, R ri) : v(vi), r(ri) {}
	};
	
	/// A Node with a stored value, and left child.
	template<class V, class L>
	struct Node<V, L, void> {
		V v; ///< Stored value
		L l; ///< Left child
		constexpr Node(V vi, L li) : v(vi), l(li) {}
	};
	
	/// Leaf Node
	template<class V>
	struct Node<V, void, void> {
		V v; ///< Stored value
		constexpr Node(V vi) : v(vi) {}
	};
	/**
	 * @}
	 ****************************************************************************/
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Convert a sequence of types into a balanced binary tree of types
	template<typename Left, typename Right, typename TargetLeft>
	struct SplitJointIf;
	
	template<typename _Left, typename _Right, typename TargetLeft>
	class Split {
	public:
		using Left = void;
		using Here = void;
		using Right = void;
	};
	
	template<typename ...Ls, typename H, typename ...Rs, size_t TargetLeft>
	class Split<tuple<Ls...>, tuple<H, Rs...>, integral_constant<size_t, TargetLeft> > {
		using IfThunk = SplitJointIf<tuple<Ls...>, tuple<H, Rs...>, integral_constant<size_t, TargetLeft> >;
	public:
		using Left = typename IfThunk::Left;
		using Here = typename IfThunk::Here;
		using Right = typename IfThunk::Right;
		
		template<typename ...Args>
		static constexpr auto split(Args&& ...args) {
			return IfThunk::split(std::forward<Args>(args)...);
		}
	};
	
	/// Note: integral constant required per gcc missing implementation of http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0263r1.html#1315
	template<typename Left, typename Right, typename TargetLeft>
	struct SplitJointIf {};
	template<typename ...Ls, typename H, typename ...Rs>
	struct SplitJointIf<tuple<Ls...>, tuple<H, Rs...>, integral_constant<size_t, sizeof...(Ls)> > {
		using Left = tuple<Ls...>;
		using Here = H;
		using Right = tuple<Rs...>;
		
		template<typename ...Args>
		static constexpr tuple<Left,Here,Right> split(Args&& ...args) {
			auto holder = make_tuple(std::forward<Args>(args) ...);
			return make_tuple(tuple_slice<0,sizeof...(Ls)>(holder), std::get<sizeof...(Ls)>(holder), tuple_slice<1+sizeof...(Ls),sizeof...(Args)>(holder));
		}
		
	};
	
	template<typename ...Ls, typename H, typename ...Rs, size_t TargetLeft>
	struct SplitJointIf<tuple<Ls...>, tuple<H, Rs...>, integral_constant<size_t, TargetLeft> > {
	private:
		static_assert(sizeof...(Ls) < TargetLeft, "Missed the loop exit condition");
		using SplitThunk = Split<tuple<Ls..., H>, tuple<Rs...>, integral_constant<size_t, TargetLeft> >;
	public:
		using Left = typename SplitThunk::Left;
		using Here = typename SplitThunk::Here;
		using Right = typename SplitThunk::Right;
		
		template<typename ...Args>
		static constexpr auto split(Args&& ...args) {
			return SplitThunk::split(std::forward<Args>(args)...);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Entry point for our list->tree process
	template<template<typename, typename, typename> class Node, class V, class L, class R>
	struct NodeMaker;
	
	template<template<typename, typename, typename> class Node, typename HL>
	class SortedToTree { public: using type = void; };
	
	template<template<typename, typename, typename> class Node, typename HeadT, typename ...TailTs>
	class SortedToTree<Node, tuple<HeadT, TailTs...>> {
		using SplitThunk = Split<tuple<>, tuple<HeadT, TailTs...>, integral_constant<size_t, sizeof...(TailTs) / 2> >;
		using Left = typename SplitThunk::Left;
		using Here = typename SplitThunk::Here;
		using Right = typename SplitThunk::Right;
		
		
		using LeftThunk = SortedToTree<Node, Left>;
		using RightThunk =  SortedToTree<Node, Right>;
	public:
		using ArgType = tuple<HeadT, TailTs...>;
		using type = Node<Here, typename LeftThunk::type, typename RightThunk::type>;
		
		constexpr static type apply(const tuple<HeadT, TailTs...>& args) {
			using SplitF = tuple<Left,Here,Right>(&)(const HeadT&, const TailTs&...);
			auto ret = std::apply(static_cast<SplitF>(SplitThunk::template split<const HeadT&, const TailTs&...>), args);
			auto& [left, here, right] = ret;
			return NodeMaker<Node, Here, LeftThunk, RightThunk>::apply(here, left, right);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Deferred evaluation to select between node variants from the tree-assembly
	template<template<typename, typename, typename> class Node, class V, class L, class R>
	struct NodeMaker {
		using LR = typename L::type;
		using LA = typename L::ArgType;
		using RR = typename R::type;
		using RA = typename R::ArgType;
		constexpr static Node<V, LR, RR> apply(V& v, LA& l, RA& r) {
			return Node(v, L::apply(l), R::apply(r));
		}
	};
	
	template<template<typename, typename, typename> class Node, class V, class L>
	struct NodeMaker<Node, V, L, SortedToTree<Node, tuple<>>> {
		using LR = typename L::type;
		using LA = typename L::ArgType;
		constexpr static Node<V, LR, void> apply(V& v, LA& l, tuple<>&) {
			return Node<V, LR, void>(v, L::apply(l));
		}
	};
	
	template<template<typename, typename, typename> class Node, class V, class R>
	struct NodeMaker<Node, V, SortedToTree<Node, tuple<>>, R> {
		using RR = typename R::type;
		using RA = typename R::ArgType;
		constexpr static Node<V, void, RR> apply(V& v, tuple<>&, RA& r) {
			return Node<V, void, RR>(v, R::apply(r));
		}
	};
	
	template<template<typename, typename, typename> class Node, class V>
	struct NodeMaker<Node, V, SortedToTree<Node, tuple<>>, SortedToTree<Node, tuple<>>> {
		constexpr static Node<V, void, void> apply(V& v, tuple<>&, tuple<>&) {
			return Node<V, void, void>(v);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Helper routines for traversing a binary search tree
	template<typename Node, typename LB, typename RB, bool LL, bool RR>
	struct FindInTreeHelper {
		static_assert(!LL || !RR, "Can't split traversal");
		constexpr static auto& apply(Node &n) {
			return n.v;
		}
		
		constexpr static const auto& apply(const Node &n) {
			return n.v;
		}
	};
	
	template<typename Node, typename LB, typename RB>
	struct FindInTreeHelper<Node, LB, RB, true, false> {
		constexpr static auto& apply(Node &n) {
			return LB::apply(n.l);
		}
		
		constexpr static const auto& apply(const Node &n) {
			return LB::apply(n.l);
		}
	};
	
	template<typename Node, typename LB, typename RB>
	struct FindInTreeHelper<Node, LB, RB, false, true> {
		constexpr static auto& apply(Node &n) {
			return RB::apply(n.r);
		}
		
		constexpr static const auto& apply(const Node &n) {
			return RB::apply(n.r);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Entry point for binary search tree traversal
	template<typename FindMe, typename T>
	class FindInTree { public: using type = void; };
	
	template<typename FindMe, template<typename, typename, typename> class Node, typename V, typename L, typename R>
	class FindInTree<FindMe, Node<V, L, R> > {
		constexpr static const bool LookLeft = KeyLess<FindMe, V>::value;
		constexpr static const bool LookRight = KeyLess<V, FindMe>::value;
		using LeftBranch = FindInTree<FindMe, L>;
		using RightBranch = FindInTree<FindMe, R>;
	public:
		using type = typename std::conditional<LookLeft, typename LeftBranch::type, typename std::conditional<LookRight, typename RightBranch::type, V>::type>::type;
		
		constexpr static auto& apply(Node<V, L, R> &n) {
			return FindInTreeHelper<Node<V,L,R>, LeftBranch, RightBranch, LookLeft, LookRight>::apply(n);
		}
		
		constexpr static const auto& apply(const Node<V, L, R> &n) {
			return FindInTreeHelper<Node<V,L,R>, LeftBranch, RightBranch, LookLeft, LookRight>::apply(n);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Tools to make the compiler emit more useful diagnostics in response to
	// user errors further up the call-stack.
	template<class Thunk, class Tree, class ValueType>
	struct HMapSafeIndexer {
		constexpr static auto& apply(Tree &tree) {
			return Thunk::apply(tree).v;
		}
		
		constexpr static const auto& apply(const Tree &tree) {
			return Thunk::apply(tree).v;
		}
	};
	
	template<class Thunk, class Tree>
	struct HMapSafeIndexer<Thunk, Tree, void> {
	private:
		constexpr static const bool DUMMY = false;
	public:
		// Disappear one set of compiler errors
		// To improve readability/debugging
		constexpr static const auto& apply(const Tree &tree) { return DUMMY; }
	};
	
	template<typename ...Args>
	struct NoRepeats {
		constexpr static const bool value = true;
	};
	
	template<typename A, typename B, typename ...Args>
	struct NoRepeats<A, B, Args...> {
		constexpr static const bool value = NoRepeats<B, Args...>::value;
	};
	
	template<typename A, typename ...Args>
	struct NoRepeats<A, A, Args...> {
		constexpr static const bool value = false;
	};
	
	template<typename T>
	struct NoRepeatsDispatcher {};
	
	template<typename ...Args>
	struct NoRepeatsDispatcher<tuple<Args...>> {
		static constexpr bool value = NoRepeats<typename Args::KeyType::Typeless ...>::value;
	};
	//////////////////////////////////////////////////////////////////////////////
}

/*****************************************************************************
 * @defgroup HMapAPI The public API for working with HMaps
 * @{
 **/
/// Obtain a `detail::KeyType` from a constexpr lambda wrapping a string literal.
template<typename Value, typename StringHolder>
constexpr auto keyType(StringHolder holder) {
	constexpr std::string_view text = holder();
	return detail::keyTypeImpl<Value>(holder, std::make_index_sequence<1+text.length()>());
}
/// Obtain a `detail::CharList` from a constexpr lambda wrapping a string literal.
template<typename StringHolder>
constexpr auto inferredKeyType(StringHolder holder) {
	constexpr std::string_view text = holder();
	return detail::inferredKeyTypeImpl(holder, std::make_index_sequence<1+text.length()>());
}

/**********************************************************
 * A balanced binary search tree, associating string-like
 * keys with values of potentially varying types.
 * @note For the _same_ `N` keys there are up to `N!` 
 * equivalent instantiations of `HMap`, sharing an identical
 * tree structure. Future work might choose to make the sorted
 * order canonical, or to expose conversions between equivalent
 * instantiations.
 * 
 * @warning Does not directly support mapping multiple values
 * of different types with the same string key. This is different
 * from the behavior of `DynamicHMap` which treats the same name
 * for different types as existing within different "universes
 * of discourse". The primary reason for the limitation here
 * is that all of the mechanisms which C++ provides for obtaining
 * an ordering on types are, ironically, only useable at runtime,
 * and, annoyingly, non-portable so best not to bake into the ABI
 * even if we could.
 * 
 * @tparam KeyTypes `detail::KeyType`s specifying which keys
 * are present in the `HMap` and the type of their associated
 * values. 
 **********************************************************/
template<typename ...KeyTypes>
class HMap {
	using MergeThunk = detail::MergeSort<typename KeyTypes::ValueType...>; ///< Sort key-value pairs lexicographically
	using Sorted = typename MergeThunk::type; ///< Extract the sorted order
	using TreeThunk = detail::SortedToTree<detail::Node, Sorted>; ///< Construct balanced binary search tree from sorted order
	using Tree = typename TreeThunk::type; ///< Extract the node definitions for the tree
	constexpr static const bool NoDuplicateKeys = detail::NoRepeatsDispatcher<Sorted>::value; ///< Ensure uniqueness of keys
	
	template<typename KeyType> using ValueThunk = detail::FindInTree<KeyType, Tree>; ///< Helper for doing lookups.
	
	Tree tree_; ///< The actual data-structure.
public:
	/// Construct tree from input key-value pairs in arbitrary order
	template<typename ...Values>
	constexpr HMap(Values&& ...values)
	: tree_(TreeThunk::apply(MergeThunk::apply(std::forward<Values>(values)...))) {
		static_assert(NoDuplicateKeys, "HMap would contain duplicate keys");
	}
	
	/// Lookup value of specified type for given key (only the key's type matters, runtime value is just a tag to guide dispatch)
	template<typename KeyType, std::enable_if_t<detail::IsKeyType<KeyType>::value, bool> = false>
	constexpr auto& operator[](const KeyType&){
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		// Only check against key's type if the previous test passed
		static_assert(std::is_same_v<ValueType, void> || std::is_same_v<ValueType, typename KeyType::ValueType>, "HMap contains key, but it has the wrong type");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}

	/// Lookup value of inferred type for given key (only the key's type matters, runtime value is just a tag to guide dispatch)
	template<typename KeyType, std::enable_if_t<detail::IsCharList<KeyType>::value, bool> = false>
	constexpr auto& operator[](const KeyType&){
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}

	/// `const` overload. Lookup value of specified type for given key (only the key's type matters, runtime value is just a tag to guide dispatch)
	template<typename KeyType, std::enable_if_t<detail::IsKeyType<KeyType>::value, bool> = false>
	constexpr const auto& operator[](const KeyType&) const{
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		// Only check against key's type if the previous test passed
		static_assert(std::is_same_v<ValueType, void> || std::is_same_v<ValueType, typename KeyType::ValueType>, "HMap contains key, but it has the wrong type");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}

	/// `const` overload. Lookup value of inferred type for given key (only the key's type matters, runtime value is just a tag to guide dispatch).
	template<typename KeyType, std::enable_if_t<detail::IsCharList<KeyType>::value, bool> = false>
	constexpr const auto& operator[](const KeyType&) const {
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}
};

/*************************************************
 * Construct an `HMap` from a sequence of key-value 
 * pairs (`detail::detail::ValueType`).
 * Typical usage:
 * <pre class="markdeep">
 * ```c++
 * auto myMap = make_hmap((TK("foo",int), 1), (TK("bar",float), 2.), (TK("baz",std::string), "hello"));
 * ```
 * </pre>
 *************************************************/
template<typename ...Values>
constexpr HMap<typename Values::KeyType...> make_hmap(Values&& ...values) {
	return HMap<typename Values::KeyType...>(std::forward<Values>(values)...);
}

/*************************************************
 * @def TK(stringliteral) Obtain a typed key,
 * requiring the mapped value's type to match.
 * @param stringliteral The key, `"A string literal"`
 * @param T The required type for mapped value.
 *************************************************/
#define TK(stringliteral,T) keyType<T>([]() constexpr { return stringliteral; })
/*************************************************
 * @def IK(stringliteral) Obtain an untyped key,
 * allowing the mapped value's type to be inferred.
 * @param stringliteral The key, `"A string literal"`
 *************************************************/
#define IK(stringliteral) inferredKeyType([]() constexpr { return stringliteral; })
/**
 * @}
 ****************************************************************************/

