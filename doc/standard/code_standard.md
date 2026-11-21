# 项目服务器编码规范

[named_standard]: ./cpp_style_guide.md
[comment_standard]: ./cpp_style_guide.md

> 本仓库服务器标准分为三份,关系如下:
> - **编码规范**(本文)——写代码时必须遵守的硬约束与写法。
> - **review 规范**——审查代码的检查清单,从系统视角提问;review 首先要遵循本编码规范。
> - **重构规范**——保持逻辑不变前提下的改写约束;重构首先要遵循本编码规范。

## 通用规范

**代码首先是工程产物，同时也是团队协作中的表达。正确只是底线，清晰、整洁、易读、易改是长期维护质量的一部分。**

1. 异步调用
	
	1. cpp中异步回调需要显式的列出捕获变量，禁止捕获指针、引用、局部变量等生命周期不确定的变量

	2. lua中异步回调禁止捕获Account对象、Guild对象、或者这些对象的子对象等，回调函数被调用时可能这些对象的数据已经存库、从管理器中移除了，同样要捕获accountId、guildId等，再从回调函数内获取这些对象
	
	3. lua中异步回调同样禁止捕获通过cpp函数获取的指针

	6. 同步调用禁止通过 callback / lambda / 闭包 / 回调对象形成调用链。同步流程应使用返回值、输出参数或明确的结果对象表达执行结果，不能把立即执行、立即返回的逻辑伪装成异步回调。callback 只应用于真正的异步回调、事件通知、定时器、延迟执行、框架要求的注册接口、遍历器 / visitor / filter / map 等函数式工具、多线程任务完成通知等场景

2. 循环

	1. 对于指定长度的循环，要在外层增加判断指定长度是否超过目标容器长度
	
	2. 配置表指定的长度作为循环终止条件时，代码必须设置以常量形式的最大值，取二者最小，不能无条件信任配置
	
	3. 对于while true等等这类循环必须增加break条件
	
	4. 递归调用要增加递归结束条件
	
	5. 通过迭代器遍历时要注意迭代器失效，通常有两种：循环体内erase有可能失效，循环体内调用其他函数，而其他函数内对容器进行了删除操作
	
3. 反序列化

	1. 根据proto数据进行反序列化时，要保持proto数据的const属性（cpp中）
	
	2. 根据proto数据进行反序列化时，反序列化函数内禁止有逻辑操作，只能是初始化内存值
	
	3. 根据proto数据进行反序列化时，反序列化函数内禁止修改proto值，例如：mutable操作、set操作
	
	4. 根据proto数据进行反序列化时，如果有逻辑要执行时请在下一帧执行，或者反序列化函数调用端的下面增加类似OnUnserializeFinished()函数等
	
4. proto类型参数
	
	1. 禁止对swap过的字段或者proto对象进行读取、写入操作
	2. lua中可以捕获proto类型对象，cpp中禁止捕获proto对象的引用、指针

5. 请求关键字段校验

	1. 网络请求、客户端协议、跨服务请求中的关键标量字段必须做基础合法性校验。数量、ID、索引、枚举、列表长度等字段不能只依赖配置表范围、配置表校验或下游接口失败来兜底；参与资源扣除、资源发放、次数累计、权限判断、数组访问、跨服务入账或 DB 保存的字段，入口处必须先校验基础边界，例如 `count > 0`、ID 有效、枚举合法、列表非空且长度符合预期
	2. 对于来自于客户端的协议，如果监听处理这个协议的服务已经检测了数据合法性，那么下游服务无需检测，例如索引的合法性、字符串的长度、敏感字等
	
6. 玩家获得游戏资源
	
	1. 业务逻辑涉及到购买物品逻辑时，先扣钱，后给物品，不能先给物品
	
	2. 业务逻辑涉及到扣A物品获得B物品时，先扣A物品，后给B物品，不能先给B物品，再扣A物品
	
7. 玩法系统编写
	
	代码文件分为如下：
	
	1. 玩法数据定义：XXX_data.lua或者XXX_component.h或者XXX_data.h，这类文件定义系统的数据结构
	
	2. 玩法系统消息监听：XXX_handle.lua或者XXX_handle.h，这类文件负责监听消息，消息处理，调用对应系统文件内的实际业务函数，对XXX_system.lua内的实际业务处理函数返回码处理，处理包括回包、记录日志等
	
	3. 玩法系统逻辑处理：XXX_system.lua或者XXX_system.h，这类文件是真正处理业务的文件，与handle的交互方式通过PbResultCode或者bool或者自定义数据类型等
	
	4. 为特定服务所提供的请将文件命名为:服务名_系统名_system.h，通用的代码文件则命名为:系统名_common_system.h，例如：logic_lego_system.h、cell_lego_system.h

8. null判断
	
	1. 指针、引用要判空
	2. 获取容器value时，要先判断key对应的value是否存在，或者数组长度和index的比较判断

9. 客户端发起修改字符串类型数据
	
	1. 必须做长度检查
	2. 必须做敏感字检测
	
10. 业务逻辑层禁止有文件io操作
	
	1. 起服阶段不算

11. 代码文件统一使用 UTF-8（无 BOM）；换行符统一使用 LF。只有 .bat 和 .cmd 使用 CRLF，且内容必须为纯 ASCII。`everest/` 已统一为 UTF-8；CRLF 换行在组件重写时转换，未重写文件暂时保持原样。

## cpp规范
	
1. 业务逻辑层禁止使用weak_ptr
	
2. 禁止在构造、析构函数内调用virtual函数
	
3. 如果对象表现出 non-copyable 语义，继承 `NonCopyable` 或显式 `= delete` 拷贝构造/赋值
	
4. 如果对象表现出 only-move-copyable 语义，使用 `= default` 的移动构造/赋值，并 `= delete` 拷贝构造/赋值
	
5. 如果对象表现出single语义，则一定要实现single
	
6. 类的定义必要时才定义析构、拷贝构造等函数，此时必须遵循五法则，非必要时要遵循零法则
	
7. 子类重写基类函数时，函数声明后面一定增加override关键字

8. 虚函数的参数不能有缺省值

9. 使用初始化列表并且按照声明顺序

10. 禁止对有虚函数的类使用memset或者memcopy，只能对表现出trivial的类使用memset，谨慎使用memset，注意扩展带来的影响

11. 非void返回值函数必须有返回值
	
12. 业务逻辑层禁止使用shared_ptr的引用计数做逻辑处理

13. 使用完美转发的函数内禁止使用move操作，否则会改变变量的属性
	
14. 成员变量必须初始化

15. 对于容器或者自定义类型等等作为参数时要使用引用，能加const则加const

16. 成员函数、参数等能加const则加const

17. 注意整型数据的隐式转换带来的隐患，例如用转换后的值作为循环结束条件， 例如转换后的值作为数量

18. 涉及协议、存库、跨平台、跨服务通信的整数类型，应使用固定宽度类型，例如int32_t、uint32_t、int64_t、uint64_t，避免使用平台相关宽度的long、unsigned long等类型

19. 警惕auto掩盖的两类隐患：一是范围for中`for(auto x : container)`会静默拷贝整个元素，遍历不修改时用`const auto&`，需要拷贝才显式写出；二是涉及存库、协议、跨端的字段不要用auto，应显式写出固定宽度类型（如int64_t），避免宽度被悄悄推导错而截断

20. 业务逻辑层使用裸指针时，裸指针只表示观察关系，不表示所有权；禁止通过裸指针管理生命周期，对象生命周期应由管理器、ECS、对象池或明确的所有权类型负责

21. 业务逻辑层禁止直接使用裸new/delete管理对象生命周期；确有必要动态创建对象时，应使用项目统一的管理器、对象池或智能指针封装，释放路径必须明确

22. 禁止业务逻辑层使用const_cast修改const对象；禁止随意使用reinterpret_cast，确有必要时必须说明原因，并限制在底层封装、序列化、网络或第三方库适配层

23. lambda禁止使用默认捕获`[=]`、`[&]`，应显式列出捕获变量；异步、协程、延迟执行场景尤其必须显式捕获，并避免捕获生命周期不确定的对象

24. 涉及到算法的逻辑能使用标准库尽量使用标准库

25. 多态、数据统一管理时析构函数一定增加virtual，无论是不是使用的shared_ptr作为生命周期管理

26. 定义枚举使用enum class，避免裸enum污染命名空间、隐式转int；需要数值时显式转换

27. switch处理枚举时，要么列全所有枚举值（不写default以便编译器对漏分支告警），要么写default并在其中加断言；目的是新增枚举值时不漏改switch，配合状态机避免死状态

28. 浮点数禁止用==或!=直接比较，应使用epsilon容差比较；物理（PhysX）、导航、移动等浮点运算要警惕NaN传播，一个NaN坐标可能让整条物理/寻路链路出错，必要时在输入处做有效性校验

29. 协程与多线程安全：协程中跨co_await挂起点禁止持有裸指针、引用、迭代器等生命周期不确定的对象（与异步回调禁捕指针同源），挂起点恢复后重新获取；共享数据的并发访问要有明确的同步约定；持锁期间禁止调用可能阻塞或回调用户代码的函数

## lua规范
	
1. lua文件内的函数如果不需要对外开放调用，则声明为local function

2. 除模块导出接口外，禁止创建全局变量；文件内变量、函数、常量默认使用local，新增全局变量必须有明确用途和统一命名

3. require的模块结果应保存到local变量中，禁止在热路径反复require或反复通过全局路径查找模块

4. lua文件内代码区规划，按照下面顺序
	
	xxx.lua
	
	------------------------------------------------------------
	require区
	------------------------------------------------------------
	
	------------------------------------------------------------
	文件常量区
	------------------------------------------------------------
	
	------------------------------------------------------------
	local function 区
	------------------------------------------------------------
	
	------------------------------------------------------------
	对外开放代码区
	------------------------------------------------------------

5. nil安全：访问可能为nil的表字段前要先判断，函数入参（尤其是网络来的数据）要做nil校验，避免`attempt to index a nil value`；与cpp侧null判断对称

6. Lua函数返回多个值时，调用方必须明确处理返回值含义；失败返回值应有统一约定，避免同一模块内混用nil、false、错误码、错误字符串等多种失败形式

7. table当作数组使用时，中间禁止置nil挖洞，否则会破坏`#`长度运算和ipairs遍历

8. 同一个table不建议同时作为数组和字典混用；确有必要混用时，不得依赖`#`获取长度

9. 数组遍历使用ipairs或明确的数值for；字典遍历使用pairs，不得依赖pairs的遍历顺序

10. 本项目Lua为5.3，number区分integer与float子类型：做位运算、存库或参与协议序列化时要注意整数与浮点的区别，避免浮点意外参与整数运算或序列化类型不符合预期

11. 当一个table[k,v]（非数组）作为一个字段值时，当key超过三个时要用class代替，如果用class代替，如果有对应的XXX_data.lua文件则将class定义在这个lua文件中，如果没有则创建，如果这个系统只有一到两个这样的table时，可以定义在对应的system文件的[文件常量区]下面，并注释每个key代表含义，如果超过了两个要创建XXX_data.lua文件，将这些table或者class定义在该文件中，下面是个反面例子：
	
	table.insert(vehicleData.applyList, 1, {
	        applicantId = playerRoleId,
	        applyTime = now,
	        applyExpireTime = now + GetApplyExpireSec(),
	        useDurationSec = useDurationSec,
	        isRenewJump = true,
	        autoApprove = false,
	    })
	
	vehicleData.applyList是个数组，数组的元素是个table，{applicantId = playerRoleId,applyTime = now,applyExpireTime = now + GetApplyExpireSec(),useDurationSec = useDurationSec,isRenewJump = true,autoApprove = false}
	这个table可读性很差，这时要考虑将这个table独立为一个class
	
## 命名规范
	
命名规范参考：[命名规范][named_standard]

## 注释规范

cpp注释规范参考：[注释规范][comment_standard]

使用 @ 风格的 Doxygen 命令

cpp例子：
	
	/**
	 * @file calculator.h
	 * @brief 简单计算器接口。
	 */
	
	/**
	 * @brief 计算器错误码。
	 */
	enum class CalculatorError {
	    // 成功。
	    kOK,
	    // 结果指针为空。
	    kNullResult,
	    // 整数溢出。
	    kOverflow,
	};
	
	/**
	 * @brief 简单计算器接口。
	 */
	class Calculator {
	public:
	    // 计算器名称。
	    using Name = std::string;
	
	    /**
	     * @brief 计算两数之和。
	     *
	     * @param a 第一个加数。
	     * @param b 第二个加数。
	     * @param result 结果指针，不能为空。
	     * @return 成功返回 kOK，失败返回对应错误码。
	     * @note 该函数线程安全。
	     * @warning 传入未初始化的指针会导致未定义行为。
	     * @deprecated 建议使用 Subtract()。
	     */
	    static CalculatorError Add(int a, int b, int* result);
	
	private:
	    // 名称；为空表示未初始化。
	    Name name_;
	};

只在 .cpp 中出现的辅助函数在定义处写 Doxygen：

	/**
	 * @brief 校验结果指针是否有效。
	 * @param result 结果指针。
	 * @return 有效返回 true。
	 */
	static bool IsValidResult(const int* result) {
	    return result != nullptr;
	}
	
lua例子
	
	-- 成员所有职业标签权限的并集（不含职位权限）
	---@param guild Guild
	---@param member Member
	---@return integer
	function GuildSystem.GetMemberProfessionPrivileges(guild,member)
	end
	
proto例子

	//创建工会
	message PbPlayerCreateGuildReq {
	    int64 player_id = 1;
	
	    // 公会名称
		string name = 2;
	    
	    // 公会专精
	    int32 specialty_id= 3;
	}


**注释要加到代码上面，而非代码后面**

## 日志规范
	
1. log级别选择要根据实际业务决定，有数据修改的要记录info，还要看数据修改的频率，外部参数错误要记录error，error原因英文简述
2. log内容要增加函数名唯一标识，id靠前，例如playerRoleId要在最前面，如果还有公会id则公会id最前面，字段的旧值，字段的新值，例子如下
	
	log:Info("GuildSystem.ChangeJob guildId:{},approveId:{},targetId:{},targetJob:{},oldJob:{}",guild.id,playerRoleId,targetMemberId,targetJob,old)

3. 禁止日志输出账号密码、token、完整身份证、完整手机号等敏感信息；确需定位问题时应脱敏

4. tick、循环、广播、高频网络消息中禁止无条件打印info或error日志；确需打印时要限频、采样或只在异常状态打印
	
## 编码风格

1. 函数参数建议不超过3个，超过3个可以将参数整理为一个参数类；确有必要超过时需在 review 说明理由
	
2. 函数内的语句块嵌套层次建议在3层以内；确有必要超过时需在 review 说明理由
	
3. 函数使用卫语句
		
	bad:
		void Test(){
			if(a){
				if(b){
					if(c){
						-----
					}
				}
			}
		}
		
	good:
		void Test(){
			if(!a){
				return;
			}
			
			if(!b){
				return;
			}
			
			if(!c){
				return;
			}
			
			//逻辑
			
		}

4. 避免长函数，尽量将里面的逻辑切分，建议一个函数不超过100行，能拆分的则拆分成一个独立的函数；确有必要超过时需在 review 说明理由
	
5. 代码有段落感
	
	bad:
		void Test(){
			if(!a){
				return;
			}
			int p1 = 1;
			int p2 = 2;
			TestOtherFunc(p1,p2);
			auto row = GET_MB(table)->Find(1);
			if(null == row){
				return;
			}
			message clientMessage;
			clientMessage.name = "zhangsan";
			clientMessage.level = 10;
			SendMessage(clientMessage);
		}
		
	good:
		
		void Test(){
			if(!a){
				return;
			}
			
			auto row = GET_MB(table)->Find(1);
			if(null == row){
				return;
			}
			
			int p1 = 1;
			int p2 = 2;
			
			// do what
			TestOtherFunc(p1,p2);
			
			message clientMessage;
			clientMessage.name = p1;
			clientMessage.level = p2;
			
			SendMessage(clientMessage);
		}

6. 编写业务函数时，函数体代码遵循如下顺序，不能更换顺序

		void FunctionName(){
			/* 第1代码块
			卫语句，各种条件判断
			*/
		   
			/* 第2代码块
			算法
			设置内存值
			*/
		  
			/* 第3代码块
			记录日志
			*/
		 
			/* 第4代码块
			数据保存
			*/
			
			/* 第5代码块
			向外抛出事件
			*/
		}

7. 单行代码不超过 180 个字符；超过时换行，续行缩进 4 个空格