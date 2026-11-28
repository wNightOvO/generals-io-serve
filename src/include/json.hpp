/*
    极简 JSON 解析器
    单头文件 零外部依赖 只使用标准库
    支持对象 数组 字符串 数字 布尔 空值
    字符串支持 \" \\ \/ \b \f \n \r \t \uXXXX 转义
    解析失败抛出 ParseError 带行列信息 不会崩溃
*/

#pragma once
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gen::json {

// JSON 值类型
enum class Type {
    Null, // 空值
    Bool, // 布尔值
    Number, // 数字 统一以双精度浮点存储
    String, // 字符串
    Array, // 数组
    Object // 对象
};

// 返回值类型的可读中文名 参数 type 为值类型 返回常量字符串
inline const char* typeName(Type type) {
    switch(type) {
        case Type::Null: return "空值";
        case Type::Bool: return "布尔值";
        case Type::Number: return "数字";
        case Type::String: return "字符串";
        case Type::Array: return "数组";
        case Type::Object: return "对象";
        default: return "未知";
    }
}

// 解析错误 继承标准异常 带出错行号与列号 消息为中文
class ParseError : public std::runtime_error {
public:
    // 参数 message 为中文错误说明 参数 line 与 column 为出错位置 0 表示位置未知
    explicit ParseError(const std::string &message, std::size_t line = 0, std::size_t column = 0)
        : std::runtime_error(buildText(message, line, column)), line_(line), column_(column) {}

    // 返回出错行号 从 1 开始 0 表示位置未知
    std::size_t line() const noexcept {
        return line_;
    }

    // 返回出错列号 从 1 开始 0 表示位置未知
    std::size_t column() const noexcept {
        return column_;
    }

private:
    // 把说明与行列信息拼成完整消息
    static std::string buildText(const std::string &message, std::size_t line, std::size_t column) {
        if(line == 0) return message;
        return "第 " + std::to_string(line) + " 行 第 " + std::to_string(column) + " 列 " + message;
    }

    std::size_t line_ = 0; // 出错行号
    std::size_t column_ = 0; // 出错列号
};

// 取值错误 类型不匹配 键不存在 下标越界时抛出 消息为中文
class ValueError : public std::runtime_error {
public:
    explicit ValueError(const std::string &message): std::runtime_error(message) {}
};

namespace detail { // namespace detail

class Parser; // 解析器 定义在文件末尾

} // namespace detail

// JSON 值 可以表示空值 布尔值 数字 字符串 数组 对象
class Value {
public:
    using Array = std::vector<Value>; // 数组容器
    using Object = std::map<std::string, Value>; // 对象容器 键为字符串

    // 默认构造为空值
    Value() = default;

    // 由布尔值构造
    explicit Value(bool value): type_(Type::Bool), bool_(value) {}

    // 由 32 位整数构造
    explicit Value(int value): type_(Type::Number), number_(static_cast<double>(value)) {}

    // 由 64 位整数构造
    explicit Value(std::int64_t value): type_(Type::Number), number_(static_cast<double>(value)) {}

    // 由双精度浮点数构造
    explicit Value(double value): type_(Type::Number), number_(value) {}

    // 由字符串构造 参数 value 为内容 移动保存
    explicit Value(std::string value): type_(Type::String), string_(std::move(value)) {}

    // 由 C 字符串构造 空指针按空字符串处理
    explicit Value(const char *value): type_(Type::String), string_(value == nullptr ? "" : value) {}

    // 返回值的类型
    Type type() const noexcept {
        return type_;
    }

    // 返回类型的中文名 例如 数字
    const char* typeName() const noexcept {
        return gen::json::typeName(type_);
    }

    // 是否为空值
    bool isNull() const noexcept {
        return type_ == Type::Null;
    }

    // 是否为布尔值
    bool isBool() const noexcept {
        return type_ == Type::Bool;
    }

    // 是否为数字
    bool isNumber() const noexcept {
        return type_ == Type::Number;
    }

    // 是否为字符串
    bool isString() const noexcept {
        return type_ == Type::String;
    }

    // 是否为数组
    bool isArray() const noexcept {
        return type_ == Type::Array;
    }

    // 是否为对象
    bool isObject() const noexcept {
        return type_ == Type::Object;
    }

    // 是否为可以精确转换为 64 位整数的数字
    bool isInteger() const noexcept {
        if(type_ != Type::Number) return false;
        if(std::isfinite(number_) == false) return false;
        if(std::floor(number_) != number_) return false;
        return std::fabs(number_) <= 9.0e18;
    }

    // 取布尔值 类型不匹配时抛出 ValueError
    bool asBool() const {
        expectType(Type::Bool);
        return bool_;
    }

    // 取双精度数字 类型不匹配时抛出 ValueError
    double asNumber() const {
        expectType(Type::Number);
        return number_;
    }

    // 取 64 位整数 数字必须为整数 否则抛出 ValueError
    std::int64_t asInteger() const {
        expectType(Type::Number);
        if(isInteger() == false) throw ValueError("取整数失败 数字不是 64 位范围内的整数 当前为数字");
        return static_cast<std::int64_t>(number_);
    }

    // 取 32 位整数 超出范围时抛出 ValueError
    int asInt() const {
        std::int64_t value = asInteger();
        if(value < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
           value > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            throw ValueError("取整型失败 数字超出 32 位整数范围");
        }
        return static_cast<int>(value);
    }

    // 取字符串引用 类型不匹配时抛出 ValueError
    const std::string& asString() const {
        expectType(Type::String);
        return string_;
    }

    // 取数组引用 类型不匹配时抛出 ValueError
    const Array& asArray() const {
        expectType(Type::Array);
        return array_;
    }

    // 取对象引用 类型不匹配时抛出 ValueError
    const Object& asObject() const {
        expectType(Type::Object);
        return object_;
    }

    // 返回数组元素个数或对象键个数 其它类型返回 0
    std::size_t size() const noexcept {
        if(type_ == Type::Array) return array_.size();
        if(type_ == Type::Object) return object_.size();
        return 0;
    }

    // 数组或对象是否没有元素 其它类型返回 true
    bool empty() const noexcept {
        return size() == 0;
    }

    // 对象是否包含指定键 非对象抛出 ValueError
    bool contains(const std::string &key) const {
        expectType(Type::Object);
        return object_.find(key) != object_.end();
    }

    // 在对象中查找键 找不到返回空指针 非对象抛出 ValueError
    const Value* find(const std::string &key) const {
        expectType(Type::Object);
        auto hit = object_.find(key);
        return hit == object_.end() ? nullptr : &hit->second;
    }

    // 取对象成员 键不存在或非对象抛出 ValueError
    const Value& at(const std::string &key) const {
        expectType(Type::Object);
        auto hit = object_.find(key);
        if(hit == object_.end()) throw ValueError("对象中不存在键 " + key);
        return hit->second;
    }

    // 取数组元素 下标越界或非数组抛出 ValueError
    const Value& at(std::size_t index) const {
        expectType(Type::Array);
        if(index >= array_.size()) {
            throw ValueError("数组下标越界 下标 " + std::to_string(index) + " 长度 " + std::to_string(array_.size()));
        }
        return array_[index];
    }

    // 同 at 便于使用 value["map"]["rows"] 形式读取对象成员
    const Value& operator[] (const std::string &key) const {
        return at(key);
    }

    // 同 at 便于使用 value[0] 形式读取数组元素
    const Value& operator[] (std::size_t index) const {
        return at(index);
    }

private:
    friend class detail::Parser; // 允许解析器写入内部数据

    // 检查当前类型 不符合时抛出 ValueError
    void expectType(Type expected) const {
        if(type_ == expected) return;
        throw ValueError(std::string("取值类型错误 期望") + gen::json::typeName(expected) + " 实际为" + gen::json::typeName(type_));
    }

    Type type_ = Type::Null; // 值类型
    bool bool_ = false; // 布尔值内容
    double number_ = 0.0; // 数字内容
    std::string string_; // 字符串内容
    Array array_; // 数组内容
    Object object_; // 对象内容
};

namespace detail { // namespace detail

// JSON 解析器 递归下降 只由 parse 与 parseFile 使用
class Parser {
public:
    // 参数 text 为完整 JSON 文本 只保存引用 调用期间文本必须有效
    explicit Parser(const std::string &text): text_(text) {}

    // 解析整个文档 返回根值 失败抛出 ParseError
    Value parseDocument() {
        skipBom();
        skipSpace();
        if(atEnd()) fail("内容为空 缺少 JSON 值");
        Value value = parseValue(0);
        skipSpace();
        if(atEnd() == false) fail("根节点之后存在多余内容");
        return value;
    }

private:
    static constexpr int MAX_DEPTH = 256; // 最大嵌套层数 防止递归过深导致栈溢出

    const std::string &text_; // 待解析文本
    std::size_t pos_ = 0; // 当前字节位置
    std::size_t line_ = 1; // 当前行号 从 1 开始
    std::size_t column_ = 1; // 当前列号 从 1 开始

    // 是否到达文本末尾
    bool atEnd() const {
        return pos_ >= text_.size();
    }

    // 返回当前字符 调用前必须确认未到末尾
    char peek() const {
        return text_[pos_];
    }

    // 前进一个字节 并更新行列
    char advance() {
        char ch = text_[pos_++];
        if(ch == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return ch;
    }

    // 跳过文件开头的 UTF-8 字节顺序标记
    // Windows 记事本与部分 PowerShell 命令保存 UTF-8 文件时会写入该标记
    // 同时识别 UTF-16 与 UTF-32 的字节顺序标记 并给出明确的中文错误
    void skipBom() {
        if(text_.size() >= 3 &&
           static_cast<unsigned char>(text_[0]) == 0xEF &&
           static_cast<unsigned char>(text_[1]) == 0xBB &&
           static_cast<unsigned char>(text_[2]) == 0xBF) {
            pos_ = 3; // 跳过 UTF-8 字节顺序标记
            return;
        }
        if(text_.size() >= 2) {
            unsigned char first = static_cast<unsigned char>(text_[0]);
            unsigned char second = static_cast<unsigned char>(text_[1]);
            bool utf16Little = (first == 0xFF && second == 0xFE);
            bool utf16Big = (first == 0xFE && second == 0xFF);
            if(utf16Little || utf16Big) {
                fail("文件不是 UTF-8 编码 检测到 UTF-16 或 UTF-32 字节顺序标记 请把文件另存为 UTF-8 编码");
            }
        }
    }

    // 跳过空白字符
    void skipSpace() {
        while(atEnd() == false) {
            char ch = peek();
            if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') advance();
            else break;
        }
    }

    // 抛出解析错误 位置为当前行列
    [[noreturn]] void fail(const std::string &message) const {
        throw ParseError(message, line_, column_);
    }

    // 抛出解析错误 位置为指定行列
    [[noreturn]] void failAt(const std::string &message, std::size_t line, std::size_t column) const {
        throw ParseError(message, line, column);
    }

    // 把字符转为十六进制字节串
    static std::string hexByte(unsigned char code) {
        const char *digits = "0123456789ABCDEF";
        std::string text;
        text.push_back(digits[code >> 4]);
        text.push_back(digits[code & 0x0F]);
        return text;
    }

    // 返回字符的可读描述 用于错误消息
    static std::string describeChar(char ch) {
        unsigned char code = static_cast<unsigned char>(ch);
        if(code >= 0x20 && code < 0x7F) {
            std::string text = " '";
            text.push_back(ch);
            text.push_back('\'');
            return text;
        }
        return " 字节 0x" + hexByte(code);
    }

    // 解析一个值 参数 depth 为当前嵌套层数
    Value parseValue(int depth) {
        if(depth > MAX_DEPTH) fail("嵌套层数过深 最多 " + std::to_string(MAX_DEPTH) + " 层");
        if(atEnd()) fail("缺少 JSON 值");
        char ch = peek();
        if(ch == '{') return parseObject(depth);
        if(ch == '[') return parseArray(depth);
        if(ch == '"') return parseStringValue();
        if(ch == 't') {
            expectLiteral("true");
            return Value(true);
        }
        if(ch == 'f') {
            expectLiteral("false");
            return Value(false);
        }
        if(ch == 'n') {
            expectLiteral("null");
            return Value();
        }
        if(ch == '-' || (ch >= '0' && ch <= '9')) return parseNumber();
        fail(std::string("无法识别的字符") + describeChar(ch));
    }

    // 校验并跳过固定字面量 例如 true
    void expectLiteral(const char *literal) {
        for(const char *cursor = literal; *cursor != '\0'; ++cursor) {
            if(atEnd()) fail(std::string("期望字面量 ") + literal + " 但文本已结束");
            if(peek() != *cursor) fail(std::string("期望字面量 ") + literal + " 但遇到" + describeChar(peek()));
            advance();
        }
    }

    // 解析数字 支持负号 小数 指数
    Value parseNumber() {
        std::size_t startPos = pos_;
        std::size_t startLine = line_;
        std::size_t startColumn = column_;
        if(peek() == '-') advance();
        if(atEnd()) failAt("数字缺少数字部分", startLine, startColumn);
        if(peek() == '0') {
            advance();
            if(atEnd() == false && peek() >= '0' && peek() <= '9') failAt("数字不能有前导零", startLine, startColumn);
        } else if(peek() >= '1' && peek() <= '9') {
            while(atEnd() == false && peek() >= '0' && peek() <= '9') advance();
        } else {
            failAt("数字缺少数字部分", startLine, startColumn);
        }
        if(atEnd() == false && peek() == '.') {
            advance();
            if(atEnd() || peek() < '0' || peek() > '9') failAt("小数点之后必须至少有一位数字", startLine, startColumn);
            while(atEnd() == false && peek() >= '0' && peek() <= '9') advance();
        }
        if(atEnd() == false && (peek() == 'e' || peek() == 'E')) {
            advance();
            if(atEnd() == false && (peek() == '+' || peek() == '-')) advance();
            if(atEnd() || peek() < '0' || peek() > '9') failAt("指数部分必须至少有一位数字", startLine, startColumn);
            while(atEnd() == false && peek() >= '0' && peek() <= '9') advance();
        }
        if(atEnd() == false) {
            char next = peek();
            if(next != ' ' && next != '\t' && next != '\r' && next != '\n' && next != ',' && next != ']' && next != '}') {
                failAt(std::string("数字之后存在非法字符") + describeChar(next), startLine, startColumn);
            }
        }
        std::string token = text_.substr(startPos, pos_ - startPos);
        double number = 0.0;
        auto result = std::from_chars(token.data(), token.data() + token.size(), number);
        if(result.ec != std::errc() || result.ptr != token.data() + token.size()) failAt("数字格式非法 内容为 " + token, startLine, startColumn);
        if(std::isfinite(number) == false) failAt("数字超出双精度浮点范围 内容为 " + token, startLine, startColumn);
        Value value;
        value.type_ = Type::Number;
        value.number_ = number;
        return value;
    }

    // 解析字符串值
    Value parseStringValue() {
        std::string content = parseStringContent();
        Value value;
        value.type_ = Type::String;
        value.string_ = std::move(content);
        return value;
    }

    // 解析字符串字面量并返回内容 调用前当前字符必须是双引号
    std::string parseStringContent() {
        advance(); // 跳过左侧双引号
        std::string content;
        while(true) {
            if(atEnd()) fail("字符串没有结束 缺少右侧双引号");
            char ch = peek();
            if(ch == '"') {
                advance();
                return content;
            }
            if(static_cast<unsigned char>(ch) < 0x20) fail("字符串中出现未转义的控制字符");
            if(ch == '\\') {
                advance(); // 跳过反斜杠
                if(atEnd()) fail("字符串转义不完整");
                char escape = peek();
                if(escape == '"') {
                    content.push_back('"');
                    advance();
                } else if(escape == '\\') {
                    content.push_back('\\');
                    advance();
                } else if(escape == '/') {
                    content.push_back('/');
                    advance();
                } else if(escape == 'b') {
                    content.push_back('\b');
                    advance();
                } else if(escape == 'f') {
                    content.push_back('\f');
                    advance();
                } else if(escape == 'n') {
                    content.push_back('\n');
                    advance();
                } else if(escape == 'r') {
                    content.push_back('\r');
                    advance();
                } else if(escape == 't') {
                    content.push_back('\t');
                    advance();
                } else if(escape == 'u') {
                    advance(); // 跳过字母 u
                    appendUnicode(content);
                } else {
                    fail(std::string("非法的转义字符") + describeChar(escape));
                }
            } else {
                content.push_back(advance()); // 直接复制 UTF-8 字节
            }
        }
    }

    // 解析 \u 之后的 4 位十六进制数字 处理代理对 追加 UTF-8 字节
    void appendUnicode(std::string &content) {
        std::uint32_t code = readHex4();
        if(code >= 0xD800 && code <= 0xDBFF) {
            if(atEnd() || peek() != '\\') fail("unicode 代理对不完整 缺少低位代理");
            advance();
            if(atEnd() || peek() != 'u') fail("unicode 代理对不完整 缺少低位代理");
            advance();
            std::uint32_t low = readHex4();
            if(low < 0xDC00 || low > 0xDFFF) fail("unicode 代理对不完整 低位代理非法");
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        } else if(code >= 0xDC00 && code <= 0xDFFF) {
            fail("非法的 unicode 代理项 不能单独出现低位代理");
        }
        appendUtf8(content, code);
    }

    // 读取 4 位十六进制数字 返回码值
    std::uint32_t readHex4() {
        std::uint32_t value = 0;
        for(int count = 0; count < 4; ++count) {
            if(atEnd()) fail("unicode 转义需要 4 位十六进制数字");
            int digit = hexDigit(peek());
            if(digit < 0) fail(std::string("unicode 转义需要 4 位十六进制数字 遇到") + describeChar(peek()));
            value = value * 16 + static_cast<std::uint32_t>(digit);
            advance();
        }
        return value;
    }

    // 返回十六进制数字的值 非法字符返回 -1
    static int hexDigit(char ch) {
        if(ch >= '0' && ch <= '9') return ch - '0';
        if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    }

    // 把码点追加为 UTF-8 字节序列
    static void appendUtf8(std::string &content, std::uint32_t code) {
        if(code <= 0x7F) {
            content.push_back(static_cast<char>(code));
        } else if(code <= 0x7FF) {
            content.push_back(static_cast<char>(0xC0 | (code >> 6)));
            content.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if(code <= 0xFFFF) {
            content.push_back(static_cast<char>(0xE0 | (code >> 12)));
            content.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            content.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            content.push_back(static_cast<char>(0xF0 | (code >> 18)));
            content.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            content.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            content.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    // 解析数组
    Value parseArray(int depth) {
        advance(); // 跳过左方括号
        Value value;
        value.type_ = Type::Array;
        skipSpace();
        if(atEnd()) fail("数组没有结束 缺少右方括号");
        if(peek() == ']') {
            advance();
            return value;
        }
        while(true) {
            skipSpace();
            value.array_.push_back(parseValue(depth + 1));
            skipSpace();
            if(atEnd()) fail("数组没有结束 缺少右方括号");
            char ch = peek();
            if(ch == ']') {
                advance();
                return value;
            }
            if(ch != ',') fail(std::string("数组元素之间缺少逗号或右方括号") + describeChar(ch));
            advance(); // 跳过逗号
            skipSpace();
            if(atEnd()) fail("数组没有结束 缺少右方括号");
            if(peek() == ']') fail("数组末尾不能有逗号");
        }
    }

    // 解析对象
    Value parseObject(int depth) {
        advance(); // 跳过左花括号
        Value value;
        value.type_ = Type::Object;
        skipSpace();
        if(atEnd()) fail("对象没有结束 缺少右花括号");
        if(peek() == '}') {
            advance();
            return value;
        }
        while(true) {
            skipSpace();
            if(atEnd()) fail("对象没有结束 缺少右花括号");
            if(peek() != '"') fail(std::string("对象的键必须是字符串 遇到") + describeChar(peek()));
            std::string key = parseStringContent();
            skipSpace();
            if(atEnd() || peek() != ':') fail("对象的键之后缺少冒号");
            advance(); // 跳过冒号
            skipSpace();
            Value member = parseValue(depth + 1);
            if(value.object_.find(key) != value.object_.end()) fail("对象中出现重复的键 " + key);
            value.object_[key] = std::move(member);
            skipSpace();
            if(atEnd()) fail("对象没有结束 缺少右花括号");
            char ch = peek();
            if(ch == '}') {
                advance();
                return value;
            }
            if(ch != ',') fail(std::string("对象成员之间缺少逗号或右花括号") + describeChar(ch));
            advance(); // 跳过逗号
            skipSpace();
            if(atEnd()) fail("对象没有结束 缺少右花括号");
            if(peek() == '}') fail("对象末尾不能有逗号");
        }
    }
};

} // namespace detail

// 解析 JSON 文本 参数 text 为完整文本 失败抛出 ParseError
inline Value parse(const std::string &text) {
    detail::Parser parser(text);
    return parser.parseDocument();
}

// 解析 JSON 文件 参数 filePath 为文件路径 失败抛出 ParseError
inline Value parseFile(const std::filesystem::path &filePath) {
    std::ifstream input(filePath, std::ios::binary);
    if(input.is_open() == false) throw ParseError("无法打开文件 " + filePath.string());
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if(input.bad()) throw ParseError("读取文件失败 " + filePath.string());
    return parse(text);
}

} // namespace gen::json
