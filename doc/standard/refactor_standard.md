# 重构规范

> 本仓库服务器标准分为三份,关系如下:
> - **编码规范**——写代码时必须遵守的硬约束与写法。
> - **review 规范**——审查代码的检查清单,从系统视角提问;review 首先要遵循编码规范。
> - **重构规范**(本文)——保持逻辑不变前提下的改写约束;重构首先要遵循编码规范。

重构首先遵循编码规范

## 保证逻辑不变

1. 代码可修改，但是逻辑不变

2. 重构不得改变外部接口、协议字段、存库字段、配置含义、日志关键字段，确需改变时不属于重构，应按功能修改处理

3. 涉及存档、db proto、pb协议、配置表读取的重构，必须保证旧数据兼容

4. 重构提交中禁止混入功能需求、数值调整、业务规则调整；发现原逻辑bug需要修改时，应单独说明并拆分提交

5. 重构涉及核心玩法、资源变化、跨服务调用时，应说明验证方式，包括测试、日志对比或关键流程自测结果

## 函数切分

重构时如果函数逻辑过长、分支过重，或 callback / lambda / 闭包 / 回调对象内嵌复杂业务，应在保持逻辑不变的前提下切分函数。本规则适用于 C++、Lua 以及其他任何项目和语言；示例使用 Lua 只是为了说明问题，实际执行时应按当前语言和项目的惯用写法落地。函数切分的目标是让入口函数更容易看出主流程，让子函数表达清晰的业务动作，而不是单纯减少行数。

### 切分触发条件

满足以下任一情况时，应优先考虑函数切分：

1. 单个函数包含多个业务阶段，例如校验、扣资源、跨服务调用、回包、日志、补偿；
2. 单个函数同时处理多个业务类型、协议分支或状态分支；
3. `if / elseif / else` 分支体过长，入口函数无法快速看出主流程；
4. callback 中包含业务校验、资源变化、跨服务调用、补偿或回包逻辑；
5. 嵌套层级过深，导致错误路径和主流程混在一起；
6. 函数名描述的是一个入口动作，但函数体内包含多个可以单独命名的业务步骤。

以上条件不是机械行数限制。重构时应以“是否更容易阅读主流程、是否更容易检查错误路径、是否更容易保证逻辑不变”为判断标准。

### 切分原则

1. 优先按业务分支、流程阶段、异步边界切分，例如按捐赠类型、校验阶段、扣资源阶段、RPC callback 阶段拆分。
2. 拆出的函数必须有明确业务含义，函数名应表达完整业务动作。
3. 不应为了减少行数而拆出无业务含义的函数，例如 `Step1`、`Step2`、`HandleData`、`ProcessLogic`。
4. 拆分前后返回值、错误码、日志关键字段、资源变化顺序、跨服务调用时机必须保持一致。
5. 文件级 `local function` 应优先使用显式参数，避免隐式依赖外层变量；如果只服务于当前入口且强依赖当前上下文，可以使用入口函数内部的 local closure。
6. 参数过多时，应重新检查切分边界是否合理；不要为了传参方便引入含义不清的上下文表。
7. 重构历史代码时，如果发现同步流程通过 callback / lambda / 闭包 / 回调对象形成调用链，应在不改变逻辑的前提下改为直接调用、返回值、输出参数或结果对象；如果需要调整接口形态，应按功能修改或专项重构处理。

### 多分支入口函数切分

有些逻辑虽然没有违反编码规范或 review 规则，但从可读性上看仍然很差。例如入口函数同时处理多种业务类型时，主流程会被每个分支的细节淹没：

```lua
---@brief 捐赠给公会
---@param req PbDonationToGuildReq
---@param playerRoleId integer
---@return PbResultCode
function DonationSystem.DonationToGuild(req, playerRoleId)
    local playerRoleData = LogicPlayerRoleManager.GetData(playerRoleId)
    if not playerRoleData or not playerRoleData.donation then
        return PbLoginResultCode.RESULT_CODE_LOGIN_PLAYER_ROLE_DATA_NULL
    end

    if not playerRoleData.guildInfo or playerRoleData.guildInfo.guid <= 0 then
        return PbGuildOldResultCode.RESULT_CODE_PLAYER_HAS_NOT_GUILD
    end

    ResetDonationData(playerRoleData)

    -- 期货
    if req:type() == PbDonationType.DONATION_TYPE_DIGITAL then
        -- 有25行
    -- 道具
    elseif req:type() == PbDonationType.DONATION_TYPE_ITEM then
        -- 有26行
    end

    return PbDonateResultCode.RESULT_CODE_DONATE_GUILD_TYPE_ERROR
end
```

这类函数可以将不同业务类型拆成独立的文件级 `local function`，并通过显式参数表达依赖关系：

```lua
local function DoDonationDigital(req, playerRoleId, playerRoleData)
    -- 原代码的25行
end

local function DoDonationItem(req, playerRoleId, playerRoleData)
    -- 原代码的26行
end

---@brief 捐赠给公会
---@param req PbDonationToGuildReq
---@param playerRoleId integer
---@return PbResultCode
function DonationSystem.DonationToGuild(req, playerRoleId)
    local playerRoleData = LogicPlayerRoleManager.GetData(playerRoleId)
    if not playerRoleData or not playerRoleData.donation then
        return PbLoginResultCode.RESULT_CODE_LOGIN_PLAYER_ROLE_DATA_NULL
    end

    if not playerRoleData.guildInfo or playerRoleData.guildInfo.guid <= 0 then
        return PbGuildOldResultCode.RESULT_CODE_PLAYER_HAS_NOT_GUILD
    end

    ResetDonationData(playerRoleData)

    -- 期货
    if req:type() == PbDonationType.DONATION_TYPE_DIGITAL then
        return DoDonationDigital(req, playerRoleId, playerRoleData)
    -- 道具
    elseif req:type() == PbDonationType.DONATION_TYPE_ITEM then
        return DoDonationItem(req, playerRoleId, playerRoleData)
    end

    return PbDonateResultCode.RESULT_CODE_DONATE_GUILD_TYPE_ERROR
end
```

拆分后，入口函数只保留公共校验和分发逻辑，具体业务分支由命名函数承载。注意成功路径和失败路径都必须保持原返回值，不能因为函数切分而丢失 `return PbResultCode.RESULT_CODE_SUCCESS`。

### Callback 切分

异步 callback 中如果包含资源扣除、状态校验、跨服务调用、回包、释放 token 等业务逻辑，应拆成命名函数。callback 内只保留参数绑定和对命名函数的调用。

例如下面这类 callback 不应长期内嵌在发起 RPC 的函数中：

```lua
---@brief 验证道具捐献Token
---@param guildId integer
---@param playerRoleId integer
---@param type PbDonationType
---@param id integer
---@param count integer
---@param itemGuid integer
local function DonateItemToGuildGetToken(guildId, playerRoleId, type, id, count, itemGuid)
    -- 获取捐赠道具的token
    local ctrl = network.RpcCaller.new()
    ctrl:set_id(guildId)
    ctrl:set_callback(function(code, token)
        -- 下面有50行
    end)
    network.Call(ctrl, service_t.guild, stsmsg.GetGuildDonationItemToken, playerRoleId)
end
```

应拆为类似下面的结构：

```lua
local function OnGetDonationItemToken(guildId, playerRoleId, type, id, count, itemGuid, code, token)
    -- 原代码的50行
end

---@brief 验证道具捐献Token
---@param guildId integer
---@param playerRoleId integer
---@param type PbDonationType
---@param id integer
---@param count integer
---@param itemGuid integer
local function DonateItemToGuildGetToken(guildId, playerRoleId, type, id, count, itemGuid)
    -- 获取捐赠道具的token
    local ctrl = network.RpcCaller.new()
    ctrl:set_id(guildId)
    ctrl:set_callback(function(code, token)
        OnGetDonationItemToken(guildId, playerRoleId, type, id, count, itemGuid, code, token)
    end)
    network.Call(ctrl, service_t.guild, stsmsg.GetGuildDonationItemToken, playerRoleId)
end
```

callback 函数建议使用 `On...` 命名，表示异步返回后的处理逻辑。拆分时要特别检查每个错误路径的回包、日志、资源释放和 return 行为，避免漏掉补偿逻辑。效果图可参考本文件同级目录下的 `lua_code.png`。
