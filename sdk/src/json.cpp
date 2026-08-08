#include "json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace localsend {
namespace json {

namespace {
const Value kNullValue;
const Value::Array kEmptyArray;
const Value::Object kEmptyObject;
} // namespace

Value::Value() : v_(nullptr) {}
Value::Value(std::nullptr_t) : v_(nullptr) {}
Value::Value(bool b) : v_(b) {}
Value::Value(int i) : v_(static_cast<double>(i)) {}
Value::Value(int64_t i) : v_(static_cast<double>(i)) {}
Value::Value(uint64_t i) : v_(static_cast<double>(static_cast<int64_t>(i))) {}
Value::Value(double d) : v_(d) {}
Value::Value(const char* s) : v_(std::string(s ? s : "")) {}
Value::Value(const std::string& s) : v_(s) {}
Value::Value(Array a) : v_(std::move(a)) {}
Value::Value(Object o) : v_(std::move(o)) {}

bool Value::isNull() const { return std::holds_alternative<std::nullptr_t>(v_); }
bool Value::isBool() const { return std::holds_alternative<bool>(v_); }
bool Value::isNumber() const { return std::holds_alternative<double>(v_); }
bool Value::isString() const { return std::holds_alternative<std::string>(v_); }
bool Value::isArray() const { return std::holds_alternative<Array>(v_); }
bool Value::isObject() const { return std::holds_alternative<Object>(v_); }

bool Value::asBool(bool def) const { return isBool() ? std::get<bool>(v_) : def; }
int64_t Value::asInt(int64_t def) const { return isNumber() ? static_cast<int64_t>(std::get<double>(v_)) : def; }
double Value::asDouble(double def) const { return isNumber() ? std::get<double>(v_) : def; }
std::string Value::asString(const std::string& def) const {
  return isString() ? std::get<std::string>(v_) : def;
}
const Value::Array& Value::asArray() const { return isArray() ? std::get<Array>(v_) : emptyArray(); }
const Value::Object& Value::asObject() const { return isObject() ? std::get<Object>(v_) : emptyObject(); }

bool Value::has(const std::string& key) const {
  return isObject() && std::get<Object>(v_).count(key) > 0;
}

const Value& Value::at(const std::string& key) const {
  if (!isObject()) return nullRef();
  const auto& m = std::get<Object>(v_);
  auto it = m.find(key);
  return it != m.end() ? it->second : nullRef();
}
const Value& Value::at(size_t index) const {
  if (!isArray()) return nullRef();
  const auto& a = std::get<Array>(v_);
  return index < a.size() ? a[index] : nullRef();
}
const Value& Value::operator[](const std::string& key) const { return at(key); }
const Value& Value::operator[](size_t index) const { return at(index); }

Value& Value::set(const std::string& key, const Value& v) {
  if (!isObject()) v_ = Object();
  std::get<Object>(v_)[key] = v;
  return *this;
}
void Value::push(const Value& v) {
  if (!isArray()) v_ = Array();
  std::get<Array>(v_).push_back(v);
}
size_t Value::size() const {
  if (isArray()) return std::get<Array>(v_).size();
  if (isObject()) return std::get<Object>(v_).size();
  return 0;
}

const Value& Value::nullRef() { return kNullValue; }
const Value::Array& Value::emptyArray() { return kEmptyArray; }
const Value::Object& Value::emptyObject() { return kEmptyObject; }

namespace {

void dumpString(const std::string& s, std::string& out) {
  out.push_back('"');
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  out.push_back('"');
}

void dumpValue(const Value& v, std::string& out) {
  if (v.isNull()) { out += "null"; return; }
  if (v.isBool()) { out += v.asBool() ? "true" : "false"; return; }
  if (v.isNumber()) {
    double d = v.asDouble();
    if (d == static_cast<int64_t>(d) && std::fabs(d) < 9.2e18) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(d));
      out += buf;
    } else {
      char buf[40];
      std::snprintf(buf, sizeof(buf), "%.10g", d);
      out += buf;
    }
    return;
  }
  if (v.isString()) { dumpString(v.asString(), out); return; }
  if (v.isArray()) {
    out.push_back('[');
    bool first = true;
    for (const auto& e : v.asArray()) {
      if (!first) out.push_back(',');
      first = false;
      dumpValue(e, out);
    }
    out.push_back(']');
    return;
  }
  out.push_back('{');
  bool first = true;
  for (const auto& kv : v.asObject()) {
    if (!first) out.push_back(',');
    first = false;
    dumpString(kv.first, out);
    out.push_back(':');
    dumpValue(kv.second, out);
  }
  out.push_back('}');
}

struct Parser {
  const std::string& text;
  size_t pos = 0;
  bool failed = false;

  explicit Parser(const std::string& t) : text(t) {}

  void skipWs() {
    while (pos < text.size()) {
      char c = text[pos];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos;
      else break;
    }
  }

  Value parse() {
    skipWs();
    Value v = parseValue();
    skipWs();
    return v;
  }

  Value parseValue() {
    if (pos >= text.size()) { failed = true; return Value(); }
    char c = text[pos];
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == '"') return parseString();
    if (c == 't') { expect("true"); return Value(true); }
    if (c == 'f') { expect("false"); return Value(false); }
    if (c == 'n') { expect("null"); return Value(); }
    return parseNumber();
  }

  void expect(const char* word) {
    size_t n = std::strlen(word);
    if (text.compare(pos, n, word) != 0) { failed = true; return; }
    pos += n;
  }

  Value parseObject() {
    Value obj = Value::object();
    ++pos; // {
    skipWs();
    if (pos < text.size() && text[pos] == '}') { ++pos; return obj; }
    while (pos < text.size()) {
      skipWs();
      if (pos >= text.size() || text[pos] != '"') { failed = true; return obj; }
      std::string key = parseStringRaw();
      skipWs();
      if (pos >= text.size() || text[pos] != ':') { failed = true; return obj; }
      ++pos;
      skipWs();
      obj.set(key, parseValue());
      skipWs();
      if (pos >= text.size()) { failed = true; return obj; }
      if (text[pos] == ',') { ++pos; continue; }
      if (text[pos] == '}') { ++pos; break; }
      failed = true;
      break;
    }
    return obj;
  }

  Value parseArray() {
    Value arr = Value::array();
    ++pos; // [
    skipWs();
    if (pos < text.size() && text[pos] == ']') { ++pos; return arr; }
    while (pos < text.size()) {
      skipWs();
      arr.push(parseValue());
      skipWs();
      if (pos >= text.size()) { failed = true; return arr; }
      if (text[pos] == ',') { ++pos; continue; }
      if (text[pos] == ']') { ++pos; break; }
      failed = true;
      break;
    }
    return arr;
  }

  Value parseString() { return Value(parseStringRaw()); }

  std::string parseStringRaw() {
    std::string out;
    ++pos; // opening quote
    while (pos < text.size()) {
      char c = text[pos++];
      if (c == '"') return out;
      if (c == '\\') {
        if (pos >= text.size()) { failed = true; return out; }
        char e = text[pos++];
        switch (e) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'u': {
            if (pos + 4 > text.size()) { failed = true; return out; }
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
              char h = text[pos++];
              code <<= 4;
              if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
              else { failed = true; return out; }
            }
            if (code < 0x80) out.push_back(static_cast<char>(code));
            else if (code < 0x800) {
              out.push_back(static_cast<char>(0xC0 | (code >> 6)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
              out.push_back(static_cast<char>(0xE0 | (code >> 12)));
              out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            break;
          }
          default: out.push_back(e); break;
        }
      } else {
        out.push_back(c);
      }
    }
    failed = true;
    return out;
  }

  Value parseNumber() {
    size_t start = pos;
    if (pos < text.size() && text[pos] == '-') ++pos;
    while (pos < text.size()) {
      char c = text[pos];
      if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') ++pos;
      else break;
    }
    std::string num = text.substr(start, pos - start);
    char* end = nullptr;
    double d = std::strtod(num.c_str(), &end);
    if (end != num.c_str() + num.size()) { failed = true; return Value(0); }
    return Value(d);
  }
};

} // namespace

std::string Value::dump() const {
  std::string out;
  dumpValue(*this, out);
  return out;
}

Value Value::parse(const std::string& text) {
  Parser p(text);
  Value v = p.parse();
  return p.failed ? Value() : v;
}

} // namespace json
} // namespace localsend
