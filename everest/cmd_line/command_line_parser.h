/**
 * @file command_line_parser.h
 * @brief 命令行参数解析器。
 */

#ifndef COMMAND_LINE_PARSER_H_
#define COMMAND_LINE_PARSER_H_

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/define.h"
#include "non_copyable/non_copyable.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief 命令行参数解析器。
 *
 * 线程安全：非线程安全。先通过 Add 系列接口定义选项，再调用 Parse，须在同一线程完成；
 * 通常配合 ThreadSafeSingleton<CommandLineParser> 作为进程级单例使用。
 *
 * 所有权：选项状态由 PImpl 持有，实现位于静态库；本类不可拷贝、不可移动。
 *
 * 退出语义：Parse / Get / TryGet 不退出进程；ParseCheck / Print / ParseGetOrSetBehavior
 * 保留原 cmdline 的「打印后退出」CLI 语义。
 */
class CommandLineParser : private NonCopyable {
public:
    /**
     * @brief 选项的公共元数据。
     *
     * 只在注册调用期内使用：解析器会立即拷贝 name 与 desc，不保存 string_view，
     * 因此可以用字面量、std::string 或临时对象构造。
     */
    struct OptionDefine {
        // 长名，不能为空。
        std::string_view name;
        // 短名；0 表示无短名。
        char short_name = 0;
        // usage 中的描述。
        std::string_view desc;
        // 是否必需；开关选项忽略该字段。
        bool need = true;
    };

    /**
     * @brief 整型区间约束。
     *
     * @tparam T 区间值类型。
     */
    template <class T>
    struct RangeOption {
        // 允许的最小值。
        T low = 0;
        // 允许的最大值。
        T high = 0;
    };

    /**
     * @brief 构造解析器。
     */
    CommandLineParser();

    /**
     * @brief 析构解析器。
     */
    ~CommandLineParser();

    /**
     * @brief 注册不带值的开关选项。
     *
     * @param define 选项元数据；need 字段被忽略。
     * @note 重名时打印错误并忽略本次注册，不退出进程。
     */
    void AddWithoutValueCommand(const OptionDefine& define);

    /**
     * @brief 注册带值选项。
     *
     * @tparam T 支持的类型：std::string、bool、int、int32_t、int64_t、uint32_t、uint64_t、
     *           size_t、float、double。
     * @param define 选项元数据。
     * @param def 未传入时 Get 返回的默认值。
     * @note 重名时打印错误并忽略本次注册，不退出进程。
     */
    template <class T>
    void Add(const OptionDefine& define, const T& def = T()) {
        AddValue(define, ValueSpec{ValueKindOf<T>(), &def, sizeof(T)});
    }

    /**
     * @brief 注册带区间校验的 int 选项。
     *
     * @param define 选项元数据。
     * @param def 未传入时的默认值。
     * @param range 允许的取值区间。
     * @note 取值超出 [range.low, range.high] 时解析失败，不写入选项值。
     */
    void AddIntRange(const OptionDefine& define, int def, RangeOption<int> range);

    /**
     * @brief 注册带区间校验的 int64_t 选项。
     *
     * @param define 选项元数据。
     * @param def 未传入时的默认值。
     * @param range 允许的取值区间。
     */
    void AddInt64Range(const OptionDefine& define, int64_t def, RangeOption<int64_t> range);

    /**
     * @brief 注册带区间校验的 uint32_t 选项。
     *
     * @param define 选项元数据。
     * @param def 未传入时的默认值。
     * @param range 允许的取值区间。
     */
    void AddUint32Range(const OptionDefine& define, uint32_t def, RangeOption<uint32_t> range);

    /**
     * @brief 注册带区间校验的 uint64_t 选项。
     *
     * @param define 选项元数据。
     * @param def 未传入时的默认值。
     * @param range 允许的取值区间。
     */
    void AddUint64Range(const OptionDefine& define, uint64_t def, RangeOption<uint64_t> range);

    /**
     * @brief 注册带取值集合校验的 std::string 选项。
     *
     * @param define 选项元数据。
     * @param def 未传入时的默认值。
     * @param allowed 允许的取值集合；只支持花括号列表，内部立即拷贝。
     * @note 取值不在 allowed 中时解析失败，不写入选项值。
     */
    void AddStringOneOf(const OptionDefine& define, std::string_view def, std::initializer_list<std::string> allowed);

    /**
     * @brief 设置 usage 末尾附加文本。
     *
     * @param footer 附加文本。
     */
    void Footer(std::string_view footer);

    /**
     * @brief 设置程序名，用于 usage 首行。
     *
     * @param name 程序名。
     */
    void SetProgramName(std::string_view name);

    /**
     * @brief 获取程序名。
     *
     * @return 程序名；未设置且未按 argv 解析过时为空。
     */
    const std::string& GetProgramName() const;

    /**
     * @brief 按 argc/argv 解析，argv[0] 视为程序名。
     *
     * @param argc 参数个数。
     * @param argv 参数数组，argv[0] 是程序名。
     * @return 全部选项合法时返回 true。
     * @note 不打印、不退出；失败原因可通过 Error / ErrorFull 获取。
     */
    [[nodiscard]] bool Parse(int argc, const char* const argv[]);

    /**
     * @brief 按参数列表解析，首元素视为程序名。
     *
     * @param args 参数列表，首元素是程序名。
     * @return 全部选项合法时返回 true。
     * @note 空列表直接返回 false。
     */
    [[nodiscard]] bool Parse(const std::vector<std::string>& args);

    /**
     * @brief 按命令行字符串解析。
     *
     * 以空格切分，双引号内保留空格，反斜杠转义下一个字符；
     * 切分出的所有 token 都视为参数，不包含程序名。
     *
     * @param command_line 命令行字符串。
     * @return 切分与解析均成功时返回 true。
     * @note 未闭合引号或末尾反斜杠返回 false 并记录错误。
     */
    [[nodiscard]] bool Parse(const std::string& command_line);

    /**
     * @brief 解析并在失败时打印错误与 usage 后退出进程。
     *
     * @param argc 参数个数。
     * @param argv 参数数组。
     * @note 若未注册 help，先行注册；失败时 exit(EXIT_FAILURE)；
     *       成功且 help 被设置时打印 usage 并 exit(EXIT_SUCCESS)。
     */
    void ParseCheck(int argc, char** argv);

    /**
     * @brief 解析参数列表并在失败时打印错误与 usage 后退出进程。
     *
     * @param args 参数列表，首元素是程序名。
     * @note 成功且 help 被设置时打印 usage 并 exit(EXIT_SUCCESS)。
     */
    void ParseCheck(const std::vector<std::string>& args);

    /**
     * @brief 解析命令行字符串并在失败时打印错误与 usage 后退出进程。
     *
     * @param command_line 命令行字符串。
     * @note 成功且 help 被设置时打印 usage 并 exit(EXIT_SUCCESS)。
     */
    void ParseCheck(const std::string& command_line);

    /**
     * @brief 根据参数形态执行「打印取值」或「解析」行为。
     *
     * argc <= 1 直接返回；argc > 2 走解析；argc == 2 且为 --name 时打印该选项，
     * --name=value 时走解析；argc == 2 且为 -x 时打印该选项；其它单参数走解析。
     *
     * @param argc 参数个数。
     * @param argv 参数数组。
     * @note 打印路径成功后 exit(EXIT_SUCCESS)，失败后 exit(EXIT_FAILURE)。
     */
    void ParseGetOrSetBehavior(int argc, char** argv);

    /**
     * @brief 判断选项是否已注册。
     *
     * @param name 长名。
     * @return 已注册返回 true。
     */
    bool Exist(const std::string& name) const;

    /**
     * @brief 判断选项是否被显式设置。
     *
     * @param name 长名。
     * @return 已注册且解析时被设置返回 true。
     */
    bool IsSet(const std::string& name) const;

    /**
     * @brief 获取解析后剩余的非选项参数。
     *
     * @return 位置参数列表。
     */
    const std::vector<std::string>& Rest() const;

    /**
     * @brief 取值；失败时记录并打印错误后返回 T()，不退出进程。
     *
     * @tparam T 支持的类型，见 Add。
     * @param name 长名。
     * @return 选项值；未传入时为默认值。
     */
    template <class T>
    T Get(const std::string& name) const {
        T out{};
        if (!TryGetRaw(name, ValueKindOf<T>(), &out)) {
            ReportLookupFailure(name);
        }
        return out;
    }

    /**
     * @brief 不打印、不退出地取值。
     *
     * @tparam T 支持的类型，见 Add。
     * @param name 长名。
     * @param out 输出参数；失败时保持不变。
     * @return 选项已注册、有值且类型匹配时返回 true。
     */
    template <class T>
    [[nodiscard]] bool TryGet(const std::string& name, T& out) const {
        return TryGetRaw(name, ValueKindOf<T>(), &out);
    }

    /**
     * @brief 打印选项值（开关打印描述）与 usage 后退出进程。
     *
     * @param name 长名。
     * @note 找到时 exit(EXIT_SUCCESS)，未找到时打印错误并 exit(EXIT_FAILURE)。
     */
    void Print(const std::string& name) const;

    /**
     * @brief 按短名打印选项值（开关打印描述）与 usage 后退出进程。
     *
     * @param short_name 短名。
     * @note 找到时 exit(EXIT_SUCCESS)，未找到时打印错误并 exit(EXIT_FAILURE)。
     */
    void Print(char short_name) const;

    /**
     * @brief 生成 usage 文本。
     *
     * @return usage 文本。
     */
    std::string Usage() const;

    /**
     * @brief 获取第一条错误信息。
     *
     * @return 第一条错误的副本；无错误时为空字符串。
     */
    std::string Error() const;

    /**
     * @brief 获取全部错误信息。
     *
     * @return 每条错误一行拼接后的文本。
     */
    std::string ErrorFull() const;

private:
    /**
     * @brief 选项值类别；类型别名按位宽归一到同一类别。
     */
    enum class ValueKind : std::uint8_t {
        // 无值（开关）。
        kNone,
        // std::string。
        kString,
        // bool。
        kBool,
        // int / int32_t。
        kInt,
        // int64_t。
        kInt64,
        // uint32_t。
        kUint32,
        // uint64_t / size_t（64 位平台）。
        kUint64,
        // float。
        kFloat,
        // double。
        kDouble,
    };

    /**
     * @brief 默认值的类型擦除描述。
     */
    struct ValueSpec {
        // 值类别。
        ValueKind kind;
        // 默认值地址；只在注册调用期内有效。
        const void* default_value;
        // 默认值字节数。
        std::size_t value_size;
    };

    /**
     * @brief 编译期把受支持类型映射为 ValueKind。
     *
     * @tparam T 值类型。
     * @return 对应的 ValueKind。
     */
    template <class T>
    static constexpr ValueKind ValueKindOf() {
        static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
                          std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> || std::is_same_v<T, size_t> || std::is_same_v<T, float> || std::is_same_v<T, double>,
                      "unsupported command line value type");
        if constexpr (std::is_same_v<T, std::string>) {
            return ValueKind::kString;
        } else if constexpr (std::is_same_v<T, bool>) {
            return ValueKind::kBool;
        } else if constexpr (std::is_same_v<T, float>) {
            return ValueKind::kFloat;
        } else if constexpr (std::is_same_v<T, double>) {
            return ValueKind::kDouble;
        } else if constexpr (std::is_signed_v<T>) {
            return sizeof(T) <= sizeof(int) ? ValueKind::kInt : ValueKind::kInt64;
        } else {
            return sizeof(T) <= sizeof(uint32_t) ? ValueKind::kUint32 : ValueKind::kUint64;
        }
    }

    /**
     * @brief 注册带值选项的实现入口。
     *
     * @param define 选项元数据。
     * @param value 默认值描述。
     */
    void AddValue(const OptionDefine& define, ValueSpec value);

    /**
     * @brief 取值实现入口。
     *
     * @param name 长名。
     * @param kind 调用方期望的值类别。
     * @param out 输出缓冲，类型由 kind 决定。
     * @return 成功返回 true。
     */
    bool TryGetRaw(const std::string& name, ValueKind kind, void* out) const;

    /**
     * @brief 记录并打印取值失败信息。
     *
     * @param name 长名。
     * @note 按选项状态区分文案：未注册、开关无值、类型不符。
     */
    void ReportLookupFailure(const std::string& name) const;

    /**
     * @brief 按最近一次解析结果决定是否打印并退出。
     *
     * @note 失败时向 stderr 打印错误与 usage 后 exit(EXIT_FAILURE)；
     *       成功且 help 被设置时向 stdout 打印 usage 后 exit(EXIT_SUCCESS)。
     */
    void CheckParseResult();

    /**
     * @brief 确保 help 选项已注册。
     */
    void EnsureHelpOption();

    struct Impl;

    // 解析器状态，定义位于静态库。
    std::unique_ptr<Impl> impl_;
};

NAMESPACE_EVEREST_END

#endif
