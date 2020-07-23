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

#include <string_view>
#include <type_traits>
#include <utility>

//////////////////////////////////////////////////////////////////////////////
// tuple_slice + slice_impl via: https://stackoverflow.com/a/40836163/2252298
// This should probably go in a separate header at some point:
namespace detail
{
	template <std::size_t Ofst, class Tuple, std::size_t... I>
	constexpr auto slice_impl(Tuple&& t, std::index_sequence<I...>)
	{
		return std::forward_as_tuple(
									 std::get<I + Ofst>(std::forward<Tuple>(t))...);
	}
}

template <std::size_t I1, std::size_t I2, class Cont>
constexpr auto tuple_slice(Cont&& t)
{
	static_assert(I2 >= I1, "invalid slice");
	static_assert(std::tuple_size<std::decay_t<Cont>>::value >= I2,
				  "slice index out of bounds");
	
	return detail::slice_impl<I1>(std::forward<Cont>(t),
								  std::make_index_sequence<I2 - I1>{});
}
//////////////////////////////////////////////////////////////////////////////

namespace detail {
	using std::tuple;
	using std::make_tuple;
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Our basic building blocks for key/value pairs
	template<char ...Cs> struct CharList {
		constexpr static const char data[sizeof...(Cs)] = {Cs ...};
		
		constexpr static decltype(data)& c_str() {
			static_assert(data[sizeof...(Cs) - 1 == '\0'], "C strings must be null-terminated");
			return data;
		}
	};
	
	template<typename T>
	struct IsCharList {
		constexpr static const bool value = false;
	};
	
	template<char ...Cs>
	struct IsCharList<CharList<Cs...>> {
		constexpr static const bool value = true;
	};
	
	namespace detail {
		template<typename Value, char ...Cs> struct ValueType;
	};
	
	template<typename _Value, char ...Cs> struct KeyType : CharList<Cs...> {
		using Typeless = CharList<Cs...>;
		using Typeless::c_str;
		using Value = _Value;
		using ValueType = detail::ValueType<Value, Cs...>;
		
		template<typename Arg>
		constexpr ValueType operator,(Arg &&a) {
			return ValueType(std::forward<Arg>(a));
		}
		
	};
	
	template<typename T>
	struct IsKeyType {
		constexpr static const bool value = false;
	};
	
	template<typename Value, char ...Cs>
	struct IsKeyType<KeyType<Value, Cs...>> {
		constexpr static const bool value = true;
	};
	
	namespace detail {
		template<typename Value, char ...Cs> struct ValueType : KeyType<Value, Cs...> {
			using ValueKeyType = KeyType<Value, Cs...>;
			using ValueKeyType::c_str;
			Value v;
			constexpr ValueType(Value vi) : v(vi) {}
		};
	}
	
	template<typename Left, typename Right, bool b>
	struct TakeLesser {
		constexpr static Left apply(Left l, Right) { return l; }
	};
	
	template<typename Left, typename Right>
	struct TakeLesser<Left, Right, false> {
		constexpr static Right apply(Left, Right r) { return r; }
	};
	
	template<typename Left, typename Right> struct KeyLess {
		
		constexpr static const bool value = std::string_view(Left::c_str()) < std::string_view(Right::c_str());
		
		constexpr static auto apply(Left l, Right r) {
			return TakeLesser<Left, Right, value>::apply(l, r);
		}
	};
	
	template<typename Value, typename StringHolder, std::size_t ...I>
	auto keyTypeImpl(StringHolder holder, std::index_sequence<I...>) {
		constexpr std::string_view text = holder();
		return KeyType<Value, text[I] ...>();
	}
	
	template<typename StringHolder, std::size_t ...I>
	auto inferredKeyTypeImpl(StringHolder holder, std::index_sequence<I...>) {
		constexpr std::string_view text = holder();
		return CharList<text[I] ...>();
	}
	//////////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////////
	// 2-way merge
	template<typename L, typename R, typename Head = tuple<>>
	struct Merge {
		using type = Head;
		
		constexpr static type apply(const tuple<>& dummy){
			return dummy;
		}
	};
	
	template<typename L, typename ...Ls, typename ...Hs>
	struct Merge<tuple<L, Ls...>, tuple<>, tuple<Hs...>> {
		using type = tuple<Hs..., L, Ls...>;
		
		constexpr static type apply(const tuple<L, Ls...>& l, const tuple<>&, const tuple<Hs...>& h) {
			return std::tuple_cat(h, l);
		}
	};
	
	template<typename R, typename ...Rs, typename ...Hs>
	struct Merge<tuple<>, tuple<R, Rs...>, tuple<Hs...>> {
		using type = tuple<Hs..., R, Rs...>;
		
		constexpr static type apply(const tuple<>&, tuple<R, Rs...>& r, tuple<Hs...>& h) {
			return std::tuple_cat(h, r);
		}
	};
	
	template<typename PrevL, typename PrevR, typename PrevH, typename LeftThunk, typename RightThunk, bool Left>
	struct MergeLesser;
	
	template<typename L, typename ...Ls, typename R, typename ...Rs, typename ...Hs>
	struct Merge<tuple<L, Ls...>, tuple<R, Rs...>, tuple<Hs...>> {
	private:
		constexpr static const bool TakeLeft = KeyLess<L, R>::value;
		using LeftThunk = Merge<tuple<Ls...>, tuple<R, Rs...>, tuple<Hs..., L>>;
		using RightThunk =  Merge<tuple<L, Ls...>, tuple<Rs...>, tuple<Hs..., R>>;
	public:
		using Left = tuple<L, Ls...>;
		using Right = tuple<R, Rs...>;
		using Head = tuple<Hs...>;
		
		using type = typename std::conditional<TakeLeft, typename LeftThunk::type, typename RightThunk::type>::type;
		
		constexpr static type apply(const Left &l, const Right &r, const Head &h) {
			return MergeLesser<Left, Right, Head, LeftThunk, RightThunk, TakeLeft>::apply(l, r, h);
		}
	};
	
	
	template<	typename PrevL, typename PrevR, typename PrevH,
	typename LeftThunk, typename RightThunk, bool Left>
	struct MergeLesser {
		constexpr static typename LeftThunk::type apply(const PrevL &l, const PrevR &r, const PrevH &h) {
			auto nextL = tuple_slice<1, std::tuple_size<PrevL>::value>(l);
			auto nextH = std::tuple_cat(h, tuple_slice<0,1>(l));
			return LeftThunk::apply(nextL, r, nextH);
		}
	};
	
	
	template<	typename PrevL, typename PrevR, typename PrevH,
	typename LeftThunk, typename RightThunk>
	struct MergeLesser<PrevL, PrevR, PrevH, LeftThunk, RightThunk, false> {
		constexpr static typename RightThunk::type apply(const PrevL &l, const PrevR &r, const PrevH &h) {
			auto nextR = tuple_slice<1, std::tuple_size<PrevR>::value>(r);
			auto nextH = std::tuple_cat(h, tuple_slice<0,1>(r));
			return RightThunk::apply(l, nextR, nextH);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////////
	// N-way merge
	template<typename Head, typename ...Ls>
	struct MergeN {
		// No-op: We were given nothing to merge
		using type = tuple<>;
		
		constexpr static type apply(const Head& head, const Ls& ...l) {
			return make_tuple();
		}
	};
	
	template<typename L>
	struct MergeN<tuple<>, L> {
		// This should be our recursive base-case
		using type = L;
		
		constexpr static type apply(const tuple<> &, const L& l){
			return l;
		}
	};
	
	template<typename H, typename ...Hs>
	struct MergeN<tuple<H, Hs...>> {
	private:
		using MergeThunk = MergeN<tuple<>, H, Hs... >;
	public:
		using type = typename MergeThunk::type;
		
		constexpr static auto apply(const tuple<H, Hs...> &head){
			auto args = std::tuple_cat(make_tuple(make_tuple()),head);
			return std::apply(MergeThunk::apply, args);
		}
	};
	
	template<typename H, typename ...Hs, typename L>
	struct MergeN<tuple<H, Hs...>, L> {
	private:
		using MergeThunk = MergeN<tuple<>, H, Hs ..., L >;
	public:
		using type = typename MergeThunk::type;
		
		constexpr static auto apply(const tuple<H, Hs...> &head, const L& l){
			auto args = std::tuple_cat(make_tuple(make_tuple()),head,make_tuple(l));
			return std::apply(MergeThunk::apply, args);
		}
	};
	
	template<typename ...Hs, typename L1, typename L2, typename ...Ls>
	struct MergeN<tuple<Hs...>, L1, L2, Ls...> {
	private:
		using MergeThunk = MergeN<tuple<Hs..., typename Merge<L1, L2>::type>, Ls...>;
	public:
		using type = typename MergeThunk::type;
		
		constexpr static auto apply(const tuple<Hs...> &head, const L1 &l1, const L2 &l2, const Ls& ...ls){
			auto nextHead = std::tuple_cat(head, make_tuple(Merge<L1, L2>::apply(l1, l2, make_tuple())));
			auto args = std::tuple_cat(make_tuple(nextHead), make_tuple(ls...));
			return std::apply(MergeThunk::apply, args);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////////
	// Merge-sort entry point
	template<typename ...KTs>
	struct MergeSort {
	private:
		using MergeThunk = MergeN<tuple<>, tuple<KTs>...>;
	public:
		using type = typename MergeThunk::type;
		
		template<typename ...Args> constexpr static type apply(Args...args) {
			tuple<tuple<>, tuple<KTs>...> appArgs = make_tuple(tuple<>(), tuple<KTs>(std::forward<Args>(args))...);
			return std::apply(MergeThunk::apply, appArgs);
		}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////////
	// Our basic building blocks for trees
	template<class V, class L, class R>
	struct Node {
		V v;
		L l;
		R r;
		Node(V vi, L li, R ri) : v(vi), l(li), r(ri) {}
	};
	
	template<class V, class R>
	struct Node<V, void, R> {
		V v;
		R r;
		Node(V vi, R ri) : v(vi), r(ri) {}
	};
	
	template<class V, class L>
	struct Node<V, L, void> {
		V v;
		L l;
		Node(V vi, L li) : v(vi), l(li) {}
	};
	
	template<class V>
	struct Node<V, void, void> {
		V v;
		Node(V vi) : v(vi) {}
	};
	//////////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////////
	// Convert a sequence of types into a balanced binary tree of types
	template<typename Left, typename Right, size_t TargetLeft>
	struct SplitJointIf;
	
	template<typename _Left, typename _Right, size_t TargetLeft>
	class Split {
	public:
		using Left = void;
		using Here = void;
		using Right = void;
	};
	
	template<typename ...Ls, typename H, typename ...Rs, size_t TargetLeft>
	class Split<tuple<Ls...>, tuple<H, Rs...>, TargetLeft> {
		using IfThunk = SplitJointIf<tuple<Ls...>, tuple<H, Rs...>, TargetLeft>;
	public:
		using Left = typename IfThunk::Left;
		using Here = typename IfThunk::Here;
		using Right = typename IfThunk::Right;
		
		template<typename ...Args>
		static constexpr auto split(Args&& ...args) {
			return IfThunk::split(std::forward<Args>(args)...);
		}
	};
	
	template<typename Left, typename Right, size_t TargetLeft>
	struct SplitJointIf {};
	template<typename ...Ls, typename H, typename ...Rs>
	struct SplitJointIf<tuple<Ls...>, tuple<H, Rs...>, sizeof...(Ls)> {
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
	struct SplitJointIf<tuple<Ls...>, tuple<H, Rs...>, TargetLeft> {
	private:
		static_assert(sizeof...(Ls) < TargetLeft, "Missed the loop exit condition");
		using SplitThunk = Split<tuple<Ls..., H>, tuple<Rs...>, TargetLeft>;
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
		using SplitThunk = Split<tuple<>, tuple<HeadT, TailTs...>, sizeof...(TailTs) / 2>;
		using Left = typename SplitThunk::Left;
		using Here = typename SplitThunk::Here;
		using Right = typename SplitThunk::Right;
		
		
		using LeftThunk = SortedToTree<Node, Left>;
		using RightThunk =  SortedToTree<Node, Right>;
	public:
		using ArgType = tuple<HeadT, TailTs...>;
		using type = Node<Here, typename LeftThunk::type, typename RightThunk::type>;
		
		constexpr static type apply(const tuple<HeadT, TailTs...>& args) {
			auto ret = std::apply(SplitThunk::template split<const HeadT&, const TailTs&...>, args);
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

//////////////////////////////////////////////////////////////////////////////
// The public API for working with HMaps
template<typename Value, typename StringHolder>
constexpr auto keyType(StringHolder holder) {
	constexpr std::string_view text = holder();
	return detail::keyTypeImpl<Value>(holder, std::make_index_sequence<1+text.length()>());
}

template<typename StringHolder>
constexpr auto inferredKeyType(StringHolder holder) {
	constexpr std::string_view text = holder();
	return detail::inferredKeyTypeImpl(holder, std::make_index_sequence<1+text.length()>());
}

template<typename ...KeyTypes>
class HMap {
	using MergeThunk = detail::MergeSort<typename KeyTypes::ValueType...>;
	using Sorted = typename MergeThunk::type;
	using TreeThunk = detail::SortedToTree<detail::Node, Sorted>;
	using Tree = typename TreeThunk::type;
	constexpr static const bool NoDuplicateKeys = detail::NoRepeatsDispatcher<Sorted>::value;
	
	template<typename KeyType> using ValueThunk = detail::FindInTree<KeyType, Tree>;
	
	Tree tree_;
public:
	template<typename ...Values>
	HMap(Values&& ...values)
	: tree_(TreeThunk::apply(MergeThunk::apply(std::forward<Values>(values)...))) {
		static_assert(NoDuplicateKeys, "HMap would contain duplicate keys");
	}
	
	template<typename KeyType, std::enable_if_t<detail::IsKeyType<KeyType>::value, bool> = false>
	auto& operator[](const KeyType&){
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		// Only check against key's type if the previous test passed
		static_assert(std::is_same_v<ValueType, void> || std::is_same_v<ValueType, typename KeyType::ValueType>, "HMap contains key, but it has the wrong type");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}
	
	template<typename KeyType, std::enable_if_t<detail::IsCharList<KeyType>::value, bool> = false>
	auto& operator[](const KeyType&){
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}
	
	template<typename KeyType, std::enable_if_t<detail::IsKeyType<KeyType>::value, bool> = false>
	const auto& operator[](const KeyType&) const{
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		// Only check against key's type if the previous test passed
		static_assert(std::is_same_v<ValueType, void> || std::is_same_v<ValueType, typename KeyType::ValueType>, "HMap contains key, but it has the wrong type");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}
	
	template<typename KeyType, std::enable_if_t<detail::IsCharList<KeyType>::value, bool> = false>
	const auto& operator[](const KeyType&) const {
		using Thunk = ValueThunk<KeyType>;
		using ValueType = typename Thunk::type;
		static_assert(!std::is_same_v<ValueType, void>, "HMap doesn't contain key");
		return detail::HMapSafeIndexer<Thunk, Tree, ValueType>::apply(tree_);
	}
};

template<typename ...Values>
HMap<typename Values::KeyType...> make_hmap(Values&& ...values) {
	return HMap<typename Values::KeyType...>(std::forward<Values>(values)...);
}

#define TK(stringliteral,T) keyType<T>([](){ return stringliteral; })
#define IK(stringliteral) inferredKeyType([](){ return stringliteral; })
//////////////////////////////////////////////////////////////////////////////

