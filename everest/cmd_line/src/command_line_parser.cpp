// 本文件在统一编译单元中被 include，公共头由 common/include.h 先行包含。
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <system_error>
#include <unordered_map>
#include <variant>

NAMESPACE_EVEREST_BEGIN

/**
 * @brief CommandLineParser 的私有实现。
 */
struct CommandLineParser::Impl {
    // 选项值；按 ValueKind 选择备用类型。
    using Value = std::variant<std::monostate, std::string, bool, int, int64_t, uint32_t, uint64_t, float, double>;

    /**
     * @brief 单个选项的注册与解析状态。
     */
    struct Option {
        // 长名。
        std::string name;
        // 短名；0 表示无短名。
        char short_name = 0;
        // usage 描述。
        std::string description;
        // 是否必需。
        bool need = true;
        // 是否带值；false 表示开关。
        bool has_value = true;
        // 是否在解析中被显式设置。
        bool set = false;
        // 值类别。
        ValueKind kind = ValueKind::kNone;
        // 默认值。
        Value def;
        // 当前值。
        Value actual;
        // 是否有区间校验。
        bool has_range = false;
        // 有符号区间下界。
        int64_t low_signed = 0;
        // 有符号区间上界。
        int64_t high_signed = 0;
        // 无符号区间下界。
        uint64_t low_unsigned = 0;
        // 无符号区间上界。
        uint64_t high_unsigned = 0;
        // 是否有取值集合校验。
        bool has_allowed = false;
        // 允许的字符串取值集合。
        std::vector<std::string> allowed;
    };

    // 无效下标。
    static constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

    /**
     * @brief 注册带值选项。
     *
     * @param define 选项元数据。
     * @param value 默认值描述。
     * @return 注册成功返回 true；重名或默认值不合法返回 false。
     */
    bool AddValueOption(const OptionDefine& define, const ValueSpec& value);

    /**
     * @brief 注册开关选项。
     *
     * @param define 选项元数据。
     * @return 注册成功返回 true；重名返回 false。
     */
    bool AddFlagOption(const OptionDefine& define);

    /**
     * @brief 校验长名是否可注册。
     *
     * @param name 长名。
     * @return 可注册返回 true；空名或重名返回 false，并打印原因。
     */
    bool CanAddOption(const std::string& name);

    /**
     * @brief 校验短名是否可注册。
     *
     * @param short_name 短名；0 表示无短名。
     * @return 可注册返回 true；已被其他选项占用时返回 false，并打印原因。
     */
    bool CanAddShortName(char short_name) const;

    /**
     * @brief 追加选项并维护长名索引。
     *
     * @param option 待追加选项。
     */
    void AppendOption(Option option);

    /**
     * @brief 按值类别把调用方默认值写入选项。
     *
     * @param option 目标选项。
     * @param value 默认值描述。
     * @return 成功返回 true。
     */
    static bool AssignDefault(Option& option, const ValueSpec& value);

    /**
     * @brief 按类型把默认值写入选项值。
     *
     * @tparam T 默认值类型。
     * @param destination 目标值。
     * @param source 源地址。
     * @param source_size 源字节数。
     * @return 类型大小匹配返回 true。
     */
    template <class T>
    static bool CopyDefault(Value& destination, const void* source, std::size_t source_size) {
        if (source == nullptr || source_size != sizeof(T)) {
            return false;
        }

        destination = *static_cast<const T*>(source);
        return true;
    }

    /**
     * @brief 给最后注册的选项设置有符号区间。
     *
     * @param low 下界。
     * @param high 上界。
     */
    void SetLastSignedRange(int64_t low, int64_t high);

    /**
     * @brief 给最后注册的选项设置无符号区间。
     *
     * @param low 下界。
     * @param high 上界。
     */
    void SetLastUnsignedRange(uint64_t low, uint64_t high);

    /**
     * @brief 给最后注册的选项设置取值集合。
     *
     * @param allowed 允许的取值集合；在调用内拷贝。
     */
    void SetLastAllowed(std::initializer_list<std::string> allowed);

    /**
     * @brief 按长名查找选项。
     *
     * @param name 长名。
     * @return 找到返回选项指针，否则返回 nullptr。
     */
    const Option* FindOption(const std::string& name) const;

    /**
     * @brief 按长名查找可写选项。
     *
     * @param name 长名。
     * @return 找到返回选项指针，否则返回 nullptr。
     */
    Option* FindMutableOption(const std::string& name);

    /**
     * @brief 按长名查找下标。
     *
     * @param name 长名。
     * @return 找到返回下标，否则返回 kInvalidIndex。
     */
    std::size_t FindIndex(const std::string& name) const;

    /**
     * @brief 每轮解析前复位可变状态。
     *
     * 清空位置参数与错误列表，并把每个选项的 set 复位为 false、actual 复位为 def；
     * 不复位 def、程序名与 footer。可重复调用。
     */
    void BeginParse();

    /**
     * @brief 解析参数数组。
     *
     * @param argc 参数个数。
     * @param argv 参数数组。
     * @param has_program_name argv[0] 是否为程序名。
     * @return 全部选项合法时返回 true。
     */
    bool ParseArguments(int argc, const char* const argv[], bool has_program_name);

    /**
     * @brief 切分并解析命令行字符串。
     *
     * @param command_line 命令行字符串。
     * @return 切分与解析均成功时返回 true。
     */
    bool ParseCommandLine(const std::string& command_line);

    /**
     * @brief 重建短名索引并检测短名歧义。
     *
     * @return 无短名歧义时返回 true。
     */
    bool RebuildShortIndex();

    /**
     * @brief 遍历参数并分派长/短选项。
     *
     * @param argc 参数个数。
     * @param argv 参数数组。
     * @param first_argument 起始下标。
     */
    void ParseTokens(int argc, const char* const argv[], int first_argument);

    /**
     * @brief 处理一个长选项，必要时消费下一个参数作为值。
     *
     * @param argc 参数个数。
     * @param argv 参数数组。
     * @param index 当前下标；消费下一个参数时自增。
     */
    void HandleLongOption(int argc, const char* const argv[], int& index);

    /**
     * @brief 处理一个短选项（可能为捆绑形式）。
     *
     * @param argc 参数个数。
     * @param argv 参数数组。
     * @param index 当前下标；消费下一个参数时自增。
     */
    void HandleShortOption(int argc, const char* const argv[], int& index);

    /**
     * @brief 处理捆绑短选项中的非末位字符。
     *
     * @param short_name 短名。
     */
    void HandleBundledShortOption(char short_name);

    /**
     * @brief 处理短选项的末位字符，必要时消费下一个参数作为值。
     *
     * @param argc 参数个数。
     * @param argv 参数数组。
     * @param index 当前下标；消费下一个参数时自增。
     */
    void HandleLastShortOption(int argc, const char* const argv[], int& index);

    /**
     * @brief 把选项标记为已设置；带值选项记为缺少值。
     *
     * @param name 长名。
     */
    void SetOptionWithoutValue(const std::string& name);

    /**
     * @brief 解析并写入选项值。
     *
     * @param name 长名。
     * @param value 文本值。
     */
    void SetOptionValue(const std::string& name, const std::string& value);

    /**
     * @brief 按值类别与校验规则解析文本值。
     *
     * @param option 选项定义。
     * @param text 待解析文本。
     * @param out 解析结果。
     * @return 解析且校验通过返回 true。
     */
    bool ReadOptionValue(const Option& option, const std::string& text, Value& out) const;

    /**
     * @brief 追加必需项缺失错误。
     */
    void AppendMissingRequiredErrors();

    /**
     * @brief 按长名与值类别读取选项值。
     *
     * @param name 长名。
     * @param kind 调用方期望的值类别。
     * @param out 输出缓冲；写入类型由 kind 决定。
     * @return 选项存在、有值且类型匹配时返回 true。
     */
    bool TryGetValue(const std::string& name, ValueKind kind, void* out) const;

    /**
     * @brief 按类型把选项值写入输出缓冲。
     *
     * @tparam T 目标类型。
     * @param value 选项值。
     * @param out 输出缓冲。
     * @return 值类型匹配返回 true。
     */
    template <class T>
    static bool CopyOut(const Value& value, void* out) {
        const T* typed = std::get_if<T>(&value);
        if (typed == nullptr) {
            return false;
        }

        *static_cast<T*>(out) = *typed;
        return true;
    }

    /**
     * @brief 切分命令行字符串。
     *
     * @param command_line 命令行字符串。
     * @param tokens 输出 token 列表。
     * @param error 失败原因。
     * @return 成功返回 true。
     */
    static bool SplitArguments(const std::string& command_line, std::vector<std::string>& tokens, std::string& error);

    /**
     * @brief 从 argv[0] 提取程序名。
     *
     * @param path 程序路径。
     * @return 去掉目录与扩展名的程序名。
     */
    static std::string DeriveProgramName(const char* path);

    /**
     * @brief 去掉一个前导 '+' 并给出可解析区间。
     *
     * @param text 待解析文本。
     * @param begin 输出起始指针。
     * @param end 输出结束指针。
     * @return 去除前导 '+' 后仍有内容返回 true。
     */
    static bool NumericRange(const std::string& text, const char*& begin, const char*& end);

    /**
     * @brief 解析有符号整数，要求完整消费文本。
     *
     * @param text 待解析文本。
     * @param out 解析结果。
     * @return 成功返回 true。
     */
    static bool ParseSignedInteger(const std::string& text, int64_t& out);

    /**
     * @brief 解析无符号整数，要求完整消费文本。
     *
     * @param text 待解析文本。
     * @param out 解析结果。
     * @return 成功返回 true。
     */
    static bool ParseUnsignedInteger(const std::string& text, uint64_t& out);

    /**
     * @brief 解析浮点数，要求完整消费文本。
     *
     * @param text 待解析文本。
     * @param out 解析结果。
     * @return 成功返回 true。
     */
    static bool ParseFloatingNumber(const std::string& text, double& out);

    /**
     * @brief 解析布尔值，接受 1/0/true/false/yes/no。
     *
     * @param text 待解析文本。
     * @param out 解析结果。
     * @return 成功返回 true。
     */
    static bool ParseBooleanValue(const std::string& text, Value& out);

    /**
     * @brief 转为小写 ASCII。
     *
     * @param text 原文本。
     * @return 小写文本。
     */
    static std::string ToLower(std::string text);

    /**
     * @brief 校验有符号区间。
     *
     * @param option 选项定义。
     * @param value 待校验值。
     * @return 在区间内或未配置区间时返回 true。
     */
    static bool InSignedRange(const Option& option, int64_t value);

    /**
     * @brief 校验无符号区间。
     *
     * @param option 选项定义。
     * @param value 待校验值。
     * @return 在区间内或未配置区间时返回 true。
     */
    static bool InUnsignedRange(const Option& option, uint64_t value);

    /**
     * @brief 校验字符串取值集合。
     *
     * @param option 选项定义。
     * @param value 待校验值。
     * @return 在集合内或未配置集合时返回 true。
     */
    static bool MatchesAllowed(const Option& option, const std::string& value);

    /**
     * @brief 值类别的展示名。
     *
     * @param kind 值类别。
     * @return usage 中使用的类型名。
     */
    static std::string ValueKindName(ValueKind kind);

    /**
     * @brief 把选项值格式化为文本。
     *
     * @param kind 值类别。
     * @param value 选项值。
     * @return 展示文本。
     */
    static std::string FormatVariant(ValueKind kind, const Value& value);

    /**
     * @brief 按类型把整型选项值格式化为文本。
     *
     * @tparam T 值类型。
     * @param value 选项值。
     * @return 展示文本；类型不匹配时为空串。
     */
    template <class T>
    static std::string ToText(const Value& value) {
        const T* typed = std::get_if<T>(&value);
        if (typed == nullptr) {
            return std::string();
        }

        return std::to_string(*typed);
    }

    /**
     * @brief 把浮点数格式化为最短文本。
     *
     * @param value 浮点值。
     * @return 展示文本。
     */
    static std::string FormatFloating(double value);

    /**
     * @brief 生成单个选项的 usage 描述。
     *
     * @param option 选项定义。
     * @return 描述文本。
     */
    std::string DescribeOption(const Option& option) const;

    /**
     * @brief 生成 usage 文本。
     *
     * @return usage 文本。
     */
    std::string BuildUsage() const;

    /**
     * @brief 生成选项的展示值；开关返回描述。
     *
     * @param option 选项定义。
     * @return 展示文本。
     */
    std::string DisplayValue(const Option& option) const;

    /**
     * @brief 记录并打印取值失败信息。
     *
     * @param name 长名。
     */
    void ReportLookupFailure(const std::string& name);

    /**
     * @brief 获取第一条错误。
     *
     * @return 第一条错误的副本；无错误时为空字符串。
     */
    std::string FirstError() const;

    /**
     * @brief 拼接全部错误。
     *
     * @return 每条错误一行。
     */
    std::string AllErrors() const;

    // 已注册选项；下标即长名索引值。
    std::vector<Option> options;
    // 长名到下标。
    std::unordered_map<std::string, std::size_t> name_index;
    // 短名到长名；每次 Parse 重建。
    std::unordered_map<char, std::string> short_index;
    // 程序名。
    std::string program_name;
    // usage 末尾附加文本。
    std::string footer;
    // 非选项参数。
    std::vector<std::string> positionals;
    // 错误列表。
    std::vector<std::string> errors;
};

CommandLineParser::CommandLineParser() : impl_(std::make_unique<Impl>()) {}

CommandLineParser::~CommandLineParser() = default;

void CommandLineParser::AddWithoutValueCommand(const OptionDefine& define) {
    impl_->AddFlagOption(define);
}

void CommandLineParser::AddIntRange(const OptionDefine& define, int def, RangeOption<int> range) {
    if (!impl_->AddValueOption(define, ValueSpec{ValueKind::kInt, &def, sizeof(def)})) {
        return;
    }
    impl_->SetLastSignedRange(static_cast<int64_t>(range.low), static_cast<int64_t>(range.high));
}

void CommandLineParser::AddInt64Range(const OptionDefine& define, int64_t def, RangeOption<int64_t> range) {
    if (!impl_->AddValueOption(define, ValueSpec{ValueKind::kInt64, &def, sizeof(def)})) {
        return;
    }
    impl_->SetLastSignedRange(range.low, range.high);
}

void CommandLineParser::AddUint32Range(const OptionDefine& define, uint32_t def, RangeOption<uint32_t> range) {
    if (!impl_->AddValueOption(define, ValueSpec{ValueKind::kUint32, &def, sizeof(def)})) {
        return;
    }
    impl_->SetLastUnsignedRange(range.low, range.high);
}

void CommandLineParser::AddUint64Range(const OptionDefine& define, uint64_t def, RangeOption<uint64_t> range) {
    if (!impl_->AddValueOption(define, ValueSpec{ValueKind::kUint64, &def, sizeof(def)})) {
        return;
    }
    impl_->SetLastUnsignedRange(range.low, range.high);
}

void CommandLineParser::AddStringOneOf(const OptionDefine& define, std::string_view def, std::initializer_list<std::string> allowed) {
    const std::string default_value(def);
    if (!impl_->AddValueOption(define, ValueSpec{ValueKind::kString, &default_value, sizeof(default_value)})) {
        return;
    }
    impl_->SetLastAllowed(allowed);
}

void CommandLineParser::Footer(std::string_view footer) {
    impl_->footer = std::string(footer);
}

void CommandLineParser::SetProgramName(std::string_view name) {
    impl_->program_name = std::string(name);
}

const std::string& CommandLineParser::GetProgramName() const {
    return impl_->program_name;
}

bool CommandLineParser::Parse(int argc, const char* const argv[]) {
    return impl_->ParseArguments(argc, argv, true);
}

bool CommandLineParser::Parse(const std::vector<std::string>& args) {
    if (args.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        impl_->BeginParse();
        impl_->errors.push_back("argument number is too large");
        return false;
    }

    std::vector<const char*> views;
    views.reserve(args.size());
    for (const std::string& arg : args) {
        views.push_back(arg.c_str());
    }

    const int argc = static_cast<int>(views.size());
    const char* const* argv = views.empty() ? nullptr : views.data();
    return impl_->ParseArguments(argc, argv, true);
}

bool CommandLineParser::Parse(const std::string& command_line) {
    return impl_->ParseCommandLine(command_line);
}

void CommandLineParser::ParseCheck(int argc, char** argv) {
    EnsureHelpOption();
    (void)Parse(argc, argv);
    CheckParseResult();
}

void CommandLineParser::ParseCheck(const std::vector<std::string>& args) {
    EnsureHelpOption();
    (void)Parse(args);
    CheckParseResult();
}

void CommandLineParser::ParseCheck(const std::string& command_line) {
    EnsureHelpOption();
    (void)Parse(command_line);
    CheckParseResult();
}

void CommandLineParser::CheckParseResult() {
    if (!Error().empty()) {
        std::cerr << Error() << std::endl << Usage();
        std::exit(EXIT_FAILURE);
    }

    if (IsSet("help")) {
        std::cout << Usage();
        std::exit(EXIT_SUCCESS);
    }
}

void CommandLineParser::ParseGetOrSetBehavior(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) {
        return;
    }

    SetProgramName(Impl::DeriveProgramName(argv[0]));

    if (argc <= 1) {
        return;
    }

    if (argc > 2) {
        ParseCheck(argc, argv);
        return;
    }

    const char* token = argv[1];
    if (token == nullptr) {
        return;
    }

    if (std::strncmp(token, "--", 2) == 0) {
        if (std::strchr(token + 2, '=') != nullptr) {
            ParseCheck(argc, argv);
        } else {
            Print(std::string(token + 2));
        }
        return;
    }

    if (token[0] == '-') {
        if (token[1] == '\0') {
            std::cerr << "error command line param,need a letter,you given:" << token << std::endl;
            std::exit(EXIT_FAILURE);
        }

        if (token[2] != '\0') {
            std::cerr << "error short name,short name must be single letter,you given:" << token << std::endl;
            std::exit(EXIT_FAILURE);
        }

        Print(token[1]);
        return;
    }

    ParseCheck(argc, argv);
}

bool CommandLineParser::Exist(const std::string& name) const {
    return impl_->FindOption(name) != nullptr;
}

bool CommandLineParser::IsSet(const std::string& name) const {
    const Impl::Option* option = impl_->FindOption(name);
    return option != nullptr && option->set;
}

const std::vector<std::string>& CommandLineParser::Rest() const {
    return impl_->positionals;
}

void CommandLineParser::Print(const std::string& name) const {
    const Impl::Option* option = impl_->FindOption(name);
    if (option == nullptr) {
        std::cerr << "there is no flag: --" << name << std::endl << impl_->BuildUsage();
        std::exit(EXIT_FAILURE);
    }

    std::cout << impl_->DisplayValue(*option) << std::endl;
    std::cerr << impl_->BuildUsage() << std::endl;
    std::exit(EXIT_SUCCESS);
}

void CommandLineParser::Print(char short_name) const {
    for (const Impl::Option& option : impl_->options) {
        if (option.short_name != 0 && option.short_name == short_name) {
            Print(option.name);
        }
    }

    std::cerr << "there is no flag: -" << short_name << std::endl << impl_->BuildUsage();
    std::exit(EXIT_FAILURE);
}

std::string CommandLineParser::Usage() const {
    return impl_->BuildUsage();
}

std::string CommandLineParser::Error() const {
    return impl_->FirstError();
}

std::string CommandLineParser::ErrorFull() const {
    return impl_->AllErrors();
}

void CommandLineParser::AddValue(const OptionDefine& define, ValueSpec value) {
    impl_->AddValueOption(define, value);
}

bool CommandLineParser::TryGetRaw(const std::string& name, ValueKind kind, void* out) const {
    return impl_->TryGetValue(name, kind, out);
}

void CommandLineParser::ReportLookupFailure(const std::string& name) const {
    impl_->ReportLookupFailure(name);
}

void CommandLineParser::EnsureHelpOption() {
    if (!Exist("help")) {
        AddWithoutValueCommand(OptionDefine{"help", '?', "print this message"});
    }
}

bool CommandLineParser::Impl::AddValueOption(const OptionDefine& define, const ValueSpec& value) {
    std::string name(define.name);
    if (!CanAddOption(name) || !CanAddShortName(define.short_name)) {
        return false;
    }

    Option option;
    option.name = std::move(name);
    option.short_name = define.short_name;
    option.description = std::string(define.desc);
    option.need = define.need;
    option.has_value = true;
    option.kind = value.kind;

    if (!AssignDefault(option, value)) {
        return false;
    }

    option.actual = option.def;
    AppendOption(std::move(option));
    return true;
}

bool CommandLineParser::Impl::AddFlagOption(const OptionDefine& define) {
    std::string name(define.name);
    if (!CanAddOption(name) || !CanAddShortName(define.short_name)) {
        return false;
    }

    Option option;
    option.name = std::move(name);
    option.short_name = define.short_name;
    option.description = std::string(define.desc);
    option.need = false;
    option.has_value = false;
    option.kind = ValueKind::kNone;
    AppendOption(std::move(option));
    return true;
}

bool CommandLineParser::Impl::CanAddOption(const std::string& name) {
    if (name.empty()) {
        std::cerr << "option name must not be empty" << std::endl;
        return false;
    }

    if (name_index.find(name) != name_index.end()) {
        std::cerr << "multiple definition: " << name << std::endl;
        return false;
    }

    return true;
}

bool CommandLineParser::Impl::CanAddShortName(char short_name) const {
    if (short_name == 0) {
        return true;
    }

    for (const Option& option : options) {
        if (option.short_name == short_name) {
            std::cerr << "short option '" << short_name << "' is ambiguous" << std::endl;
            return false;
        }
    }

    return true;
}

void CommandLineParser::Impl::AppendOption(Option option) {
    const std::size_t index = options.size();
    name_index[option.name] = index;
    options.push_back(std::move(option));
}

bool CommandLineParser::Impl::AssignDefault(Option& option, const ValueSpec& value) {
    switch (value.kind) {
        case ValueKind::kString:
            return CopyDefault<std::string>(option.def, value.default_value, value.value_size);
        case ValueKind::kBool:
            return CopyDefault<bool>(option.def, value.default_value, value.value_size);
        case ValueKind::kInt:
            return CopyDefault<int>(option.def, value.default_value, value.value_size);
        case ValueKind::kInt64:
            return CopyDefault<int64_t>(option.def, value.default_value, value.value_size);
        case ValueKind::kUint32:
            return CopyDefault<uint32_t>(option.def, value.default_value, value.value_size);
        case ValueKind::kUint64:
            return CopyDefault<uint64_t>(option.def, value.default_value, value.value_size);
        case ValueKind::kFloat:
            return CopyDefault<float>(option.def, value.default_value, value.value_size);
        case ValueKind::kDouble:
            return CopyDefault<double>(option.def, value.default_value, value.value_size);
        case ValueKind::kNone:
            break;
    }

    return false;
}

void CommandLineParser::Impl::SetLastSignedRange(int64_t low, int64_t high) {
    Option& option = options.back();
    option.has_range = true;
    option.low_signed = low;
    option.high_signed = high;
}

void CommandLineParser::Impl::SetLastUnsignedRange(uint64_t low, uint64_t high) {
    Option& option = options.back();
    option.has_range = true;
    option.low_unsigned = low;
    option.high_unsigned = high;
}

void CommandLineParser::Impl::SetLastAllowed(std::initializer_list<std::string> allowed) {
    Option& option = options.back();
    option.has_allowed = true;
    option.allowed.assign(allowed.begin(), allowed.end());
}

const CommandLineParser::Impl::Option* CommandLineParser::Impl::FindOption(const std::string& name) const {
    const std::size_t index = FindIndex(name);
    if (index == kInvalidIndex) {
        return nullptr;
    }
    return &options[index];
}

CommandLineParser::Impl::Option* CommandLineParser::Impl::FindMutableOption(const std::string& name) {
    const std::size_t index = FindIndex(name);
    if (index == kInvalidIndex) {
        return nullptr;
    }
    return &options[index];
}

std::size_t CommandLineParser::Impl::FindIndex(const std::string& name) const {
    const auto found = name_index.find(name);
    if (found == name_index.end()) {
        return kInvalidIndex;
    }
    return found->second;
}

void CommandLineParser::Impl::BeginParse() {
    positionals.clear();
    errors.clear();
    for (Option& option : options) {
        option.set = false;
        option.actual = option.def;
    }
}

bool CommandLineParser::Impl::ParseArguments(int argc, const char* const argv[], bool has_program_name) {
    BeginParse();

    if (argc < 1 || argv == nullptr || argv[0] == nullptr) {
        errors.push_back("argument number must be longer than 0");
        return false;
    }

    for (int i = 0; i < argc; ++i) {
        if (argv[i] == nullptr) {
            errors.push_back("argument must not be empty");
            return false;
        }
    }

    if (has_program_name && program_name.empty()) {
        program_name = DeriveProgramName(argv[0]);
    }

    if (!RebuildShortIndex()) {
        return false;
    }

    const int first_argument = has_program_name ? 1 : 0;
    ParseTokens(argc, argv, first_argument);
    AppendMissingRequiredErrors();
    return errors.empty();
}

bool CommandLineParser::Impl::ParseCommandLine(const std::string& command_line) {
    std::vector<std::string> tokens;
    std::string split_error;
    if (!SplitArguments(command_line, tokens, split_error)) {
        BeginParse();
        errors.push_back(std::move(split_error));
        return false;
    }

    std::vector<const char*> views;
    views.reserve(tokens.size());
    for (const std::string& token : tokens) {
        views.push_back(token.c_str());
    }

    const int argc = static_cast<int>(views.size());
    const char* const* argv = views.empty() ? nullptr : views.data();
    return ParseArguments(argc, argv, false);
}

bool CommandLineParser::Impl::RebuildShortIndex() {
    short_index.clear();
    for (const Option& option : options) {
        if (option.short_name == 0) {
            continue;
        }

        const auto inserted = short_index.emplace(option.short_name, option.name);
        if (!inserted.second) {
            errors.push_back(std::string("short option '") + option.short_name + "' is ambiguous");
            return false;
        }
    }

    return true;
}

void CommandLineParser::Impl::ParseTokens(int argc, const char* const argv[], int first_argument) {
    for (int i = first_argument; i < argc; ++i) {
        const char* token = argv[i];
        if (token[0] == '-' && token[1] == '-') {
            HandleLongOption(argc, argv, i);
        } else if (token[0] == '-') {
            HandleShortOption(argc, argv, i);
        } else {
            positionals.emplace_back(token);
        }
    }
}

void CommandLineParser::Impl::HandleLongOption(int argc, const char* const argv[], int& index) {
    const char* token = argv[index];
    const char* equals = std::strchr(token + 2, '=');
    if (equals != nullptr) {
        const std::string name(token + 2, static_cast<std::size_t>(equals - token - 2));
        SetOptionValue(name, std::string(equals + 1));
        return;
    }

    const std::string name(token + 2);
    const Option* option = FindOption(name);
    if (option == nullptr) {
        errors.push_back("undefined option: --" + name);
        return;
    }

    if (!option->has_value) {
        SetOptionWithoutValue(name);
        return;
    }

    if (index + 1 >= argc) {
        errors.push_back("option needs value: --" + name);
        return;
    }

    ++index;
    SetOptionValue(name, argv[index]);
}

void CommandLineParser::Impl::HandleShortOption(int argc, const char* const argv[], int& index) {
    const char* token = argv[index];
    if (token[1] == '\0') {
        return;
    }

    for (int i = 2; token[i] != '\0'; ++i) {
        HandleBundledShortOption(token[i - 1]);
    }

    HandleLastShortOption(argc, argv, index);
}

void CommandLineParser::Impl::HandleBundledShortOption(char short_name) {
    const auto found = short_index.find(short_name);
    if (found == short_index.end()) {
        errors.push_back(std::string("undefined short option: -") + short_name);
        return;
    }

    SetOptionWithoutValue(found->second);
}

void CommandLineParser::Impl::HandleLastShortOption(int argc, const char* const argv[], int& index) {
    const char* token = argv[index];
    const char short_name = token[std::strlen(token) - 1];

    const auto found = short_index.find(short_name);
    if (found == short_index.end()) {
        errors.push_back(std::string("undefined short option: -") + short_name);
        return;
    }

    const Option* option = FindOption(found->second);
    if (option == nullptr) {
        return;
    }

    if (!option->has_value) {
        SetOptionWithoutValue(found->second);
        return;
    }

    if (index + 1 >= argc) {
        errors.push_back("option needs value: --" + option->name);
        return;
    }

    ++index;
    SetOptionValue(found->second, argv[index]);
}

void CommandLineParser::Impl::SetOptionWithoutValue(const std::string& name) {
    Option* option = FindMutableOption(name);
    if (option == nullptr) {
        errors.push_back("undefined option: --" + name);
        return;
    }

    if (option->has_value) {
        errors.push_back("option needs value: --" + name);
        return;
    }

    option->set = true;
}

void CommandLineParser::Impl::SetOptionValue(const std::string& name, const std::string& value) {
    Option* option = FindMutableOption(name);
    if (option == nullptr) {
        errors.push_back("undefined option: --" + name);
        return;
    }

    if (!option->has_value) {
        errors.push_back("option does not take value: --" + name + "=" + value);
        return;
    }

    Value parsed;
    if (!ReadOptionValue(*option, value, parsed)) {
        errors.push_back("option value is invalid: --" + name + "=" + value);
        return;
    }

    option->actual = std::move(parsed);
    option->set = true;
}

bool CommandLineParser::Impl::ReadOptionValue(const Option& option, const std::string& text, Value& out) const {
    switch (option.kind) {
        case ValueKind::kString:
            if (!MatchesAllowed(option, text)) {
                return false;
            }
            out = text;
            return true;
        case ValueKind::kBool:
            return ParseBooleanValue(text, out);
        case ValueKind::kInt: {
            int64_t parsed = 0;
            if (!ParseSignedInteger(text, parsed) || !InSignedRange(option, parsed)) {
                return false;
            }
            if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
                return false;
            }
            out = static_cast<int>(parsed);
            return true;
        }
        case ValueKind::kInt64: {
            int64_t parsed = 0;
            if (!ParseSignedInteger(text, parsed) || !InSignedRange(option, parsed)) {
                return false;
            }
            out = parsed;
            return true;
        }
        case ValueKind::kUint32: {
            uint64_t parsed = 0;
            if (!ParseUnsignedInteger(text, parsed) || !InUnsignedRange(option, parsed)) {
                return false;
            }
            if (parsed > std::numeric_limits<uint32_t>::max()) {
                return false;
            }
            out = static_cast<uint32_t>(parsed);
            return true;
        }
        case ValueKind::kUint64: {
            uint64_t parsed = 0;
            if (!ParseUnsignedInteger(text, parsed) || !InUnsignedRange(option, parsed)) {
                return false;
            }
            out = parsed;
            return true;
        }
        case ValueKind::kFloat: {
            double parsed = 0.0;
            if (!ParseFloatingNumber(text, parsed)) {
                return false;
            }
            if (parsed < -std::numeric_limits<float>::max() || parsed > std::numeric_limits<float>::max()) {
                return false;
            }
            out = static_cast<float>(parsed);
            return true;
        }
        case ValueKind::kDouble: {
            double parsed = 0.0;
            if (!ParseFloatingNumber(text, parsed)) {
                return false;
            }
            out = parsed;
            return true;
        }
        case ValueKind::kNone:
            break;
    }

    return false;
}

void CommandLineParser::Impl::AppendMissingRequiredErrors() {
    for (const Option& option : options) {
        if (option.has_value && option.need && !option.set) {
            errors.push_back("need option: --" + option.name);
        }
    }
}

bool CommandLineParser::Impl::TryGetValue(const std::string& name, ValueKind kind, void* out) const {
    const Option* option = FindOption(name);
    if (option == nullptr || out == nullptr || !option->has_value || option->kind != kind) {
        return false;
    }

    switch (kind) {
        case ValueKind::kString:
            return CopyOut<std::string>(option->actual, out);
        case ValueKind::kBool:
            return CopyOut<bool>(option->actual, out);
        case ValueKind::kInt:
            return CopyOut<int>(option->actual, out);
        case ValueKind::kInt64:
            return CopyOut<int64_t>(option->actual, out);
        case ValueKind::kUint32:
            return CopyOut<uint32_t>(option->actual, out);
        case ValueKind::kUint64:
            return CopyOut<uint64_t>(option->actual, out);
        case ValueKind::kFloat:
            return CopyOut<float>(option->actual, out);
        case ValueKind::kDouble:
            return CopyOut<double>(option->actual, out);
        case ValueKind::kNone:
            break;
    }

    return false;
}

bool CommandLineParser::Impl::SplitArguments(const std::string& command_line, std::vector<std::string>& tokens, std::string& error) {
    tokens.clear();
    std::string buffer;
    bool in_quote = false;
    bool token_started = false;

    for (std::size_t i = 0; i < command_line.size(); ++i) {
        const char character = command_line[i];
        if (character == '"') {
            in_quote = !in_quote;
            token_started = true;
            continue;
        }

        if (character == ' ' && !in_quote) {
            if (token_started) {
                tokens.push_back(buffer);
                buffer.clear();
                token_started = false;
            }
            continue;
        }

        if (character == '\\') {
            ++i;
            if (i >= command_line.size()) {
                error = "unexpected occurrence of '\\' at end of string";
                return false;
            }
            buffer.push_back(command_line[i]);
            token_started = true;
            continue;
        }

        buffer.push_back(character);
        token_started = true;
    }

    if (in_quote) {
        error = "quote is not closed";
        return false;
    }

    if (token_started) {
        tokens.push_back(buffer);
    }

    return true;
}

std::string CommandLineParser::Impl::DeriveProgramName(const char* path) {
    if (path == nullptr) {
        return std::string();
    }

    const std::string full_name(path);
    const std::size_t slash = full_name.find_last_of("/\\");
    std::string name = slash == std::string::npos ? full_name : full_name.substr(slash + 1);

    const std::size_t dot = name.find('.');
    if (dot != std::string::npos) {
        name = name.substr(0, dot);
    }

    return name;
}

bool CommandLineParser::Impl::NumericRange(const std::string& text, const char*& begin, const char*& end) {
    if (text.empty()) {
        return false;
    }

    begin = text.data();
    end = begin + text.size();
    if (*begin == '+') {
        ++begin;
    }

    return begin != end;
}

bool CommandLineParser::Impl::ParseSignedInteger(const std::string& text, int64_t& out) {
    const char* begin = nullptr;
    const char* end = nullptr;
    if (!NumericRange(text, begin, end)) {
        return false;
    }

    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc() && result.ptr == end;
}

bool CommandLineParser::Impl::ParseUnsignedInteger(const std::string& text, uint64_t& out) {
    const char* begin = nullptr;
    const char* end = nullptr;
    if (!NumericRange(text, begin, end)) {
        return false;
    }

    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc() && result.ptr == end;
}

bool CommandLineParser::Impl::ParseFloatingNumber(const std::string& text, double& out) {
    const char* begin = nullptr;
    const char* end = nullptr;
    if (!NumericRange(text, begin, end)) {
        return false;
    }

    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc() && result.ptr == end;
}

bool CommandLineParser::Impl::ParseBooleanValue(const std::string& text, Value& out) {
    const std::string lowered = ToLower(text);
    if (lowered == "1" || lowered == "true" || lowered == "yes") {
        out = true;
        return true;
    }

    if (lowered == "0" || lowered == "false" || lowered == "no") {
        out = false;
        return true;
    }

    return false;
}

std::string CommandLineParser::Impl::ToLower(std::string text) {
    for (char& character : text) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return text;
}

bool CommandLineParser::Impl::InSignedRange(const Option& option, int64_t value) {
    if (!option.has_range) {
        return true;
    }
    return value >= option.low_signed && value <= option.high_signed;
}

bool CommandLineParser::Impl::InUnsignedRange(const Option& option, uint64_t value) {
    if (!option.has_range) {
        return true;
    }
    return value >= option.low_unsigned && value <= option.high_unsigned;
}

bool CommandLineParser::Impl::MatchesAllowed(const Option& option, const std::string& value) {
    if (!option.has_allowed) {
        return true;
    }
    return std::find(option.allowed.begin(), option.allowed.end(), value) != option.allowed.end();
}

std::string CommandLineParser::Impl::ValueKindName(ValueKind kind) {
    switch (kind) {
        case ValueKind::kNone:
            return std::string();
        case ValueKind::kString:
            return "string";
        case ValueKind::kBool:
            return "bool";
        case ValueKind::kInt:
            return "int";
        case ValueKind::kInt64:
            return "int64";
        case ValueKind::kUint32:
            return "uint32";
        case ValueKind::kUint64:
            return "uint64";
        case ValueKind::kFloat:
            return "float";
        case ValueKind::kDouble:
            return "double";
    }

    return std::string();
}

std::string CommandLineParser::Impl::FormatVariant(ValueKind kind, const Value& value) {
    switch (kind) {
        case ValueKind::kNone:
            break;
        case ValueKind::kString: {
            const std::string* text = std::get_if<std::string>(&value);
            return text != nullptr ? *text : std::string();
        }
        case ValueKind::kBool: {
            const bool* flag = std::get_if<bool>(&value);
            return flag != nullptr ? (*flag ? "true" : "false") : std::string();
        }
        case ValueKind::kInt:
            return ToText<int>(value);
        case ValueKind::kInt64:
            return ToText<int64_t>(value);
        case ValueKind::kUint32:
            return ToText<uint32_t>(value);
        case ValueKind::kUint64:
            return ToText<uint64_t>(value);
        case ValueKind::kFloat: {
            const float* number = std::get_if<float>(&value);
            return number != nullptr ? FormatFloating(static_cast<double>(*number)) : std::string();
        }
        case ValueKind::kDouble: {
            const double* number = std::get_if<double>(&value);
            return number != nullptr ? FormatFloating(*number) : std::string();
        }
    }

    return std::string();
}

std::string CommandLineParser::Impl::FormatFloating(double value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc()) {
        return std::string();
    }
    return std::string(buffer, result.ptr);
}

std::string CommandLineParser::Impl::DescribeOption(const Option& option) const {
    if (!option.has_value) {
        return option.description;
    }

    std::string description = option.description;
    description += " (";
    description += ValueKindName(option.kind);
    if (!option.need) {
        description += " [=";
        description += FormatVariant(option.kind, option.def);
        description += "]";
    }
    description += ")";
    return description;
}

std::string CommandLineParser::Impl::BuildUsage() const {
    std::string usage = "usage: " + program_name + " ";
    for (const Option& option : options) {
        if (!option.need) {
            continue;
        }

        usage += "--" + option.name;
        if (option.has_value) {
            usage += "=" + ValueKindName(option.kind);
        }
        usage += " ";
    }

    usage += "[options] ... " + footer + "\n";
    usage += "options:\n";

    std::size_t max_width = 0;
    for (const Option& option : options) {
        max_width = std::max(max_width, option.name.size());
    }

    for (const Option& option : options) {
        if (option.short_name != 0) {
            usage += "  -";
            usage += option.short_name;
            usage += ", ";
        } else {
            usage += "      ";
        }

        usage += "--" + option.name;
        for (std::size_t i = option.name.size(); i < max_width + 4; ++i) {
            usage += ' ';
        }
        usage += DescribeOption(option);
        usage += "\n";
    }

    return usage;
}

std::string CommandLineParser::Impl::DisplayValue(const Option& option) const {
    if (!option.has_value) {
        return option.description;
    }
    return FormatVariant(option.kind, option.actual);
}

void CommandLineParser::Impl::ReportLookupFailure(const std::string& name) {
    const Option* option = FindOption(name);
    std::string message;
    if (option == nullptr) {
        message = "there is no flag: --" + name;
    } else if (!option->has_value) {
        message = "flag '" + name + "' has no value";
    } else {
        message = "type mismatch flag '" + name + "'";
    }

    errors.push_back(message);
    std::cerr << message << std::endl;
}

std::string CommandLineParser::Impl::FirstError() const {
    return errors.empty() ? std::string() : errors.front();
}

std::string CommandLineParser::Impl::AllErrors() const {
    std::string result;
    for (const std::string& error : errors) {
        result += error;
        result += '\n';
    }
    return result;
}

NAMESPACE_EVEREST_END
