#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace pluto::json {

class Value;
using Object = std::map<std::string, Value>;
using Array  = std::vector<Value>;

class Value {
public:
	using Data = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

	Value() : fData(nullptr) {}
	Value(std::nullptr_t) : fData(nullptr) {}
	Value(bool b) : fData(b) {}
	Value(int n) : fData(static_cast<double>(n)) {}
	Value(double n) : fData(n) {}
	Value(const char* s) : fData(std::string(s)) {}
	Value(std::string s) : fData(std::move(s)) {}
	Value(Array a) : fData(std::move(a)) {}
	Value(Object o) : fData(std::move(o)) {}

	bool is_null() const { return std::holds_alternative<std::nullptr_t>(fData); }
	bool is_bool() const { return std::holds_alternative<bool>(fData); }
	bool is_num()  const { return std::holds_alternative<double>(fData); }
	bool is_str()  const { return std::holds_alternative<std::string>(fData); }
	bool is_arr()  const { return std::holds_alternative<Array>(fData); }
	bool is_obj()  const { return std::holds_alternative<Object>(fData); }

	bool               as_bool() const { return std::get<bool>(fData); }
	double             as_num()  const { return std::get<double>(fData); }
	const std::string& as_str()  const { return std::get<std::string>(fData); }
	const Array&       as_arr()  const { return std::get<Array>(fData); }
	const Object&      as_obj()  const { return std::get<Object>(fData); }
	Array&             as_arr()        { return std::get<Array>(fData); }
	Object&            as_obj()        { return std::get<Object>(fData); }

	const Value* find(const std::string& key) const {
		if (!is_obj()) return nullptr;
		const auto& o = std::get<Object>(fData);
		auto it = o.find(key);
		return it == o.end() ? nullptr : &it->second;
	}

	// Object field as string; empty if missing or not a string.
	std::string str(const std::string& key) const {
		const Value* v = find(key);
		return (v && v->is_str()) ? v->as_str() : std::string();
	}

	void set(const std::string& key, Value v) {
		if (!is_obj()) fData = Object{};
		std::get<Object>(fData)[key] = std::move(v);
	}

	void push(Value v) {
		if (!is_arr()) fData = Array{};
		std::get<Array>(fData).push_back(std::move(v));
	}

	std::string serialize() const;
	static Value parse(const std::string& s);

private:
	Data fData;
};

}  // namespace pluto::json
