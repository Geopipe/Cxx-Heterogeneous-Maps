#include <hmap/hmap.hpp>
#include <hmap/dynamic-hmap.hpp>
#include <hmap/key-convert.hpp>

#include <iostream>
#include <string>

using namespace std::string_literals;

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
	// Test turning static keys into dynamic keys
	{
		auto keys = std::make_tuple(TK("foo",int), TK("bar",float), TK("baz",std::string));

		auto instantDynamicKeys = std::apply([](auto&& ...key) {
			return std::set<detail::KeyBase>({staticToDynamicKey(key) ...});
		}, keys);

		for(const auto& keyBase : instantDynamicKeys) {
			std::cout << keyBase.key << std::endl;
		}
	}
	return 0;
}
