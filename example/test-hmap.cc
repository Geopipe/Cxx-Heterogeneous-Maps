#include <hmap/hmap.hpp>
#include <hmap/dynamic-hmap.hpp>

#include <iostream>


int main(int argc, const char* argv[]) {
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
		auto &[optCusp, optBaz] = tup;
		std::cout << (optCusp ? (*optCusp)->c_str() : "\"cusp\" is not in map") << std::endl;
		std::cout << (optBaz ? (*optBaz)->c_str() : "\"baz\" is not in map") << std::endl;
	}
	return 0;
}
