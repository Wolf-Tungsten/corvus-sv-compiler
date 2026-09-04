# GrhSIM IR Overview

本文定义 GrhSIM IR 的通用数据结构、执行语义和扩展边界。具体方言、后端映射和 pass 系统在
各自的子目录中定义。

## 1. 总体结构

GrhSIM IR 由一个仿真模型和一个或多个后端映射组成：

```text
GrhSimIR
  model: SimModel
  backends: BackendId -> B_<backend>

SimModel
  I: InputObject[]
  O: OutputObject[]
  S: StateObject[]
  F: ExternFunction[]
  G: SimGraph
  Init: StateId -> InitSpec
```

- `I`、`O`、`S` 分别记录泛化的输入、输出和持久状态；
- `F` 记录模型调用的外部函数声明，是模型级声明，不是 `G` 的图顶点；
- `G` 是仿真流图，op 是顶点，value 是 op 之间的数据流边；
- `Init` 完整定义 `S` 的初始状态；
- `backends` 不能为空，每个目标后端在其中有且只有一个专属的 `B_<backend>`；
- `B_<backend>` 的内部结构由该后端定义，不存在所有后端共享的固定分量。

`GrhSimIR` 只是 `SimModel` 及其后端映射的容器，不增加模型语义。单独的 `SimModel` 可以作为
构建 GrhSIM IR 的输入，但不是完整的 GrhSIM IR。`SimModel` 发生变化后，原有后端映射全部
失效；至少重新生成一个目标后端的映射后，才能形成新的合法 `GrhSimIR`。

## 2. `SimModel`

### 2.1 `I`、`O`、`S` 和 `F`

前三类对象都通过方言类型描述：

```text
InputObject
  id: InputId
  type: TypeRef

OutputObject
  id: OutputId
  type: TypeRef

StateObject
  id: StateId
  type: TypeRef

ExternFunction
  id: FuncId
  decl: DialectFunctionRef
  signature: 由 decl 所属方言定义

ObjectRef = InputId | OutputId | StateId | FuncId
```

| 分量 | 含义 | graph 中的访问方式 |
| --- | --- | --- |
| `I` | 外部输入 | op 读取 |
| `O` | 对外输出 | op 写入 |
| `S` | 持久内部状态 | op 读取和写入 |
| `F` | 外部函数声明 | op 通过 object refs 引用（只读） |

`S` 可以包含 register、latch、memory 和边沿判定所需的历史状态。临时计算结果由 value 传递，
不属于 `S`。

`F` 的每个条目声明一个模型外部实现的函数。`DialectFunctionRef` 由方言名、方言版本和声明
种类名组成，与 `DialectOpRef` 同构；`signature` 的结构由该声明种类定义，描述参数方向、类型
和返回值。`F` 条目只提供声明，本身不产生任何执行行为。

`TypeRef` 由方言名、方言版本和类型名组成，引用一种语义类型。它不描述任何后端的物理表示。

### 2.2 `Init`

```text
Init = StateId -> InitSpec
```

`Init` 是从每个 `StateId` 到其初始化描述的全映射。`InitSpec` 由目标 `StateObject` 的类型所属
方言定义，由一组**有序初始化步骤**组成；步骤按序应用，后者覆盖前者，应用结果必须使该状态
对象的初始内容完全确定——不存在隐式默认值，步骤未覆盖的部分即为非法。

初始化步骤可以引用模型实例创建时才可用的来源，例如随机数或外部文件。因此同一模型的不同
实例可以得到不同的初始状态 `s_init`；这种差异是模型语义的一部分，不是未定义行为。

`Init` 不属于 `G`，后端也不能把初始化语义转移到自己的数据布局中。

### 2.3 `G`

```text
SimGraph
  ops: SimOp[]
  values: SimValue[]

SimOp
  id: OpId
  op_type: DialectOpRef
  operands: ValueId[]
  results: ValueId[]
  object_refs: ObjectRef[]
  parameters: defined by op_type

SimValue
  id: ValueId
  type: TypeRef
```

每个 value 由一个 op 的 `results` 唯一定义，并可被多个 op 的 `operands` 使用。value 的生产者、
使用者和图的邻接关系都从 op 反查，不重复存储。value 只表达 op 之间的数据流，不表示持久
状态；是否为 value 分配物理存储由后端映射决定。

`ops` 的数组顺序不作为普通计算 op 的执行顺序。方言可以规定其中某类可观察 op 必须保持相对
顺序；这种顺序属于 `G` 的语义，后端映射不能改变。

`DialectOpRef` 由方言名、方言版本和 op 名组成。`object_refs` 直接引用 I/O/S/F 对象；每个引用
是读取还是写入，由 `op_type` 规定。`parameters` 只保存常量、slice 范围等不经 value 传递的
静态参数，其名称、类型和含义也由 `op_type` 规定。

## 3. 方言

方言是 op 类型和 I/O/S/value 语义类型的扩展单元：

```text
Dialect
  name: String
  version: String
  op_types: DialectOpType[]
  types: DialectType[]
  function_decls: DialectFunctionDecl[]
```

每种 `DialectOpType` 必须定义：

- operands、results、object refs 和 parameters 的数量及类型；
- 每个 object ref 是读取还是写入；
- results 和 object writes 如何由 operands 与 object reads 决定；
- 若 op 使用事件，哪些 operands 是事件信号、检测什么边沿，以及哪些状态保存边沿历史。
- op 是否具有可观察行为，以及它与同类 op 之间是否存在语义顺序。

每种 `DialectFunctionDecl` 必须定义其 `signature` 的结构，包括参数方向、名称、类型和返回值
表示；声明本身不定义执行行为，外部函数如何绑定到具体实现由后端规定。

每种 `DialectType` 必须定义合法取值；状态类型还必须定义其 `InitSpec` 表示，包括允许的
初始化步骤种类及每个步骤的字段。

方言和后端映射是两个独立扩展点。方言规定 `SimModel` 可以出现哪些 op 和语义类型；后端规定
自己的 `B_<backend>`，并声明支持哪些方言版本。后端可以提供自己的方言，但方言类型仍然是
模型语义类型，不是物理 CPU、GPU 或其他目标类型。

初始 [core 方言](./dialects/core.md) 是 GRH IR 到 `SimModel` 的后端无关承接层。

## 4. 执行语义

### 4.1 单次图状态转移

令 `I`、`O` 和 `S` 同时表示对应对象类型的乘积空间，`i`、`o` 和 `s` 表示一次执行中的具体
取值。`G` 诱导确定的状态转移：

$$
\begin{aligned}
\llbracket G \rrbracket &: I \times S \to O \times S, \\
(o_{n+1}, s_{n+1}) &= \llbracket G \rrbracket(i, s_n).
\end{aligned}
$$

对任意 `i` 和 `s_n`，`G` 必须唯一确定配对的 `o_{n+1}` 与 `s_{n+1}`。该映射只规定结果，
不规定后端内部的分区、调度或物理表示。`Init` 在新模型实例创建时求值，给出该实例的初始
状态 `s_init`；`InitSpec` 含随机数或外部文件来源时，不同实例的 `s_init` 可以不同。

### 4.2 从 `G` 推导静止投影 `E`

`E` 是用于判断模型是否静止的状态投影：

$$
E(s) = (e_{\mathrm{edge}}, e_{\mathrm{out}}).
$$

- `e_edge` 包含所有可能影响边沿事件的状态；
- `e_out` 包含所有可能影响输出的状态。

`E` 不作为 `SimModel` 分量存储，而是从 `G` 推导：

1. 从写入 `O` 的 op 沿 value 边反向遍历，得到输出相关的状态依赖闭包；
2. 从执行边沿判定的 op 沿事件 operand 反向遍历，得到边沿相关的状态依赖闭包；
3. 遍历到读取某个 `S` 对象的 op 后，继续从写入该对象的 op 沿 operands 和其他状态读取反向
   传播，直到闭包不再变化。

两个闭包分别确定 `e_out` 和 `e_edge`。不得遗漏闭包内的状态，也不得随意加入闭包外的状态；
`G` 发生变化后必须重新推导 `E`。

定义 `s ~E t` 表示 `E(s) == E(t)`。`E` 必须满足：对于任意 `i`、`s` 和 `t`，如果
`s ~E t`，则二者经过一次 `G` 转移后产生相同输出和相同的下一静止投影。

### 4.3 `eval`

一次 `eval` 在固定输入下重复应用 `G`，直到相邻两次状态的 `E` 相同：

$$
\operatorname{Eval}: I \times S \rightharpoonup O \times S.
$$

令 `eval_G(i, s)` 表示应用一次 `G` 状态转移，`eval` 等价于：

```text
while (true) {
    (o, s_next) = eval_G(i, s)
    if (E(s_next) == E(s)) {
        return (o, s_next)
    }
    s = s_next
}
```

输入 `i` 在整个调用期间保持不变，中间输出不对外发布。如果不存在有限次迭代使 `E` 稳定，
该次 `eval` 不收敛。

新实例以 `Init` 求值得到的 `s_init` 为状态。调用方设置输入后调用 `eval`，返回的状态成为下一次
调用的起始状态；时钟、复位和其他时间推进由调用方改变输入后再次调用 `eval` 表达。

## 5. 后端映射 `B`

`B_<backend>` 只描述当前 `SimModel` 如何映射到一个目标后端，不能改变模型语义。不同后端的
映射结构可以完全不同，不要求共享固定字段。

每个映射必须只引用当前模型中存在的对象、op、value 和类型，并且目标后端必须支持模型使用
的全部方言版本以及其中实际出现的 op、类型和 parameter 取值。模型改变后，对应
`B_<backend>` 必须重新生成。

[CPU backend](./backends/cpu.md) 是一个具体案例，其 `B_cpu` 结构只适用于 CPU，不构成其他
后端的模板。

## 6. Pass 系统

GrhSIM IR pass 可以改写 `SimModel`、构建后端映射或改写一个已有后端映射。Python 负责选择
pass、提供参数和决定执行顺序；pass 本身可以由 C++ 或 Python 实现。

`SimModel` 被改写后，pass manager 必须使所有旧后端映射失效，并在返回新的 `GrhSimIR` 前
重新生成目标后端映射。后端 pass 只能修改自己的 `B_<backend>`，不能反向改变 `SimModel`。
完整接口见 [Pass System](./passes/overview.md)。

## 7. 验证

完整的 `GrhSimIR` 必须至少包含一个后端映射。

`SimModel` 必须满足：

- I/O/S/F、op 和 value ID 唯一且引用有效；
- `F` 中每个 `ExternFunction` 的 `decl` 可解析到已加载方言的声明种类，且 `signature` 满足该
  种类的定义；
- `Init` 完整覆盖 `S`，每个 `InitSpec` 的步骤合法、类型正确，且应用后完整覆盖目标状态；
- 每个 `DialectOpRef` 和 `TypeRef` 都能在对应方言版本中找到；
- value 唯一定义，op 的 operands、results、object refs 和 parameters 满足其方言定义；
- output 和 state 写入具有确定语义；
- 方言规定的可观察 op 顺序明确；
- `E` 可以从 `G` 推导并满足静止判断的充分性要求。

每个 `B_<backend>` 必须满足：

- 只引用当前 `SimModel` 中存在的 ID 和类型；
- 对应后端支持 `SimModel` 使用的全部方言版本及其中实际出现的 op、类型和 parameter 取值；
- 后端专属验证规则成立。

## 8. 文档

- [Core Dialect](./dialects/core.md)
- [CPU Backend](./backends/cpu.md)
- [Pass System](./passes/overview.md)
- [GRH IR](../grh/grh-ir.md)
