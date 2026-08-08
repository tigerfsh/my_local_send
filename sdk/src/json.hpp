#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace localsend {
namespace json {

class Value {
public:
  using Array = std::vector<Value>;
  using Object = std::map<std::string, Value>;

  Value();
  Value(std::nullptr_t);
  Value(bool b);
  Value(int i);
  Value(int64_t i);
  Value(uint64_t i);
  Value(double d);
  Value(const char* s);
  Value(const std::string& s);
  Value(Array a);
  Value(Object o);

  static Value array() { return Value(Array()); }
  static Value object() { return Value(Object()); }

  bool isNull() const;
  bool isBool() const;
  bool isNumber() const;
  bool isString() const;
  bool isArray() const;
  bool isObject() const;

  bool asBool(bool def = false) const;
  int64_t asInt(int64_t def = 0) const;
  double asDouble(double def = 0.0) const;
  std::string asString(const std::string& def = "") const;
  const Array& asArray() const;
  const Object& asObject() const;

  bool has(const std::string& key) const;
  const Value& at(const std::string& key) const;
  const Value& at(size_t index) const;
  const Value& operator[](const std::string& key) const;
  const Value& operator[](size_t index) const;

  Value& set(const std::string& key, const Value& v);
  void push(const Value& v);
  size_t size() const;

  std::string dump() const;
  static Value parse(const std::string& text);

private:
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v_;
  static const Value& nullRef();
  static const Array& emptyArray();
  static const Object& emptyObject();
};

} // namespace json
} // namespace localsend
