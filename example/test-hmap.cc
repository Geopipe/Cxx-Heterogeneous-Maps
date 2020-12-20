#include <hmap/hmap.hpp>
#include <hmap/dynamic-hmap.hpp>
#include <hmap/key-convert.hpp>

#include <iostream>

using namespace std::string_literals;

template<typename KeyInfo>
struct accessor {
	template<typename StringHolder>
	static auto& get(DynamicHMap& dynamicHMap, KeyInfo& keyInfo, StringHolder holder) {
		return dynamicHMap[keyInfo[inferredKeyType(holder)]];
	}
};

#define getRef(map, keyInfo, stringliteral) accessor<decltype(keyInfo)>::get(map, keyInfo, []() constexpr { return stringliteral; })

/*
class MyTestClass {
  public:
	const static constexpr auto Keys = std::make_tuple(TK("foo",int), TK("bar",float), TK("baz",std::string));

	const static decltype(std::apply([](auto ...key) {
			return make_hmap((TK(decltype(key)::Typeless::c_str(), detail::Key<typename decltype(key)::Value>), staticToDynamicKey(key)) ...);
		}, MyTestClass::Keys)) KeyInfo;
};

const auto keyInfo = std::apply([](auto ...key) {
	return make_hmap((TK(decltype(key)::Typeless::c_str(), detail::Key<typename decltype(key)::Value>), staticToDynamicKey(key)) ...);
}, MyTestClass::Keys);
*/

const constexpr auto Keys2 = std::make_tuple(TK("foo",int));
const constexpr auto Keys3 = std::make_tuple(TK("foo",int), TK("bar",float), TK("baz",std::string));

template<typename Tuple>
struct KeyToKeyMapper {};

template<>
struct KeyToKeyMapper<std::tuple<>> {
  static std::tuple<> apply(){
    return std::tuple<>();
  }
};

template<typename V, typename ...TailKs, char ...Cs>
struct KeyToKeyMapper<std::tuple<detail::KeyType<V, Cs...>, TailKs...>> {
  static auto apply() {
    using MetaKey = detail::KeyType<detail::Key<V>, Cs...>;
    return std::tuple_cat(std::make_tuple((MetaKey(), detail::Key<V>(MetaKey::c_str()))), KeyToKeyMapper<std::tuple<TailKs...>>::apply());
  }
};

const auto inputKeys2 = KeyToKeyMapper<std::remove_cv<decltype(Keys2)>::type>::apply();
const auto keyInfo2 = std::apply(make_hmap, inputKeys2);
//const auto keyInfo3 = std::apply(make_hmap, KeyToKeyMapper<std::remove_cv<decltype(Keys3)>::type>::apply());

int main(int argc, const char* argv[]) {
	// Verify proper functionality of the static hmap (positive and negative tests)
	{
		auto myMap = make_hmap((TK("foo",int), 1), (TK("bar",float), 2.), (TK("baz",std::string), "hello"));
		
		const auto& myConstMap = myMap;
		/////////////////////////////////////////////////
		// Good
		std::cout << myConstMap[IK("foo")] << std::endl;
		std::cout << myConstMap[TK("foo",int)] << std::endl;
		/////////////////////////////////////////////////
		
		
		/////////////////////////////////////////////////
		// Bad
		//std::cout << myConstMap[TK("foo",float)] << std::endl;
		//std::cout << myConstMap[IK("bang")] << std::endl;
		//std::cout << myConstMap[TK("bang",int)] << std::endl;
		/////////////////////////////////////////////////
		
		myMap[IK("baz")] = "goodbye";
		
		std::cout << myMap[IK("baz")] << std::endl;
		
		
		/////////////////////////////////////////////////
		// Bad
		//auto myBadMap = make_hmap((TK("foo",int), 1), (TK("bar",int), 1), (TK("foo",float), 1.));
		/////////////////////////////////////////////////
		
	}
	// Verify proper functionality of dynamic hmap
	{
		class Foo {
		private:
			const std::string& bar_;
		public:
			explicit Foo(const std::string& bar) : bar_(bar) {}
			const std::string& str() const { return bar_; }
		};
		auto myMap = make_dynamic_hmap((dK<Foo>("baz"), Foo("Listen"s)),
		                               (dK<Foo>("bobndoug"), Foo("hosers"s)),
		                               (dK<Foo>("foo"), Foo("I say"s)));

		// Since this is a citation of Herman's Hermits, not of
		// Bob and Doug of SCTV, replace "hosers" with "people"
		auto [iter, inserted] =
		    myMap.insert_or_assign(dK<Foo>("bobndoug"), "people"s);
		assert(!inserted); // Assert that "people" was assigned, not inserted
		auto [iter2, inserted2] =
		    myMap.insert_or_assign(dK<Foo>("cusp"), "to what"s);
		assert(inserted2); // Assert that "to what" was inserted

		std::cout << myMap.at(dK<Foo>("baz")).str() << ' ';
		std::cout << iter->second.str() << ' ';
		std::cout << iter2->second.str() << ' ';
		std::cout << myMap.at(dK<Foo>("foo")).str() << std::endl;

		myMap = make_dynamic_hmap((dK<Foo>("bobndoug"), Foo("Everybody's"s)),
		                          (dK<Foo>("cusp"), Foo("somebody"s)),
		                          (dK<Foo>("foo"), Foo("sometime"s)));

		auto [iter3, inserted3] =
		    myMap.try_emplace(dK<Foo>("bobndoug"), "All the hosers"s);
		assert(!inserted3); // Assert that "All the hosers" went nowhere, eh

		auto [iter4, inserted4] =
		    myMap.try_emplace(dK<Foo>("chair"), "got to love"s);
		assert(inserted4); // Assert that "got to love" was inserted

		std::cout << iter3->second.str() << ' ';
		std::cout << iter4->second.str() << ' ';
		std::cout << myMap.at(dK<Foo>("cusp")).str() << ' ';
		std::cout << myMap.at(dK<Foo>("foo")).str() << std::endl;
	}
	{
		auto myMap = make_dynamic_hmap((dK<int>("foo"), 1), (dK<float>("bar"), 2.), (dK<std::string>("baz"),"hello"));
		
		const auto& myConstMap = myMap;
		
		std::cout << myMap[dK<int>("foo")] << std::endl;
		//std::cout << myMap[dK<float>("foo")] << std::endl;
		std::cout << myMap.find(dK<int>("foo"))->second << std::endl;
		std::cout << (myMap.find(dK<float>("foo")) == myMap.end<float>()) << std::endl;
		std::cout << myConstMap.find(dK<float>("bar"))->second << std::endl;
		std::cout << (myConstMap.find(dK<float>("bar")) == myConstMap.cend<float>()) << std::endl;
		std::cout << myMap.erase(dK<float>("foo")) << std::endl;
		std::cout << myMap.erase(dK<int>("foo")) << std::endl;
		
		myMap[dK<std::string>("baz")] = "goodbye";
		auto tup =  myMap.optCheckOut(dK<std::string>("cusp"),
		                              dK<std::string>("baz"));
		auto &[cusp, baz] = tup;
		std::cout << cusp.value_or("\"cusp\" is not in map") << std::endl;
		std::cout << baz.value_or("\"baz\" is not in map") << std::endl;
		myMap.optCheckIn(std::move(tup), dK<std::string>("cusp"),
		                                 dK<std::string>("baz"));
	}
    {
		auto myMap = make_dynamic_hmap((dSK<std::string>("baz"),std::make_shared<std::string>("goodbye")));
		auto myMap2 = make_dynamic_hmap();
		myMap2.insert(myMap.extract(dSK<std::string>("baz"),
		                            dSK<std::string>("cusp")),
		              dSK<std::string>("baz"),
		              dSK<std::string>("cusp"));
		auto tup = myMap2.optCheckOut(dSK<std::string>("cusp"),
		                              dSK<std::string>("baz"));

		// Unify the boost optional and literals to a common class
		// for use by operator <<.  We could use std::string_view,
		// but the best option is to use C++ string literals
		// (added by C++14)

		auto &[optCusp, optBaz] = tup;
		std::cout << ((boost::none != optCusp) ? **optCusp : "\"cusp\" is not in map"s) << std::endl;
		std::cout << ((boost::none != optBaz) ? **optBaz : "\"baz\" is not in map"s) << std::endl;
	}
	// Test moving keys from a static keys into a dynamic hmap
	{
		auto keys = std::make_tuple(TK("foo",int), TK("bar",float), TK("baz",std::string));
		auto myDynamicHMap = make_dynamic_hmap();
		std::apply([&myDynamicHMap](auto&&... key) {
			((myDynamicHMap[staticToDynamicKey(key)]), ...); }, keys);

		// Make sure the correct keys of the correct types are present, and no others
		assert(myDynamicHMap.end<int>() != myDynamicHMap.find(dK<int>("foo")));
		assert(myDynamicHMap.end<float>() != myDynamicHMap.find(dK<float>("bar")));
		assert(myDynamicHMap.end<std::string>() != myDynamicHMap.find(dK<std::string>("baz")));
		assert(myDynamicHMap.end<std::string>() == myDynamicHMap.find(dK<std::string>("cusp")));
		assert(myDynamicHMap.end<double>() == myDynamicHMap.find(dK<double>("foo")));
	}
	// Test turning static keys into dynamic keys
	{
		auto keys = std::make_tuple(TK("foo",int), TK("bar",float), TK("baz",std::string));

		auto instantDynamicKeys = std::apply([](auto&& ...key) {
			return std::set<detail::KeyBase>({staticToDynamicKey(key) ...});
		}, keys);

		for(const auto& keyBase : instantDynamicKeys) {
			std::cout << keyBase.key << std::endl;
		}

		std::cout << typeid(inputKeys2).name() << std::endl;
	}
	/*
	// Test intelligently setting/getting keys from a dynamic HMap using information in a static HMap.
	{
		auto dynamicHMap = make_dynamic_hmap();
		getRef(dynamicHMap, keyInfo, "bar") = 3.1415;
		getRef(dynamicHMap, keyInfo, "foo") = 1337;
		getRef(dynamicHMap, keyInfo, "baz") = "22/7";

		std::cout << getRef(dynamicHMap, keyInfo, "bar") << ", " << getRef(dynamicHMap, keyInfo, "foo")<< ", " << getRef(dynamicHMap, keyInfo, "baz") << std::endl;
	}
	*/
	return 0;
}
